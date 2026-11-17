/**
 * @file ui_rooms.c
 * @brief Implementation of the rooms; see ui_rooms.h.
 *
 * The rooms are pages of a carousel: swiping left/right slides to the next or
 * previous room and wraps around at the ends. Each room has one purpose:
 *  - Living room: the pet and all its stats at a glance.
 *  - Dining room: drag the apple onto the pet to feed it.
 *  - Bedroom:     tap the button to send the pet to sleep or wake it up.
 *  - Garden:      tap the ball, the pet fetches it (counts as playing).
 *
 * The pet is a grey circle with eyes until real sprites exist.
 */

#include "ui_rooms.h"

#include <stdbool.h>

#include "lvgl.h"

#define SCREEN_SIZE 240

/** Finger movement below this many pixels still counts as a tap. */
#define TAP_SLOP 10
/** Dragging further than this many pixels switches the room on release. */
#define SWIPE_THRESHOLD 60
#define SLIDE_TIME_MS 200

#define PET_SIZE 80
#define PET_X ((SCREEN_SIZE - PET_SIZE) / 2)
#define PET_Y 75
#define PET_CENTER_X (PET_X + PET_SIZE / 2)
#define PET_CENTER_Y (PET_Y + PET_SIZE / 2)

#define FOOD_SIZE 36
#define FOOD_HOME_X ((SCREEN_SIZE - FOOD_SIZE) / 2)
#define FOOD_HOME_Y 186
/** Extra invisible touch area around the food, in pixels. */
#define FOOD_TOUCH_MARGIN 14
/** The food counts as fed when its center is this close to the pet's. */
#define FEED_RADIUS 55

#define BALL_SIZE 40
#define BALL_HOME_X ((SCREEN_SIZE - BALL_SIZE) / 2)
#define BALL_HOME_Y 182
#define BALL_HOME_CENTER_X (BALL_HOME_X + BALL_SIZE / 2)
#define BALL_HOME_CENTER_Y (BALL_HOME_Y + BALL_SIZE / 2)

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
static lv_obj_t* sleep_button_label;
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

/** Creates a plain filled circle that does not react to touch by default. */
static lv_obj_t* make_circle(lv_obj_t* parent, int size, lv_color_t color) {
  lv_obj_t* obj = lv_obj_create(parent);
  lv_obj_set_size(obj, size, size);
  lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(obj, color, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  return obj;
}

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

/** Creates a room page with background and title. */
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

  lv_obj_t* title_label = lv_label_create(page);
  lv_label_set_text(title_label, title);
  lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
  lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 22);

  rooms[index].page = page;
  return page;
}

/** Creates the placeholder pet (grey circle with eyes) at the given size. */
static lv_obj_t* make_pet(lv_obj_t* parent, int size) {
  lv_obj_t* pet = make_circle(parent, size, lv_color_make(0x9A, 0x9A, 0x9A));
  lv_obj_t* left_eye = make_circle(pet, size / 8, lv_color_black());
  lv_obj_align(left_eye, LV_ALIGN_CENTER, -size / 5, -size / 10);
  lv_obj_t* right_eye = make_circle(pet, size / 8, lv_color_black());
  lv_obj_align(right_eye, LV_ALIGN_CENTER, size / 5, -size / 10);
  return pet;
}

/** Adds bar, pet and mood text to a pet room. */
static void add_pet_to_room(int index, lv_color_t bar_color) {
  lv_obj_t* page = rooms[index].page;

  lv_obj_t* bar = make_bar(page, 100, bar_color);
  lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 46);

  lv_obj_t* pet = make_pet(page, PET_SIZE);
  lv_obj_set_pos(pet, PET_X, PET_Y);

  lv_obj_t* mood = lv_label_create(page);
  lv_label_set_text(mood, "");
  lv_obj_set_style_text_color(mood, lv_color_make(0xFF, 0xB0, 0x40), 0);
  lv_obj_align(mood, LV_ALIGN_TOP_MID, 0, 164);

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

static void create_living_room(void) {
  lv_obj_t* page = make_page(ROOM_LIVING, "Wohnzimmer", lv_color_make(0x18, 0x18, 0x20));

  static const char* const names[STAT_COUNT] = {"Sattheit", "Energie", "Glueck"};
  const lv_color_t colors[STAT_COUNT] = {lv_color_make(0x40, 0xC0, 0x60),
                                         lv_color_make(0x50, 0x90, 0xFF),
                                         lv_color_make(0xF0, 0xD0, 0x40)};
  lv_obj_t* pet = make_pet(page, STATUS_PET_SIZE);
  lv_obj_align(pet, LV_ALIGN_TOP_MID, 0, 42);

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
  lv_obj_align(status_age, LV_ALIGN_TOP_MID, 0, 204);

  rooms[ROOM_LIVING].pet = pet;
  rooms[ROOM_LIVING].mood = status_mood;
}

/* ---- Dining room: drag the food onto the pet ---- */

