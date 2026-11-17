/**
 * @file sprites.h
 * @brief Pet sprites. The data lives in sprites_data.c, generated from the
 *        PNGs in assets/sprites/ by tools/gen_sprites.py.
 */

#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Size of the raccoon sprites and of the ball sprites, in pixels. */
#define SPRITE_SIZE 80
#define SPRITE_BALL_SIZE 20
#define SPRITE_APPLE_SIZE 20
#define SPRITE_LAMP_SIZE 32

/**
 * Animation sets. The first four match pet_mood_t. SPRITE_BALL is the ball:
 * frame 1 round, 2 slightly squashed, 3 flat (all anchored at the bottom).
 * SPRITE_APPLE: frame 1 whole, 2 to 4 one more bite each, 5 only the core.
 * SPRITE_LAMP: frame 1 light on, 2 light off.
 */
typedef enum {
  SPRITE_HAPPY,
  SPRITE_HUNGRY,
  SPRITE_TIRED,
  SPRITE_SAD,
  SPRITE_SLEEP,
  SPRITE_BALL,
  SPRITE_APPLE,
  SPRITE_LAMP,
  SPRITE_SET_COUNT
} sprite_set_t;

/** Number of animation frames in a set. */
int sprites_frame_count(sprite_set_t set);

/** Frame of a set. @p frame wraps around the frame count. */
const lv_img_dsc_t* sprites_get(sprite_set_t set, int frame);

#ifdef __cplusplus
}
#endif
