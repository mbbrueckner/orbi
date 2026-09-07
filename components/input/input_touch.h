/**
 * @file input_touch.h
 * @brief Capacitive touch driver for the Seeed Round Display, wired into LVGL.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Brings up the touch controller and registers it as an LVGL input.
 *
 * Call once at startup, after ui_render_init().
 *
 * @todo Only implement once an I2C scan has clarified whether a CHSC6X or a
 *       CST816S is fitted (see roadmap step "I2C bus diagnosis").
 */
void input_touch_init(void);

#ifdef __cplusplus
}
#endif
