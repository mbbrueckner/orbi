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
#define FOOD_HOME_CENTER_Y 210
/** Extra invisible touch area around the food, in pixels. */
#define FOOD_TOUCH_MARGIN 14
/** The food counts as fed when its center is this close to the pet's. */
#define FEED_RADIUS 55

#define BALL_SIZE 40
#define BALL_HOME_CENTER_X (SCREEN_SIZE / 2)
#define BALL_HOME_CENTER_Y 216

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

/*
 * Pixel bars: ten square segments in a frame, like on a classic handheld.
 * A lit segment has a lighter top edge. Everything is drawn without rounded
 * corners, so it stays crisp on the display.
 */
#define BAR_SEGMENTS 10
#define BAR_SEG_HEIGHT 8
#define BAR_GAP 2
#define BAR_BORDER 2
#define BAR_MAX_COUNT 8

typedef struct {
  lv_obj_t* seg[BAR_SEGMENTS];
  lv_color_t color;
  int lit;  /**< Lit segments, -1 until the first update. */
} pixel_bar_t;

static pixel_bar_t pixel_bars[BAR_MAX_COUNT];
static int pixel_bar_count;

static void style_segment(lv_obj_t* seg, bool lit, lv_color_t color) {
  if (lit) {
    lv_obj_set_style_bg_color(seg, color, 0);
    lv_obj_set_style_border_color(seg, lv_color_lighten(color, 90), 0);
  } else {
    lv_obj_set_style_bg_color(seg, lv_color_make(0x3A, 0x34, 0x48), 0);
    lv_obj_set_style_border_color(seg, lv_color_make(0x48, 0x42, 0x58), 0);
  }
}

