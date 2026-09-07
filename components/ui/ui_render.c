/**
 * @file ui_render.c
 * @brief Implementation of the renderer; the API is documented in ui_render.h.
 *
 * @todo Set up the SPI/esp_lcd panel handle for the GC9A01 (see roadmap step
 *       "Drive the display"), then hook up the LVGL flush callback.
 */

#include "ui_render.h"

void ui_render_init(void) {}

void ui_render_update(const pet_state_t* state) { (void)state; }
