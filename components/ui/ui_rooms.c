/**
 * @file ui_rooms.c
 * @brief Implementation of the rooms; see ui_rooms.h.
 *
 * The rooms are pages of a carousel: swiping left/right slides to the next or
 * previous room and wraps around at the ends. Each room has one purpose:
 *  - Living room: the pet and all its stats at a glance.
 *  - Dining room: drag the apple onto the pet to feed it.
 *  - Bedroom:     tap the ceiling lamp: light off sends the pet to sleep,
 *                 light on wakes it up.
 *  - Garden:      tap the ball, the pet fetches it (counts as playing).
 *
 * The pet is drawn from the raccoon sprites (see sprites.h).
 */

#include "ui_rooms.h"

#include <stdbool.h>

#include "lvgl.h"
#include "sprites.h"

#define SCREEN_SIZE 240

/** Finger movement below this many pixels still counts as a tap. */
#define TAP_SLOP 10
/** Dragging further than this many pixels switches the room on release. */
#define SWIPE_THRESHOLD 60
#define SLIDE_TIME_MS 200

/** Size at which the pet is drawn in the pet rooms (the sprite is 80 px). */
#define PET_SIZE 160
#define PET_CENTER_X (SCREEN_SIZE / 2)
#define PET_CENTER_Y 132

/** Size at which the apple is drawn (the sprite is 20 px, shown doubled). */
#define FOOD_SIZE 40
#define FOOD_HOME_CENTER_X (SCREEN_SIZE / 2)
#define FOOD_HOME_CENTER_Y 204
/** Extra invisible touch area around the food, in pixels. */
#define FOOD_TOUCH_MARGIN 14
/** The food counts as fed when its center is this close to the pet's. */
#define FEED_RADIUS 55

#define BALL_SIZE 40
#define BALL_HOME_CENTER_X (SCREEN_SIZE / 2)
#define BALL_HOME_CENTER_Y 202

enum { ROOM_LIVING, ROOM_DINING, ROOM_BEDROOM, ROOM_GARDEN, ROOM_COUNT };

typedef struct {
  lv_obj_t* page;
  lv_obj_t* pet;
  lv_obj_t* mood;
  lv_obj_t* bar;  /**< NULL in the living room, it shows all stats. */
} room_t;

static room_t rooms[ROOM_COUNT];
static ui_action_cb_t on_action;
static lv_obj_t* pager;
static lv_obj_t* lamp;
static lv_obj_t* night_overlay;
static bool pet_sleeping;

static void send_action(ui_action_t action) {
  if (on_action != NULL) {
    on_action(action);
  }
}

static int32_t lerp(int32_t from, int32_t to, int32_t progress_permille) {
  return from + (to - from) * progress_permille / 1000;
}

/* ---- Carousel ---- */

/** The carousel starts on the living room, which shows the status. */
static int current_room = ROOM_LIVING;
/** Horizontal offset of the visible room while dragging or sliding. */
static int32_t drag_x;
static bool finger_down;
static bool finger_dragged;
static bool sliding;
static int32_t slide_target;

