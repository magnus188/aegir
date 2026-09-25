#pragma once
// Host declarations for the real ESP_PLATFORM storage branch. This fake models
// NVS commit boundaries, not Espressif flash internals or physical power cuts.
#include <cstddef>
#include <cstdint>
using esp_err_t = int;
constexpr esp_err_t ESP_OK=0, ESP_FAIL=-1, ESP_ERR_NVS_NOT_FOUND=0x1102;
constexpr esp_err_t ESP_ERR_NVS_NO_FREE_PAGES=0x110d, ESP_ERR_NVS_NEW_VERSION_FOUND=0x1110;
using nvs_handle_t = unsigned;
enum nvs_open_mode_t { NVS_READONLY, NVS_READWRITE };
struct nvs_stats_t { size_t used_entries, free_entries, total_entries, namespace_count; };
struct esp_partition_t { int unused; };
enum esp_ota_img_states_t { ESP_OTA_IMG_NEW, ESP_OTA_IMG_PENDING_VERIFY, ESP_OTA_IMG_VALID };
esp_err_t nvs_flash_init();
esp_err_t nvs_open(const char *, nvs_open_mode_t, nvs_handle_t *);
esp_err_t nvs_get_blob(nvs_handle_t, const char *, void *, size_t *);
esp_err_t nvs_set_blob(nvs_handle_t, const char *, const void *, size_t);
esp_err_t nvs_commit(nvs_handle_t);
void nvs_close(nvs_handle_t);
esp_err_t nvs_get_stats(const char *, nvs_stats_t *);
const esp_partition_t *esp_ota_get_running_partition();
esp_err_t esp_ota_get_state_partition(const esp_partition_t *, esp_ota_img_states_t *);
const char *esp_err_to_name(esp_err_t);
#define ESP_LOGE(...) ((void)0)
