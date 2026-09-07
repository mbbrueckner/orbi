/**
 * @file pet_state.c
 * @brief Implementation of the pet rules. The API is documented in pet_state.h.
 */

#include "pet_state.h"

/**
 * @brief Clamps @p v into the closed interval [@p lo, @p hi].
 *
 * @warning Expands @p v up to three times, so never pass an expression with
 *          side effects (a function call, an increment) -- it would run more
 *          than once. Safe with the plain variable expressions used below.
 *
 * @todo Replace with a static inline function. That restores type safety,
 *       guarantees single evaluation, and silences the -Wconversion warnings
 *       the implicit int -> uint8_t narrowing produces here.
 */
#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

void pet_init(pet_state_t* state) {
  state->hunger = 20;
  state->energy = 100;
  state->happiness = 80;
  state->age_seconds = 0;
}

void pet_tick(pet_state_t* state, uint32_t elapsed_seconds) {
  // TODO: define real decay rates, these are just placeholder values for now
  // TODO: the integer divisions below discard their remainder on every call.
  //       Ticking once per second therefore never moves hunger or energy at
  //       all -- see test_tick_is_additive() in test/test_main.c. Fix by
  //       carrying the leftover seconds over in pet_state_t.
  // TODO: happiness never decays here, which makes PET_MOOD_SAD unreachable.
  state->age_seconds += elapsed_seconds;

  int hunger_delta =
      (int)(elapsed_seconds / 60);  // Placeholder: 1 point/minute
  state->hunger = CLAMP(state->hunger + hunger_delta, 0, 100);

  int energy_delta = (int)(elapsed_seconds / 120);
  state->energy = CLAMP((int)state->energy - energy_delta, 0, 100);
}

void pet_feed(pet_state_t* state) {
  state->hunger = CLAMP((int)state->hunger - 30, 0, 100);
}

void pet_play(pet_state_t* state) {
  state->happiness = CLAMP(state->happiness + 20, 0, 100);
  state->energy = CLAMP((int)state->energy - 10, 0, 100);
}

void pet_sleep(pet_state_t* state, bool sleeping) {
  // TODO: flesh out the sleep logic (e.g. faster energy recovery while asleep)
  // TODO: the sleeping flag is not stored anywhere, so pet_tick() cannot know
  //       the pet is asleep. Needs a field in pet_state_t.
  if (sleeping) {
    state->energy = CLAMP(state->energy + 5, 0, 100);
  }
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
