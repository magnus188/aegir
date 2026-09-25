#pragma once
#include "analysis_history.h"
#include <cmath>
#include <cstddef>
#include <cstring>

namespace analysis_history_format {
// Persistence layout used before separate CO and source fields were introduced.
struct LegacyRecord {
    uint32_t timestamp_ms;
    uint32_t sequence;
    char mix_label[32];
    float oxygen_percent, helium_percent, nitrogen_percent, co2_ppm;
    float planned_depth_m, mod_working_m, mod_secondary_m, ppo2_at_depth;
    float ead_m, end_m, gas_density_g_l;
    analysis_gas_mode_t gas_mode;
    analysis_severity_t severity;
};
static_assert(sizeof(LegacyRecord) == offsetof(analysis_history_record_t, co_ppm),
              "History migration must preserve the original record prefix");

// Original V2 layout predates explicit oxygen-source identity.
struct LegacyV2Record {
    uint32_t timestamp_ms, sequence;
    char mix_label[32];
    float oxygen_percent, helium_percent, nitrogen_percent, co2_ppm;
    float planned_depth_m, mod_working_m, mod_secondary_m, ppo2_at_depth;
    float ead_m, end_m, gas_density_g_l;
    analysis_gas_mode_t gas_mode;
    analysis_severity_t severity;
    float co_ppm;
    bool co_valid, source_known;
    sensor_source_t source;
};
static_assert(sizeof(LegacyV2Record) == offsetof(analysis_history_record_t, oxygen_selection),
              "Preserve V2 history prefix when adding oxygen identity");
inline bool decode_v2(const LegacyV2Record *legacy, size_t bytes, uint8_t count,
                      analysis_history_record_t *out) {
    if (!legacy || !out || bytes != sizeof(LegacyV2Record) * ANALYSIS_HISTORY_CAPACITY || count > ANALYSIS_HISTORY_CAPACITY) return false;
    for (size_t i = 0; i < ANALYSIS_HISTORY_CAPACITY; ++i) {
        out[i] = {};
        std::memcpy(&out[i], &legacy[i], sizeof(LegacyV2Record));
        out[i].mix_label[sizeof(out[i].mix_label) - 1] = 0;
        // Selection generation zero explicitly means identity was not recorded.
    }
    return true;
}

inline bool decode_legacy(const LegacyRecord *legacy, size_t bytes, uint8_t count,
                          analysis_history_record_t *out) {
    if (!legacy || !out || bytes != sizeof(LegacyRecord) * ANALYSIS_HISTORY_CAPACITY ||
        count > ANALYSIS_HISTORY_CAPACITY) return false;
    for (uint8_t i = 0; i < count; ++i) {
        out[i] = {};
        std::memcpy(&out[i], &legacy[i], sizeof(LegacyRecord));
        out[i].mix_label[sizeof(out[i].mix_label) - 1] = '\0';
        out[i].co_ppm = NAN;
        out[i].co_valid = false;
        out[i].source_known = false;
    }
    return true;
}
} // namespace analysis_history_format
