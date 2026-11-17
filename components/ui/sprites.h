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

#define SPRITE_SIZE 80

/** Animation sets. The first four match pet_mood_t. */
typedef enum {
  SPRITE_HAPPY,
  SPRITE_HUNGRY,
  SPRITE_TIRED,
  SPRITE_SAD,
  SPRITE_SLEEP,
  SPRITE_SET_COUNT
} sprite_set_t;

/** Number of animation frames in a set. */
int sprites_frame_count(sprite_set_t set);

/** Frame of a set. @p frame wraps around the frame count. */
const lv_img_dsc_t* sprites_get(sprite_set_t set, int frame);

#ifdef __cplusplus
}
#endif