/** Positions the visible room and its two neighbours, hides the others. */
static void layout_rooms(void) {
  for (int i = 0; i < ROOM_COUNT; i++) {
    int rel = (i - current_room + ROOM_COUNT) % ROOM_COUNT;
    int slot;
    if (rel == 0) {
      slot = 0;
    } else if (rel == 1) {
      slot = 1;
    } else if (rel == ROOM_COUNT - 1) {
      slot = -1;
    } else {
      lv_obj_add_flag(rooms[i].page, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_clear_flag(rooms[i].page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(rooms[i].page, slot * SCREEN_SIZE + drag_x, 0);
  }
}

/**
 * True when a touch should not trigger a button: the finger swiped, or a room
 * change is still sliding in. A snap-back to the same room does not count.
 */
static bool tap_blocked(void) {
  return finger_dragged || (sliding && slide_target != 0);
}

static void slide_set_x(void* var, int32_t x) {
  (void)var;
  drag_x = x;
  layout_rooms();
}

static void slide_done(lv_anim_t* anim) {
  (void)anim;
  if (slide_target < 0) {
    current_room = (current_room + 1) % ROOM_COUNT;
  } else if (slide_target > 0) {
    current_room = (current_room + ROOM_COUNT - 1) % ROOM_COUNT;
  }
  drag_x = 0;
  sliding = false;
  layout_rooms();
}

static void pager_event_cb(lv_event_t* e) {
  switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED:
      if (!sliding) {
        finger_down = true;
        finger_dragged = false;
        drag_x = 0;
      }
      break;

    case LV_EVENT_PRESSING: {
      lv_indev_t* indev = lv_indev_get_act();
      if (!finger_down || indev == NULL) {
        break;
      }
      lv_point_t vect;
      lv_indev_get_vect(indev, &vect);
      drag_x += vect.x;
      if (drag_x > SCREEN_SIZE) {
        drag_x = SCREEN_SIZE;
      } else if (drag_x < -SCREEN_SIZE) {
        drag_x = -SCREEN_SIZE;
      }
      if (drag_x > TAP_SLOP || drag_x < -TAP_SLOP) {
        finger_dragged = true;
      }
      layout_rooms();
      break;
    }

    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST: {
      if (!finger_down) {
        break;
      }
      finger_down = false;

      if (drag_x == 0) {
        break;  // a plain tap, nothing to slide
      }

      slide_target = 0;
      if (drag_x < -SWIPE_THRESHOLD) {
        slide_target = -SCREEN_SIZE;
      } else if (drag_x > SWIPE_THRESHOLD) {
        slide_target = SCREEN_SIZE;
      }

      sliding = true;
      lv_anim_t anim;
      lv_anim_init(&anim);
      lv_anim_set_var(&anim, pager);
      lv_anim_set_exec_cb(&anim, slide_set_x);
      lv_anim_set_values(&anim, drag_x, slide_target);
      lv_anim_set_time(&anim, SLIDE_TIME_MS);
      lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
      lv_anim_set_ready_cb(&anim, slide_done);
      lv_anim_start(&anim);
      break;
    }

    default:
      break;
  }
}

/* ---- Shared building blocks ---- */

/** Creates a value bar that lets touches pass through to the carousel. */
static lv_obj_t* make_bar(lv_obj_t* parent, int width, lv_color_t color) {
  lv_obj_t* bar = lv_bar_create(parent);
  lv_obj_set_size(bar, width, 8);
  lv_bar_set_range(bar, 0, 100);
  lv_obj_set_style_bg_color(bar, lv_color_make(0x40, 0x40, 0x40), 0);
  lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);
  return bar;
}

/** Creates a room page with background and title (NULL for no title). */
static lv_obj_t* make_page(int index, const char* title, lv_color_t background) {
  lv_obj_t* page = lv_obj_create(pager);
  lv_obj_set_size(page, SCREEN_SIZE, SCREEN_SIZE);
  lv_obj_set_style_radius(page, 0, 0);
  lv_obj_set_style_border_width(page, 0, 0);
  lv_obj_set_style_pad_all(page, 0, 0);
  lv_obj_set_style_bg_color(page, background, 0);
  lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
  lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(page, LV_OBJ_FLAG_EVENT_BUBBLE);

  if (title != NULL) {
    lv_obj_t* title_label = lv_label_create(page);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 22);
  }

  rooms[index].page = page;
  return page;
}

/**
 * Scales a pet image. Whole multiples of the sprite size are drawn without
 * smoothing, so pixel art stays sharp; other sizes are smoothed.
 */
static void set_pet_size(lv_obj_t* pet, int size) {
  lv_img_set_antialias(pet, size % SPRITE_SIZE != 0);
  lv_img_set_zoom(pet, 256 * size / SPRITE_SIZE);
}

/**
 * Creates a pet image. The object keeps the sprite size (80 px) and is scaled
 * around its center, so @p size only changes how large it is drawn.
 */
static lv_obj_t* make_pet(lv_obj_t* parent, int size) {
  lv_obj_t* pet = lv_img_create(parent);
  lv_img_set_src(pet, sprites_get(SPRITE_HAPPY, 0));
  set_pet_size(pet, size);
  return pet;
}

