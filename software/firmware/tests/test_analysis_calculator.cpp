#include "analysis/analysis_calculator.h"

#include <cmath>
#include <cstdio>
#include <cstring>

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

void expect_near(float actual, float expected, float tolerance, const char* name) {
    if (std::fabs(actual - expected) <= tolerance) {
        std::printf("  PASS %s (%.2f ~= %.2f)\n", name, actual, expected);
        ++tests_passed;
    } else {
        std::printf("  FAIL %s (got %.2f, expected %.2f)\n", name, actual, expected);
        ++tests_failed;
    }
}

sensor_readings_t sample(float o2, float he, float co2, sensor_status_t status = SENSOR_STATUS_STABLE) {
    sensor_readings_t r = {};
    r.oxygen_percent = o2;
    r.helium_percent = he;
    r.co2_ppm = co2;
    r.temperature_c = 22.0f;
    r.pressure_bar = 1.0f;
    r.humidity_pct = 42.0f;
    r.status = status;
    r.source = SENSOR_SOURCE_SIMULATED;
    return r;
}

}  // namespace

int main() {
    std::printf("Analysis Calculator Tests\n");
    std::printf("=========================\n\n");

    analysis_limits_t limits = analysis_default_limits();
    analysis_input_t input = {};
    input.readings = sample(18.0f, 45.0f, 420.0f);
    input.manual_he_percent = -1.0f;
    input.planned_depth_m = 40.0f;
    input.limits = limits;

    analysis_result_t result = analysis_calculate(&input);
    expect_true(result.valid, "Trimix sample is valid");
    expect_true(std::strcmp(result.mix_label, "Trimix 18/45") == 0, "Trimix label uses measured helium");
    expect_near(result.nitrogen_percent, 37.0f, 0.01f, "Derived nitrogen");
    expect_near(result.ppo2_at_depth, 0.90f, 0.01f, "PPO2 at planned depth");
    expect_near(result.mod_working_m, 67.78f, 0.2f, "MOD for working PPO2");
    expect_near(result.ead_m, 13.42f, 0.2f, "Equivalent air depth");
    expect_near(result.end_m, 17.5f, 0.2f, "Equivalent narcotic depth");
    expect_true(result.severity == ANALYSIS_SEVERITY_NORMAL, "Nominal trimix stays nominal");

    input.readings = sample(32.0f, 0.0f, 430.0f);
    input.manual_he_percent = 0.0f;
    input.planned_depth_m = 35.0f;
    result = analysis_calculate(&input);
    expect_true(result.severity == ANALYSIS_SEVERITY_ADVISORY, "PPO2 advisory threshold triggers");

    input.readings = sample(32.0f, 0.0f, 430.0f);
    input.planned_depth_m = 60.0f;
    result = analysis_calculate(&input);
    expect_true(result.severity == ANALYSIS_SEVERITY_ALARM, "PPO2 alarm threshold triggers");

    input.readings = sample(20.9f, 0.0f, 700.0f);
    input.planned_depth_m = 10.0f;
    result = analysis_calculate(&input);
    expect_true(result.severity == ANALYSIS_SEVERITY_ADVISORY, "CO2 advisory threshold triggers");

    input.readings = sample(32.0f, 0.0f, 430.0f);
    input.planned_depth_m = 30.0f;
    input.gas_mode = ANALYSIS_GAS_MODE_DECO_GAS;
    result = analysis_calculate(&input);
    expect_true(result.severity == ANALYSIS_SEVERITY_ADVISORY, "Deco mode flags low-oxygen deco gas");

    input.readings = sample(32.0f, 0.0f, 430.0f);
    input.planned_depth_m = 40.0f;
    input.gas_mode = ANALYSIS_GAS_MODE_BAILOUT;
    result = analysis_calculate(&input);
    expect_true(result.severity == ANALYSIS_SEVERITY_ALARM, "Bailout mode escalates working PPO2 breach");

    input.readings = sample(10.0f, 60.0f, 420.0f);
    input.planned_depth_m = 6.0f;
    input.gas_mode = ANALYSIS_GAS_MODE_CCR_DILUENT;
    result = analysis_calculate(&input);
    expect_true(result.severity == ANALYSIS_SEVERITY_ADVISORY, "CCR mode flags hypoxic diluent");

    input.readings = sample(-1.0f, -1.0f, -1.0f, SENSOR_STATUS_FAULT);
    result = analysis_calculate(&input);
    expect_true(!result.valid, "Faulted sample is invalid");
    expect_true(result.severity == ANALYSIS_SEVERITY_FAULT, "Fault severity is reported");

    input.readings = sample(20.9f, 0.0f, NAN);
    input.readings.co_ppm = 3.0f;
    input.readings.co_valid = true;
    result = analysis_calculate(&input);
    expect_true(result.valid, "An absent CO2 sensor does not invalidate oxygen and helium");
    expect_true(result.co_valid && result.co_ppm == 3.0f && std::isnan(result.co2_ppm),
                "CO measurement remains distinct from missing CO2");
    input.readings.source = SENSOR_SOURCE_HARDWARE;
    input.readings.oxygen_calibrated = true;
    input.readings.helium_calibrated = true;
    input.readings.calibration_unvalidated = true;
    result = analysis_calculate(&input);
    expect_true(!result.valid, "Unvalidated physical helium cannot produce a usable gas analysis");
    input.readings.source = SENSOR_SOURCE_SIMULATED;
    input.readings.oxygen_percent = NAN;
    result = analysis_calculate(&input);
    expect_true(!result.valid, "Nonfinite oxygen is rejected");
    input.readings = sample(80, 30, 420); input.manual_he_percent = -1;
    result = analysis_calculate(&input);
    expect_true(!result.valid && result.helium_percent == 30 && std::isnan(result.nitrogen_percent),
                "Impossible O2 plus He is rejected without shrinking helium or inventing nitrogen");
    input.readings = sample(20, 0, 420); input.manual_he_percent = 101;
    expect_true(!analysis_calculate(&input).valid, "Manual helium above 100 percent is rejected");
    input.manual_he_percent = NAN;
    expect_true(!analysis_calculate(&input).valid, "Nonfinite manual helium does not silently fall back");
    input.manual_he_percent = -1; input.readings.oxygen_configuration_required = true;
    expect_true(!analysis_calculate(&input).valid, "Unconfigured oxygen blocks derived gas claims");
    input.readings.oxygen_configuration_required = false; input.readings.oxygen_calibration_required = true;
    expect_true(!analysis_calculate(&input).valid, "Required replacement calibration blocks derived gas claims");

    std::printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
