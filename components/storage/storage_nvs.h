/**
 * @file storage_nvs.h
 * @brief Persists the pet across power cycles using the ESP32 NVS partition.
 *
 * Without this the pet would be reborn on every reset. NVS lives in its own
 * flash partition and survives reflashing the application.
 */

#pragma once

#include <stdbool.h>

#include "pet_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mounts the NVS partition.
 *
 * Call once at startup, before any load or save.
 */
void storage_nvs_init(void);

/**
 * @brief Restores a previously saved pet.
 *
 * @param state State to fill. Left untouched when nothing was stored.
 * @return true when a saved pet was found and loaded, false on a fresh device.
 *         On false the caller should fall back to pet_init().
 */
bool storage_nvs_load(pet_state_t* state);

/**
 * @brief Writes the pet to flash.
 *
 * Flash endurance is finite, so call this on meaningful changes rather than on
 * every tick.
 *
 * @param state State to persist. Not modified.
 */
void storage_nvs_save(const pet_state_t* state);

#ifdef __cplusplus
}
#endif
