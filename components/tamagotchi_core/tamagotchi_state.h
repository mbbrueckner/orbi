#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Pure logic, no hardware dependency -> also usable in the PC test/simulator

typedef enum {
    TAMAGOTCHI_MOOD_HAPPY,
    TAMAGOTCHI_MOOD_HUNGRY,
    TAMAGOTCHI_MOOD_TIRED,
    TAMAGOTCHI_MOOD_SAD,
    // TODO: add further states if needed (see the options discussion:
    // minimal / classic / mixed)
} tamagotchi_mood_t;

typedef struct {
    uint8_t hunger;   // 0 = full, 100 = very hungry
    uint8_t energy;   // 0 = exhausted, 100 = well rested
    uint8_t happiness; // 0 = sad, 100 = happy
    uint32_t age_seconds;
} tamagotchi_state_t;

void tamagotchi_state_init(tamagotchi_state_t *state);

// Called periodically (e.g. from tick_scheduler), simulates decay over time
void tamagotchi_state_tick(tamagotchi_state_t *state, uint32_t elapsed_seconds);

// Interactions
void tamagotchi_feed(tamagotchi_state_t *state);
void tamagotchi_play(tamagotchi_state_t *state);
void tamagotchi_sleep(tamagotchi_state_t *state, bool sleeping);

tamagotchi_mood_t tamagotchi_get_mood(const tamagotchi_state_t *state);

#ifdef __cplusplus
}
#endif
