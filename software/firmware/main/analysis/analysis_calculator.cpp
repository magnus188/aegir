#include "analysis_calculator.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

float clampf(float value, float min_value, float max_value) {
    return std::max(min_value, std::min(max_value, value));
}

float mod_for_limit(float oxygen_percent, float ppo2_limit) {
    if (oxygen_percent <= 0.0f || ppo2_limit <= 0.0f) {
        return 0.0f;
    }
    float mod = ((ppo2_limit / (oxygen_percent / 100.0f)) - 1.0f) * 10.0f;
    return std::max(0.0f, mod);
}

float density_for_depth(float oxygen_percent, float helium_percent, float depth_m) {
    float fo2 = oxygen_percent / 100.0f;
    float fhe = helium_percent / 100.0f;
    float fn2 = std::max(0.0f, 1.0f - fo2 - fhe);
    float p_abs = (depth_m / 10.0f) + 1.0f;
    return (fo2 * 1.429f + fn2 * 1.251f + fhe * 0.179f) * p_abs;
}

float equivalent_air_depth(float nitrogen_percent, float depth_m) {
    constexpr float kAirNitrogenFraction = 0.790f;
    float fn2 = clampf(nitrogen_percent / 100.0f, 0.0f, 1.0f);
    float p_abs = (depth_m / 10.0f) + 1.0f;
    return std::max(0.0f, ((fn2 * p_abs) / kAirNitrogenFraction - 1.0f) * 10.0f);
}

float equivalent_narcotic_depth(float helium_percent, float depth_m) {
    float narcotic_fraction = 1.0f - clampf(helium_percent / 100.0f, 0.0f, 1.0f);
    float p_abs = (depth_m / 10.0f) + 1.0f;
    return std::max(0.0f, (narcotic_fraction * p_abs - 1.0f) * 10.0f);
}

void set_label(char* dst, size_t dst_size, const char* text) {
    if (!dst || dst_size == 0) return;
    std::snprintf(dst, dst_size, "%s", text);
}

bool valid_gas_mode(analysis_gas_mode_t mode) {
    return mode >= ANALYSIS_GAS_MODE_OC_BACK_GAS && mode < ANALYSIS_GAS_MODE_COUNT;
}

void raise_advisory(analysis_result_t& result, analysis_severity_t severity, const char* text) {
    if (severity > result.severity) {
        result.severity = severity;
        set_label(result.advisory, sizeof(result.advisory), text);
    }
}

}  // namespace

