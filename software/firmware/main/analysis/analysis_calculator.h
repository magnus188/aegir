#pragma once

#include "sensors/sensor_interface.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ANALYSIS_SEVERITY_NORMAL = 0,
    ANALYSIS_SEVERITY_ADVISORY,
    ANALYSIS_SEVERITY_ALARM,
    ANALYSIS_SEVERITY_FAULT
} analysis_severity_t;

typedef enum {
    ANALYSIS_GAS_MODE_OC_BACK_GAS = 0,
    ANALYSIS_GAS_MODE_DECO_GAS,
    ANALYSIS_GAS_MODE_CCR_DILUENT,
    ANALYSIS_GAS_MODE_BAILOUT,
    ANALYSIS_GAS_MODE_COUNT
} analysis_gas_mode_t;

typedef struct {
    int32_t ppo2_working_x100;
    int32_t ppo2_secondary_x100;
    int32_t density_advisory_x10;
    int32_t density_alarm_x10;
    int32_t co2_advisory_ppm;
} analysis_limits_t;

typedef struct {
    sensor_readings_t readings;
    // Set to a negative value to use the helium value from the sensor reading.
    float manual_he_percent;
    float planned_depth_m;
    analysis_gas_mode_t gas_mode;
    analysis_limits_t limits;
} analysis_input_t;

typedef struct {
    bool valid;
    float oxygen_percent;
    float helium_percent;
    float nitrogen_percent;
    float co2_ppm;
    float planned_depth_m;
    float ppo2_at_depth;
    float mod_working_m;
    float mod_secondary_m;
    float ead_m;
    float end_m;
    float gas_density_g_l;
    analysis_gas_mode_t gas_mode;
    analysis_severity_t severity;
    char mix_label[32];
    char advisory[96];
    float co_ppm;
    bool co_valid;
} analysis_result_t;

analysis_limits_t analysis_default_limits(void);
analysis_result_t analysis_calculate(const analysis_input_t* input);
const char* analysis_severity_label(analysis_severity_t severity);
const char* analysis_gas_mode_label(analysis_gas_mode_t mode);

#ifdef __cplusplus
}
#endif
