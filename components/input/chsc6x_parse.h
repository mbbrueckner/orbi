/**
 * @file chsc6x_parse.h
 * @brief Decodes the 5-byte report of the CHSC6X touch controller.
 *
 * Pure C with no ESP-IDF dependency, so it is unit-tested on the PC.
 *
 * Report layout (read 5 bytes from I2C address 0x2E):
 * | Byte | Meaning                                  |
 * |------|------------------------------------------|
 * | 0    | 1 = finger down, 0 = no touch            |
 * | 1    | 0x80 when touched, 0x40 when not (unused)|
 * | 2    | X coordinate, 0..239                     |
 * | 3    | unused                                   |
 * | 4    | Y coordinate, 0..239                     |
 *
 * The chip sometimes answers with garbage (e.g. 7F FF FF FF FF) when read
 * while it updates, so anything else than 0/1 in byte 0 is rejected.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** I2C address of the CHSC6X on the Seeed Round Display. */
#define CHSC6X_I2C_ADDRESS 0x2E

/** Size of one report in bytes. */
#define CHSC6X_REPORT_SIZE 5

/** Display size the coordinates refer to. */
#define CHSC6X_RESOLUTION 240

typedef enum {
  CHSC6X_RELEASED,  /**< Valid report, no finger on the screen. */
  CHSC6X_PRESSED,   /**< Valid report, finger down; x/y are set. */
  CHSC6X_INVALID,   /**< Garbage, ignore and keep the previous state. */
} chsc6x_status_t;

/**
 * @brief Decodes one report.
 *
 * @param raw CHSC6X_REPORT_SIZE bytes as read from the chip.
 * @param x Output X, written only for CHSC6X_PRESSED.
 * @param y Output Y, written only for CHSC6X_PRESSED.
 * @return Status of the report.
 */
chsc6x_status_t chsc6x_parse(const uint8_t* raw, uint16_t* x, uint16_t* y);

#ifdef __cplusplus
}
#endif
