#include "services/settings_service.h"
#include "services/storage_service.h"

#include <cstdio>

namespace {
int failed = 0;

void expect(bool condition, const char* message) {
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", message);
    if (!condition) ++failed;
}
}

int main() {
    // The older journal contains exactly the original eleven keys. A new
    // firmware must retain those values while adding sensor-alert defaults.
    constexpr int legacy_count = SETTING_CO2_ADVISORY_PPM + 1;
    int32_t legacy[legacy_count] = {80, 1, 1, 0, 0, 0, 140, 160, 52, 63, 500};
    legacy[SETTING_BRIGHTNESS] = 65;
    legacy[SETTING_DENSITY_ADVISORY_X10] = 57;
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
    return failed ? 1 : 0;
}
