/**
 * @file rtc_time.h
 * @brief Date/time conversion and register coding for the BM8563 / PCF8563.
 *
 * Pure C with no ESP-IDF dependency, so it is unit-tested on the PC. The chip
 * has no time zone: whatever is written is what is read back. "Unix seconds"
 * here simply means seconds since 1970-01-01 00:00:00 of that same clock.
 * Years 2000..2099 are supported.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Number of time registers (seconds .. years), read from register 0x02. */
#define RTC_REGISTER_COUNT 7

typedef struct {
  uint16_t year; /**< 2000..2099 */
  uint8_t month; /**< 1..12 */
  uint8_t day;   /**< 1..31 */
  uint8_t hour;  /**< 0..23 */
  uint8_t minute;
  uint8_t second;
} rtc_datetime_t;

/** @brief Converts a date and time to seconds since 1970-01-01. */
uint32_t rtc_datetime_to_unix(const rtc_datetime_t* dt);

/** @brief Converts seconds since 1970-01-01 to a date and time. */
void rtc_unix_to_datetime(uint32_t unix_s, rtc_datetime_t* dt);

/**
 * @brief Decodes the 7 time registers (seconds, minutes, hours, days,
 *        weekdays, months, years).
 *
 * @param raw RTC_REGISTER_COUNT bytes as read from register 0x02.
 * @param dt Output, written only on success.
 * @return false when the chip reports "voltage low" (time not trustworthy) or
 *         a field is not a valid BCD date/time.
 */
bool rtc_decode(const uint8_t* raw, rtc_datetime_t* dt);

/**
 * @brief Encodes a date and time into the 7 time registers.
 *
 * Writing the seconds register this way also clears the voltage-low flag.
 *
 * @param dt Date and time to write.
 * @param raw Output of RTC_REGISTER_COUNT bytes.
 */
void rtc_encode(const rtc_datetime_t* dt, uint8_t* raw);

/**
 * @brief Parses the compiler's __DATE__ and __TIME__ strings.
 *
 * @param date Format "Nov  7 2026" (day space padded).
 * @param time Format "08:18:21".
 * @param dt Output, written only on success.
 * @return false if the strings do not have the expected format.
 */
bool rtc_parse_build_time(const char* date, const char* time, rtc_datetime_t* dt);

#ifdef __cplusplus
}
#endif
