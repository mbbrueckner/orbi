#pragma once

#include "pet_state.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void storage_nvs_init(void);
bool storage_nvs_load(pet_state_t *state);
void storage_nvs_save(const pet_state_t *state);

#ifdef __cplusplus
}
#endif
