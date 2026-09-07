#include "storage_nvs.h"

// TODO: nvs_flash_init() + key-value storage of the pet_state_t

void storage_nvs_init(void)
{
}

bool storage_nvs_load(pet_state_t *state)
{
    (void)state;
    return false;
}

void storage_nvs_save(const pet_state_t *state)
{
    (void)state;
}
