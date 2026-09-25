#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { GAS_CAL_AO2 = 0, GAS_CAL_JJCCR, GAS_CAL_HELIUM, GAS_CAL_CHANNEL_COUNT } gas_cal_channel_t;
typedef enum {
    GAS_CAL_OK = 0, GAS_CAL_BAD_ARGUMENT, GAS_CAL_NO_SAMPLE, GAS_CAL_WARMING,
    GAS_CAL_UNSTABLE, GAS_CAL_STALE, GAS_CAL_SENSOR_FAULT, GAS_CAL_NEED_TWO_POINTS,
    GAS_CAL_INSUFFICIENT_SPAN, GAS_CAL_STORAGE_ERROR, GAS_CAL_WRONG_SOURCE,
    GAS_CAL_SELECTION_REQUIRED
} gas_cal_result_t;
enum {
    GAS_FAULT_TRANSPORT = 1u << 0, GAS_FAULT_ADC_CONFIG = 1u << 1,
    GAS_FAULT_CLIPPED = 1u << 2, GAS_FAULT_EXCITATION = 1u << 3,
    GAS_FAULT_BIAS = 1u << 4, GAS_FAULT_NONFINITE = 1u << 5,
    GAS_FAULT_STALE = 1u << 6
};

typedef struct {
    double voltage_v;
    int32_t adc_code;
    uint32_t timestamp_ms;
    uint32_t sequence;
    uint32_t faults;
    uint8_t gain;
    bool simulated;
    bool warmed;
    bool stable;
    double mean_voltage_v;
    double peak_to_peak_v;
    float excitation_v;
    float bias_v;
    float temperature_c;
    float humidity_pct;
    float pressure_bar;
    bool environment_valid;
} gas_raw_sample_t;

typedef struct {
    float oxygen_percent;
    float helium_percent;
    // Uncertainty in percentage points of this channel's stated reference.
    float uncertainty_pp;
    bool uncertainty_known;
    char reference_id[64];
} gas_cal_reference_t;

typedef struct {
    gas_raw_sample_t sample;
    gas_cal_reference_t reference;
    bool captured;
} gas_cal_point_t;

typedef struct {
    uint32_t format_version;
    uint32_t acquisition_revision;
    uint32_t revision;
    gas_cal_channel_t channel;
    bool simulated;
    bool valid;
    // Bench calibration is never a physical accuracy qualification.
    bool characterization_validated;
    bool reference_budget_met;
    double zero_voltage_v;
    double percent_per_volt;
    gas_cal_point_t points[2];
    uint32_t crc32;
} gas_cal_record_t;

// Nonblocking cached acquisition. Returns false until a sample exists.
bool sensor_get_raw(gas_cal_channel_t channel, gas_raw_sample_t *out);
// Returns the last intact record, including an inactive older acquisition profile.
bool gas_calibration_get_record(gas_cal_channel_t channel, gas_cal_record_t *out);
// Configuration/source compatibility only; not installation or accuracy approval.
bool gas_calibration_record_is_current(const gas_cal_record_t *record);
bool gas_calibration_get_point(gas_cal_channel_t channel, unsigned point, gas_cal_point_t *out);
gas_cal_result_t gas_calibration_capture(gas_cal_channel_t channel, unsigned point,
                                        const gas_cal_reference_t *reference);
gas_cal_result_t gas_calibration_save(gas_cal_channel_t channel);
void gas_calibration_cancel(gas_cal_channel_t channel);
const char *gas_calibration_result_label(gas_cal_result_t result);
const char *gas_calibration_channel_label(gas_cal_channel_t channel);
// Diagnostic snapshot never claims that software fitting validates MD62 helium performance.
bool gas_calibration_is_simulated(void);

#ifdef __cplusplus
}
#endif
