/**
 * @file ui_render.c
 * @brief Implementation of the renderer; the API is documented in ui_render.h.
 *
 * Pin mapping for the Seeed Round Display on a XIAO ESP32-C6 (XIAO pin ->
 * GPIO): D1 -> 1 LCD_CS, D3 -> 21 LCD_DC, D6 -> 16 backlight, D8 -> 19 SCK,
 * D10 -> 18 MOSI. The panel has no reset pin, a software reset is used.
 *
 * LVGL is not thread safe, so every call into it is wrapped by lvgl_mutex.
 * LVGL itself runs in its own task (lvgl_task).
 */

#include "ui_render.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include <stdbool.h>

#include "esp_heap_caps.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "ui_rooms.h"

#define LCD_HOST SPI2_HOST
#define PIN_LCD_SCK 19
#define PIN_LCD_MOSI 18
#define PIN_LCD_CS 1
#define PIN_LCD_DC 21
#define PIN_LCD_BACKLIGHT 16

#define LCD_WIDTH 240
#define LCD_HEIGHT 240
/** Rows per LVGL draw buffer; two buffers of this size are allocated. */
#define LVGL_BUF_ROWS 20
/** Without touch for this long, the picture dims. */
#define DIM_AFTER_MS 60000
/**
 * The backlight of this display cannot be regulated: PWM from 200 Hz to
 * 5 kHz changed nothing. So dimming is a black layer over everything, with
 * this opacity (0 to 255). Never fully black: the screen is never off.
 */
#define DIM_OPACITY 190
#define DIM_FADE_MS 500
/** LVGL tick period in milliseconds. */
#define LVGL_TICK_MS 2

static const char* TAG = "ui";

static esp_lcd_panel_handle_t panel;
static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t disp_drv;
static SemaphoreHandle_t lvgl_mutex;
static lv_obj_t* dim_layer;

/** Called by the SPI driver when a flush transfer is finished. */
static bool on_flush_done(esp_lcd_panel_io_handle_t io,
                          esp_lcd_panel_io_event_data_t* event,
                          void* user_ctx) {
  (void)io;
  (void)event;
  lv_disp_flush_ready((lv_disp_drv_t*)user_ctx);
  return false;
}

/** LVGL asks us to copy a rendered area to the display. */
static void flush_cb(lv_disp_drv_t* drv,
                     const lv_area_t* area,
                     lv_color_t* color_p) {
  (void)drv;
  esp_lcd_panel_draw_bitmap(
      panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_p);
}

static void tick_cb(void* arg) {
  (void)arg;
  lv_tick_inc(LVGL_TICK_MS);
}

static void dim_opacity_cb(void* var, int32_t opacity) {
  lv_obj_set_style_bg_opa((lv_obj_t*)var, (lv_opa_t)opacity, 0);
}

/** Fades the dim layer in or out. Call with the LVGL lock held. */
static void set_dimmed(bool dimmed) {
  lv_anim_del(dim_layer, dim_opacity_cb);

  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, dim_layer);
  lv_anim_set_exec_cb(&anim, dim_opacity_cb);
  lv_anim_set_values(
      &anim, lv_obj_get_style_bg_opa(dim_layer, 0), dimmed ? DIM_OPACITY : 0);
  lv_anim_set_time(&anim, DIM_FADE_MS);
  lv_anim_start(&anim);
}

