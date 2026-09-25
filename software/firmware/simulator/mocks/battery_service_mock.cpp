#include "services/battery_service.h"
#include "ui/components/status_icons.h"

namespace {

uint8_t g_percentage = 86;
bool g_charging = false;
uint32_t g_voltage_mv = 4030;
bool g_available = true;

}  // namespace

void battery_service_init(void) {
    status_set_battery(g_percentage, g_charging);
}
bool battery_is_available(void) { return g_available; }
extern "C" void battery_mock_set_available(bool available) {
    g_available = available;
    status_set_battery_available(available);
}

uint8_t battery_get_percentage(void) {
    return g_percentage;
}

bool battery_is_charging(void) {
    return g_charging;
}

uint32_t battery_get_voltage_mv(void) {
    return g_voltage_mv;
}

void battery_start_monitoring(void) {
    status_set_battery(g_percentage, g_charging);
}

void battery_stop_monitoring(void) {}

battery_hw_type_t battery_get_hw_type(void) {
    return g_available ? BATTERY_HW_MOCK : BATTERY_HW_UNAVAILABLE;
}

bool battery_has_fuel_gauge(void) {
    return false;
}

float battery_get_soc(void) {
    return static_cast<float>(g_percentage);
}

float battery_get_charge_rate(void) {
    return g_charging ? 18.0f : -2.0f;
}