/** Moves and scales a pet image so that it is drawn around a center point. */
static void place_pet(lv_obj_t* pet, int center_x, int center_y, int size) {
  lv_obj_set_pos(pet, center_x - SPRITE_SIZE / 2, center_y - SPRITE_SIZE / 2);
  set_pet_size(pet, size);
}

/** Adds bar, pet and mood text to a pet room. */
static void add_pet_to_room(int index, lv_color_t bar_color) {
  lv_obj_t* page = rooms[index].page;

  lv_obj_t* bar = make_bar(page, 100, bar_color);
  lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 46);

  lv_obj_t* pet = make_pet(page, PET_SIZE);
  place_pet(pet, PET_CENTER_X, PET_CENTER_Y, PET_SIZE);

  lv_obj_t* mood = lv_label_create(page);
  lv_label_set_text(mood, "");
  lv_obj_set_style_text_color(mood, lv_color_make(0xFF, 0xB0, 0x40), 0);
  lv_obj_align(mood, LV_ALIGN_TOP_MID, 0, 172);
  // The sprite shows the mood, the text only lives on the status page.
  lv_obj_add_flag(mood, LV_OBJ_FLAG_HIDDEN);

  rooms[index].pet = pet;
  rooms[index].mood = mood;
  rooms[index].bar = bar;
}

/* ---- Living room: shows all stats ---- */

enum { STAT_SATIETY, STAT_ENERGY, STAT_HAPPINESS, STAT_COUNT };

static lv_obj_t* stat_bars[STAT_COUNT];
static lv_obj_t* stat_values[STAT_COUNT];
/** Size of the small pet shown at the top of the living room. */
#define STATUS_PET_SIZE 50

static lv_obj_t* status_mood;
static lv_obj_t* status_age;
static char status_note[32];