/** Creates the black layer on top of everything; touches pass through it. */
static void create_dim_layer(void) {
  dim_layer = lv_obj_create(lv_layer_top());
  lv_obj_set_size(dim_layer, LCD_WIDTH, LCD_HEIGHT);
  lv_obj_set_pos(dim_layer, 0, 0);
  lv_obj_set_style_radius(dim_layer, 0, 0);
  lv_obj_set_style_border_width(dim_layer, 0, 0);
  lv_obj_set_style_bg_color(dim_layer, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(dim_layer, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(dim_layer, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
}

static void lvgl_task(void* arg) {
  (void)arg;
  bool dimmed = false;
  while (1) {
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
    uint32_t wait_ms = lv_timer_handler();
    bool idle = lv_disp_get_inactive_time(NULL) > DIM_AFTER_MS;
    if (idle != dimmed) {
      dimmed = idle;
      set_dimmed(dimmed);
    }
    xSemaphoreGive(lvgl_mutex);

    if (wait_ms < 5) {
      wait_ms = 5;
    }
    if (wait_ms > 30) {
      wait_ms = 30;  // LV_NO_TIMER_READY and friends must not sleep forever
    }
    TickType_t delay = pdMS_TO_TICKS(wait_ms);
    vTaskDelay(delay > 0 ? delay : 1);
  }
}

static void init_panel(void) {
  spi_bus_config_t bus_config = {
      .sclk_io_num = PIN_LCD_SCK,
      .mosi_io_num = PIN_LCD_MOSI,
      .miso_io_num = -1,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = LCD_WIDTH * LVGL_BUF_ROWS * 2,
  };
  ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_config, SPI_DMA_CH_AUTO));

  esp_lcd_panel_io_handle_t io;
  esp_lcd_panel_io_spi_config_t io_config =
      GC9A01_PANEL_IO_SPI_CONFIG(PIN_LCD_CS, PIN_LCD_DC, on_flush_done,
                                 &disp_drv);
  io_config.pclk_hz = 40 * 1000 * 1000;
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
      (esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io));

  esp_lcd_panel_dev_config_t panel_config = {
      .reset_gpio_num = -1,
      .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
      .bits_per_pixel = 16,
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io, &panel_config, &panel));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
  ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
  // Rotate the picture to match how the display sits in the case.
  // input_touch.c applies the same rotation to the touch position.
  ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, true));
  ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, true, true));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

  // The backlight is simply on (see DIM_OPACITY for why there is no PWM).
  gpio_config_t backlight = {
      .pin_bit_mask = 1ULL << PIN_LCD_BACKLIGHT,
      .mode = GPIO_MODE_OUTPUT,
  };
  ESP_ERROR_CHECK(gpio_config(&backlight));
  ESP_ERROR_CHECK(gpio_set_level(PIN_LCD_BACKLIGHT, 1));
}

static void init_lvgl(void) {
  lv_init();

  size_t buf_bytes = LCD_WIDTH * LVGL_BUF_ROWS * sizeof(lv_color_t);
  lv_color_t* buf1 = heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA);
  lv_color_t* buf2 = heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA);
  assert(buf1 != NULL && buf2 != NULL);
  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, LCD_WIDTH * LVGL_BUF_ROWS);

  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = LCD_WIDTH;
  disp_drv.ver_res = LCD_HEIGHT;
  disp_drv.flush_cb = flush_cb;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  const esp_timer_create_args_t tick_args = {
      .callback = tick_cb,
      .name = "lvgl_tick",
  };
  esp_timer_handle_t tick_timer;
  ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_timer));
  ESP_ERROR_CHECK(
      esp_timer_start_periodic(tick_timer, LVGL_TICK_MS * 1000));
}

void ui_render_init(ui_action_cb_t action_cb) {
  lvgl_mutex = xSemaphoreCreateMutex();
  assert(lvgl_mutex != NULL);

  init_lvgl();  // disp_drv must exist before the panel callback uses it
  init_panel();
  ui_rooms_create(action_cb);
  create_dim_layer();

  xTaskCreate(lvgl_task, "lvgl", 6144, NULL, 2, NULL);
  ESP_LOGI(TAG, "display and LVGL initialised");
}

void ui_render_lock(void) { xSemaphoreTake(lvgl_mutex, portMAX_DELAY); }

void ui_render_unlock(void) { xSemaphoreGive(lvgl_mutex); }

void ui_render_update(const pet_state_t* state) {
  xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
  ui_rooms_update(state);
  xSemaphoreGive(lvgl_mutex);
}
