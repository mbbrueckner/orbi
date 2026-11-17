/**
 * @file ui_render.h
 * @brief Draws the pet on the round GC9A01 display using LVGL.
 *
 * The only component that knows what the pet looks like. It reads pet_state_t
 * but never modifies it, so the game rules stay independent of the display.
 */

#pragma once

#include "pet_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Brings up the display and LVGL.
 *
 * Call once at startup, before any call to ui_render_update().
 *
 * Sets up the SPI bus, the GC9A01 panel and the backlight, initialises LVGL
 * and starts the task that drives it. Currently shows demo widgets only.
 */
void ui_render_init(void);

/**
 * @brief Takes the LVGL lock. LVGL is not thread safe, so every LVGL call
 *        from outside ui_render must be wrapped in lock/unlock.
 */
void ui_render_lock(void);

/** @brief Releases the LVGL lock taken by ui_render_lock(). */
void ui_render_unlock(void);

/**
 * @brief Redraws the screen to match the current state.
 *
 * Picks the sprite from pet_get_mood() and refreshes the status bars.
 *
 * @param state State to render. Not modified.
 */
void ui_render_update(const pet_state_t* state);

#ifdef __cplusplus
}
#endif
