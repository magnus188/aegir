#include "services/settings_service.h"
#include "services/storage_service.h"

#include <cstdio>
#include <cstring>

namespace {
int failed = 0;

void expect(bool condition, const char* message) {
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", message);
    if (!condition) ++failed;
}
}

int main(int argc, char** argv) {
    // The older journal contains exactly the original eleven keys. A new
    // firmware must retain those values while adding sensor-alert defaults.
    constexpr int legacy_count = SETTING_CO2_ADVISORY_PPM + 1;
    int32_t legacy[legacy_count] = {80, 1, 1, 0, 0, 0, 140, 160, 52, 63, 500};
    legacy[SETTING_BRIGHTNESS] = 65;
    legacy[SETTING_DENSITY_ADVISORY_X10] = 57;
    if (argc > 1) {
        int32_t damaged[SETTING_COUNT] = {65, 1, 1, 0, 0, 0, 180, 120, 80, 40, 500, 20, 5, 95, 50};
        const bool v4 = std::strcmp(argv[1], "invalid-v4") == 0;
        const size_t bytes = v4 ? sizeof(damaged) : sizeof(legacy);
        const char* key = v4 ? "settings_v4" : "settings_v3";
        expect(storage_init() && storage_write_blob(key, v4 ? 4 : 3, damaged, bytes), "Inverted legacy limits can be seeded");
        settings_init();
        expect(settings_get(SETTING_BRIGHTNESS) == 65, "Migration retains unrelated preferences");
        expect(settings_get(SETTING_PPO2_WORKING_X100) == 140 && settings_get(SETTING_PPO2_SECONDARY_X100) == 160 &&
               settings_get(SETTING_DENSITY_ADVISORY_X10) == 52 && settings_get(SETTING_DENSITY_ALARM_X10) == 63 &&
               settings_get(SETTING_CO_ADVISORY_PPM) == 3 && settings_get(SETTING_CO_ALARM_PPM) == 5 &&
               settings_get(SETTING_HUMIDITY_ADVISORY_PCT) == 80 && settings_get(SETTING_HUMIDITY_ALARM_PCT) == 90,
               "All inverted pairs normalize to ordered defaults");
        int32_t archived[SETTING_COUNT]{};
        expect(storage_read_blob(key, v4 ? 4 : 3, archived, bytes) == STORAGE_OK &&
               std::memcmp(damaged, archived, bytes) == 0, "Migration preserves the archived journal for rollback");
        return failed ? 1 : 0;
    }
    expect(storage_init() && storage_write_blob("settings_v3", 3, legacy, sizeof(legacy)),
           "Older settings journal can be seeded");

    settings_init();
    expect(settings_get(SETTING_BRIGHTNESS) == 65 &&
           settings_get(SETTING_DENSITY_ADVISORY_X10) == 57,
           "Previous device and safety settings survive migration");
    expect(settings_get(SETTING_CO_ADVISORY_PPM) == 3 &&
           settings_get(SETTING_CO_ALARM_PPM) == 5 &&
           settings_get(SETTING_HUMIDITY_ADVISORY_PCT) == 80 &&
           settings_get(SETTING_HUMIDITY_ALARM_PCT) == 90,
           "New sensor-alert settings receive defaults");

    expect(settings_set(SETTING_CO_ALARM_PPM, 7), "New CO alarm can be saved");
    int32_t current[SETTING_COUNT]{};
    expect(storage_read_blob("settings_v4", 4, current, sizeof(current)) == STORAGE_OK &&
           current[SETTING_BRIGHTNESS] == 65 &&
           current[SETTING_DENSITY_ADVISORY_X10] == 57 &&
           current[SETTING_CO_ALARM_PPM] == 7,
           "New journal persists both older preferences and the CO alarm");
    int32_t original[legacy_count]{};
    expect(storage_read_blob("settings_v3", 3, original, sizeof(original)) == STORAGE_OK &&
           original[SETTING_BRIGHTNESS] == 65,
           "Older journal remains available for rollback");
    const struct { setting_key_t low, high; } pairs[] = {
        {SETTING_PPO2_WORKING_X100, SETTING_PPO2_SECONDARY_X100},
        {SETTING_DENSITY_ADVISORY_X10, SETTING_DENSITY_ALARM_X10},
        {SETTING_CO_ADVISORY_PPM, SETTING_CO_ALARM_PPM},
        {SETTING_HUMIDITY_ADVISORY_PCT, SETTING_HUMIDITY_ALARM_PCT},
    };
    for (const auto& pair : pairs) {
        const auto low = settings_get(pair.low), high = settings_get(pair.high);
        expect(!settings_set(pair.low, high) && !settings_set(pair.high, low) &&
               settings_get(pair.low) == low && settings_get(pair.high) == high,
               "Service rejects equal or inverted thresholds without changing the active values");
    }
    expect(storage_pause_writes(true), "Storage write failure can be injected");
    expect(!settings_set(SETTING_BRIGHTNESS, 90) && settings_get(SETTING_BRIGHTNESS) == 65,
           "Failed save retains the current setting");
    settings_reset_category(SETTINGS_CAT_SAFETY);
    expect(settings_get(SETTING_CO_ALARM_PPM) == 7, "Failed reset retains all safety settings");
    storage_pause_writes(false);
    return failed ? 1 : 0;
}
