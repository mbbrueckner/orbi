#pragma once

#include "pet_state.h"

#ifdef __cplusplus
extern "C" {
#endif

// TODO: display + LVGL initialization (esp_lcd_gc9a01 + flush callback)
void ui_render_init(void);

// Update the screen from the current state (sprite selection per mood)
void ui_render_update(const pet_state_t *state);

#ifdef __cplusplus
}
#endif
