#include "cylinder_profiles.h"
#include "storage_service.h"
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstring>

#ifndef TRIMIX_SIMULATOR
#include <esp_log.h>
#include <nvs.h>
#include <nvs_flash.h>
#endif

namespace {

#ifndef TRIMIX_SIMULATOR
constexpr const char* kNvsNamespace = "cyl_profiles";
constexpr const char* kNvsKeyProfiles = "profiles";
constexpr const char* kNvsKeySelected = "selected";
const char* TAG = "CYL_PROFILES";
#endif

cylinder_profile_t g_profiles[CYLINDER_PROFILE_CAPACITY] = {};
uint8_t g_selected = 0;
bool g_initialized = false;

void set_profile(cylinder_profile_t& profile, const char* name, const char* serial,
                 float o2, float he, float depth, analysis_gas_mode_t mode) {
    profile = {};
    profile.configured = true;
    profile.needs_recheck = true;
    std::snprintf(profile.name, sizeof(profile.name), "%s", name);
    std::snprintf(profile.serial, sizeof(profile.serial), "%s", serial);
    profile.oxygen_percent = o2;
    profile.helium_percent = he;
    profile.planned_depth_m = depth;
    profile.gas_mode = mode;
}

void load_defaults() {
    set_profile(g_profiles[0], "Back Gas", "BG-01", 18.0f, 45.0f, 60.0f,
                ANALYSIS_GAS_MODE_OC_BACK_GAS);
    set_profile(g_profiles[1], "Deco 50", "STG-50", 50.0f, 0.0f, 21.0f,
                ANALYSIS_GAS_MODE_DECO_GAS);
    set_profile(g_profiles[2], "Oxygen", "STG-O2", 100.0f, 0.0f, 6.0f,
                ANALYSIS_GAS_MODE_DECO_GAS);
    set_profile(g_profiles[3], "Diluent", "DIL-01", 10.0f, 60.0f, 80.0f,
                ANALYSIS_GAS_MODE_CCR_DILUENT);
    set_profile(g_profiles[4], "Bailout", "BO-01", 21.0f, 35.0f, 45.0f,
                ANALYSIS_GAS_MODE_BAILOUT);
    set_profile(g_profiles[5], "Spare", "SPARE", 20.9f, 0.0f, 30.0f,
                ANALYSIS_GAS_MODE_OC_BACK_GAS);
    g_selected = 0;
}

bool valid_index(uint8_t index) {
    return index < CYLINDER_PROFILE_CAPACITY;
}

void normalize() {
    if (!valid_index(g_selected)) {
        g_selected = 0;
    }
    for (auto& profile : g_profiles) {
        if (!profile.configured) {
            continue;
        }
        profile.oxygen_percent = std::max(0.0f, std::min(100.0f, profile.oxygen_percent));
        profile.helium_percent = std::max(0.0f, std::min(95.0f, profile.helium_percent));
        if (profile.oxygen_percent + profile.helium_percent > 100.0f) {
            profile.helium_percent = 100.0f - profile.oxygen_percent;
        }
        profile.planned_depth_m = std::max(0.0f, std::min(150.0f, profile.planned_depth_m));
        if (profile.gas_mode < ANALYSIS_GAS_MODE_OC_BACK_GAS ||
            profile.gas_mode >= ANALYSIS_GAS_MODE_COUNT) {
            profile.gas_mode = ANALYSIS_GAS_MODE_OC_BACK_GAS;
        }
    }
}

#ifndef TRIMIX_SIMULATOR
struct CylinderSnapshot { uint32_t selected; cylinder_profile_t profiles[CYLINDER_PROFILE_CAPACITY]; };
void load_from_nvs() {
    if (!storage_ready()) return;
    CylinderSnapshot snapshot{};
    const auto result = storage_read_blob("cyl_v2", 2, &snapshot, sizeof(snapshot));
    if (result == STORAGE_OK) {
        if (snapshot.selected < CYLINDER_PROFILE_CAPACITY) {
            std::memcpy(g_profiles, snapshot.profiles, sizeof(g_profiles)); g_selected = snapshot.selected;
        }
        normalize(); return;
    }
    if (result == STORAGE_ERROR) return;
    nvs_handle_t handle;
    if (nvs_open(kNvsNamespace, NVS_READONLY, &handle) != ESP_OK) {
        return;
    }

    uint8_t selected = 0;
    if (nvs_get_u8(handle, kNvsKeySelected, &selected) == ESP_OK) {
        g_selected = selected;
    }

    size_t required = sizeof(g_profiles);
    if (nvs_get_blob(handle, kNvsKeyProfiles, g_profiles, &required) != ESP_OK ||
        required != sizeof(g_profiles)) {
        load_defaults();
    }
    normalize();
    nvs_close(handle);
}

bool save_to_nvs() {
    CylinderSnapshot snapshot{}; snapshot.selected = g_selected;
    std::memcpy(snapshot.profiles, g_profiles, sizeof(g_profiles));
    return storage_write_blob("cyl_v2", 2, &snapshot, sizeof(snapshot));
}
#else
void load_from_nvs() {}
bool save_to_nvs() { return true; }
#endif

}  // namespace

