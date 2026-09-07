/**
 * @file pet_state.c
 * @brief Implementation of the pet rules. The API is documented in pet_state.h.
 */

#include "pet_state.h"

/**
 * @brief Limits a raw stat value to the valid range of 0..100.
 *
 * Takes an int on purpose. Every calculation that feeds into it therefore
 * stays signed, which is what lets a stat that dropped below zero clamp to 0.
 * An unsigned expression could never do that: it would wrap to a huge value
 * and clamp to 100 instead, so a neglected pet would come out healthier.
 *
 * @param v Raw value; may be negative or above 100.
 * @return The value limited to 0..100.
 */
static inline uint8_t clamp_stat(int v) {
  if (v < 0) {
    return 0;
  }
  if (v > 100) {
    return 100;
  }
  return (uint8_t)v;
}

/**
 * @brief Converts elapsed time into whole points, keeping the remainder.
 *
 * The leftover seconds stay in @p carry, so reporting a span in small pieces
 * adds up to the same result as reporting it in one go.
 *
 * @param carry Seconds left over from previous calls. Updated in place.
 * @param elapsed_seconds Seconds that have passed since the last call.
 * @param seconds_per_point How long one point of decay takes.
 * @return Number of whole points earned by the elapsed time.
 */
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
}

void pet_tick(pet_state_t* state, uint32_t elapsed_seconds) {
  // TODO: define real decay rates, these are just placeholder values for now
  state->age_seconds += elapsed_seconds;

  // Each delta is cast to int right here. From this point on every stat
  // calculation is signed, so dropping below zero clamps to 0 instead of
  // wrapping around.
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

void pet_feed(pet_state_t* state) {
  state->hunger = clamp_stat((int)state->hunger - 30);
}

void pet_play(pet_state_t* state) {
  state->happiness = clamp_stat((int)state->happiness + 20);
  state->energy = clamp_stat((int)state->energy - 10);
}

void pet_sleep(pet_state_t* state, bool sleeping) {
  // TODO: flesh out the sleep logic (e.g. faster energy recovery while asleep)
  // TODO: the sleeping flag is not stored anywhere, so pet_tick() cannot know
  //       the pet is asleep. Needs a field in pet_state_t.
  if (sleeping) {
    state->energy = clamp_stat((int)state->energy + 5);
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
