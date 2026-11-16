/**
 * @file storage_nvs.h
 * @brief Persists the pet across power cycles using the ESP32 NVS partition.
 *
 * Without this the pet would be reborn on every reset. NVS lives in its own
 * flash partition and survives reflashing the application. The bytes written
 * are the versioned record from storage_record.h, stored as one blob so a
 * power cut can never leave a half-updated mix of old and new values.
 */

#pragma once

#include <stdbool.h>

#include "storage_record.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mounts the NVS partition.
 *
 * Erases and re-initialises it if it is full or from a newer IDF version.
 * Call once at startup, before any load or save.
 */
void storage_nvs_init(void);

/**
 * @brief Restores a previously saved pet.
 *
 * @param record Record to fill. Left untouched when nothing valid was stored.
 * @return true when a valid record was found and loaded, false on a fresh
 *         device or corrupt/unknown data. On false the caller should fall back
 *         to pet_init().
 */
bool storage_nvs_load(storage_record_t* record);

/**
 * @brief Writes the record to flash.
 *
 * Flash endurance is finite, so call this on user actions and every few
 * minutes (see tick_scheduler_save_due()), not on every tick.
 *
 * @param record Record to persist. Not modified.
 * @return true if the write was committed.
 */
bool storage_nvs_save(const storage_record_t* record);

#ifdef __cplusplus
}
#endif
