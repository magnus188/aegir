#include <esp_err.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_timer.h>
#include <stdint.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "services/storage_service.h"
#include "services/system_power.h"
#include "services/sd_log_service.h"
#include "services/ota_core.h"
#include "sensors/sensor_hardware.h"

#include "ui/lvgl/lvgl_port.h"
#include "ui/screens/screen_manager.h"
#include "services/settings_service.h"
#include "services/wifi_service.h"
#include "services/c6_update_service.h"
#include "services/battery_service.h"
#include "services/analysis_history.h"
#include "services/cylinder_profiles.h"

namespace {
std::atomic<unsigned> ui_heartbeats{0};
ota::BootProbation boot_probation;
void heartbeat(lv_timer_t *) { ui_heartbeats.fetch_add(1); }
uint32_t now_ms() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }
void rollback_if_available(const char *reason) {
    ESP_LOGE("MAIN", "%s; existing storage preserved", reason);
    if (esp_ota_check_rollback_is_possible()) {
        const esp_err_t result = esp_ota_mark_app_invalid_rollback_and_reboot();
        ESP_LOGE("MAIN", "Rollback returned: %s", esp_err_to_name(result));
    } else {
        ESP_LOGE("MAIN", "No previous bootable image; retain available recovery UI and wired recovery");
    }
}
void startup_supervisor(void *) {
    while (boot_probation.pending()) {
        if (boot_probation.expire(now_ms())) {
            rollback_if_available("Update exceeded its 60-second essential startup deadline");
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    vTaskDelete(nullptr);
}
#if CONFIG_TRIMIX_ENABLE_WIFI
void optional_wifi_worker(void *) {
    // Hosted C6 initialization may block or fail. It must not gate local startup
    // or the OTA probation deadline. These service calls publish plain state.
    wifi_service_init();
    if (wifi_service_is_ready()) wifi_service_auto_connect();
    vTaskDelete(nullptr);
}
#endif
}
extern "C" void app_main(void) {
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    const bool pending = running_partition && esp_ota_get_state_partition(running_partition, &ota_state) == ESP_OK &&
                         ota_state == ESP_OTA_IMG_PENDING_VERIFY;
    boot_probation.begin(now_ms(), pending);
    if (pending && xTaskCreate(startup_supervisor, "boot_probation", 4096, nullptr, 5, nullptr) != pdPASS) {
        rollback_if_available("Could not create update startup supervisor");
        // If rollback cannot run, keep persistence read only while recovering.
        boot_probation.expire(now_ms() + ota::BootProbation::kDeadlineMs);
    }
    // One initializer owns NVS. Errors enter recovery without erasing user calibration.
    storage_init();
    // Initialize persistent, non-UI state before creating screens.
    settings_init();
    analysis_history_init();
    cylinder_profiles_init();

    // esp_lvgl_adapter owns the LVGL tick and worker task. Screen creation is
    // the only application-side LVGL work performed outside its callbacks.
    ESP_ERROR_CHECK(lvgl_port_init());
    ESP_ERROR_CHECK(lvgl_port_lock(UINT32_MAX));
    screens_init();
    lv_timer_t *health_timer = lv_timer_create(heartbeat, 250, nullptr);
    lvgl_port_unlock();
    const bool power_started = system_power_start();
    // Removable media is optional and never part of OTA startup acceptance.
    sd_log_start();

    // Start asynchronous services only after the UI exists. Their callbacks
    // publish plain state; they never call LVGL directly.
#if CONFIG_TRIMIX_ENABLE_WIFI
    if (xTaskCreate(optional_wifi_worker, "wifi_start", 8192, nullptr, 3, nullptr) != pdPASS)
        ESP_LOGE("MAIN", "Wi-Fi startup task unavailable; local analyzer remains available");
#elif CONFIG_TRIMIX_UPDATE_C6_ON_BOOT
    c6_update_service_run();
#endif
    battery_service_init();
    battery_start_monitoring();
#if !CONFIG_TRIMIX_ENABLE_WIFI
    ESP_LOGW("MAIN", "Wi-Fi is disabled in this recovery build");
#endif

    // Require observed UI progress and a responsive worker, not connected optional
    // sensors, Wi-Fi association, or completed thermal warm-up. This is bounded.
    bool healthy = false;
    for (unsigned attempt = 0; attempt < 80; ++attempt) {
        healthy = ota::boot_healthy(ui_heartbeats.load(), power_started && system_power_ready() && sensor_hardware_service_healthy(), storage_ready());
        if (healthy) break;
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    if (lvgl_port_lock(1000) == ESP_OK) {
        if (health_timer) lv_timer_delete(health_timer);
        lvgl_port_unlock();
    }
    // The supervisor and this path arbitrate completion atomically. A startup
    // which finished after its deadline must never accept itself later.
    const auto completion = boot_probation.complete(now_ms());
    const bool completed_in_time = completion == ota::BootProbation::Completion::Timely;
    healthy = healthy && completed_in_time;
    if (completion == ota::BootProbation::Completion::Timeout)
        rollback_if_available("Update completed after its essential startup deadline");
    if (pending && healthy) {
        if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
            storage_accept_boot();
            if(storage_ready())system_power_accept_startup();
        }
    } else if (pending) {
        if (completed_in_time) rollback_if_available("Update failed bounded UI/worker/storage health check");
        // With no bootable previous image, keep this recovery UI rather than a reboot loop.
    } else if (healthy) {
        storage_accept_boot();
        if(storage_ready())system_power_accept_startup();
    }
    if (!storage_ready()) ESP_LOGE("MAIN", "%s", storage_status_message());
}
