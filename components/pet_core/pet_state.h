/**
 * @file pet_state.h
 * @brief Data model and rules of the virtual pet.
 *
 * Deliberately free of any hardware dependency -- it includes nothing beyond
 * <stdint.h> and <stdbool.h>. That is what lets the very same source file be
 * compiled both for the ESP32-C6 and for the host test runner in test/.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mood to show on screen, derived from the stats.
 *
 * Never stored anywhere: pet_get_mood() recomputes it from pet_state_t on
 * every call, so what is displayed can never drift out of sync with the
 * underlying values.
 *
 * @todo Add further states if needed (see the options discussion:
 *       minimal / classic / mixed).
 */
typedef enum {
  PET_MOOD_HAPPY,  /*!< Nothing is wrong; the fallback mood. */
  PET_MOOD_HUNGRY, /*!< Hunger above its threshold. Takes priority. */
  PET_MOOD_TIRED,  /*!< Energy below its threshold. */
  PET_MOOD_SAD,    /*!< Happiness below its threshold. */
} pet_mood_t;

/**
 * @brief Everything the pet is.
 *
 * The three stats are the single source of truth: sprites, moods and status
 * bars are all derived from them. The carry fields are bookkeeping for
 * pet_tick() so that no elapsed time is lost between calls.
 * storage_nvs_save() persists exactly this struct across reboots.
 */
typedef struct {
  uint8_t hunger;       /*!< 0 = full, 100 = very hungry. */
  uint8_t energy;       /*!< 0 = exhausted, 100 = well rested. */
  uint8_t happiness;    /*!< 0 = sad, 100 = happy. */
  uint32_t age_seconds; /*!< Total lifetime; keeps counting across reboots. */
  uint16_t hunger_carry_s; /*!< Seconds not yet turned into a hunger point. */
  uint16_t energy_carry_s; /*!< Seconds not yet turned into an energy point. */
  uint16_t
      happiness_carry_s; /*!< Seconds not yet turned into a happiness point. */
} pet_state_t;

/**
 * @brief Fills a state with the values a newborn pet starts with.
 *
 * Must be called before anything else. A pet_state_t is a plain struct with no
 * constructor, so an uninitialised one holds whatever was left on the stack.
 *
 * @param state State to initialise.
 */
void pet_init(pet_state_t* state);

/**
 * @brief Advances the simulation by a span of time.
 *
 * Called periodically while running, and once right after boot with the time
 * that passed while the device was switched off (see tick_scheduler.h).
 *
 * Reporting a span in smaller pieces must not change the outcome: one call
 * with 60 has to be equivalent to sixty calls with 1. The firmware ticks about
 * once per second, so any time lost per call would add up quickly.
 *
 * @param state State to advance.
 * @param elapsed_seconds Seconds that have passed since the previous call.
 */
void pet_tick(pet_state_t* state, uint32_t elapsed_seconds);

/**
 * @brief Feeds the pet, lowering hunger.
 * @param state State to modify.
 */
void pet_feed(pet_state_t* state);

/**
 * @brief Plays with the pet: raises happiness at the cost of energy.
 * @param state State to modify.
 */
void pet_play(pet_state_t* state);

/**
 * @brief Puts the pet to sleep or wakes it up again.
 *
 * @param state State to modify.
 * @param sleeping true to send it to sleep, false to wake it.
 */
void pet_sleep(pet_state_t* state, bool sleeping);

/**
 * @brief Derives the current mood from the stats.
 *
 * Pure function: it never modifies @p state. When several thresholds are
 * exceeded at once, the first match wins in the order hungry, tired, sad.
 *
 * @param state State to inspect.
 * @return The mood the UI should render.
 */
pet_mood_t pet_get_mood(const pet_state_t* state);

#ifdef __cplusplus
}
#endif
