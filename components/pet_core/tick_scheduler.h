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

#include <stdint.h>

#include "pet_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Remembers when the pet was last advanced. */
typedef struct {
  uint32_t last_time_s;  //< Timestamp (seconds) of the last advance.
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

#ifdef __cplusplus
}
#endif
