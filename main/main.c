#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// TODO: include the individual modules once they exist:
// #include "tamagotchi_state.h"
// #include "ui_render.h"
// #include "input_touch.h"
// #include "storage_nvs.h"

static const char* TAG = "orbi";

void app_main(void) {
  ESP_LOGI(TAG, "orbi starting...");

  // TODO order according to the roadmap:
  // 1. storage_nvs_init() + load the persisted state
  // 2. Initialize display + LVGL (ui_render_init)
  // 3. Initialize touch and register it as an LVGL input
  // 4. tamagotchi_state_init()
  // 5. Start main loop / tick timer

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
