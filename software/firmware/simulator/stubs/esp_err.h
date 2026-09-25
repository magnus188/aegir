#pragma once

typedef int esp_err_t;

#ifndef ESP_OK
#define ESP_OK 0
#endif

#ifndef ESP_FAIL
#define ESP_FAIL -1
#endif

#ifndef ESP_ERR_INVALID_ARG
#define ESP_ERR_INVALID_ARG 0x102
#endif

#ifndef ESP_ERR_INVALID_STATE
#define ESP_ERR_INVALID_STATE 0x103
#endif

static inline const char* esp_err_to_name(esp_err_t err) {
    switch (err) {
        case ESP_OK:
            return "ESP_OK";
        case ESP_FAIL:
            return "ESP_FAIL";
        case ESP_ERR_INVALID_ARG:
            return "ESP_ERR_INVALID_ARG";
        case ESP_ERR_INVALID_STATE:
            return "ESP_ERR_INVALID_STATE";
        default:
            return "ESP_ERR_UNKNOWN";
    }
}
