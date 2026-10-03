#include "gas_calculator.h"
#include <cmath>
#include <cstdio>

/**
 * Gas calculation implementations
 * 
 * Pressure at depth: P = (depth/10) + 1 bar (absolute)
 * PPO2 = FO2 * P
 * EAD compares nitrogen partial pressure with air (79% nitrogen).
 */

float calc_o2_for_depth_ppo2(float depth_m, float ppo2) {
    if (!std::isfinite(depth_m) || !std::isfinite(ppo2) || depth_m < 0 || ppo2 < 0) return NAN;
    // P_abs = depth/10 + 1
    // PPO2 = FO2 * P_abs
    // FO2 = PPO2 / P_abs
    float p_abs = (depth_m / 10.0f) + 1.0f;
    float fo2 = ppo2 / p_abs;
    float o2_percent = fo2 * 100.0f;
    return clamp_float(o2_percent, 0.0f, 100.0f);
}

float calc_mod(float o2_percent, float ppo2) {
    if (!std::isfinite(o2_percent) || !std::isfinite(ppo2) || o2_percent < 0 || o2_percent > 100 || ppo2 < 0) return NAN;
    // MOD = (PPO2 / FO2 - 1) * 10
    float fo2 = o2_percent / 100.0f;
    if (fo2 <= 0.0f) return 0.0f;
    float mod = ((ppo2 / fo2) - 1.0f) * 10.0f;
    return fmaxf(mod, 0.0f);
}

float calc_ppo2(float depth_m, float o2_percent) {
    if (!std::isfinite(depth_m) || !std::isfinite(o2_percent) || depth_m < 0 || o2_percent < 0 || o2_percent > 100) return NAN;
    // PPO2 = FO2 * P_abs
    float p_abs = (depth_m / 10.0f) + 1.0f;
    float fo2 = o2_percent / 100.0f;
    return fo2 * p_abs;
}

float calc_ead(float depth_m, float he_percent, float o2_percent) {
    if (!std::isfinite(depth_m) || !std::isfinite(he_percent) || !std::isfinite(o2_percent) ||
        depth_m < 0 || he_percent < 0 || o2_percent < 0 || he_percent + o2_percent > 100) return NAN;
    const float nitrogen = (100.0f - o2_percent - he_percent) / 100.0f;
    return fmaxf(nitrogen * (depth_m + 10.0f) / 0.79f - 10.0f, 0.0f);
}

float calc_depth_for_ead(float ead_m, float he_percent, float o2_percent) {
    if (!std::isfinite(ead_m) || !std::isfinite(he_percent) || !std::isfinite(o2_percent) ||
        ead_m < 0 || he_percent < 0 || o2_percent < 0 || he_percent + o2_percent >= 100) return NAN;
    const float nitrogen = (100.0f - o2_percent - he_percent) / 100.0f;
    return fmaxf((ead_m + 10.0f) * 0.79f / nitrogen - 10.0f, 0.0f);
}

float calc_helium_for_ead(float depth_m, float ead_m, float o2_percent) {
    if (!std::isfinite(depth_m) || !std::isfinite(ead_m) || !std::isfinite(o2_percent) ||
        depth_m < 0 || ead_m < 0 || o2_percent < 0 || o2_percent > 100) return NAN;
    return clamp_float(100.0f - o2_percent - (ead_m + 10.0f) * 79.0f / (depth_m + 10.0f),
                       0.0f, 100.0f - o2_percent);
}

blend_topup_result_t calc_blend_topup(float current_pressure_bar,
                                      float final_pressure_bar,
                                      float current_o2_percent,
                                      float current_he_percent,
                                      float target_o2_percent,
                                      float target_he_percent) {
    blend_topup_result_t result = {};
    if (!std::isfinite(current_pressure_bar) || !std::isfinite(final_pressure_bar) ||
        !std::isfinite(current_o2_percent) || !std::isfinite(current_he_percent) ||
        !std::isfinite(target_o2_percent) || !std::isfinite(target_he_percent)) {
        std::snprintf(result.status, sizeof(result.status), "Enter finite pressures and gas fractions");
        return result;
    }
    result.fill_pressure_bar = final_pressure_bar - current_pressure_bar;

    if (current_pressure_bar < 0.0f || final_pressure_bar <= current_pressure_bar) {
        std::snprintf(result.status, sizeof(result.status),
                      "Final pressure must be above current pressure");
        return result;
    }

    if (current_o2_percent < 0.0f || current_he_percent < 0.0f ||
        target_o2_percent < 0.0f || target_he_percent < 0.0f ||
        current_o2_percent + current_he_percent > 100.0f ||
        target_o2_percent + target_he_percent > 100.0f) {
        std::snprintf(result.status, sizeof(result.status), "Invalid gas fractions");
        return result;
    }

    const float fill = result.fill_pressure_bar;
    const float current_o2_partial = current_pressure_bar * (current_o2_percent / 100.0f);
    const float current_he_partial = current_pressure_bar * (current_he_percent / 100.0f);
    const float target_o2_partial = final_pressure_bar * (target_o2_percent / 100.0f);
    const float target_he_partial = final_pressure_bar * (target_he_percent / 100.0f);

    const float oxygen_needed = target_o2_partial - current_o2_partial;
    const float helium_needed = target_he_partial - current_he_partial;
    const float oxygen_add = (oxygen_needed - 0.21f * fill + 0.21f * helium_needed) / 0.79f;
    const float air_add = fill - oxygen_add - helium_needed;

    if (!std::isfinite(oxygen_add) || !std::isfinite(helium_needed) || !std::isfinite(air_add)) {
        std::snprintf(result.status, sizeof(result.status), "Pressures exceed calculation range");
        return result;
    }
    constexpr float tolerance = 0.05f;
    if (oxygen_add < -tolerance || helium_needed < -tolerance || air_add < -tolerance) {
        std::snprintf(result.status, sizeof(result.status),
                      "Drain or different source gas required");
        return result;
    }

    result.oxygen_add_bar = oxygen_add < 0.0f ? 0.0f : oxygen_add;
    result.helium_add_bar = helium_needed < 0.0f ? 0.0f : helium_needed;
    result.air_add_bar = air_add < 0.0f ? 0.0f : air_add;
    result.final_o2_percent = target_o2_percent;
    result.final_he_percent = target_he_percent;
    result.valid = true;
    std::snprintf(result.status, sizeof(result.status), "Blend plan ready");
    return result;
}

float clamp_float(float value, float min_val, float max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}
