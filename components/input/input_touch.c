/**
 * @file input_touch.c
 * @brief Implementation of the touch driver; see input_touch.h.
 */

#include "input_touch.h"

#include "chsc6x_parse.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "touch";

static i2c_master_dev_handle_t touch_dev;

/** Last valid state, reported again when a read fails or is garbage. */
static lv_indev_state_t last_state = LV_INDEV_STATE_RELEASED;
static lv_coord_t last_x;
static lv_coord_t last_y;

/** Called by LVGL (inside lv_timer_handler) to poll the touch position. */
static void read_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  (void)drv;

  uint8_t raw[CHSC6X_REPORT_SIZE];
  uint16_t x;
  uint16_t y;
  esp_err_t err = i2c_master_receive(touch_dev, raw, sizeof(raw), 20);

  if (err != ESP_OK) {
    // Rate limited: this callback runs about every 30 ms.
    static int64_t last_log_us;
    int64_t now_us = esp_timer_get_time();
    if (now_us - last_log_us > 2000000) {
      last_log_us = now_us;
      ESP_LOGD(TAG, "read failed: %s", esp_err_to_name(err));
    }
  } else {
    switch (chsc6x_parse(raw, &x, &y)) {
      case CHSC6X_PRESSED:
        if (last_state != LV_INDEV_STATE_PRESSED) {
          ESP_LOGD(TAG, "pressed at x=%u y=%u", x, y);
        }
        last_state = LV_INDEV_STATE_PRESSED;
        last_x = (lv_coord_t)x;
        last_y = (lv_coord_t)y;
        break;
      case CHSC6X_RELEASED:
        if (last_state != LV_INDEV_STATE_RELEASED) {
          ESP_LOGD(TAG, "released");
        }
        last_state = LV_INDEV_STATE_RELEASED;
        break;
      case CHSC6X_INVALID:
        break;  // keep the previous state
    }
  }

  data->state = last_state;
  data->point.x = last_x;
  data->point.y = last_y;
}

lv_indev_t* input_touch_init(i2c_master_bus_handle_t bus) {
  i2c_device_config_t dev_config = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = CHSC6X_I2C_ADDRESS,
      .scl_speed_hz = 100000,
  };
  esp_err_t err = i2c_master_bus_add_device(bus, &dev_config, &touch_dev);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "add device failed: %s", esp_err_to_name(err));
    return NULL;
  }

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = read_cb;
  lv_indev_t* indev = lv_indev_drv_register(&indev_drv);

  ESP_LOGI(TAG, "touch initialised");
  return indev;
}
