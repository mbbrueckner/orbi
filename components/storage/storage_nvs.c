/**
 * @file storage_nvs.c
 * @brief Implementation of the NVS persistence; see storage_nvs.h.
 *
 * @todo nvs_flash_init() + key-value storage of the pet_state_t.
 */

#include "storage_nvs.h"

void storage_nvs_init(void) {}

bool storage_nvs_load(pet_state_t* state) {
  (void)state;
  return false;
}

void storage_nvs_save(const pet_state_t* state) { (void)state; }
