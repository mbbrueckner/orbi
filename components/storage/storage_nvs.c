/**
 * @file storage_nvs.c
 * @brief Implementation of the NVS persistence; see storage_nvs.h.
 */

#include "storage_nvs.h"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char* TAG = "storage";
static const char* NVS_NAMESPACE = "orbi";
static const char* NVS_KEY = "pet";

void storage_nvs_init(void) {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
}

bool storage_nvs_load(storage_record_t* record) {
  nvs_handle_t handle;
  if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
    return false;  // namespace does not exist yet: first boot
  }

  uint8_t buf[STORAGE_RECORD_SIZE + 1];
  size_t len = sizeof(buf);
  esp_err_t err = nvs_get_blob(handle, NVS_KEY, buf, &len);
  nvs_close(handle);
  if (err != ESP_OK) {
    return false;  // key missing, or blob larger than any valid record
  }

  if (!storage_record_unpack(buf, len, record)) {
    ESP_LOGW(TAG, "stored record invalid, starting fresh");
    return false;
  }
  return true;
}

bool storage_nvs_save(const storage_record_t* record) {
  uint8_t buf[STORAGE_RECORD_SIZE];
  size_t len = storage_record_pack(record, buf);

  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
    return false;
  }

  err = nvs_set_blob(handle, NVS_KEY, buf, len);
  if (err == ESP_OK) {
    err = nvs_commit(handle);
  }
  nvs_close(handle);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "save failed: %s", esp_err_to_name(err));
    return false;
  }
  return true;
}
