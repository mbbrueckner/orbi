#include "tamagotchi_state.h"

#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

void tamagotchi_state_init(tamagotchi_state_t *state)
{
    state->hunger = 20;
    state->energy = 100;
    state->happiness = 80;
    state->age_seconds = 0;
}

void tamagotchi_state_tick(tamagotchi_state_t *state, uint32_t elapsed_seconds)
{
    // TODO: define real decay rates, these are just placeholder values for now
    state->age_seconds += elapsed_seconds;

    int hunger_delta = (int)(elapsed_seconds / 60); // Placeholder: 1 point/minute
    state->hunger = CLAMP(state->hunger + hunger_delta, 0, 100);

    int energy_delta = (int)(elapsed_seconds / 120);
    state->energy = CLAMP((int)state->energy - energy_delta, 0, 100);
}

void tamagotchi_feed(tamagotchi_state_t *state)
{
    state->hunger = CLAMP((int)state->hunger - 30, 0, 100);
}

void tamagotchi_play(tamagotchi_state_t *state)
{
    state->happiness = CLAMP(state->happiness + 20, 0, 100);
    state->energy = CLAMP((int)state->energy - 10, 0, 100);
}

void tamagotchi_sleep(tamagotchi_state_t *state, bool sleeping)
{
    // TODO: flesh out the sleep logic (e.g. faster energy recovery while asleep)
    if (sleeping) {
        state->energy = CLAMP(state->energy + 5, 0, 100);
    }
}

tamagotchi_mood_t tamagotchi_get_mood(const tamagotchi_state_t *state)
{
    if (state->hunger > 70) {
        return TAMAGOTCHI_MOOD_HUNGRY;
    }
    if (state->energy < 20) {
        return TAMAGOTCHI_MOOD_TIRED;
    }
    if (state->happiness < 30) {
        return TAMAGOTCHI_MOOD_SAD;
    }
    return TAMAGOTCHI_MOOD_HAPPY;
}
