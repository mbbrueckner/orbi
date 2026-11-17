/**
 * @file chsc6x_parse.c
 * @brief Implementation of the report decoder; see chsc6x_parse.h.
 */

#include "chsc6x_parse.h"

chsc6x_status_t chsc6x_parse(const uint8_t* raw, uint16_t* x, uint16_t* y) {
  if (raw[0] == 0) {
    return CHSC6X_RELEASED;
  }
  if (raw[0] != 1 || raw[2] >= CHSC6X_RESOLUTION ||
      raw[4] >= CHSC6X_RESOLUTION) {
    return CHSC6X_INVALID;
  }

  *x = raw[2];
  *y = raw[4];
  return CHSC6X_PRESSED;
}
