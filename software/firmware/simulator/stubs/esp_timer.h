#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef int esp_err_t;

#ifndef ESP_OK
#define ESP_OK 0
#endif

#ifndef ESP_ERROR_CHECK
#define ESP_ERROR_CHECK(expr)                                                     \
    do {                                                                          \
        esp_err_t _esp_err = (expr);                                               \
        if (_esp_err != ESP_OK) {                                                  \
            fprintf(stderr, "[sim] ESP_ERROR_CHECK failed: %d\n", _esp_err);      \
            abort();                                                              \
        }                                                                         \
    } while (0)
#endif

typedef void (*esp_timer_cb_t)(void*);

typedef enum {
    ESP_TIMER_TASK = 0,
} esp_timer_dispatch_t;

typedef struct {
    esp_timer_cb_t callback;
    void* arg;
    esp_timer_dispatch_t dispatch_method;
    const char* name;
    bool skip_unhandled_events;
} esp_timer_create_args_t;

typedef struct {
    bool active;
    esp_timer_cb_t callback;
    void* arg;
} esp_timer_mock_t;

typedef esp_timer_mock_t* esp_timer_handle_t;

static inline esp_err_t esp_timer_create(const esp_timer_create_args_t* args, esp_timer_handle_t* out_timer) {
    if (!args || !out_timer) return 1;
    esp_timer_mock_t* timer = (esp_timer_mock_t*)malloc(sizeof(esp_timer_mock_t));
    if (!timer) return 1;
    timer->active = false;
    timer->callback = args->callback;
    timer->arg = args->arg;
    *out_timer = timer;
    return ESP_OK;
}

static inline bool esp_timer_is_active(esp_timer_handle_t timer) {
    return timer ? timer->active : false;
}

static inline esp_err_t esp_timer_start_once(esp_timer_handle_t timer, unsigned long long) {
    if (!timer) return 1;
    timer->active = true;
    return ESP_OK;
}