static void create_living_room(void) {
  lv_obj_t* page = make_page(ROOM_LIVING, "Wohnzimmer", lv_color_make(0x18, 0x18, 0x20));

  static const char* const names[STAT_COUNT] = {"Sattheit", "Energie", "Glueck"};
  const lv_color_t colors[STAT_COUNT] = {lv_color_make(0x40, 0xC0, 0x60),
                                         lv_color_make(0x50, 0x90, 0xFF),
                                         lv_color_make(0xF0, 0xD0, 0x40)};
  lv_obj_t* pet = make_pet(page, STATUS_PET_SIZE);
  lv_obj_align(pet, LV_ALIGN_TOP_MID, 0, 42 + STATUS_PET_SIZE / 2 - SPRITE_SIZE / 2);

  for (int i = 0; i < STAT_COUNT; i++) {
    int top = 100 + i * 28;

    lv_obj_t* name = lv_label_create(page);
    lv_label_set_text(name, names[i]);
    lv_obj_set_style_text_color(name, lv_color_white(), 0);
    lv_obj_set_pos(name, 28, top);

    stat_bars[i] = make_bar(page, 84, colors[i]);
    lv_obj_set_pos(stat_bars[i], 98, top + 4);

    stat_values[i] = lv_label_create(page);
    lv_label_set_text(stat_values[i], "");
    lv_obj_set_style_text_color(stat_values[i], lv_color_white(), 0);
    lv_obj_set_pos(stat_values[i], 192, top);
  }

  status_mood = lv_label_create(page);
  lv_label_set_text(status_mood, "");
  lv_obj_set_style_text_color(status_mood, lv_color_make(0xFF, 0xB0, 0x40), 0);
  lv_obj_align(status_mood, LV_ALIGN_TOP_MID, 0, 184);

  status_age = lv_label_create(page);
  lv_label_set_text(status_age, "");
  lv_obj_set_style_text_color(status_age, lv_color_make(0xA0, 0xA0, 0xA0), 0);
  lv_obj_set_style_text_align(status_age, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(status_age, LV_ALIGN_TOP_MID, 0, 200);

  rooms[ROOM_LIVING].pet = pet;
  rooms[ROOM_LIVING].mood = status_mood;
}

/* ---- Dining room: drag the food onto the pet ---- */

/*
 * Feeding: when the apple is dropped on the pet it moves in front of the
 * pet's nose and is eaten there. The five apple frames play one after the
 * other (whole, three bites, core); every bite feeds the pet once.
 *
 *   0..200 ms   the apple moves in front of the nose
 *   then        one bite every BITE_MS: frames 2, 3, 4 and 5 (the core)
 *   at the end  the core disappears, a new apple lies at its home
 */
#define EAT_MOVE_MS 200
#define EAT_BITE_MS 450
#define EAT_BITES 4
#define EAT_HOLD_MS 300
#define EAT_TOTAL_MS (EAT_MOVE_MS + EAT_BITES * EAT_BITE_MS + EAT_HOLD_MS)
/** Where the apple is eaten: in front of the nose of the raccoon sprite. */
#define EAT_CENTER_X 90
#define EAT_CENTER_Y (PET_CENTER_Y + 4)

static lv_obj_t* food;
static bool apple_eating;
static int apple_frame;
static int eat_from_x;
static int eat_from_y;

static void place_food(int center_x, int center_y) {
  lv_obj_set_pos(
      food, center_x - SPRITE_APPLE_SIZE / 2, center_y - SPRITE_APPLE_SIZE / 2);
}

static void show_apple_frame(int frame) {
  apple_frame = frame;
  lv_img_set_src(food, sprites_get(SPRITE_APPLE, frame));
}

static void eat_step(void* var, int32_t t) {
  (void)var;

  if (t < EAT_MOVE_MS) {
    int32_t u = t * 1000 / EAT_MOVE_MS;
    place_food(lerp(eat_from_x, EAT_CENTER_X, u),
               lerp(eat_from_y, EAT_CENTER_Y, u));
    return;
  }

  place_food(EAT_CENTER_X, EAT_CENTER_Y);
  int bites = (t - EAT_MOVE_MS) / EAT_BITE_MS;
  if (bites > EAT_BITES) {
    bites = EAT_BITES;
  }
  while (apple_frame < bites) {
    show_apple_frame(apple_frame + 1);
    send_action(UI_ACTION_FEED);
  }
}

static void eat_done(lv_anim_t* anim) {
  (void)anim;
  apple_eating = false;
  show_apple_frame(0);
  place_food(FOOD_HOME_CENTER_X, FOOD_HOME_CENTER_Y);
}

static void start_eating(void) {
  apple_eating = true;
  eat_from_x = lv_obj_get_x(food) + SPRITE_APPLE_SIZE / 2;
  eat_from_y = lv_obj_get_y(food) + SPRITE_APPLE_SIZE / 2;

  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, food);
  lv_anim_set_exec_cb(&anim, eat_step);
  lv_anim_set_values(&anim, 0, EAT_TOTAL_MS);
  lv_anim_set_time(&anim, EAT_TOTAL_MS);
  lv_anim_set_path_cb(&anim, lv_anim_path_linear);
  lv_anim_set_ready_cb(&anim, eat_done);
  lv_anim_start(&anim);
}

static void food_event_cb(lv_event_t* e) {
  if (apple_eating) {
    return;
  }

  switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSING: {
      lv_indev_t* indev = lv_indev_get_act();
      if (indev == NULL) {
        break;
      }
      lv_point_t vect;
      lv_indev_get_vect(indev, &vect);
      lv_obj_set_pos(
          food, lv_obj_get_x(food) + vect.x, lv_obj_get_y(food) + vect.y);
      break;
    }

    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST: {
      int dx = lv_obj_get_x(food) + SPRITE_APPLE_SIZE / 2 - PET_CENTER_X;
      int dy = lv_obj_get_y(food) + SPRITE_APPLE_SIZE / 2 - PET_CENTER_Y;
      if (!pet_sleeping && dx * dx + dy * dy < FEED_RADIUS * FEED_RADIUS) {
        start_eating();
      } else {
        place_food(FOOD_HOME_CENTER_X, FOOD_HOME_CENTER_Y);
      }
      break;
    }

    default:
      break;
  }
}

