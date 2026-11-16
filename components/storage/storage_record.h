/**
 * @file storage_record.h
 * @brief Fixed-layout byte format for the persisted pet.
 *
 * Pure C with no ESP-IDF dependency, so it is unit-tested on the PC. The NVS
 * layer (storage_nvs.c) only moves these bytes in and out of flash.
 *
 * Layout (little-endian, 19 bytes, fields written one by one so struct
 * padding never leaks into the format):
 *
 * | Offset | Size | Field             |
 * |--------|------|-------------------|
 * | 0      | 1    | format version    |
 * | 1      | 1    | hunger            |
 * | 2      | 1    | energy            |
 * | 3      | 1    | happiness         |
 * | 4      | 1    | sleeping (0 or 1) |
 * | 5      | 4    | age_seconds       |
 * | 9      | 2    | hunger_carry_s    |
 * | 11     | 2    | energy_carry_s    |
 * | 13     | 2    | happiness_carry_s |
 * | 15     | 4    | last_time_s       |
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pet_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Current format version. Bump it whenever the layout changes. */
#define STORAGE_RECORD_VERSION 1

/** Size in bytes of a packed record. */
#define STORAGE_RECORD_SIZE 19

/** Everything that is persisted. */
typedef struct {
  pet_state_t pet;       /**< The pet itself. */
  uint32_t last_time_s;  /**< Scheduler timestamp at the time of saving. */
} storage_record_t;

/**
 * @brief Serialises a record.
 *
 * @param record Record to write. Not modified.
 * @param buf Output buffer of at least STORAGE_RECORD_SIZE bytes.
 * @return Number of bytes written (STORAGE_RECORD_SIZE).
 */
size_t storage_record_pack(const storage_record_t* record, uint8_t* buf);

/**
 * @brief Parses and validates a record.
 *
 * Fails when the length is wrong, the version is unknown, or a value is out
 * of range (stats above 100, sleeping not 0 or 1).
 *
 * @param buf Input bytes.
 * @param len Number of bytes in @p buf.
 * @param record Output. Left untouched on failure.
 * @return true on success, false if the data must be discarded.
 */
bool storage_record_unpack(const uint8_t* buf,
                           size_t len,
                           storage_record_t* record);

#ifdef __cplusplus
}
#endif
