/**
 * @file tick_scheduler.c
 * @brief Implementation of the tick scheduler; see tick_scheduler.h.
 */

#include "tick_scheduler.h"

void tick_scheduler_init(tick_scheduler_t* s, uint32_t now_s) {
  s->last_time_s = now_s;
  s->last_saved_s = now_s;
}

uint32_t tick_scheduler_update(tick_scheduler_t* s,
                               pet_state_t* state,
                               uint32_t now_s) {
  if (now_s <= s->last_time_s) {
    s->last_time_s = now_s;
    if (now_s < s->last_saved_s) {
      s->last_saved_s = now_s;
    }
    return 0;
  }

  uint32_t elapsed = now_s - s->last_time_s;
  s->last_time_s = now_s;
  pet_tick(state, elapsed);
  return elapsed;
}

bool tick_scheduler_save_due(const tick_scheduler_t* s,
                             uint32_t now_s,
                             uint32_t interval_s) {
  return now_s >= s->last_saved_s && now_s - s->last_saved_s >= interval_s;
}

void tick_scheduler_mark_saved(tick_scheduler_t* s, uint32_t now_s) {
  s->last_saved_s = now_s;
}
