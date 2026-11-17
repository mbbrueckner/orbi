/**
 * @file input_touch.h
 * @brief Capacitive touch driver for the Seeed Round Display, wired into LVGL.
 *
 * The display carries a CHSC6X controller at I2C address 0x2E.
 */

#pragma once

#include "driver/i2c_master.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Brings up the touch controller and registers it as an LVGL input.
 *
 * Call once at startup, after ui_render_init(). The I2C bus is shared with the
 * RTC, so the caller creates it and passes it in.
 *
 * LVGL calls are not thread safe: wrap further use of the returned device in
 * ui_render_lock() / ui_render_unlock().
 *
 * @param bus I2C master bus on SDA = GPIO22, SCL = GPIO23.
 * @return The registered LVGL input device, or NULL on failure.
 */
lv_indev_t* input_touch_init(i2c_master_bus_handle_t bus);

#ifdef __cplusplus
}
#endif
