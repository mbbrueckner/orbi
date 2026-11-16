/**
 * @file ui_render.c
 * @brief Implementation of the renderer; the API is documented in ui_render.h.
 *
 * Pin mapping for the Seeed Round Display on a XIAO ESP32-C6 (XIAO pin ->
 * GPIO): D1 -> 1 LCD_CS, D3 -> 21 LCD_DC, D6 -> 16 backlight, D8 -> 19 SCK,
 * D10 -> 18 MOSI. The panel has no reset pin, a software reset is used.
 *
 * @todo Hook up LVGL with a flush callback on top of the panel handle.
 */

#include "ui_render.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"

#define LCD_HOST SPI2_HOST
#define PIN_LCD_SCK 19
#define PIN_LCD_MOSI 18
#define PIN_LCD_CS 1
#define PIN_LCD_DC 21
#define PIN_LCD_BACKLIGHT 16

#define LCD_WIDTH 240
#define LCD_HEIGHT 240
/** Rows sent per SPI transfer; keeps the DMA buffer small. */
#define LCD_FILL_ROWS 20

static const char* TAG = "ui";
static esp_lcd_panel_handle_t panel;

void ui_render_init(void) {
  spi_bus_config_t bus_config = {
      .sclk_io_num = PIN_LCD_SCK,
      .mosi_io_num = PIN_LCD_MOSI,
      .miso_io_num = -1,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = LCD_WIDTH * LCD_FILL_ROWS * 2,
  };
  ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_config, SPI_DMA_CH_AUTO));

  esp_lcd_panel_io_handle_t io;
  esp_lcd_panel_io_spi_config_t io_config =
      GC9A01_PANEL_IO_SPI_CONFIG(PIN_LCD_CS, PIN_LCD_DC, NULL, NULL);
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
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

  gpio_config_t backlight = {
      .pin_bit_mask = 1ULL << PIN_LCD_BACKLIGHT,
      .mode = GPIO_MODE_OUTPUT,
  };
  ESP_ERROR_CHECK(gpio_config(&backlight));
  ESP_ERROR_CHECK(gpio_set_level(PIN_LCD_BACKLIGHT, 1));

  ESP_LOGI(TAG, "display initialised");
}

void ui_render_fill(uint16_t rgb565) {
  uint16_t* line = heap_caps_malloc(
      LCD_WIDTH * LCD_FILL_ROWS * sizeof(uint16_t), MALLOC_CAP_DMA);
  if (line == NULL) {
    ESP_LOGE(TAG, "no memory for fill buffer");
    return;
  }

  // The panel expects the high byte first, the CPU stores the low byte first.
  uint16_t swapped = (uint16_t)((rgb565 >> 8) | (rgb565 << 8));
  for (int i = 0; i < LCD_WIDTH * LCD_FILL_ROWS; i++) {
    line[i] = swapped;
  }

  for (int y = 0; y < LCD_HEIGHT; y += LCD_FILL_ROWS) {
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(
        panel, 0, y, LCD_WIDTH, y + LCD_FILL_ROWS, line));
  }
  heap_caps_free(line);
}

void ui_render_update(const pet_state_t* state) { (void)state; }