static void food_event_cb(lv_event_t* e) {
  lv_obj_t* food = lv_event_get_target(e);

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
      int dx = lv_obj_get_x(food) + FOOD_SIZE / 2 - PET_CENTER_X;
      int dy = lv_obj_get_y(food) + FOOD_SIZE / 2 - PET_CENTER_Y;
      if (dx * dx + dy * dy < FEED_RADIUS * FEED_RADIUS) {
        send_action(UI_ACTION_FEED);
      }
      lv_obj_set_pos(food, FOOD_HOME_X, FOOD_HOME_Y);
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
  lv_obj_t* food = make_circle(page, FOOD_SIZE, lv_color_make(0xE0, 0x30, 0x30));
  lv_obj_set_pos(food, FOOD_HOME_X, FOOD_HOME_Y);
  lv_obj_add_flag(food, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_set_ext_click_area(food, FOOD_TOUCH_MARGIN);  // easier to grab
  lv_obj_add_event_cb(food, food_event_cb, LV_EVENT_ALL, NULL);
}

/* ---- Bedroom: button toggles sleep ---- */

static void sleep_button_cb(lv_event_t* e) {
  (void)e;
  if (tap_blocked()) {
    return;
  }
  send_action(pet_sleeping ? UI_ACTION_WAKE : UI_ACTION_SLEEP);
}

static void create_bedroom(void) {
  lv_obj_t* page =
      make_page(ROOM_BEDROOM, "Schlafzimmer", lv_color_make(0x10, 0x14, 0x30));
  add_pet_to_room(ROOM_BEDROOM, lv_color_make(0x50, 0x90, 0xFF));

  lv_obj_t* button = lv_btn_create(page);
  lv_obj_set_size(button, 100, 34);
  lv_obj_align(button, LV_ALIGN_TOP_MID, 0, 182);
  lv_obj_add_flag(button, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_add_event_cb(button, sleep_button_cb, LV_EVENT_CLICKED, NULL);

  sleep_button_label = lv_label_create(button);
  lv_label_set_text(sleep_button_label, "Schlafen");
  lv_obj_center(sleep_button_label);
}

/* ---- Garden: throw the ball, the pet fetches it ---- */

/*
 * One animation drives everything, split into phases so that real sprites can
 * replace the placeholder circles later (one sprite sequence per phase):
 *   THROW    the ball flies from the player to the back of the garden
 *   RUN_OUT  the pet runs to the ball and gets smaller (perspective)
 *   RUN_BACK the pet runs back carrying the ball and gets bigger again
 */
#define FETCH_THROW_END_MS 500
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

static void place_centered(lv_obj_t* obj, int center_x, int center_y, int size) {
  lv_obj_set_size(obj, size, size);
  lv_obj_set_pos(obj, center_x - size / 2, center_y - size / 2);
}

static void fetch_step(void* var, int32_t t) {
  (void)var;

  if (t < FETCH_THROW_END_MS) {
    int32_t u = t * 1000 / FETCH_THROW_END_MS;
    int32_t arc = 4 * BALL_ARC_HEIGHT * u * (1000 - u) / 1000000;
    place_centered(garden_ball,
                   lerp(BALL_HOME_CENTER_X, BALL_FAR_X, u),
                   lerp(BALL_HOME_CENTER_Y, BALL_FAR_Y, u) - arc,
                   lerp(BALL_SIZE, BALL_FAR_SIZE, u));
  } else if (t < FETCH_RUN_OUT_END_MS) {
    int32_t u = (t - FETCH_THROW_END_MS) * 1000 /
                (FETCH_RUN_OUT_END_MS - FETCH_THROW_END_MS);
    place_centered(garden_ball, BALL_FAR_X, BALL_FAR_Y, BALL_FAR_SIZE);
    place_centered(garden_pet,
                   lerp(PET_CENTER_X, PET_FAR_X, u),
                   lerp(PET_CENTER_Y, PET_FAR_Y, u),
                   lerp(PET_SIZE, PET_FAR_SIZE, u));
  } else {
    int32_t u = (t - FETCH_RUN_OUT_END_MS) * 1000 /
                (FETCH_TOTAL_MS - FETCH_RUN_OUT_END_MS);
    int32_t pet_size = lerp(PET_FAR_SIZE, PET_SIZE, u);
    int32_t pet_x = lerp(PET_FAR_X, PET_CENTER_X, u);
    int32_t pet_y = lerp(PET_FAR_Y, PET_CENTER_Y, u);
    place_centered(garden_pet, pet_x, pet_y, pet_size);
    // The ball is carried at the pet's feet.
    place_centered(garden_ball,
                   pet_x,
                   pet_y + pet_size / 2,
                   lerp(BALL_FAR_SIZE, BALL_SIZE / 2, u));
  }
}

static void fetch_done(lv_anim_t* anim) {
  (void)anim;
  place_centered(garden_pet, PET_CENTER_X, PET_CENTER_Y, PET_SIZE);
  place_centered(garden_ball, BALL_HOME_CENTER_X, BALL_HOME_CENTER_Y, BALL_SIZE);
  fetching = false;
  send_action(UI_ACTION_PLAY);
}

static void ball_event_cb(lv_event_t* e) {
  (void)e;
  if (fetching || tap_blocked()) {
    return;
  }
  fetching = true;

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

  garden_ball = make_circle(page, BALL_SIZE, lv_color_make(0xF0, 0x80, 0x20));
  lv_obj_set_pos(garden_ball, BALL_HOME_X, BALL_HOME_Y);
  lv_obj_add_flag(garden_ball, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_add_event_cb(garden_ball, ball_event_cb, LV_EVENT_CLICKED, NULL);
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
  lv_color_t pet_color = state->sleeping ? lv_color_make(0x50, 0x50, 0x58)
                                         : lv_color_make(0x9A, 0x9A, 0x9A);
  for (int i = 0; i < ROOM_COUNT; i++) {
    lv_label_set_text(rooms[i].mood, text);
    lv_obj_set_style_bg_color(rooms[i].pet, pet_color, 0);
  }

  lv_label_set_text_fmt(status_age,
                        "Alter: %dh %02dm",
                        (int)(state->age_seconds / 3600),
                        (int)(state->age_seconds % 3600 / 60));

  lv_label_set_text(sleep_button_label,
                    state->sleeping ? "Aufwachen" : "Schlafen");
}
