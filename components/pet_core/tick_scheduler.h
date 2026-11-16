/**
 * @file tick_scheduler.h
 * @brief Turns real wall-clock time into pet_tick() calls.
 *
 * Separated from pet_state so that the game rules stay hardware independent:
 * everything that needs to know what time it actually is lives here.
 *
 * The current time is passed in by the caller (RTC, SNTP, esp_timer, or a
 * fake value in tests), so this module never touches hardware.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pet_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Remembers when the pet was last advanced. */
typedef struct {
  uint32_t last_time_s;   //< Timestamp (seconds) of the last advance.
  uint32_t last_saved_s;  //< Timestamp (seconds) of the last save.
} tick_scheduler_t;

/**
 * @brief Starts a scheduler.
 *
 * @param s Scheduler to initialise.
 * @param now_s Timestamp to count from. On boot, pass the persisted
 *        timestamp so the first update() covers the time the device was off.
 */
void tick_scheduler_init(tick_scheduler_t* s, uint32_t now_s);

/**
 * @brief Advances the pet by the time passed since the last call.
 *
 * If @p now_s is not later than the last timestamp (no time passed, or the
 * clock went backwards), the pet is not ticked and the scheduler resyncs to
 * @p now_s.
 *
 * @param s Scheduler. Updated.
 * @param state Pet to advance. Updated via pet_tick().
 * @param now_s Current timestamp in seconds.
 * @return Seconds applied to the pet (0 if none).
 */
uint32_t tick_scheduler_update(tick_scheduler_t* s,
                               pet_state_t* state,
                               uint32_t now_s);

/**
 * @brief Tells whether a periodic save is due.
 *
 * @param s Scheduler. Not modified.
 * @param now_s Current timestamp in seconds.
 * @param interval_s Minimum seconds between periodic saves.
 * @return true if at least @p interval_s passed since the last save.
 */
bool tick_scheduler_save_due(const tick_scheduler_t* s,
                             uint32_t now_s,
                             uint32_t interval_s);

/**
 * @brief Records that a save just happened (periodic or after an action).
 *
 * @param s Scheduler. Updated.
 * @param now_s Current timestamp in seconds.
 */
void tick_scheduler_mark_saved(tick_scheduler_t* s, uint32_t now_s);

#ifdef __cplusplus
}
#endif