extern "C" {

analysis_limits_t analysis_default_limits(void) {
    return {
        .ppo2_working_x100 = 140,
        .ppo2_secondary_x100 = 160,
        .density_advisory_x10 = 52,
        .density_alarm_x10 = 63,
        .co2_advisory_ppm = 500,
    };
}

analysis_result_t analysis_calculate(const analysis_input_t* input) {
    analysis_result_t result = {};
    if (!input) {
        result.valid = false;
        result.severity = ANALYSIS_SEVERITY_FAULT;
        set_label(result.mix_label, sizeof(result.mix_label), "No sample");
        set_label(result.advisory, sizeof(result.advisory), "No sample data");
        return result;
    }

    const sensor_readings_t& readings = input->readings;
    const float effective_helium = input->manual_he_percent >= 0.0f ? input->manual_he_percent : readings.helium_percent;
    const bool impossible_gas = !std::isfinite(effective_helium) || effective_helium < 0 || effective_helium > 100 ||
        !std::isfinite(readings.oxygen_percent) || readings.oxygen_percent < 0 || readings.oxygen_percent > 100 ||
        readings.oxygen_percent + effective_helium > 100.0f ||
        !std::isfinite(readings.helium_percent) || readings.helium_percent < 0 || readings.helium_percent > 100;
    result.valid = readings.status != SENSOR_STATUS_FAULT &&
                   !readings.oxygen_configuration_required && !readings.oxygen_calibration_required &&
                   !impossible_gas && std::isfinite(input->manual_he_percent) && std::isfinite(input->planned_depth_m) &&
                   std::isfinite(readings.oxygen_percent) &&
                   readings.oxygen_percent > 0.0f && readings.oxygen_percent <= 100.0f &&
                   std::isfinite(readings.helium_percent) && readings.helium_percent >= 0.0f;
    if (readings.source == SENSOR_SOURCE_HARDWARE &&
        (!readings.oxygen_calibrated || !readings.helium_calibrated || readings.calibration_unvalidated)) {
        result.valid = false;
    }
    result.oxygen_percent = readings.oxygen_percent;
    result.helium_percent = effective_helium;
    result.planned_depth_m = clampf(input->planned_depth_m, 0.0f, 150.0f);
    result.gas_mode = valid_gas_mode(input->gas_mode) ?
                          input->gas_mode :
                          ANALYSIS_GAS_MODE_OC_BACK_GAS;
    result.co2_ppm = readings.co2_ppm;
    result.co_valid = readings.co_valid && std::isfinite(readings.co_ppm) && readings.co_ppm >= 0.0f;
    result.co_ppm = result.co_valid ? readings.co_ppm : NAN;

    if (!result.valid) {
        result.nitrogen_percent = NAN;
        result.severity = ANALYSIS_SEVERITY_FAULT;
        set_label(result.mix_label, sizeof(result.mix_label), readings.oxygen_configuration_required ? "Set up oxygen" :
            readings.oxygen_calibration_required ? "Calibrate oxygen" : impossible_gas ? "Invalid composition" : "Sensor fault");
        set_label(result.advisory, sizeof(result.advisory), readings.oxygen_configuration_required ? "Confirm the installed AO2 or JJ-CCR sensor" :
            readings.oxygen_calibration_required ? "Selected oxygen sensor requires fresh calibration" :
            impossible_gas ? "Invalid gas values; O2 plus He must not exceed 100%" :
            readings.calibration_unvalidated ? "Bench calibration only; gas model not validated" : "Sample unavailable or uncalibrated");
        return result;
    }

    result.nitrogen_percent = std::max(0.0f, 100.0f - result.oxygen_percent - result.helium_percent);

    const float ppo2_working = input->limits.ppo2_working_x100 / 100.0f;
    const float ppo2_secondary = input->limits.ppo2_secondary_x100 / 100.0f;
    result.ppo2_at_depth = (result.oxygen_percent / 100.0f) * ((result.planned_depth_m / 10.0f) + 1.0f);
    result.mod_working_m = mod_for_limit(result.oxygen_percent, ppo2_working);
    result.mod_secondary_m = mod_for_limit(result.oxygen_percent, ppo2_secondary);
    result.ead_m = equivalent_air_depth(result.nitrogen_percent, result.planned_depth_m);
    result.end_m = equivalent_narcotic_depth(result.helium_percent, result.planned_depth_m);
    result.gas_density_g_l = density_for_depth(result.oxygen_percent, result.helium_percent, result.planned_depth_m);

    if (result.helium_percent >= 0.5f) {
        std::snprintf(result.mix_label, sizeof(result.mix_label), "Trimix %.0f/%.0f",
                      result.oxygen_percent, result.helium_percent);
    } else if (std::fabs(result.oxygen_percent - 20.9f) <= 0.7f) {
        set_label(result.mix_label, sizeof(result.mix_label), "Air");
    } else {
        std::snprintf(result.mix_label, sizeof(result.mix_label), "EAN%.0f", result.oxygen_percent);
    }

    result.severity = ANALYSIS_SEVERITY_NORMAL;
    set_label(result.advisory, sizeof(result.advisory), "Within configured limits");

    if (readings.status == SENSOR_STATUS_WARMING || readings.status == SENSOR_STATUS_STABILIZING) {
        result.severity = ANALYSIS_SEVERITY_ADVISORY;
        set_label(result.advisory, sizeof(result.advisory), "Sample not stable");
    } else if (readings.status == SENSOR_STATUS_UNSTABLE) {
        result.severity = ANALYSIS_SEVERITY_ADVISORY;
        set_label(result.advisory, sizeof(result.advisory), "Sample unstable");
    }

    const float density_advisory = input->limits.density_advisory_x10 / 10.0f;
    const float density_alarm = input->limits.density_alarm_x10 / 10.0f;
    if (result.gas_density_g_l > density_alarm) {
        result.severity = ANALYSIS_SEVERITY_ALARM;
        set_label(result.advisory, sizeof(result.advisory), "Gas density above configured alarm");
    } else if (result.gas_density_g_l > density_advisory &&
               result.severity < ANALYSIS_SEVERITY_ADVISORY) {
        result.severity = ANALYSIS_SEVERITY_ADVISORY;
        set_label(result.advisory, sizeof(result.advisory), "Gas density above configured advisory");
    }

    if (result.ppo2_at_depth > ppo2_secondary) {
        result.severity = ANALYSIS_SEVERITY_ALARM;
        set_label(result.advisory, sizeof(result.advisory), "PPO2 above configured secondary limit");
    } else if (result.ppo2_at_depth > ppo2_working &&
               result.severity < ANALYSIS_SEVERITY_ADVISORY) {
        result.severity = ANALYSIS_SEVERITY_ADVISORY;
        set_label(result.advisory, sizeof(result.advisory), "PPO2 above configured working limit");
    }

    if (result.co2_ppm > static_cast<float>(input->limits.co2_advisory_ppm) &&
        result.severity < ANALYSIS_SEVERITY_ADVISORY) {
        result.severity = ANALYSIS_SEVERITY_ADVISORY;
        set_label(result.advisory, sizeof(result.advisory), "CO2 above configured advisory");
    }

    const float ppo2_working_limit = input->limits.ppo2_working_x100 / 100.0f;
    switch (result.gas_mode) {
        case ANALYSIS_GAS_MODE_OC_BACK_GAS:
            if (result.ppo2_at_depth < 0.16f) {
                raise_advisory(result, ANALYSIS_SEVERITY_ALARM,
                               "Back gas PPO2 below breathable minimum at planned depth");
            }
            break;
        case ANALYSIS_GAS_MODE_DECO_GAS:
            if (result.mod_working_m + 0.1f < result.planned_depth_m) {
                raise_advisory(result, ANALYSIS_SEVERITY_ALARM,
                               "Deco gas exceeds working MOD at planned depth");
            } else if (result.oxygen_percent < 40.0f) {
                raise_advisory(result, ANALYSIS_SEVERITY_ADVISORY,
                               "Deco gas mode expects a higher oxygen mix");
            }
            break;
        case ANALYSIS_GAS_MODE_CCR_DILUENT:
            if (result.ppo2_at_depth > 1.20f) {
                raise_advisory(result, ANALYSIS_SEVERITY_ALARM,
                               "CCR diluent PPO2 high at planned depth");
            } else if (result.oxygen_percent < 16.0f) {
                raise_advisory(result, ANALYSIS_SEVERITY_ADVISORY,
                               "Hypoxic diluent; confirm travel procedure");
            } else if (result.oxygen_percent > 25.0f) {
                raise_advisory(result, ANALYSIS_SEVERITY_ADVISORY,
                               "CCR diluent oxygen is high for the selected mode");
            }
            break;
        case ANALYSIS_GAS_MODE_BAILOUT:
            if (result.ppo2_at_depth < 0.18f) {
                raise_advisory(result, ANALYSIS_SEVERITY_ALARM,
                               "Bailout PPO2 below breathable minimum at planned depth");
            } else if (result.ppo2_at_depth > ppo2_working_limit) {
                raise_advisory(result, ANALYSIS_SEVERITY_ALARM,
                               "Bailout PPO2 exceeds working limit at planned depth");
            }
            break;
        default:
            break;
    }

    return result;
}

const char* analysis_severity_label(analysis_severity_t severity) {
    switch (severity) {
        case ANALYSIS_SEVERITY_NORMAL:
            return "Nominal";
        case ANALYSIS_SEVERITY_ADVISORY:
            return "Advisory";
        case ANALYSIS_SEVERITY_ALARM:
            return "Alarm";
        case ANALYSIS_SEVERITY_FAULT:
            return "Fault";
        default:
            return "Unknown";
    }
}

const char* analysis_gas_mode_label(analysis_gas_mode_t mode) {
    switch (mode) {
        case ANALYSIS_GAS_MODE_OC_BACK_GAS:
            return "OC Back Gas";
        case ANALYSIS_GAS_MODE_DECO_GAS:
            return "Deco Gas";
        case ANALYSIS_GAS_MODE_CCR_DILUENT:
            return "CCR Diluent";
        case ANALYSIS_GAS_MODE_BAILOUT:
            return "Bailout";
        default:
            return "Unknown";
    }
}

}  // extern "C"
