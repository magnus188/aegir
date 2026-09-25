#pragma once
#include <lvgl.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// WiFi signal strength levels
typedef enum {
    WIFI_SIGNAL_NONE = 0,
    WIFI_SIGNAL_WEAK,      // 1 bar
    WIFI_SIGNAL_FAIR,      // 2 bars
    WIFI_SIGNAL_GOOD,      // 3 bars
    WIFI_SIGNAL_EXCELLENT  // Full
} wifi_signal_level_t;

// Battery charge levels
typedef enum {
    BATTERY_LEVEL_CRITICAL = 0,  // < 10%
    BATTERY_LEVEL_LOW,           // 10-25%
    BATTERY_LEVEL_MEDIUM,        // 25-50%
    BATTERY_LEVEL_HIGH,          // 50-75%
    BATTERY_LEVEL_FULL           // > 75%
} battery_level_t;

/**
 * Create status bar icons container (WiFi + Battery)
 * @param parent Parent object (typically navbar)
 * @return The status container object
 */
lv_obj_t* status_icons_create(lv_obj_t* parent);

/**
 * Update WiFi status icon
 * @param connected Whether WiFi is connected
 * @param signal_level Signal strength (ignored if not connected)
 */
void status_set_wifi(bool connected, wifi_signal_level_t signal_level);

/**
 * Update battery status
 * @param percentage Battery percentage (0-100)
 * @param charging Whether battery is charging
 */
void status_set_battery(uint8_t percentage, bool charging);
void status_set_battery_available(bool available);

/**
 * Get current WiFi connection status
 */
bool status_get_wifi_connected(void);

/**
 * Get current battery percentage
 */
uint8_t status_get_battery_percentage(void);

#ifdef __cplusplus
}
#endif
