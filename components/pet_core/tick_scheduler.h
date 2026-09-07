/**
 * @file tick_scheduler.h
 * @brief Turns real wall-clock time into pet_tick() calls.
 *
 * Separated from pet_state so that the game rules stay hardware independent:
 * everything that needs to know what time it actually is lives here.
 *
 * @todo Will later encapsulate the conversion from real RTC time into
 *       pet_tick() calls (e.g. time since the last boot on startup).
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif
