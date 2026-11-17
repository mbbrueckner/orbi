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

/** Things the player can do in the UI. */
typedef enum {
  UI_ACTION_FEED,  /**< Food was dropped on the pet. */
  UI_ACTION_PLAY,  /**< The ball was tapped. */
  UI_ACTION_SLEEP, /**< Send the pet to sleep. */
  UI_ACTION_WAKE,  /**< Wake the pet up. */
} ui_action_t;

/**
 * @brief Receives player actions.
 *
 * Called from the LVGL task with the LVGL lock held, so it must return
 * quickly and must not call into LVGL. Hand the action over to another task,
 * e.g. through a queue.
 */
typedef void (*ui_action_cb_t)(ui_action_t action);

/**
 * @brief Brings up the display and LVGL.
 *
 * Call once at startup, before any call to ui_render_update().
 *
 * Sets up the SPI bus, the GC9A01 panel and the backlight, initialises LVGL
 * and starts the task that drives it, then builds the rooms.
 *
 * @param action_cb Called when the player feeds, plays or puts the pet to
 *        sleep. May be NULL.
 */
void ui_render_init(ui_action_cb_t action_cb);

/**
 * @brief Takes the LVGL lock. LVGL is not thread safe, so every LVGL call
 *        from outside ui_render must be wrapped in lock/unlock.
 */
void ui_render_lock(void);

/** @brief Releases the LVGL lock taken by ui_render_lock(). */
void ui_render_unlock(void);

/**
 * @brief Sets a short note under the age in the living room (e.g. how the
 *        last boot went). Shown on the next ui_render_update().
 *
 * @param text At most 31 characters. Copied.
 */
void ui_render_set_note(const char* text);

/**
 * @brief Redraws the screen to match the current state.
 *
 * Refreshes the status bars, the mood text and the sleeping look.
 *
 * @param state State to render. Not modified.
 */
void ui_render_update(const pet_state_t* state);

#ifdef __cplusplus
}
#endif
