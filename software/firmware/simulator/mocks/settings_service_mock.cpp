#include "services/settings_service.h"

#include <algorithm>

namespace {

const setting_def_t kSettingDefs[SETTING_COUNT] = {
    {SETTING_BRIGHTNESS, SETTINGS_CAT_DEVICE, "Brightness", "brightness", 80, 10, 100},
    {SETTING_SCREEN_TIMEOUT, SETTINGS_CAT_DEVICE, "Screen Timeout", "scr_timeout", 1, 0, 3},
    {SETTING_SOUND_ENABLED, SETTINGS_CAT_DEVICE, "Sound", "sound", 1, 0, 1},
    {SETTING_UNITS_DEPTH, SETTINGS_CAT_DEVICE, "Depth Units", "units_depth", 0, 0, 1},
    {SETTING_UNITS_TEMP, SETTINGS_CAT_DEVICE, "Temp Units", "units_temp", 0, 0, 1},
    {SETTING_UNITS_PRESSURE, SETTINGS_CAT_DEVICE, "Pressure Units", "units_pres", 0, 0, 1},
    {SETTING_PPO2_WORKING_X100, SETTINGS_CAT_SAFETY, "PPO2 Working", "ppo2_work", 140, 100, 200},
    {SETTING_PPO2_SECONDARY_X100, SETTINGS_CAT_SAFETY, "PPO2 Secondary", "ppo2_second", 160, 100, 200},
    {SETTING_DENSITY_ADVISORY_X10, SETTINGS_CAT_SAFETY, "Density Advisory", "dens_adv", 52, 30, 90},
    {SETTING_DENSITY_ALARM_X10, SETTINGS_CAT_SAFETY, "Density Alarm", "dens_alarm", 63, 30, 90},
    {SETTING_CO2_ADVISORY_PPM, SETTINGS_CAT_SAFETY, "CO2 Advisory", "co2_adv", 500, 300, 2000},
};

const char* kCategoryNames[SETTINGS_CAT_COUNT] = {
    "Device",
    "Safety",
    "Calibration",
};

int32_t g_values[SETTING_COUNT] = {};
bool g_initialized = false;

void load_defaults() {
    for (int i = 0; i < SETTING_COUNT; ++i) {
        g_values[i] = kSettingDefs[i].default_value;
    }
}

}  // namespace

void settings_init(void) {
    if (g_initialized) return;
    load_defaults();
    g_initialized = true;
}

int32_t settings_get(setting_key_t key) {
    settings_init();
    if (key >= SETTING_COUNT) return 0;
    return g_values[key];
}

bool settings_set(setting_key_t key, int32_t value) {
    settings_init();
    if (key >= SETTING_COUNT) return false;

    const setting_def_t& def = kSettingDefs[key];
    value = std::max(def.min_value, std::min(def.max_value, value));
    if (g_values[key] == value) return false;

    g_values[key] = value;
    return true;
}

void settings_reset(setting_key_t key) {
    if (key >= SETTING_COUNT) return;
    settings_set(key, kSettingDefs[key].default_value);
}

void settings_reset_category(settings_category_t category) {
    settings_init();
    if (category >= SETTINGS_CAT_COUNT) return;
    for (int i = 0; i < SETTING_COUNT; ++i) {
        if (kSettingDefs[i].category == category) {
            g_values[i] = kSettingDefs[i].default_value;
        }
    }
}

void settings_factory_reset(void) {
    settings_init();
    load_defaults();
}

int32_t settings_get_default(setting_key_t key) {
    if (key >= SETTING_COUNT) return 0;
    return kSettingDefs[key].default_value;
}

bool settings_is_modified(setting_key_t key) {
    settings_init();
    if (key >= SETTING_COUNT) return false;
    return g_values[key] != kSettingDefs[key].default_value;
}

const setting_def_t* settings_get_def(setting_key_t key) {
    if (key >= SETTING_COUNT) return nullptr;
    return &kSettingDefs[key];
}

const char* settings_get_category_name(settings_category_t category) {
    if (category >= SETTINGS_CAT_COUNT) return "Unknown";
    return kCategoryNames[category];
}
