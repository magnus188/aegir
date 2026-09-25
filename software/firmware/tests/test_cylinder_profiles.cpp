#include "services/cylinder_profiles.h"
#include "services/mix_label_service.h"

#include <cstdio>
#include <cstring>
#include <cmath>

namespace {

int tests_passed = 0;
int tests_failed = 0;

void expect_true(bool condition, const char* name) {
    if (condition) {
        std::printf("  PASS %s\n", name);
        ++tests_passed;
    } else {
        std::printf("  FAIL %s\n", name);
        ++tests_failed;
    }
}

analysis_history_record_t make_record() {
    analysis_history_record_t record = {};
    record.timestamp_ms = 12000;
    record.sequence = 12;
    std::snprintf(record.mix_label, sizeof(record.mix_label), "Trimix 18/45");
    record.oxygen_percent = 18.0f;
    record.helium_percent = 45.0f;
    record.nitrogen_percent = 37.0f;
    record.co2_ppm = 420.0f;
    record.planned_depth_m = 60.0f;
    record.mod_working_m = 67.0f;
    record.mod_secondary_m = 78.0f;
    record.ppo2_at_depth = 1.26f;
    record.ead_m = 22.0f;
    record.end_m = 28.0f;
    record.gas_density_g_l = 4.4f;
    record.gas_mode = ANALYSIS_GAS_MODE_OC_BACK_GAS;
    record.severity = ANALYSIS_SEVERITY_NORMAL;
    return record;
}

}  // namespace

int main() {
    std::printf("Cylinder Profile Tests\n");
    std::printf("======================\n\n");

    cylinder_profiles_init();
    cylinder_profiles_reset_defaults();
    expect_true(cylinder_profiles_count() == CYLINDER_PROFILE_CAPACITY, "Default profile count");

    cylinder_profile_t profile = {};
    expect_true(cylinder_profiles_get_selected(&profile), "Read selected profile");
    expect_true(profile.needs_recheck, "Default profile needs analysis");

    analysis_history_record_t record = make_record();
    expect_true(cylinder_profiles_update_selected_from_record(&record) == ESP_OK,
                "Update selected profile from analysis");
    expect_true(cylinder_profiles_get_selected(&profile), "Read updated selected profile");
    expect_true(!profile.needs_recheck, "Updated profile is marked ready");
    expect_true(profile.gas_mode == ANALYSIS_GAS_MODE_OC_BACK_GAS, "Gas mode is stored");

    char label[512];
    mix_label_build_text(&record, &profile, label, sizeof(label));
    expect_true(std::strstr(label, "TRIMIX ANALYSER LABEL") != nullptr, "Label has title");
    expect_true(std::strstr(label, "Share payload: trimix-label-v1") != nullptr,
                "Label has future export payload marker");

    char csv[512];
    mix_label_build_csv(&record, &profile, csv, sizeof(csv));
    expect_true(std::strstr(csv, "cylinder,serial,mode") != nullptr, "CSV label has header");

    auto invalid = profile; invalid.oxygen_percent = NAN;
    expect_true(cylinder_profiles_set(0, &invalid) == ESP_ERR_INVALID_ARG, "Nonfinite cylinder composition rejected");
    invalid = profile; invalid.oxygen_percent = 80; invalid.helium_percent = 40;
    expect_true(cylinder_profiles_set(0, &invalid) == ESP_ERR_INVALID_ARG, "Impossible cylinder composition rejected");
    invalid = profile; std::memset(invalid.name, 'x', sizeof(invalid.name));
    expect_true(cylinder_profiles_set(0, &invalid) == ESP_ERR_INVALID_ARG, "Unterminated cylinder label rejected");

    std::printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
