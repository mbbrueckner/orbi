/**
 * @file main.c
 * @brief Application entry point: wires the components together and runs the
 *        main loop.
 */

#include <stdbool.h>
#include <stdio.h>

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "input_touch.h"
#include "pet_state.h"
#include "rtc_bm8563.h"
#include "rtc_time.h"
#include "storage_nvs.h"
#include "tick_scheduler.h"
#include "ui_render.h"

/** Tag prefixed to every log line from this file. */
static const char* TAG = "orbi";

/**
 * Pet time runs this many times faster than real time. 1 = real time. For
 * testing, raise it to see moods change and the absence catch-up quickly.
 * The saved time is in the same scale, so keep it unchanged between a save and
 * the next boot (after changing it, erase the flash for a fresh start).
 */
#define TIME_SPEEDUP 1

/**
 * Pet time counts from this moment (2026-01-01), so the sped-up seconds still
 * fit into 32 bits for a few years.
 */
#define TIME_BASE_S 1767225600u

/** Seconds between periodic saves. */
#define SAVE_INTERVAL_S 300
/** After a player action, save as soon as this many seconds passed since the
 *  last save. Keeps flash wear low when someone taps a lot. */
#define MIN_SAVE_GAP_S 5

/** Player actions travel from the LVGL task to the main loop through this. */
static QueueHandle_t action_queue;

/** Called from the LVGL task: only queue the action, never touch the pet. */
static void on_ui_action(ui_action_t action) {
  xQueueSend(action_queue, &action, 0);
}

/** Wall-clock seconds at boot, and the timer value at that moment. */
static uint32_t boot_time_s;
/** False if the RTC had lost its time at boot and had to be reset. */
static bool rtc_was_valid;
static int64_t boot_timer_us;

/**
 * @brief Current wall-clock time in seconds.
 *
 * The RTC is read once at boot; after that the chip's timer counts on, so the
 * I2C bus stays free for the touch controller. Both parts are in pet time, see
 * TIME_SPEEDUP.
 */
static uint32_t now_s(void) {
  uint32_t elapsed_s =
      (uint32_t)((esp_timer_get_time() - boot_timer_us) / 1000000);
  return boot_time_s + elapsed_s * TIME_SPEEDUP;
}

/** Converts real RTC seconds to pet time (sped up, counted from the base). */
static uint32_t to_pet_time(uint32_t rtc_s) {
  uint32_t since_base = rtc_s > TIME_BASE_S ? rtc_s - TIME_BASE_S : 0;
  return since_base * TIME_SPEEDUP;
}

/**
 * @brief Reads the RTC. If it lost its time (never set, or lost power), sets
 *        it to the moment this firmware was built.
 *
 * The build time is only approximate: it is stale if main.c was not rebuilt
 * recently. That is acceptable, the pet only needs a clock that keeps running.
 */
static uint32_t read_boot_time(i2c_master_bus_handle_t bus) {
  uint32_t build_s = 0;
  rtc_datetime_t build;
  if (rtc_parse_build_time(__DATE__, __TIME__, &build)) {
    build_s = rtc_datetime_to_unix(&build);
  }

  if (!rtc_bm8563_init(bus)) {
    ESP_LOGW(TAG, "RTC not available, using the build time");
    return build_s;
  }

  uint32_t rtc_s;
  if (rtc_bm8563_read(&rtc_s)) {
    ESP_LOGI(TAG, "RTC time: %u", (unsigned)rtc_s);
    rtc_was_valid = true;
    return rtc_s;
  }

  ESP_LOGW(TAG, "RTC time not valid, setting it to the build time");
  rtc_bm8563_write(build_s);
  return build_s;
}

/** Writes the pet and the scheduler time to flash. */
static void save_state(const pet_state_t* pet,
                       tick_scheduler_t* scheduler,
                       uint32_t now) {
  storage_record_t record = {.pet = *pet, .last_time_s = scheduler->last_time_s};
  if (storage_nvs_save(&record)) {
    tick_scheduler_mark_saved(scheduler, now);
  }
}

/** Applies one player action to the pet. */
static void apply_action(pet_state_t* pet, ui_action_t action) {
  switch (action) {
    case UI_ACTION_FEED:
      pet_feed(pet);
      break;
    case UI_ACTION_PLAY:
      pet_play(pet);
      break;
    case UI_ACTION_SLEEP:
      pet_sleep(pet, true);
      break;
    case UI_ACTION_WAKE:
      pet_sleep(pet, false);
      break;
  }
}

/** XIAO ESP32-C6 pin D4 (GPIO22): I2C data, shared by touch and RTC. */
#define I2C_SDA_GPIO 22
/** XIAO ESP32-C6 pin D5 (GPIO23): I2C clock. */
#define I2C_SCL_GPIO 23

/** Creates the I2C bus shared by touch controller and RTC. */
static i2c_master_bus_handle_t i2c_bus_init(void) {
  i2c_master_bus_config_t config = {
      .i2c_port = -1,
      .sda_io_num = I2C_SDA_GPIO,
      .scl_io_num = I2C_SCL_GPIO,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = true,
  };
  i2c_master_bus_handle_t bus;
  ESP_ERROR_CHECK(i2c_new_master_bus(&config, &bus));
  return bus;
}

/**
 * @brief ESP-IDF entry point, called by the FreeRTOS startup task.
 *
 * Unlike a plain main(), this must not return: the task it runs in would be
 * torn down. Hence the endless loop at the end.
 */
void app_main(void) {
  ESP_LOGI(TAG, "orbi starting...");

  storage_nvs_init();
  i2c_master_bus_handle_t bus = i2c_bus_init();
  boot_time_s = to_pet_time(read_boot_time(bus));
  boot_timer_us = esp_timer_get_time();

  action_queue = xQueueCreate(8, sizeof(ui_action_t));
  ui_render_init(on_ui_action);
  input_touch_init(bus);

  // Restore the saved pet, or start a new one on a fresh device. The scheduler
  // counts from the saved time, so its first update covers the time the
  // device was switched off.
  pet_state_t pet;
  uint32_t scheduler_start;
  storage_record_t saved;
  if (storage_nvs_load(&saved)) {
    pet = saved.pet;
    scheduler_start = saved.last_time_s;
    ESP_LOGI(TAG, "restored pet, last saved at %u", (unsigned)scheduler_start);
  } else {
    pet_init(&pet);
    scheduler_start = now_s();
    ESP_LOGI(TAG, "no saved pet, starting a new one");
  }
  tick_scheduler_t scheduler;
  tick_scheduler_init(&scheduler, scheduler_start);

  bool dirty = false;
  bool first_update = true;
  while (1) {
    // Wake up for player actions right away, otherwise tick once a second.
    ui_action_t action;
    if (xQueueReceive(action_queue, &action, pdMS_TO_TICKS(1000)) == pdTRUE) {
      apply_action(&pet, action);
      dirty = true;
    }

    uint32_t now = now_s();
    uint32_t applied_s = tick_scheduler_update(&scheduler, &pet, now);
    if (first_update) {
      first_update = false;
      if (rtc_was_valid) {
        ESP_LOGI(TAG, "RTC ok, caught up %us", (unsigned)(applied_s / TIME_SPEEDUP));
      } else {
        ESP_LOGW(TAG, "RTC time invalid, started from the build time");
      }
    }
    ui_render_update(&pet);

    if ((dirty && tick_scheduler_save_due(&scheduler, now, MIN_SAVE_GAP_S)) ||
        tick_scheduler_save_due(&scheduler, now, SAVE_INTERVAL_S)) {
      save_state(&pet, &scheduler, now);
      dirty = false;
    }
  }
}
