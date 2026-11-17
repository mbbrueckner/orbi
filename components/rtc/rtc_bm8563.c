/**
 * @file rtc_bm8563.c
 * @brief Implementation of the BM8563 driver; see rtc_bm8563.h.
 */

#include "rtc_bm8563.h"

#include "esp_log.h"
#include "rtc_time.h"

#define RTC_I2C_ADDRESS 0x51
#define REG_CONTROL1 0x00
#define REG_SECONDS 0x02
#define TIMEOUT_MS 100

static const char* TAG = "rtc";

static i2c_master_dev_handle_t rtc_dev;

bool rtc_bm8563_init(i2c_master_bus_handle_t bus) {
  i2c_device_config_t config = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = RTC_I2C_ADDRESS,
      .scl_speed_hz = 100000,
  };
  esp_err_t err = i2c_master_bus_add_device(bus, &config, &rtc_dev);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "add device failed: %s", esp_err_to_name(err));
    return false;
  }
  return true;
}

bool rtc_bm8563_read(uint32_t* unix_s) {
  uint8_t reg = REG_SECONDS;
  uint8_t raw[RTC_REGISTER_COUNT];
  esp_err_t err = i2c_master_transmit_receive(
      rtc_dev, &reg, sizeof(reg), raw, sizeof(raw), TIMEOUT_MS);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "read failed: %s", esp_err_to_name(err));
    return false;
  }

  rtc_datetime_t dt;
  if (!rtc_decode(raw, &dt)) {
    return false;  // voltage low or garbage: the time cannot be trusted
  }
  *unix_s = rtc_datetime_to_unix(&dt);
  return true;
}

bool rtc_bm8563_write(uint32_t unix_s) {
  rtc_datetime_t dt;
  rtc_unix_to_datetime(unix_s, &dt);

  // Registers 0x00 and 0x01 (control): make sure the clock is running.
  const uint8_t control[] = {REG_CONTROL1, 0x00, 0x00};
  esp_err_t err =
      i2c_master_transmit(rtc_dev, control, sizeof(control), TIMEOUT_MS);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "control write failed: %s", esp_err_to_name(err));
    return false;
  }

  uint8_t buf[1 + RTC_REGISTER_COUNT];
  buf[0] = REG_SECONDS;
  rtc_encode(&dt, &buf[1]);
  err = i2c_master_transmit(rtc_dev, buf, sizeof(buf), TIMEOUT_MS);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "time write failed: %s", esp_err_to_name(err));
    return false;
  }
  return true;
}