extern "C" {

void cylinder_profiles_init(void) {
    if (g_initialized) {
        return;
    }
#ifndef TRIMIX_SIMULATOR
    if (!storage_init()) ESP_LOGE(TAG, "%s", storage_status_message());
#endif
    load_defaults();
    load_from_nvs();
    normalize();
    g_initialized = true;
}

uint8_t cylinder_profiles_count(void) {
    cylinder_profiles_init();
    return CYLINDER_PROFILE_CAPACITY;
}

uint8_t cylinder_profiles_selected_index(void) {
    cylinder_profiles_init();
    return g_selected;
}

bool cylinder_profiles_get(uint8_t index, cylinder_profile_t* out) {
    if (!out || !valid_index(index)) {
        return false;
    }
    cylinder_profiles_init();
    *out = g_profiles[index];
    return true;
}

bool cylinder_profiles_get_selected(cylinder_profile_t* out) {
    return cylinder_profiles_get(cylinder_profiles_selected_index(), out);
}

esp_err_t cylinder_profiles_select(uint8_t index) {
    if (!valid_index(index)) {
        return ESP_ERR_INVALID_ARG;
    }
    cylinder_profiles_init();
    const uint8_t previous_selected = g_selected;
    cylinder_profile_t previous[CYLINDER_PROFILE_CAPACITY]; std::memcpy(previous, g_profiles, sizeof(previous));
    g_selected = index;
    if (!save_to_nvs()) { std::memcpy(g_profiles, previous, sizeof(previous)); g_selected = previous_selected; return ESP_FAIL; }
    return ESP_OK;
}

esp_err_t cylinder_profiles_select_next(void) {
    cylinder_profiles_init();
    const uint8_t previous_selected = g_selected;
    cylinder_profile_t previous[CYLINDER_PROFILE_CAPACITY]; std::memcpy(previous, g_profiles, sizeof(previous));
    g_selected = (g_selected + 1U) % CYLINDER_PROFILE_CAPACITY;
    if (!save_to_nvs()) { std::memcpy(g_profiles, previous, sizeof(previous)); g_selected = previous_selected; return ESP_FAIL; }
    return ESP_OK;
}

esp_err_t cylinder_profiles_set(uint8_t index, const cylinder_profile_t* profile) {
    if (!profile || !valid_index(index) || !std::isfinite(profile->oxygen_percent) || !std::isfinite(profile->helium_percent) ||
        !std::isfinite(profile->planned_depth_m) || profile->oxygen_percent < 0 || profile->helium_percent < 0 ||
        profile->oxygen_percent + profile->helium_percent > 100 || !std::memchr(profile->name, 0, sizeof(profile->name)) ||
        !std::memchr(profile->serial, 0, sizeof(profile->serial))) {
        return ESP_ERR_INVALID_ARG;
    }
    cylinder_profiles_init();
    const uint8_t previous_selected = g_selected;
    cylinder_profile_t previous[CYLINDER_PROFILE_CAPACITY]; std::memcpy(previous, g_profiles, sizeof(previous));
    g_profiles[index] = *profile;
    g_profiles[index].configured = true;
    normalize();
    if (!save_to_nvs()) { std::memcpy(g_profiles, previous, sizeof(previous)); g_selected = previous_selected; return ESP_FAIL; }
    return ESP_OK;
}

esp_err_t cylinder_profiles_update_selected_from_record(const analysis_history_record_t* record) {
    if (!record) {
        return ESP_ERR_INVALID_ARG;
    }
    cylinder_profiles_init();
    const uint8_t previous_selected = g_selected;
    cylinder_profile_t previous[CYLINDER_PROFILE_CAPACITY]; std::memcpy(previous, g_profiles, sizeof(previous));
    cylinder_profile_t& profile = g_profiles[g_selected];
    profile.configured = true;
    profile.needs_recheck = false;
    profile.oxygen_percent = record->oxygen_percent;
    profile.helium_percent = record->helium_percent;
    profile.planned_depth_m = record->planned_depth_m;
    profile.gas_mode = record->gas_mode;
    profile.last_analyzed_ms = record->timestamp_ms;
    normalize();
    if (!save_to_nvs()) { std::memcpy(g_profiles, previous, sizeof(previous)); g_selected = previous_selected; return ESP_FAIL; }
    return ESP_OK;
}

esp_err_t cylinder_profiles_mark_selected_recheck(bool needs_recheck) {
    cylinder_profiles_init();
    const uint8_t previous_selected = g_selected;
    cylinder_profile_t previous[CYLINDER_PROFILE_CAPACITY]; std::memcpy(previous, g_profiles, sizeof(previous));
    g_profiles[g_selected].needs_recheck = needs_recheck;
    if (!save_to_nvs()) { std::memcpy(g_profiles, previous, sizeof(previous)); g_selected = previous_selected; return ESP_FAIL; }
    return ESP_OK;
}

void cylinder_profiles_reset_defaults(void) {
    cylinder_profiles_init();
    const uint8_t previous_selected = g_selected;
    cylinder_profile_t previous[CYLINDER_PROFILE_CAPACITY]; std::memcpy(previous, g_profiles, sizeof(previous));
    load_defaults();
    if (!save_to_nvs()) { std::memcpy(g_profiles, previous, sizeof(previous)); g_selected = previous_selected; return; }
}

}  // extern "C"
