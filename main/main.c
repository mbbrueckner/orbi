/**
 * @file main.c
 * @brief Application entry point: wires the components together and runs the
 *        main loop.
 */

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "input_touch.h"
#include "pet_state.h"
#include "tick_scheduler.h"
#include "ui_render.h"

// TODO: include the individual modules once they exist:
// #include "pet_state.h"
// #include "ui_render.h"
// #include "input_touch.h"
// #include "storage_nvs.h"

/** Tag prefixed to every log line from this file. */
static const char* TAG = "orbi";

/** Pet time runs this much faster than real time. 1 = real time; raise it
 *  temporarily to see the mood change without waiting. */
#define DEMO_SPEEDUP 1

/** Player actions travel from the LVGL task to the main loop through this. */
static QueueHandle_t action_queue;

/** Called from the LVGL task: only queue the action, never touch the pet. */
static void on_ui_action(ui_action_t action) {
  xQueueSend(action_queue, &action, 0);
}

/** Temporary time source until the RTC is wired up. */
static uint32_t demo_time_s(void) {
  return (uint32_t)(esp_timer_get_time() / 1000000) * DEMO_SPEEDUP;
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

  // TODO order according to the roadmap:
  // 1. storage_nvs_init() + load the persisted state
  // 2. Initialize display + LVGL (ui_render_init)
  // 3. Initialize touch and register it as an LVGL input
  // 4. pet_init()
  // 5. Start main loop / tick timer

  action_queue = xQueueCreate(8, sizeof(ui_action_t));
  ui_render_init(on_ui_action);
  input_touch_init(i2c_bus_init());

  pet_state_t pet;
  pet_init(&pet);
  tick_scheduler_t scheduler;
  tick_scheduler_init(&scheduler, demo_time_s());
  ui_render_update(&pet);

  while (1) {
    // Wake up for player actions right away, otherwise tick once a second.
    ui_action_t action;
    if (xQueueReceive(action_queue, &action, pdMS_TO_TICKS(1000)) == pdTRUE) {
      apply_action(&pet, action);
    }
    tick_scheduler_update(&scheduler, &pet, demo_time_s());
    ui_render_update(&pet);
  }
}
