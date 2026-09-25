#include "sensor_interface.h"
#include <esp_err.h>
#include <esp_log.h>
#include <cmath>
#include "sensor_hardware.h"
#include "services/gas_calibration_internal.h"

#if !defined(ESP_PLATFORM) || defined(TRIMIX_SIMULATOR)
static const char *TAG = "SENSOR_IF";
#endif

namespace {

struct MockProfileSpec {
    const char* name;
    float oxygen_percent;
    float helium_percent;
    float co2_ppm;
    float temperature_c;
    float pressure_bar;
    float humidity_pct;
};

constexpr MockProfileSpec kProfiles[SENSOR_MOCK_PROFILE_COUNT] = {
    {"Air", 20.9f, 0.0f, 420.0f, 22.1f, 1.00f, 44.0f},
    {"EAN32", 32.0f, 0.0f, 430.0f, 22.4f, 1.01f, 42.0f},
    {"Trimix 18/45", 18.0f, 45.0f, 425.0f, 21.8f, 1.00f, 41.0f},
    {"High CO2", 20.8f, 0.0f, 900.0f, 23.0f, 1.02f, 48.0f},
    {"Unstable", 21.0f, 0.0f, 460.0f, 22.6f, 1.00f, 45.0f},
    {"Sensor Fault", 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
};

sensor_mock_profile_t g_profile = SENSOR_MOCK_PROFILE_AIR;
#if !defined(ESP_PLATFORM) || defined(TRIMIX_SIMULATOR)
uint32_t g_sequence = 0;
uint32_t g_profile_start_sequence = 0;
uint32_t g_last_co2_calibration_sequence = 0;

float deterministic_noise(uint32_t sequence, uint32_t salt, float amplitude) {
    uint32_t v = (sequence * 37U + salt * 17U) % 101U;
    float centered = (static_cast<float>(v) - 50.0f) / 50.0f;
    return centered * amplitude;
}

float settle_to_target(float target, uint32_t local_sequence, float offset) {
    if (local_sequence < 3) {
        return target + offset;
    }
    if (local_sequence < 9) {
        float progress = static_cast<float>(local_sequence - 2U) / 6.0f;
        return target + offset * (1.0f - progress);
    }
    return target;
}

sensor_status_t status_for_profile(sensor_mock_profile_t profile, uint32_t local_sequence) {
    if (profile == SENSOR_MOCK_PROFILE_SENSOR_FAULT) {
        return SENSOR_STATUS_FAULT;
    }
    if (profile == SENSOR_MOCK_PROFILE_UNSTABLE && local_sequence >= 6) {
        return SENSOR_STATUS_UNSTABLE;
    }
    if (local_sequence < 3) {
        return SENSOR_STATUS_WARMING;
    }
    if (local_sequence < 9) {
        return SENSOR_STATUS_STABILIZING;
    }
    return SENSOR_STATUS_STABLE;
}
#endif

bool valid_profile(sensor_mock_profile_t profile) {
    return profile >= SENSOR_MOCK_PROFILE_AIR && profile < SENSOR_MOCK_PROFILE_COUNT;
}

}  // namespace

extern "C" {
esp_err_t sensor_read_all(sensor_readings_t *out) {
    if (!out) return ESP_ERR_INVALID_ARG;
#if defined(ESP_PLATFORM) && !defined(TRIMIX_SIMULATOR)
    return sensor_hardware_read(out);
#else
    *out = {};

    const sensor_mock_profile_t profile = valid_profile(g_profile) ? g_profile : SENSOR_MOCK_PROFILE_AIR;
    const MockProfileSpec& spec = kProfiles[profile];
    const uint32_t sequence = ++g_sequence;
    const uint32_t local_sequence = sequence - g_profile_start_sequence;
    const sensor_status_t status = status_for_profile(profile, local_sequence);

    out->timestamp_ms = sequence * 1000U;
    out->sequence = sequence;
    out->status = status;
    out->source = SENSOR_SOURCE_SIMULATED;
    out->calibration_unvalidated = true;
    out->environment_valid = status != SENSOR_STATUS_FAULT;
    out->co_ppm = status == SENSOR_STATUS_FAULT ? NAN : 0.3f;
    out->co_valid = status != SENSOR_STATUS_FAULT;
    oxygen_selection_status_t selection{};
    oxygen_selection_get_status(&selection);
    out->oxygen_selection = selection.choice;
    out->oxygen_selection_generation = selection.generation;
    out->oxygen_configuration_required = !selection.configured;
    out->oxygen_calibration_required = selection.calibration_required;

    if (status == SENSOR_STATUS_FAULT) {
        out->oxygen_percent = -1.0f;
        out->helium_percent = -1.0f;
        out->co2_ppm = -1.0f;
        out->temperature_c = 0.0f;
        out->pressure_bar = 0.0f;
        out->humidity_pct = 0.0f;
        for (int i = 0; i < GAS_CAL_CHANNEL_COUNT; ++i) {
            gas_raw_sample_t raw{}; raw.voltage_v = NAN; raw.sequence = sequence; raw.timestamp_ms = out->timestamp_ms;
            raw.simulated = true; raw.faults = GAS_FAULT_TRANSPORT;
            gas_calibration_publish(static_cast<gas_cal_channel_t>(i), raw);
        }
        return ESP_OK;
    }

    float helium = settle_to_target(spec.helium_percent, local_sequence, 1.2f);
    float co2 = settle_to_target(spec.co2_ppm, local_sequence, 70.0f);
    float temp = settle_to_target(spec.temperature_c, local_sequence, -0.7f);
    float pressure = settle_to_target(spec.pressure_bar, local_sequence, -0.02f);
    float humidity = settle_to_target(spec.humidity_pct, local_sequence, 4.0f);

    if (profile == SENSOR_MOCK_PROFILE_UNSTABLE) {
        helium += std::cos(static_cast<float>(local_sequence) * 0.5f) * 1.2f;
        co2 += std::cos(static_cast<float>(local_sequence) * 0.7f) * 160.0f;
    } else {
        helium += deterministic_noise(sequence, 6, 0.08f);
        co2 += deterministic_noise(sequence, 2, 5.0f);
    }

    out->oxygen_percent = NAN;
    out->helium_percent = helium < 0.0f ? 0.0f : helium;
    out->co2_ppm = co2;
    out->temperature_c = temp + deterministic_noise(sequence, 3, 0.08f);
    out->pressure_bar = pressure + deterministic_noise(sequence, 4, 0.003f);
    out->humidity_pct = humidity + deterministic_noise(sequence, 5, 0.3f);
    out->oxygen_jj_percent = NAN;
    out->oxygen_jj_valid = false;
    for (int i = 0; i < GAS_CAL_CHANNEL_COUNT; ++i) {
        const auto channel = static_cast<gas_cal_channel_t>(i);
        gas_raw_sample_t raw{};
        raw.simulated = true; raw.sequence = sequence; raw.timestamp_ms = out->timestamp_ms;
        raw.warmed = local_sequence >= 9;
        raw.gain = channel == GAS_CAL_HELIUM ? 1 : 8;
        raw.excitation_v = 3.0f; raw.bias_v = 1.65f;
        raw.temperature_c = out->temperature_c; raw.humidity_pct = out->humidity_pct; raw.pressure_bar = out->pressure_bar;
        raw.environment_valid = true;
        // Artificial transfer functions for exercising the wizard only. Never used by the hardware backend.
        if (channel == GAS_CAL_HELIUM)
            raw.voltage_v = 0.030 - settle_to_target(spec.helium_percent, local_sequence, 1.2f) * 0.0008 + deterministic_noise(sequence, 8, 2e-6f);
        else raw.voltage_v = settle_to_target(spec.oxygen_percent, local_sequence, -1.6f) * (channel == GAS_CAL_AO2 ? 0.0005 : 0.00055) +
                10e-6 + deterministic_noise(sequence, 7, 0.5e-6f);
        if (profile == SENSOR_MOCK_PROFILE_UNSTABLE) raw.voltage_v += std::sin(local_sequence * 0.9) * 0.001;
        raw.adc_code = static_cast<int32_t>(raw.voltage_v * raw.gain * 8388608.0 / 2.048);
        gas_calibration_publish(channel, raw);
        double calibrated; uint32_t applied_revision=0;
        if (gas_calibration_convert_snapshot(channel, raw.voltage_v, calibrated, applied_revision) && calibrated >= 0 && calibrated <= 100) {
            if (channel != GAS_CAL_HELIUM && oxygen_selection_channel_enabled(channel)) {
                out->oxygen_percent = calibrated; out->oxygen_calibrated = true;
                out->oxygen_calibration_revision = applied_revision;
            }
            if (channel == GAS_CAL_JJCCR) { out->oxygen_jj_percent = calibrated; out->oxygen_jj_valid = true; }
            if (channel == GAS_CAL_HELIUM) { out->helium_percent = calibrated; out->helium_calibrated = true; out->helium_calibration_revision=applied_revision; }
        }
    }
    return ESP_OK;
#endif
}

esp_err_t sensor_calibrate_oxygen_air(void) {
    // Legacy one-click API cannot capture a gas identity or two valid references.
    // Callers must use the source-aware two-point calibration service.
    return ESP_ERR_INVALID_STATE;
}

esp_err_t sensor_calibrate_co2_zero(void) {
#if defined(ESP_PLATFORM) && !defined(TRIMIX_SIMULATOR)
    return ESP_ERR_INVALID_STATE; // ZE07-CO is not a CO2 sensor.
#else
    if (g_profile == SENSOR_MOCK_PROFILE_SENSOR_FAULT) {
        ESP_LOGW(TAG, "CO2 zero calibration rejected while sensor fault profile is active");
        return ESP_ERR_INVALID_STATE;
    }
    g_last_co2_calibration_sequence = g_sequence;
    ESP_LOGI(TAG, "Recording CO2 zero calibration in simulation");
    return ESP_OK;
#endif
}

esp_err_t sensor_calibrate_co2_reference(uint16_t reference_ppm) {
    if (reference_ppm < 300 || reference_ppm > 2000) {
        return ESP_ERR_INVALID_ARG;
    }
#if defined(ESP_PLATFORM) && !defined(TRIMIX_SIMULATOR)
    return ESP_ERR_INVALID_STATE;
#else
    if (g_profile == SENSOR_MOCK_PROFILE_SENSOR_FAULT) {
        ESP_LOGW(TAG, "CO2 reference calibration rejected while sensor fault profile is active");
        return ESP_ERR_INVALID_STATE;
    }
    g_last_co2_calibration_sequence = g_sequence;
    ESP_LOGI(TAG, "Recording CO2 reference calibration at %u ppm in simulation", reference_ppm);
    return ESP_OK;
#endif
}

void sensor_set_mock_profile(sensor_mock_profile_t profile) {
#if defined(ESP_PLATFORM) && !defined(TRIMIX_SIMULATOR)
    (void)profile; // A production build cannot silently switch physical inputs to demonstration data.
#else
    if (!valid_profile(profile)) {
        return;
    }
    g_profile = profile;
    g_profile_start_sequence = g_sequence;
    gas_calibration_reset_history();
    ESP_LOGI(TAG, "Mock sensor profile: %s", sensor_mock_profile_name(profile));
#endif
}

sensor_mock_profile_t sensor_get_mock_profile(void) {
    return valid_profile(g_profile) ? g_profile : SENSOR_MOCK_PROFILE_AIR;
}

const char* sensor_mock_profile_name(sensor_mock_profile_t profile) {
    if (!valid_profile(profile)) {
        return "Unknown";
    }
    return kProfiles[profile].name;
}

const char* sensor_status_label(sensor_status_t status) {
    switch (status) {
        case SENSOR_STATUS_WARMING:
            return "Warming";
        case SENSOR_STATUS_STABILIZING:
            return "Stabilizing";
        case SENSOR_STATUS_STABLE:
            return "Stable";
        case SENSOR_STATUS_UNSTABLE:
            return "Unstable";
        case SENSOR_STATUS_FAULT:
            return "Fault";
        default:
            return "Unknown";
    }
}

const char* sensor_source_label(sensor_source_t source) {
    switch (source) {
        case SENSOR_SOURCE_SIMULATED:
            return "Simulated";
        case SENSOR_SOURCE_HARDWARE:
            return "Hardware";
        default:
            return "Unknown";
    }
}
}
