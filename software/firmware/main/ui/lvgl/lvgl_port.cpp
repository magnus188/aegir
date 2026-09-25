#include "lvgl_port.h"

#include "board/guition_jc4880p443.h"
#include "services/backlight_service.h"

#include <esp_check.h>
#include <esp_log.h>
#include <esp_lv_adapter.h>

namespace {

constexpr char TAG[] = "LVGL_PORT";
bool g_initialized = false;

}  // namespace

extern "C" esp_err_t lvgl_port_init(void) {
    if (g_initialized) {
        return ESP_OK;
    }

    const esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    ESP_RETURN_ON_ERROR(esp_lv_adapter_init(&adapter_config), TAG, "LVGL adapter init failed");

    esp_lcd_panel_handle_t panel = nullptr;
    esp_lcd_panel_io_handle_t panel_io = nullptr;
    ESP_RETURN_ON_ERROR(guition_display_init(&panel, &panel_io), TAG, "Guition display init failed");

    // The glass is natively 480x800 portrait, so no rotation is required.
    esp_lv_adapter_display_config_t display_config = {
        .panel = panel,
        .panel_io = panel_io,
        .profile = {
            .interface = ESP_LV_ADAPTER_PANEL_IF_MIPI_DSI,
            .rotation = ESP_LV_ADAPTER_ROTATE_0,
            .hor_res = GUITION_LCD_H_RES,
            .ver_res = GUITION_LCD_V_RES,
            .buffer_height = 50,
            .use_psram = false,
            .enable_ppa_accel = false,
            .require_double_buffer = false,
        },
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL,
        .te_sync = ESP_LV_ADAPTER_TE_SYNC_DISABLED(),
    };

    lv_display_t* display = esp_lv_adapter_register_display(&display_config);
    if (!display) {
        ESP_LOGE(TAG, "Failed to start the native portrait display");
        return ESP_FAIL;
    }

    esp_lcd_touch_handle_t touch = nullptr;
    ESP_RETURN_ON_ERROR(guition_touch_init(&touch), TAG, "Guition touch init failed");
    const esp_lv_adapter_touch_config_t touch_config =
        ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(display, touch);
    if (!esp_lv_adapter_register_touch(&touch_config)) {
        ESP_LOGE(TAG, "Failed to register the GT911 touch device");
        return ESP_FAIL;
    }

    ESP_RETURN_ON_ERROR(esp_lv_adapter_start(), TAG, "LVGL adapter start failed");

    // The Guition board driver owns the active-high GPIO23 PWM channel.
    backlight_init();
    backlight_set(100);

    g_initialized = true;
    ESP_LOGI(TAG, "Native 480x800 MIPI-DSI display initialized");
    return ESP_OK;
}

extern "C" esp_err_t lvgl_port_lock(uint32_t timeout_ms) {
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_lv_adapter_lock(timeout_ms == UINT32_MAX ? -1 : static_cast<int32_t>(timeout_ms));
}

extern "C" void lvgl_port_unlock(void) {
    if (g_initialized) {
        esp_lv_adapter_unlock();
    }
}
