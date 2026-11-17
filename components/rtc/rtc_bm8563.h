/**
 * @file rtc_bm8563.h
 * @brief Driver for the BM8563 real-time clock on the Seeed Round Display.
 *
 * The chip (PCF8563 compatible, I2C address 0x51) keeps the time while the
 * device is off, as long as the display's battery or USB power supplies it.
 * It shares the I2C bus with the touch controller.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Registers the RTC on the given I2C bus.
 *
 * @param bus I2C master bus on SDA = GPIO22, SCL = GPIO23.
 * @return false if the device could not be added.
 */
bool rtc_bm8563_init(i2c_master_bus_handle_t bus);

/**
 * @brief Reads the current time.
 *
 * @param unix_s Output: seconds since 1970-01-01 of the chip's own clock.
 * @return false on a bus error or when the chip's time is not trustworthy
 *         (voltage-low flag set, e.g. it lost power and was never set).
 */
bool rtc_bm8563_read(uint32_t* unix_s);

/**
 * @brief Sets the time and clears the voltage-low flag.
 *
 * @param unix_s Seconds since 1970-01-01 to write.
 * @return false on a bus error.
 */
bool rtc_bm8563_write(uint32_t unix_s);

#ifdef __cplusplus
}
#endif
