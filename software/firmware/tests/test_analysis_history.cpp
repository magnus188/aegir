#include "services/analysis_history.h"
#include "services/analysis_history_format.h"

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

analysis_history_record_t make_record(uint32_t sequence) {
    analysis_history_record_t record = {};
    record.timestamp_ms = sequence * 1000U;
    record.sequence = sequence;
    std::snprintf(record.mix_label, sizeof(record.mix_label), "Mix %u", sequence);
    record.oxygen_percent = 20.0f + static_cast<float>(sequence % 10);
    record.helium_percent = static_cast<float>(sequence % 50);
    record.nitrogen_percent = 100.0f - record.oxygen_percent - record.helium_percent;
    record.co2_ppm = 400.0f + static_cast<float>(sequence);
    record.planned_depth_m = 30.0f;
    record.mod_working_m = 50.0f;
    record.mod_secondary_m = 66.0f;
    record.ppo2_at_depth = 1.1f;
    record.ead_m = 24.0f;
    record.end_m = 18.0f;
    record.gas_density_g_l = 4.2f;
    record.severity = ANALYSIS_SEVERITY_NORMAL;
    return record;
}

}  // namespace

int main() {
    std::printf("Analysis History Tests\n");
    std::printf("======================\n\n");

    analysis_history_init();
    analysis_history_clear();
    expect_true(analysis_history_count() == 0, "History starts empty after clear");

    analysis_history_record_t first = make_record(1);
    expect_true(analysis_history_add(&first) == ESP_OK, "Add first record");
    expect_true(analysis_history_count() == 1, "Count increments");

    analysis_history_record_t out = {};
    expect_true(analysis_history_get(0, &out), "Read latest record");
    expect_true(out.sequence == 1, "Latest record sequence matches");

    for (uint32_t i = 2; i <= ANALYSIS_HISTORY_CAPACITY + 5; ++i) {
        analysis_history_record_t record = make_record(i);
        analysis_history_add(&record);
    }
    expect_true(analysis_history_count() == ANALYSIS_HISTORY_CAPACITY, "History caps at capacity");
    expect_true(analysis_history_get(0, &out), "Read newest capped record");
    expect_true(out.sequence == ANALYSIS_HISTORY_CAPACITY + 5, "Newest record is first");
    expect_true(analysis_history_get(ANALYSIS_HISTORY_CAPACITY - 1, &out), "Read oldest capped record");
    expect_true(out.sequence == 6, "Old records are evicted");
    expect_true(!analysis_history_get(ANALYSIS_HISTORY_CAPACITY, &out), "Out-of-range read fails");
    expect_true(analysis_history_add(nullptr) == ESP_ERR_INVALID_ARG, "Null add validates argument");

    analysis_history_clear();
    expect_true(analysis_history_count() == 0, "Clear removes records");

    sensor_readings_t readings{};
    readings.co_ppm = 7.0f;
    readings.co_valid = true;
    readings.source = SENSOR_SOURCE_SIMULATED;
    analysis_result_t result{};
    result.co2_ppm = 420.0f;
    auto gases = analysis_history_record_from_result(&readings, &result);
    expect_true(gases.co_valid && gases.co_ppm == 7.0f && gases.co2_ppm == 420.0f,
                "CO and legacy CO2 retain different values");
    expect_true(gases.source_known && gases.source == SENSOR_SOURCE_SIMULATED,
                "New records preserve simulation provenance");
    readings.co_valid = false;
    gases = analysis_history_record_from_result(&readings, &result);
    expect_true(!gases.co_valid && std::isnan(gases.co_ppm) && gases.co2_ppm == 420.0f,
                "Missing CO never inherits a CO2 value");

    analysis_history_format::LegacyRecord legacy[ANALYSIS_HISTORY_CAPACITY]{};
    legacy[0].co2_ppm = 900.0f;
    legacy[0].oxygen_percent = 32.0f;
    legacy[0].sequence = 123;
    analysis_history_record_t migrated[ANALYSIS_HISTORY_CAPACITY]{};
    expect_true(analysis_history_format::decode_legacy(legacy, sizeof(legacy), 1, migrated),
                "Original binary history layout decodes");
    expect_true(migrated[0].co2_ppm == 900.0f && migrated[0].oxygen_percent == 32.0f &&
                migrated[0].sequence == 123 && !migrated[0].co_valid &&
                std::isnan(migrated[0].co_ppm) && !migrated[0].source_known,
                "Migration preserves CO2 and does not invent CO or provenance");
    expect_true(!analysis_history_format::decode_legacy(legacy, sizeof(legacy) - 1, 1, migrated),
                "Truncated legacy history is rejected");
    expect_true(migrated[0].co2_ppm == 900.0f, "Rejected migration preserves destination records");

    analysis_history_format::LegacyV2Record legacy_v2[ANALYSIS_HISTORY_CAPACITY]{};
    legacy_v2[0].co_ppm = 3.5f; legacy_v2[0].co_valid = true;
    legacy_v2[0].source = SENSOR_SOURCE_HARDWARE; legacy_v2[0].source_known = true;
    std::memset(legacy_v2[0].mix_label, 'x', sizeof(legacy_v2[0].mix_label));
    expect_true(analysis_history_format::decode_v2(legacy_v2, sizeof(legacy_v2), 1, migrated),
                "V2 history migrates without rewriting legacy data");
    expect_true(migrated[0].co_ppm == 3.5f && migrated[0].co_valid && migrated[0].source == SENSOR_SOURCE_HARDWARE &&
                migrated[0].oxygen_selection == OXYGEN_UNCONFIGURED && migrated[0].oxygen_selection_generation == 0 &&
                migrated[0].mix_label[31] == 0, "V2 migration preserves CO/provenance and marks oxygen identity unknown");
    expect_true(!analysis_history_format::decode_v2(legacy_v2, sizeof(legacy_v2)-1, 1, migrated) && migrated[0].co_ppm == 3.5f,
                "Failed V2 migration preserves prior output");
    readings.oxygen_selection = OXYGEN_JJCCR; readings.oxygen_selection_generation = 9;
    readings.oxygen_calibration_revision = 4;
    gases = analysis_history_record_from_result(&readings, &result);
    expect_true(gases.oxygen_selection == OXYGEN_JJCCR && gases.oxygen_selection_generation == 9 &&
                gases.oxygen_calibration_revision == 4, "New history retains actual selected-cell and calibration identity");

    std::printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
