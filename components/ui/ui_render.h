/**
 * @file ui_render.h
 * @brief Draws the pet on the round GC9A01 display using LVGL.
 *
 * The only component that knows what the pet looks like. It reads pet_state_t
 * but never modifies it, so the game rules stay independent of the display.
 */

#pragma once

#include <stdint.h>

#include "pet_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Brings up the display and LVGL.
 *
 * Call once at startup, before any call to ui_render_update().
 *
 * Currently sets up the SPI bus, the GC9A01 panel and the backlight. LVGL
 * follows later.
 */
void ui_render_init(void);

/**
 * @brief Fills the whole screen with one color (bring-up helper).
 *
 * @param rgb565 Color as RGB565, e.g. 0xF800 red, 0x07E0 green, 0x001F blue.
 */
void ui_render_fill(uint16_t rgb565);

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
