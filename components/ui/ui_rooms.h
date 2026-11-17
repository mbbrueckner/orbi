/**
 * @file ui_rooms.h
 * @brief The three swipeable rooms (dining room, bedroom, living room).
 *
 * Internal to the ui component: ui_render.c calls these once LVGL is up.
 * All functions must be called with the LVGL lock held.
 */

#pragma once

#include "pet_state.h"
#include "ui_render.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Builds the rooms on the active screen.
 *
 * @param action_cb Called (from the LVGL task) when the player does something.
 */
void ui_rooms_create(ui_action_cb_t action_cb);

/**
 * @brief Refreshes bars, mood text and sleep look from the pet state.
 *
 * @param state Current pet. Not modified.
 */
void ui_rooms_update(const pet_state_t* state);

#ifdef __cplusplus
}
#endif