static void create_dining_room(void) {
  lv_obj_t* page =
      make_page(ROOM_DINING, "Esszimmer", lv_color_make(0x2A, 0x1A, 0x10));
  add_pet_to_room(ROOM_DINING, lv_color_make(0x40, 0xC0, 0x60));

  // No event bubbling: dragging the food must not slide the carousel.
  food = lv_img_create(page);
  show_apple_frame(0);
  lv_img_set_antialias(food, FOOD_SIZE % SPRITE_APPLE_SIZE != 0);
  lv_img_set_zoom(food, 256 * FOOD_SIZE / SPRITE_APPLE_SIZE);
  place_food(FOOD_HOME_CENTER_X, FOOD_HOME_CENTER_Y);
  lv_obj_add_flag(food, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_set_ext_click_area(food, FOOD_TOUCH_MARGIN);  // easier to grab
  lv_obj_add_event_cb(food, food_event_cb, LV_EVENT_ALL, NULL);
}

/* ---- Bedroom: the ceiling lamp toggles sleep ---- */

/** The lamp sprite is 32 px and drawn doubled, hanging from the top edge. */
#define LAMP_SIZE 64
#define LAMP_CENTER_X (SCREEN_SIZE / 2)
#define LAMP_CENTER_Y (LAMP_SIZE / 2)
/** Extra touch area around the lamp, in pixels. */
#define LAMP_TOUCH_MARGIN 24
/** Darkness over the room while the pet sleeps (0 to 255). */
#define NIGHT_OPACITY 150
#define BEDROOM_BAR_Y 218

static void lamp_cb(lv_event_t* e) {
  (void)e;
  if (tap_blocked()) {
    return;
  }
  send_action(pet_sleeping ? UI_ACTION_WAKE : UI_ACTION_SLEEP);
}

static void create_bedroom(void) {
  // No title: the lamp hangs at the top of the room.
  lv_obj_t* page =
      make_page(ROOM_BEDROOM, NULL, lv_color_make(0x10, 0x14, 0x30));
  add_pet_to_room(ROOM_BEDROOM, lv_color_make(0x50, 0x90, 0xFF));
  lv_obj_align(rooms[ROOM_BEDROOM].bar, LV_ALIGN_TOP_MID, 0, BEDROOM_BAR_Y);

  // Darkens everything below it, but not the lamp.
  night_overlay = lv_obj_create(page);
  lv_obj_set_size(night_overlay, SCREEN_SIZE, SCREEN_SIZE);
  lv_obj_set_pos(night_overlay, 0, 0);
  lv_obj_set_style_radius(night_overlay, 0, 0);
  lv_obj_set_style_border_width(night_overlay, 0, 0);
  lv_obj_set_style_bg_color(night_overlay, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(night_overlay, NIGHT_OPACITY, 0);
  lv_obj_clear_flag(night_overlay, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(night_overlay, LV_OBJ_FLAG_HIDDEN);

  lamp = lv_img_create(page);
  lv_img_set_src(lamp, sprites_get(SPRITE_LAMP, 0));
  lv_img_set_antialias(lamp, LAMP_SIZE % SPRITE_LAMP_SIZE != 0);
  lv_img_set_zoom(lamp, 256 * LAMP_SIZE / SPRITE_LAMP_SIZE);
  lv_obj_set_pos(lamp,
                 LAMP_CENTER_X - SPRITE_LAMP_SIZE / 2,
                 LAMP_CENTER_Y - SPRITE_LAMP_SIZE / 2);
  lv_obj_set_ext_click_area(lamp, LAMP_TOUCH_MARGIN);
  lv_obj_add_flag(lamp, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_add_event_cb(lamp, lamp_cb, LV_EVENT_CLICKED, NULL);
}

/* ---- Garden: throw the ball, the pet fetches it ---- */

/*
 * One animation drives everything, split into phases so that real sprites can
 * replace the placeholder circles later (one sprite sequence per phase):
 *   THROW    the ball flies to the back of the garden and lands (squashes)
 *   RUN_OUT  the pet runs to the ball and gets smaller (perspective)
 *   RUN_BACK the pet runs back carrying the ball and gets bigger again
 */
#define FETCH_THROW_END_MS 500
/** The ball flies until here and then lands with the squash frames. */
#define FETCH_FLIGHT_END_MS 350
#define BALL_LAND_STEP_MS 50
#define FETCH_RUN_OUT_END_MS 1300
#define FETCH_TOTAL_MS 2300

#define BALL_FAR_X 178
#define BALL_FAR_Y 72
#define BALL_FAR_SIZE 20
#define PET_FAR_X 158
#define PET_FAR_Y 82
#define PET_FAR_SIZE 44
#define BALL_ARC_HEIGHT 35

static lv_obj_t* garden_ball;
static lv_obj_t* garden_pet;
static bool fetching;

/** Moves and scales the ball image so that it is drawn around a center. */
static void place_ball(lv_obj_t* ball, int center_x, int center_y, int size) {
  lv_obj_set_pos(
      ball, center_x - SPRITE_BALL_SIZE / 2, center_y - SPRITE_BALL_SIZE / 2);
  lv_img_set_antialias(ball, size % SPRITE_BALL_SIZE != 0);
  lv_img_set_zoom(ball, 256 * size / SPRITE_BALL_SIZE);
}

static int ball_frame = 1;

/** Shows ball frame 1 (round), 2 (squashed) or 3 (flat). */
static void show_ball_frame(int frame) {
  if (frame == ball_frame) {
    return;
  }
  ball_frame = frame;
  lv_img_set_src(garden_ball, sprites_get(SPRITE_BALL, frame - 1));
}

/*
 * The waiting ball bounces. One cycle, in milliseconds:
 *   0..350    up, slowing down (round)
 *   350..700  down, speeding up (round)
 *   700..740  lands, slightly squashed
 *   740..800  flat
 *   800..840  slightly squashed again, then it starts over
 */
#define BOUNCE_UP_END_MS 350
#define BOUNCE_DOWN_END_MS 700
#define BOUNCE_FLAT_START_MS 740
#define BOUNCE_FLAT_END_MS 800
#define BOUNCE_TOTAL_MS 840
#define BOUNCE_HEIGHT 40

static bool ball_bouncing;

static void bounce_step(void* var, int32_t t) {
  (void)var;
  int32_t height = 0;
  int frame = 1;

  if (t < BOUNCE_UP_END_MS) {
    int32_t u = t * 1000 / BOUNCE_UP_END_MS;
    height = BOUNCE_HEIGHT * (1000000 - (1000 - u) * (1000 - u)) / 1000000;
  } else if (t < BOUNCE_DOWN_END_MS) {
    int32_t u = (t - BOUNCE_UP_END_MS) * 1000 /
                (BOUNCE_DOWN_END_MS - BOUNCE_UP_END_MS);
    height = BOUNCE_HEIGHT * (1000000 - u * u) / 1000000;
  } else if (t >= BOUNCE_FLAT_START_MS && t < BOUNCE_FLAT_END_MS) {
    frame = 3;
  } else {
    frame = 2;
  }

  show_ball_frame(frame);
  place_ball(garden_ball, BALL_HOME_CENTER_X, BALL_HOME_CENTER_Y - height, BALL_SIZE);
}

/** Starts or stops the bouncing; the ball rests at home when stopped. */
static void set_ball_bounce(bool on) {
  if (on == ball_bouncing) {
    return;
  }
  ball_bouncing = on;
  lv_anim_del(garden_ball, bounce_step);

  if (on) {
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, garden_ball);
    lv_anim_set_exec_cb(&anim, bounce_step);
    lv_anim_set_values(&anim, 0, BOUNCE_TOTAL_MS);
    lv_anim_set_time(&anim, BOUNCE_TOTAL_MS);
    lv_anim_set_path_cb(&anim, lv_anim_path_linear);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&anim);
  } else {
    show_ball_frame(1);
    place_ball(garden_ball, BALL_HOME_CENTER_X, BALL_HOME_CENTER_Y, BALL_SIZE);
  }
}

static void fetch_step(void* var, int32_t t) {
  (void)var;

  if (t < FETCH_THROW_END_MS) {
    if (t < FETCH_FLIGHT_END_MS) {
      int32_t u = t * 1000 / FETCH_FLIGHT_END_MS;
      int32_t arc = 4 * BALL_ARC_HEIGHT * u * (1000 - u) / 1000000;
      show_ball_frame(1);
      place_ball(garden_ball,
                 lerp(BALL_HOME_CENTER_X, BALL_FAR_X, u),
                 lerp(BALL_HOME_CENTER_Y, BALL_FAR_Y, u) - arc,
                 lerp(BALL_SIZE, BALL_FAR_SIZE, u));
    } else {
      // The ball lands: squashed, flat, squashed (the bounce frames).
      int32_t since = t - FETCH_FLIGHT_END_MS;
      show_ball_frame(since < BALL_LAND_STEP_MS       ? 2
                      : since < 2 * BALL_LAND_STEP_MS ? 3
                                                      : 2);
      place_ball(garden_ball, BALL_FAR_X, BALL_FAR_Y, BALL_FAR_SIZE);
    }
  } else if (t < FETCH_RUN_OUT_END_MS) {
    int32_t u = (t - FETCH_THROW_END_MS) * 1000 /
                (FETCH_RUN_OUT_END_MS - FETCH_THROW_END_MS);
    show_ball_frame(1);
    place_ball(garden_ball, BALL_FAR_X, BALL_FAR_Y, BALL_FAR_SIZE);
    place_pet(garden_pet,
              lerp(PET_CENTER_X, PET_FAR_X, u),
              lerp(PET_CENTER_Y, PET_FAR_Y, u),
              lerp(PET_SIZE, PET_FAR_SIZE, u));
  } else {
    int32_t u = (t - FETCH_RUN_OUT_END_MS) * 1000 /
                (FETCH_TOTAL_MS - FETCH_RUN_OUT_END_MS);
    int32_t pet_size = lerp(PET_FAR_SIZE, PET_SIZE, u);
    int32_t pet_x = lerp(PET_FAR_X, PET_CENTER_X, u);
    int32_t pet_y = lerp(PET_FAR_Y, PET_CENTER_Y, u);
    place_pet(garden_pet, pet_x, pet_y, pet_size);
    // The ball is carried at the pet's feet.
    show_ball_frame(1);
    place_ball(garden_ball,
               pet_x,
               pet_y + pet_size / 2,
               lerp(BALL_FAR_SIZE, BALL_SIZE / 2, u));
  }
}

static void fetch_done(lv_anim_t* anim) {
  (void)anim;
  place_pet(garden_pet, PET_CENTER_X, PET_CENTER_Y, PET_SIZE);
  place_ball(garden_ball, BALL_HOME_CENTER_X, BALL_HOME_CENTER_Y, BALL_SIZE);
  fetching = false;
  set_ball_bounce(!pet_sleeping);
  send_action(UI_ACTION_PLAY);
}

static void ball_event_cb(lv_event_t* e) {
  (void)e;
  if (fetching || pet_sleeping || tap_blocked()) {
    return;
  }
  fetching = true;
  set_ball_bounce(false);

  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, garden_ball);
  lv_anim_set_exec_cb(&anim, fetch_step);
  lv_anim_set_values(&anim, 0, FETCH_TOTAL_MS);
  lv_anim_set_time(&anim, FETCH_TOTAL_MS);
  lv_anim_set_path_cb(&anim, lv_anim_path_linear);
  lv_anim_set_ready_cb(&anim, fetch_done);
  lv_anim_start(&anim);
}

static void create_garden(void) {
  lv_obj_t* page =
      make_page(ROOM_GARDEN, "Garten", lv_color_make(0x10, 0x28, 0x1C));
  add_pet_to_room(ROOM_GARDEN, lv_color_make(0xF0, 0xD0, 0x40));
  garden_pet = rooms[ROOM_GARDEN].pet;

  garden_ball = lv_img_create(page);
  lv_img_set_src(garden_ball, sprites_get(SPRITE_BALL, 0));
  place_ball(garden_ball, BALL_HOME_CENTER_X, BALL_HOME_CENTER_Y, BALL_SIZE);
  // The image is only 20 px wide: make the touch area as big as the old ball.
  lv_obj_set_ext_click_area(garden_ball, BALL_SIZE);
  lv_obj_add_flag(garden_ball, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_add_event_cb(garden_ball, ball_event_cb, LV_EVENT_CLICKED, NULL);
  set_ball_bounce(true);
}

/* ---- Pet animation ---- */

#define SPRITE_FRAME_MS 400

static sprite_set_t sprite_set = SPRITE_HAPPY;
static int sprite_frame;

/** Shows the current frame of the current animation on every pet. */
static void show_sprite_frame(void) {
  const lv_img_dsc_t* frame = sprites_get(sprite_set, sprite_frame);
  for (int i = 0; i < ROOM_COUNT; i++) {
    lv_img_set_src(rooms[i].pet, frame);
  }
}

static void sprite_timer_cb(lv_timer_t* timer) {
  (void)timer;
  sprite_frame++;
  show_sprite_frame();
}

/* ---- Public API ---- */

void ui_rooms_create(ui_action_cb_t action_cb) {
  on_action = action_cb;

  pager = lv_obj_create(lv_scr_act());
  lv_obj_set_size(pager, SCREEN_SIZE, SCREEN_SIZE);
  lv_obj_set_style_radius(pager, 0, 0);
  lv_obj_set_style_border_width(pager, 0, 0);
  lv_obj_set_style_pad_all(pager, 0, 0);
  lv_obj_set_style_bg_color(pager, lv_color_black(), 0);
  lv_obj_clear_flag(pager, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(pager, pager_event_cb, LV_EVENT_ALL, NULL);

  create_living_room();
  create_dining_room();
  create_bedroom();
  create_garden();
  layout_rooms();
  lv_timer_create(sprite_timer_cb, SPRITE_FRAME_MS, NULL);
}

void ui_rooms_set_note(const char* text) {
  lv_snprintf(status_note, sizeof(status_note), "%s", text);
}

void ui_rooms_update(const pet_state_t* state) {
  static const char* const mood_names[] = {"happy", "hungry", "tired", "sad"};

  pet_sleeping = state->sleeping;

  const int values[STAT_COUNT] = {
      100 - state->hunger, state->energy, state->happiness};
  for (int i = 0; i < STAT_COUNT; i++) {
    lv_bar_set_value(stat_bars[i], values[i], LV_ANIM_OFF);
    lv_label_set_text_fmt(stat_values[i], "%d", values[i]);
  }
  lv_bar_set_value(rooms[ROOM_DINING].bar, values[STAT_SATIETY], LV_ANIM_OFF);
  lv_bar_set_value(rooms[ROOM_BEDROOM].bar, values[STAT_ENERGY], LV_ANIM_OFF);
  lv_bar_set_value(rooms[ROOM_GARDEN].bar, values[STAT_HAPPINESS], LV_ANIM_OFF);

  const char* text = state->sleeping ? "zzz" : mood_names[pet_get_mood(state)];
  for (int i = 0; i < ROOM_COUNT; i++) {
    lv_label_set_text(rooms[i].mood, text);
  }
  if (!fetching) {
    set_ball_bounce(!state->sleeping);
  }
  sprite_set = state->sleeping ? SPRITE_SLEEP : (sprite_set_t)pet_get_mood(state);
  show_sprite_frame();

  lv_label_set_text_fmt(status_age,
                        "Alter: %dh %02dm%s%s",
                        (int)(state->age_seconds / 3600),
                        (int)(state->age_seconds % 3600 / 60),
                        status_note[0] != '\0' ? "\n" : "",
                        status_note);

  lv_img_set_src(lamp, sprites_get(SPRITE_LAMP, state->sleeping ? 1 : 0));
  if (state->sleeping) {
    lv_obj_clear_flag(night_overlay, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(night_overlay, LV_OBJ_FLAG_HIDDEN);
  }
}
