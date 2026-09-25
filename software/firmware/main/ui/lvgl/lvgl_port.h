#pragma once
#include <esp_err.h>
#include <stdint.h>
#include <lvgl.h>
#ifdef __cplusplus
extern "C" {
#endif

// Initialize the Guition JC4880P443 native portrait display, touch input, and
// the ESP LVGL adapter task.
esp_err_t lvgl_port_init(void);

// LVGL is not thread-safe. Code running outside an LVGL callback must hold this
// lock while it creates or modifies LVGL objects.
esp_err_t lvgl_port_lock(uint32_t timeout_ms);
void lvgl_port_unlock(void);

#ifdef __cplusplus
}
#endif
