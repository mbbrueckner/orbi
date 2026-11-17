#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  PET_MOOD_HAPPY,
  PET_MOOD_HUNGRY,
  PET_MOOD_TIRED,
  PET_MOOD_SAD,
} pet_mood_t;

typedef struct {
  uint8_t hunger;
  uint8_t energy;
  uint8_t happiness;
  uint32_t age_seconds;
  uint16_t hunger_carry_s;
  uint16_t energy_carry_s;
  uint16_t happiness_carry_s;
  bool sleeping;
} pet_state_t;

void pet_init(pet_state_t* state);

void pet_tick(pet_state_t* state, uint32_t elapsed_seconds);

/** One bite of food: lowers hunger by 8. An apple has four bites. Does nothing while asleep. */
void pet_feed(pet_state_t* state);

void pet_play(pet_state_t* state);

void pet_sleep(pet_state_t* state, bool sleeping);

pet_mood_t pet_get_mood(const pet_state_t* state);

#ifdef __cplusplus
}
#endif
