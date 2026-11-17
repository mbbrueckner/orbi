/**
 * @file rtc_time.c
 * @brief Implementation of the RTC helpers; see rtc_time.h.
 */

#include "rtc_time.h"

#include <string.h>

#define VOLTAGE_LOW_BIT 0x80

/** Days since 1970-01-01 for a date in the proleptic Gregorian calendar. */
static int32_t days_from_civil(int year, int month, int day) {
  year -= month <= 2;
  int era = (year >= 0 ? year : year - 399) / 400;
  int yoe = year - era * 400;
  int doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

/** Inverse of days_from_civil(). */
static void civil_from_days(int32_t days, int* year, int* month, int* day) {
  days += 719468;
  int era = (days >= 0 ? days : days - 146096) / 146097;
  int doe = days - era * 146097;
  int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  int mp = (5 * doy + 2) / 153;
  *day = doy - (153 * mp + 2) / 5 + 1;
  *month = mp < 10 ? mp + 3 : mp - 9;
  *year = yoe + era * 400 + (*month <= 2);
}

uint32_t rtc_datetime_to_unix(const rtc_datetime_t* dt) {
  int32_t days = days_from_civil(dt->year, dt->month, dt->day);
  return (uint32_t)days * 86400u + dt->hour * 3600u + dt->minute * 60u +
         dt->second;
}

void rtc_unix_to_datetime(uint32_t unix_s, rtc_datetime_t* dt) {
  int year;
  int month;
  int day;
  civil_from_days((int32_t)(unix_s / 86400u), &year, &month, &day);
  uint32_t secs = unix_s % 86400u;

  dt->year = (uint16_t)year;
  dt->month = (uint8_t)month;
  dt->day = (uint8_t)day;
  dt->hour = (uint8_t)(secs / 3600u);
  dt->minute = (uint8_t)(secs % 3600u / 60u);
  dt->second = (uint8_t)(secs % 60u);
}

/** Converts one BCD byte, returns -1 if a nibble is not a decimal digit. */
static int bcd_to_bin(uint8_t bcd) {
  if ((bcd & 0x0F) > 9 || (bcd >> 4) > 9) {
    return -1;
  }
  return (bcd >> 4) * 10 + (bcd & 0x0F);
}

static uint8_t bin_to_bcd(int value) {
  return (uint8_t)(((value / 10) << 4) | (value % 10));
}

bool rtc_decode(const uint8_t* raw, rtc_datetime_t* dt) {
  if (raw[0] & VOLTAGE_LOW_BIT) {
    return false;
  }

  int second = bcd_to_bin(raw[0] & 0x7F);
  int minute = bcd_to_bin(raw[1] & 0x7F);
  int hour = bcd_to_bin(raw[2] & 0x3F);
  int day = bcd_to_bin(raw[3] & 0x3F);
  int month = bcd_to_bin(raw[5] & 0x1F);  // bit 7 is the century flag
  int year = bcd_to_bin(raw[6]);

  if (second < 0 || second > 59 || minute < 0 || minute > 59 || hour < 0 ||
      hour > 23 || day < 1 || day > 31 || month < 1 || month > 12 ||
      year < 0) {
    return false;
  }

  dt->year = (uint16_t)(2000 + year);
  dt->month = (uint8_t)month;
  dt->day = (uint8_t)day;
  dt->hour = (uint8_t)hour;
  dt->minute = (uint8_t)minute;
  dt->second = (uint8_t)second;
  return true;
}

void rtc_encode(const rtc_datetime_t* dt, uint8_t* raw) {
  int32_t days = days_from_civil(dt->year, dt->month, dt->day);
  int weekday = (int)((days + 4) % 7);  // 1970-01-01 was a Thursday; 0 = Sunday

  raw[0] = bin_to_bcd(dt->second);
  raw[1] = bin_to_bcd(dt->minute);
  raw[2] = bin_to_bcd(dt->hour);
  raw[3] = bin_to_bcd(dt->day);
  raw[4] = (uint8_t)weekday;
  raw[5] = bin_to_bcd(dt->month);  // century flag 0 = 20xx
  raw[6] = bin_to_bcd(dt->year - 2000);
}

bool rtc_parse_build_time(const char* date, const char* time, rtc_datetime_t* dt) {
  static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

  if (strlen(date) != 11 || strlen(time) != 8) {
    return false;
  }

  int month = 0;
  for (int i = 0; i < 12; i++) {
    if (strncmp(date, &months[i * 3], 3) == 0) {
      month = i + 1;
    }
  }
  if (month == 0) {
    return false;
  }

  int day = (date[4] == ' ' ? 0 : date[4] - '0') * 10 + (date[5] - '0');
  int year = (date[7] - '0') * 1000 + (date[8] - '0') * 100 +
             (date[9] - '0') * 10 + (date[10] - '0');
  int hour = (time[0] - '0') * 10 + (time[1] - '0');
  int minute = (time[3] - '0') * 10 + (time[4] - '0');
  int second = (time[6] - '0') * 10 + (time[7] - '0');

  if (year < 2000 || year > 2099 || day < 1 || day > 31 || hour > 23 ||
      minute > 59 || second > 59) {
    return false;
  }

  dt->year = (uint16_t)year;
  dt->month = (uint8_t)month;
  dt->day = (uint8_t)day;
  dt->hour = (uint8_t)hour;
  dt->minute = (uint8_t)minute;
  dt->second = (uint8_t)second;
  return true;
}