/** Creates a pixel bar of about @p width pixels that ignores touches. */
static lv_obj_t* make_bar(lv_obj_t* parent, int width, lv_color_t color) {
  pixel_bar_t* bar = &pixel_bars[pixel_bar_count++];
  bar->color = color;
  bar->lit = -1;

  int seg_w = (width - 2 * BAR_BORDER - (BAR_SEGMENTS - 1) * BAR_GAP) / BAR_SEGMENTS;
  int inner_w = BAR_SEGMENTS * seg_w + (BAR_SEGMENTS - 1) * BAR_GAP;

  lv_obj_t* frame = lv_obj_create(parent);
  lv_obj_set_size(
      frame, inner_w + 2 * BAR_BORDER, BAR_SEG_HEIGHT + 2 * BAR_BORDER);
  lv_obj_set_style_radius(frame, 0, 0);
  lv_obj_set_style_pad_all(frame, 0, 0);
  lv_obj_set_style_border_width(frame, BAR_BORDER, 0);
  lv_obj_set_style_border_color(frame, lv_color_make(0xE2, 0xD6, 0xBE), 0);
  lv_obj_set_style_bg_color(frame, lv_color_make(0x22, 0x1C, 0x2C), 0);
  lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, 0);
  lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_user_data(frame, bar);

  for (int i = 0; i < BAR_SEGMENTS; i++) {
    lv_obj_t* seg = lv_obj_create(frame);
    lv_obj_set_size(seg, seg_w, BAR_SEG_HEIGHT);
    lv_obj_set_pos(seg, i * (seg_w + BAR_GAP), 0);
    lv_obj_set_style_radius(seg, 0, 0);
    lv_obj_set_style_pad_all(seg, 0, 0);
    lv_obj_set_style_bg_opa(seg, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(seg, 2, 0);
    lv_obj_set_style_border_side(seg, LV_BORDER_SIDE_TOP, 0);
    lv_obj_clear_flag(seg, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    style_segment(seg, false, color);
    bar->seg[i] = seg;
  }
  return frame;
}

/** Lights as many segments as the value (0 to 100) needs, rounding up. */
static void bar_set_value(lv_obj_t* frame, int value) {
  pixel_bar_t* bar = lv_obj_get_user_data(frame);
  int lit = (value + 9) / 10;
  if (lit < 0) {
    lit = 0;
  } else if (lit > BAR_SEGMENTS) {
    lit = BAR_SEGMENTS;
  }
  if (lit == bar->lit) {
    return;
  }
  bar->lit = lit;
  for (int i = 0; i < BAR_SEGMENTS; i++) {
    style_segment(bar->seg[i], i < lit, bar->color);
  }
}

/**
 * Creates a room page with background color, an optional background picture
 * (NULL for none, drawn doubled) and a title (NULL for no title).
 */
static lv_obj_t* make_page(int index,
                           const char* title,
                           lv_color_t background,
                           const lv_img_dsc_t* picture) {
  lv_obj_t* page = lv_obj_create(pager);
  lv_obj_set_size(page, SCREEN_SIZE, SCREEN_SIZE);
  lv_obj_set_style_radius(page, 0, 0);
  lv_obj_set_style_border_width(page, 0, 0);
  lv_obj_set_style_pad_all(page, 0, 0);
  lv_obj_set_style_bg_color(page, background, 0);
  lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
  lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(page, LV_OBJ_FLAG_EVENT_BUBBLE);

  if (picture != NULL) {
    lv_obj_t* bg = lv_img_create(page);
    lv_img_set_src(bg, picture);
    lv_img_set_antialias(bg, false);
    lv_img_set_zoom(bg, 256 * SCREEN_SIZE / ROOM_BG_SIZE);
    // The doubled picture is drawn around the center of its box.
    lv_obj_set_pos(bg, (SCREEN_SIZE - ROOM_BG_SIZE) / 2, (SCREEN_SIZE - ROOM_BG_SIZE) / 2);
  }

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
static void add_pet_to_room(int index, lv_color_t bar_color, int bar_width) {
  lv_obj_t* page = rooms[index].page;

  lv_obj_t* bar = make_bar(page, bar_width, bar_color);
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
/** Layout of the stat panel: three rows of icon, bar and value. */
#define STATUS_PANEL_W 190
#define STATUS_PANEL_H 78
#define STATUS_PANEL_Y 96
#define STATUS_ROWS_Y 100
#define STATUS_ROW_H 24
#define STATUS_ICON_X 54
#define STATUS_BAR_X 72
#define STATUS_BAR_W 92
#define STATUS_VALUE_X 172

static lv_obj_t* status_mood;
static lv_obj_t* status_age;

static void create_living_room(void) {
  lv_obj_t* page = make_page(ROOM_LIVING,
                             "Wohnzimmer",
                             lv_color_make(0x18, 0x18, 0x20),
                             sprites_room(ROOM_BG_LIVING));

  // The pet sits on the sofa that is part of the background picture.
  lv_obj_t* pet = make_pet(page, STATUS_PET_SIZE);
  lv_obj_align(pet, LV_ALIGN_TOP_MID, 0, 42 + STATUS_PET_SIZE / 2 - SPRITE_SIZE / 2);

  // A framed panel behind the three stat rows.
  lv_obj_t* panel = lv_obj_create(page);
  lv_obj_set_size(panel, STATUS_PANEL_W, STATUS_PANEL_H);
  lv_obj_set_pos(panel, (SCREEN_SIZE - STATUS_PANEL_W) / 2, STATUS_PANEL_Y);
  lv_obj_set_style_radius(panel, 0, 0);
  lv_obj_set_style_pad_all(panel, 0, 0);
  lv_obj_set_style_border_width(panel, 2, 0);
  lv_obj_set_style_border_color(panel, lv_color_make(0xE2, 0xD6, 0xBE), 0);
  lv_obj_set_style_outline_width(panel, 2, 0);
  lv_obj_set_style_outline_color(panel, lv_color_make(0x0C, 0x08, 0x14), 0);
  lv_obj_set_style_bg_color(panel, lv_color_make(0x14, 0x10, 0x1E), 0);
  lv_obj_set_style_bg_opa(panel, 220, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

  const lv_color_t colors[STAT_COUNT] = {lv_color_make(0x40, 0xC0, 0x60),
                                         lv_color_make(0x50, 0x90, 0xFF),
                                         lv_color_make(0xF0, 0xD0, 0x40)};
  for (int i = 0; i < STAT_COUNT; i++) {
    int top = STATUS_ROWS_Y + i * STATUS_ROW_H;

    // Icon sprite (10 px, shown doubled), drawn around the center of its box.
    lv_obj_t* icon = lv_img_create(page);
    lv_img_set_src(icon, sprites_get(SPRITE_ICON, i));
    lv_img_set_antialias(icon, false);
    lv_img_set_zoom(icon, 512);
    lv_obj_set_pos(icon,
                   STATUS_ICON_X - SPRITE_ICON_SIZE / 2,
                   top + STATUS_ROW_H / 2 - SPRITE_ICON_SIZE / 2);

    stat_bars[i] = make_bar(page, STATUS_BAR_W, colors[i]);
    lv_obj_set_pos(stat_bars[i], STATUS_BAR_X, top + 4);

    stat_values[i] = lv_label_create(page);
    lv_label_set_text(stat_values[i], "");
    lv_obj_set_style_text_color(stat_values[i], lv_color_white(), 0);
    lv_obj_set_pos(stat_values[i], STATUS_VALUE_X, top + 2);
  }

  status_mood = lv_label_create(page);
  lv_label_set_text(status_mood, "");
  lv_obj_set_style_text_color(status_mood, lv_color_make(0xFF, 0xB0, 0x40), 0);
  lv_obj_align(status_mood, LV_ALIGN_TOP_MID, 0, 178);

  status_age = lv_label_create(page);
  lv_label_set_text(status_age, "");
  lv_obj_set_style_text_color(status_age, lv_color_make(0xC0, 0xB8, 0xD0), 0);
  lv_obj_set_style_text_font(status_age, &lv_font_unscii_8, 0);  // small
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
      make_page(ROOM_DINING,
                "Esszimmer",
                lv_color_make(0x2A, 0x1A, 0x10),
                sprites_room(ROOM_BG_DINING));
  add_pet_to_room(ROOM_DINING, lv_color_make(0x40, 0xC0, 0x60), 100);

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
/*
 * The title sits at the very top, above the lamp's shade. The lamp's thin
 * cord runs behind the letters, so the title is created after the lamp.
 */
#define BEDROOM_TITLE_Y 12
#define BEDROOM_BAR_Y 214
#define BEDROOM_BAR_W 100

static void lamp_cb(lv_event_t* e) {
  (void)e;
  if (tap_blocked()) {
    return;
  }
  send_action(pet_sleeping ? UI_ACTION_WAKE : UI_ACTION_SLEEP);
}

static void create_bedroom(void) {
  lv_obj_t* page =
      make_page(ROOM_BEDROOM,
                NULL,
                lv_color_make(0x10, 0x14, 0x30),
                sprites_room(ROOM_BG_BEDROOM));
  add_pet_to_room(
      ROOM_BEDROOM, lv_color_make(0x50, 0x90, 0xFF), BEDROOM_BAR_W);
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

  lv_obj_t* title = lv_label_create(page);
  lv_label_set_text(title, "Schlafzimmer");
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, BEDROOM_TITLE_Y);
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
#define BALL_FAR_Y 98
#define BALL_FAR_SIZE 20
#define PET_FAR_X 158
#define PET_FAR_Y 90
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

/*
 * Running: there is no running sprite, so the sitting raccoon hops and tilts
 * from side to side. One hop per half turn of the sine wave; the hop height
 * and the tilt scale with the drawn size, so the far pet hops less.
 */
#define RUN_OUT_HOPS 4
#define RUN_BACK_HOPS 5
/** Hop height for a pet drawn at PET_SIZE, in pixels. */
#define HOP_HEIGHT 18
/** Tilt at the middle of a hop, in tenths of a degree. */
#define RUN_TILT 50

/** Lift (pixels) and tilt (0.1 degrees) of the running pet at u (permille). */
static void run_pose(int32_t u, int hops, int size, int32_t* lift, int32_t* tilt) {
  int32_t phase = hops * 180 * u / 1000;  // degrees of the sine wave
  int32_t wave = lv_trigo_sin((int16_t)phase);
  *lift = (wave < 0 ? -wave : wave) * HOP_HEIGHT / 32768 * size / PET_SIZE;
  *tilt = wave * RUN_TILT / 32768;
}

/**
 * The ball in the pet's mouth. The mouth sits left of and a bit below the
 * center of the sprite (pixel 25, 35 of 80), scaled with the drawn size.
 */
static void place_ball_in_mouth(int pet_x, int pet_y, int pet_size) {
  int ball_size = pet_size / 8;  // 20 px for the full-size pet
  if (ball_size < 10) {
    ball_size = 10;
  }
  place_ball(garden_ball,
             pet_x + (25 - 40) * pet_size / SPRITE_SIZE,
             pet_y + (35 - 40) * pet_size / SPRITE_SIZE,
             ball_size);
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
    int32_t size = lerp(PET_SIZE, PET_FAR_SIZE, u);
    int32_t lift, tilt;
    run_pose(u, RUN_OUT_HOPS, size, &lift, &tilt);
    place_pet(garden_pet,
              lerp(PET_CENTER_X, PET_FAR_X, u),
              lerp(PET_CENTER_Y, PET_FAR_Y, u) - lift,
              size);
    lv_img_set_angle(garden_pet, tilt);
  } else {
    int32_t u = (t - FETCH_RUN_OUT_END_MS) * 1000 /
                (FETCH_TOTAL_MS - FETCH_RUN_OUT_END_MS);
    int32_t pet_size = lerp(PET_FAR_SIZE, PET_SIZE, u);
    int32_t pet_x = lerp(PET_FAR_X, PET_CENTER_X, u);
    int32_t pet_y = lerp(PET_FAR_Y, PET_CENTER_Y, u);
    int32_t lift, tilt;
    run_pose(u, RUN_BACK_HOPS, pet_size, &lift, &tilt);
    pet_y -= lift;
    place_pet(garden_pet, pet_x, pet_y, pet_size);
    lv_img_set_angle(garden_pet, tilt);
    // The ball is carried in the pet's mouth.
    show_ball_frame(1);
    place_ball_in_mouth(pet_x, pet_y, pet_size);
  }
}

static void fetch_done(lv_anim_t* anim) {
  (void)anim;
  place_pet(garden_pet, PET_CENTER_X, PET_CENTER_Y, PET_SIZE);
  lv_img_set_angle(garden_pet, 0);
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
      make_page(ROOM_GARDEN,
                "Garten",
                lv_color_make(0x10, 0x28, 0x1C),
                sprites_room(ROOM_BG_GARDEN));
  add_pet_to_room(ROOM_GARDEN, lv_color_make(0xF0, 0xD0, 0x40), 100);
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

void ui_rooms_update(const pet_state_t* state) {
  static const char* const mood_names[] = {"froehlich", "hungrig", "muede", "traurig"};

  pet_sleeping = state->sleeping;

  const int values[STAT_COUNT] = {
      100 - state->hunger, state->energy, state->happiness};
  for (int i = 0; i < STAT_COUNT; i++) {
    bar_set_value(stat_bars[i], values[i]);
    lv_label_set_text_fmt(stat_values[i], "%d", values[i]);
  }
  bar_set_value(rooms[ROOM_DINING].bar, values[STAT_SATIETY]);
  bar_set_value(rooms[ROOM_BEDROOM].bar, values[STAT_ENERGY]);
  bar_set_value(rooms[ROOM_GARDEN].bar, values[STAT_HAPPINESS]);

  const char* text = state->sleeping ? "schlaeft" : mood_names[pet_get_mood(state)];
  for (int i = 0; i < ROOM_COUNT; i++) {
    lv_label_set_text(rooms[i].mood, text);
  }
  if (!fetching) {
    set_ball_bounce(!state->sleeping);
  }
  sprite_set = state->sleeping ? SPRITE_SLEEP : (sprite_set_t)pet_get_mood(state);
  show_sprite_frame();

  lv_label_set_text_fmt(status_age,
                        "Alter: %dh %02dm",
                        (int)(state->age_seconds / 3600),
                        (int)(state->age_seconds % 3600 / 60));

  lv_img_set_src(lamp, sprites_get(SPRITE_LAMP, state->sleeping ? 1 : 0));
  if (state->sleeping) {
    lv_obj_clear_flag(night_overlay, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(night_overlay, LV_OBJ_FLAG_HIDDEN);
  }
}
