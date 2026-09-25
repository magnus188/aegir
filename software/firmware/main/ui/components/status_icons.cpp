#include "status_icons.h"
#include "../styles/styles.h"
#include <esp_log.h>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static const char* TAG = "STATUS_ICONS";

namespace {

// Mutex to protect shared state access
SemaphoreHandle_t g_status_mutex = nullptr;

// Track all status icon instances for global updates
struct StatusInstance {
    lv_obj_t* container;
    lv_obj_t* wifi_icon;
    lv_obj_t* battery_icon;
    lv_obj_t* battery_pct;
};

std::vector<StatusInstance> g_instances;

// Global status state (shared across all instances)
bool g_wifi_connected = false;
wifi_signal_level_t g_wifi_signal = WIFI_SIGNAL_NONE;
uint8_t g_battery_percentage = 100;
bool g_battery_charging = false;
bool g_battery_available = false;
uint32_t g_status_generation = 1;
uint32_t g_applied_generation = 0;
lv_timer_t* g_status_timer = nullptr;

struct StatusSnapshot {
    bool wifi_connected;
    wifi_signal_level_t wifi_signal;
    uint8_t battery_percentage;
    bool battery_charging;
    bool battery_available;
    uint32_t generation;
};

StatusSnapshot snapshot_status() {
    StatusSnapshot snapshot;
    if (g_status_mutex) {
        xSemaphoreTake(g_status_mutex, portMAX_DELAY);
    }
    snapshot.wifi_connected = g_wifi_connected;
    snapshot.wifi_signal = g_wifi_signal;
    snapshot.battery_percentage = g_battery_percentage;
    snapshot.battery_charging = g_battery_charging;
    snapshot.battery_available = g_battery_available;
    snapshot.generation = g_status_generation;
    if (g_status_mutex) {
        xSemaphoreGive(g_status_mutex);
    }
    return snapshot;
}

// WiFi symbols based on connection state
const char* get_wifi_symbol(bool connected) {
    return LV_SYMBOL_WIFI;
}

uint32_t get_wifi_color(wifi_signal_level_t level) {
    switch (level) {
        case WIFI_SIGNAL_EXCELLENT:
        case WIFI_SIGNAL_GOOD:
            return STYLE_COLOR_TEXT_LIGHT;
        case WIFI_SIGNAL_FAIR:
            return STYLE_COLOR_WARNING;
        case WIFI_SIGNAL_WEAK:
        case WIFI_SIGNAL_NONE:
        default:
            return STYLE_COLOR_TEXT_DIM;
    }
}

const char* get_battery_symbol(uint8_t percentage, bool charging) {
    if (charging) return LV_SYMBOL_CHARGE;
    if (percentage > 75) return LV_SYMBOL_BATTERY_FULL;
    if (percentage > 50) return LV_SYMBOL_BATTERY_3;
    if (percentage > 25) return LV_SYMBOL_BATTERY_2;
    if (percentage > 10) return LV_SYMBOL_BATTERY_1;
    return LV_SYMBOL_BATTERY_EMPTY;
}

uint32_t get_battery_color(uint8_t percentage, bool charging) {
    if (charging) return STYLE_COLOR_SUCCESS;
    if (percentage <= 10) return STYLE_COLOR_ERROR;
    if (percentage <= 25) return STYLE_COLOR_WARNING;
    return STYLE_COLOR_TEXT_LIGHT;
}

// Remove invalid instances from tracking list
void cleanup_invalid_instances() {
    g_instances.erase(
        std::remove_if(g_instances.begin(), g_instances.end(), 
            [](const StatusInstance& inst) {
                return !inst.container || !lv_obj_is_valid(inst.container);
            }),
        g_instances.end()
    );
}

bool is_active_instance(const StatusInstance& inst) {
    return inst.container && lv_obj_is_valid(inst.container) &&
           lv_obj_get_screen(inst.container) == lv_screen_active();
}

// Update a single instance's WiFi display
void update_wifi_for_instance(const StatusInstance& inst, const StatusSnapshot& snapshot) {
    if (!inst.wifi_icon || !lv_obj_is_valid(inst.wifi_icon)) return;
    
    lv_label_set_text(inst.wifi_icon, get_wifi_symbol(snapshot.wifi_connected));
    
    if (snapshot.wifi_connected) {
        lv_obj_set_style_text_color(inst.wifi_icon, 
            lv_color_hex(get_wifi_color(snapshot.wifi_signal)), 0);
        lv_obj_set_style_text_opa(inst.wifi_icon, LV_OPA_COVER, 0);
    } else {
        lv_obj_set_style_text_color(inst.wifi_icon, 
            lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
        lv_obj_set_style_text_opa(inst.wifi_icon, LV_OPA_40, 0);
    }
    // Note: WiFi icon is never hidden - removed clear_flag call that triggered layout shifts
}

// Update a single instance's battery display
void update_battery_for_instance(const StatusInstance& inst, const StatusSnapshot& snapshot) {
    if (!inst.battery_icon || !inst.battery_pct ||
        !lv_obj_is_valid(inst.battery_icon) || !lv_obj_is_valid(inst.battery_pct)) return;
    if (!snapshot.battery_available) {
        lv_label_set_text(inst.battery_icon, LV_SYMBOL_BATTERY_EMPTY);
        lv_obj_set_style_text_color(inst.battery_icon, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
        lv_label_set_text(inst.battery_pct, "--");
        return;
    }
    
    lv_label_set_text(inst.battery_icon, 
        get_battery_symbol(snapshot.battery_percentage, snapshot.battery_charging));
    lv_obj_set_style_text_color(inst.battery_icon,
        lv_color_hex(get_battery_color(snapshot.battery_percentage, snapshot.battery_charging)), 0);
    
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", snapshot.battery_percentage);
    lv_label_set_text(inst.battery_pct, buf);
}

// Only the visible navbar can affect the current frame. Hidden screens receive
// the latest snapshot when they become active.
void update_active_instances(const StatusSnapshot& snapshot) {
    cleanup_invalid_instances();
    for (const auto& inst : g_instances) {
        if (is_active_instance(inst)) {
            update_wifi_for_instance(inst, snapshot);
            update_battery_for_instance(inst, snapshot);
        }
    }
}

// This timer always runs in the LVGL adapter task. Background services only
// change the protected state above, so no LVGL API crosses task boundaries.
void status_timer_cb(lv_timer_t*) {
    StatusSnapshot snapshot = snapshot_status();
    if (snapshot.generation == g_applied_generation) {
        return;
    }

    update_active_instances(snapshot);
    g_applied_generation = snapshot.generation;
}

void screen_loaded_cb(lv_event_t* event) {
    auto* container = static_cast<lv_obj_t*>(lv_event_get_user_data(event));
    StatusSnapshot snapshot = snapshot_status();
    cleanup_invalid_instances();
    for (const auto& inst : g_instances) {
        if (inst.container == container) {
            update_wifi_for_instance(inst, snapshot);
            update_battery_for_instance(inst, snapshot);
            break;
        }
    }
}

}  // namespace

lv_obj_t* status_icons_create(lv_obj_t* parent) {
    // Initialize mutex on first call
    if (!g_status_mutex) {
        g_status_mutex = xSemaphoreCreateMutex();
    }
    if (!g_status_timer) {
        g_status_timer = lv_timer_create(status_timer_cb, 100, nullptr);
    }

    StatusSnapshot snapshot = snapshot_status();
    
    ESP_LOGI(TAG, "Creating status icons (total instances: %d)", (int)g_instances.size());
    
    // Clean up any invalid instances first
    cleanup_invalid_instances();
    
    // Container for status icons - use fixed size to prevent layout jumps
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, 110, 24);
    lv_obj_align(cont, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(cont, 8, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    
    // Create new instance
    StatusInstance inst;
    inst.container = cont;
    
    // WiFi icon
    inst.wifi_icon = lv_label_create(cont);
    lv_label_set_text(inst.wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(inst.wifi_icon, &lv_font_montserrat_16, 0);
    if (snapshot.wifi_connected) {
        lv_obj_set_style_text_color(inst.wifi_icon, lv_color_hex(get_wifi_color(snapshot.wifi_signal)), 0);
        lv_obj_set_style_text_opa(inst.wifi_icon, LV_OPA_COVER, 0);
    } else {
        lv_obj_set_style_text_color(inst.wifi_icon, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
        lv_obj_set_style_text_opa(inst.wifi_icon, LV_OPA_40, 0);
    }
    
    // Battery percentage - fixed width to prevent layout shifts when text changes
    inst.battery_pct = lv_label_create(cont);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", snapshot.battery_percentage);
    lv_label_set_text(inst.battery_pct, buf);
    lv_obj_set_style_text_font(inst.battery_pct, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(inst.battery_pct, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_set_width(inst.battery_pct, 32);  // Fixed width for "100%"
    lv_obj_set_style_text_align(inst.battery_pct, LV_TEXT_ALIGN_RIGHT, 0);
    
    // Battery icon
    inst.battery_icon = lv_label_create(cont);
    lv_label_set_text(inst.battery_icon,
                      get_battery_symbol(snapshot.battery_percentage, snapshot.battery_charging));
    lv_obj_set_style_text_font(inst.battery_icon, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(inst.battery_icon, 
        lv_color_hex(get_battery_color(snapshot.battery_percentage, snapshot.battery_charging)), 0);
    
    // Track this instance for future updates
    g_instances.push_back(inst);
    lv_obj_add_event_cb(lv_obj_get_screen(cont), screen_loaded_cb,
                        LV_EVENT_SCREEN_LOADED, cont);
    
    ESP_LOGI(TAG, "Status icons created, now tracking %d instance(s)", (int)g_instances.size());
    return cont;
}

void status_set_wifi(bool connected, wifi_signal_level_t signal_level) {
    bool changed = false;

    // Update state (thread-safe)
    if (g_status_mutex) {
        xSemaphoreTake(g_status_mutex, portMAX_DELAY);
    }
    changed = g_wifi_connected != connected || g_wifi_signal != signal_level;
    g_wifi_connected = connected;
    g_wifi_signal = signal_level;
    if (changed) {
        ++g_status_generation;
    }
    if (g_status_mutex) {
        xSemaphoreGive(g_status_mutex);
    }

    ESP_LOGI(TAG, "WiFi status: %s, signal: %d",
             connected ? "connected" : "disconnected", signal_level);
}

void status_set_battery(uint8_t percentage, bool charging) {
    if (percentage > 100) percentage = 100;
    bool changed = false;

    // Update state (thread-safe)
    if (g_status_mutex) {
        xSemaphoreTake(g_status_mutex, portMAX_DELAY);
    }
    changed = !g_battery_available || g_battery_percentage != percentage || g_battery_charging != charging;
    g_battery_available = true;
    g_battery_percentage = percentage;
    g_battery_charging = charging;
    if (changed) {
        ++g_status_generation;
    }
    if (g_status_mutex) {
        xSemaphoreGive(g_status_mutex);
    }

    ESP_LOGD(TAG, "Battery: %d%%, charging: %s",
             percentage, charging ? "yes" : "no");
}

bool status_get_wifi_connected(void) {
    StatusSnapshot snapshot = snapshot_status();
    return snapshot.wifi_connected;
}

void status_set_battery_available(bool available) {
    if (g_status_mutex) xSemaphoreTake(g_status_mutex, portMAX_DELAY);
    if (g_battery_available != available) { g_battery_available = available; ++g_status_generation; }
    if (g_status_mutex) xSemaphoreGive(g_status_mutex);
}

uint8_t status_get_battery_percentage(void) {
    StatusSnapshot snapshot = snapshot_status();
    return snapshot.battery_percentage;
}
