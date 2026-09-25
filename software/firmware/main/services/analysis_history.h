#pragma once

#include "analysis/analysis_calculator.h"
#include <esp_err.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ANALYSIS_HISTORY_CAPACITY 20

typedef struct {
    uint32_t timestamp_ms;
    uint32_t sequence;
    char mix_label[32];
    float oxygen_percent;
    float helium_percent;
    float nitrogen_percent;
    float co2_ppm;
    float planned_depth_m;
    float mod_working_m;
    float mod_secondary_m;
    float ppo2_at_depth;
    float ead_m;
    float end_m;
    float gas_density_g_l;
    analysis_gas_mode_t gas_mode;
    analysis_severity_t severity;
    // Additive fields. The original CO2 value remains CO2 during migration.
    float co_ppm;
    bool co_valid;
    bool source_known;
    sensor_source_t source;
    oxygen_selection_t oxygen_selection;
    uint32_t oxygen_selection_generation;
    uint32_t oxygen_calibration_revision;
} analysis_history_record_t;

void analysis_history_init(void);
esp_err_t analysis_history_add(const analysis_history_record_t* record);
uint8_t analysis_history_count(void);
bool analysis_history_get(uint8_t index, analysis_history_record_t* out);
void analysis_history_clear(void);
analysis_history_record_t analysis_history_record_from_result(const sensor_readings_t* readings,
                                                              const analysis_result_t* result);

#ifdef __cplusplus
}
#endif
