#include "battery_service.h"
#include "system_power.h"
#include "../ui/components/status_icons.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
#include <cmath>

namespace {
std::atomic<bool> active{false};
std::atomic<bool> running{false};
void monitor(void *) {
    while (active.load()) {
        const auto s=system_power_sample();
        if (s.gauge_valid) status_set_battery(static_cast<uint8_t>(std::lround(s.soc_percent)),s.charger_valid && s.charging);
        else status_set_battery_available(false);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    running=false;
    // A start requested during teardown is serviced only after ownership is released.
    if (active.load()) battery_start_monitoring();
    vTaskDelete(nullptr);
}
}
void battery_service_init() { status_set_battery_available(false); }
void battery_start_monitoring() {
    active=true;
    bool expected=false;
    if (!running.compare_exchange_strong(expected,true)) return;
    const BaseType_t created=xTaskCreate(monitor,"battery_ui",3072,nullptr,3,nullptr);
    if (created!=pdPASS) { running=false; active=false; }
}
void battery_stop_monitoring() { active=false; }
bool battery_is_available() { return system_power_sample().gauge_valid; }
uint8_t battery_get_percentage() {
    const auto s=system_power_sample();
    return s.gauge_valid ? static_cast<uint8_t>(std::lround(s.soc_percent)) : 0;
}
bool battery_is_charging() { const auto s=system_power_sample(); return s.charger_valid && s.charging; }
uint32_t battery_get_voltage_mv() {
    const auto s=system_power_sample(); return s.gauge_valid ? static_cast<uint32_t>(std::lround(s.voltage_mv)) : 0;
}
battery_hw_type_t battery_get_hw_type() { return battery_is_available() ? BATTERY_HW_MAX17048 : BATTERY_HW_UNAVAILABLE; }
bool battery_has_fuel_gauge() { return battery_is_available(); }
float battery_get_soc() { return system_power_sample().soc_percent; }
float battery_get_charge_rate() { return system_power_sample().rate_percent_hour; }
