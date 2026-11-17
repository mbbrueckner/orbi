#include "pet_state.h"

static inline uint8_t clamp_stat(int v) {
  if (v < 0) {
    return 0;
  }
  if (v > 100) {
    return 100;
  }
  return (uint8_t)v;
}

static uint32_t take_points(uint16_t* carry,
                            uint32_t elapsed_seconds,
                            uint16_t seconds_per_point) {
  uint32_t total_seconds = *carry + elapsed_seconds;

  *carry = (uint16_t)(total_seconds % seconds_per_point);
  return total_seconds / seconds_per_point;
}

void pet_init(pet_state_t* state) {
  state->hunger = 20;
  state->energy = 100;
  state->happiness = 80;
  state->age_seconds = 0;
  state->hunger_carry_s = 0;
  state->energy_carry_s = 0;
  state->happiness_carry_s = 0;
  state->sleeping = false;
}

void pet_tick(pet_state_t* state, uint32_t elapsed_seconds) {
  state->age_seconds += elapsed_seconds;

  if (state->sleeping) {
    int hunger_delta =
        (int)take_points(&state->hunger_carry_s, elapsed_seconds, 600);
    state->hunger = clamp_stat((int)state->hunger + hunger_delta);

    int energy_delta =
        (int)take_points(&state->energy_carry_s, elapsed_seconds, 30);
    state->energy = clamp_stat((int)state->energy + energy_delta);

    int happiness_delta =
        (int)take_points(&state->happiness_carry_s, elapsed_seconds, 900);
    state->happiness = clamp_stat((int)state->happiness - happiness_delta);
  } else {
    int hunger_delta =
        (int)take_points(&state->hunger_carry_s, elapsed_seconds, 60);
    state->hunger = clamp_stat((int)state->hunger + hunger_delta);

    int energy_delta =
        (int)take_points(&state->energy_carry_s, elapsed_seconds, 120);
    state->energy = clamp_stat((int)state->energy - energy_delta);

    int happiness_delta =
        (int)take_points(&state->happiness_carry_s, elapsed_seconds, 180);
    state->happiness = clamp_stat((int)state->happiness - happiness_delta);
  }
}

void pet_feed(pet_state_t* state) {
  if (state->sleeping) {
    return;
  }
  state->hunger = clamp_stat((int)state->hunger - 30);
}

void pet_play(pet_state_t* state) {
  if (state->sleeping) {
    return;
  }
  state->happiness = clamp_stat((int)state->happiness + 20);
  state->energy = clamp_stat((int)state->energy - 10);
}

void pet_sleep(pet_state_t* state, bool sleeping) {
  state->sleeping = sleeping;
}

pet_mood_t pet_get_mood(const pet_state_t* state) {
  if (state->hunger > 70) {
    return PET_MOOD_HUNGRY;
  }
  if (state->energy < 20) {
    return PET_MOOD_TIRED;
  }
  if (state->happiness < 30) {
    return PET_MOOD_SAD;
  }
  return PET_MOOD_HAPPY;
}
