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
  // TODO: add further states if needed (see the options discussion:
  // minimal / classic / mixed)
} pet_mood_t;

typedef struct {
  uint8_t hunger;     // 0 = full, 100 = very hungry
  uint8_t energy;     // 0 = exhausted, 100 = well rested
  uint8_t happiness;  // 0 = sad, 100 = happy
  uint32_t age_seconds;
} pet_state_t;

void pet_init(pet_state_t* state);

// Called periodically (e.g. from tick_scheduler), simulates decay over time
void pet_tick(pet_state_t* state, uint32_t elapsed_seconds);

// Interactions
void pet_feed(pet_state_t* state);
void pet_play(pet_state_t* state);
void pet_sleep(pet_state_t* state, bool sleeping);

pet_mood_t pet_get_mood(const pet_state_t* state);

#ifdef __cplusplus
}
#endif
