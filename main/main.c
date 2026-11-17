/**
 * @file main.c
 * @brief Application entry point: wires the components together and runs the
 *        main loop.
 */

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "input_touch.h"
#include "lvgl.h"
#include "pet_state.h"
#include "ui_render.h"

// TODO: include the individual modules once they exist:
// #include "pet_state.h"
// #include "ui_render.h"
// #include "input_touch.h"
// #include "storage_nvs.h"

/** Tag prefixed to every log line from this file. */
static const char* TAG = "orbi";

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

  // Temporary demo: run the pet at 30x speed and show its mood.
  ui_render_init();

  // Temporary: a small dot follows the finger to check the touch mapping.
  lv_indev_t* touch = input_touch_init(i2c_bus_init());
  if (touch != NULL) {
    ui_render_lock();
    lv_obj_t* dot = lv_obj_create(lv_layer_sys());
    lv_obj_set_size(dot, 16, 16);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_make(0xFF, 0x30, 0x30), 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_indev_set_cursor(touch, dot);
    ui_render_unlock();
  }

  pet_state_t pet;
  pet_init(&pet);

  while (1) {
    pet_tick(&pet, 30);
    ui_render_update(&pet);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
