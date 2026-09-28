#pragma once

#include <cstdint>
#include <esp_err.h>

#define ESP_ERR_NVS_NOT_FOUND 0x1102

using nvs_handle_t = int;

#define NVS_READONLY 0

inline esp_err_t nvs_open(const char*, int, nvs_handle_t*) { return ESP_ERR_NVS_NOT_FOUND; }
inline esp_err_t nvs_get_i32(nvs_handle_t, const char*, int32_t*) { return ESP_ERR_NVS_NOT_FOUND; }
inline void nvs_close(nvs_handle_t) {}
