#pragma once

#include <cstdint>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Gas calculation functions for dive planning
 * 
 * Key formulas:
 * - PPO2 = FO2 * (depth/10 + 1)  where FO2 is fraction (0.21 for air)
 * - MOD = (PPO2 / FO2 - 1) * 10
 * - The current helium planning helper uses (1 - FHe) because O2 is not an input.
 *   This is closer to an END-style narcotic fraction than a strict N2-only EAD.
 */

/**
 * Calculate required O2 percentage for given depth and PPO2
 * @param depth_m Depth in meters
 * @param ppo2 Partial pressure of O2 in bar (e.g., 1.4)
 * @return O2 percentage (21-100)
 */
float calc_o2_for_depth_ppo2(float depth_m, float ppo2);

/**
 * Calculate Maximum Operating Depth for given O2% and PPO2
 * @param o2_percent O2 percentage (21-100)
 * @param ppo2 Target PPO2 in bar
 * @return MOD in meters
 */
float calc_mod(float o2_percent, float ppo2);

/**
 * Calculate PPO2 for given depth and O2%
 * @param depth_m Depth in meters
 * @param o2_percent O2 percentage
 * @return PPO2 in bar
 */
float calc_ppo2(float depth_m, float o2_percent);

/**
 * Calculate the current helium-adjusted narcotic depth helper.
 * @param depth_m Actual depth in meters
 * @param he_percent Helium percentage (0-70)
 * @return Equivalent depth in meters under the current app model
 */
float calc_ead(float depth_m, float he_percent);

/**
 * Calculate required depth for target EAD with given helium
 * @param ead_m Target EAD in meters
 * @param he_percent Helium percentage
 * @return Required actual depth in meters
 */
float calc_depth_for_ead(float ead_m, float he_percent);

/**
 * Calculate required helium for target EAD at given depth
 * @param depth_m Actual depth in meters
 * @param ead_m Target EAD in meters
 * @return Required helium percentage
 */
float calc_helium_for_ead(float depth_m, float ead_m);

typedef struct {
    bool valid;
    float oxygen_add_bar;
    float helium_add_bar;
    float air_add_bar;
    float fill_pressure_bar;
    float final_o2_percent;
    float final_he_percent;
    char status[96];
} blend_topup_result_t;

/**
 * Calculate a partial-pressure top-up using pure O2, pure He, and air.
 * Pressures are in bar. Gas percentages are 0-100.
 */
blend_topup_result_t calc_blend_topup(float current_pressure_bar,
                                      float final_pressure_bar,
                                      float current_o2_percent,
                                      float current_he_percent,
                                      float target_o2_percent,
                                      float target_he_percent);

/**
 * Clamp a value between min and max
 */
float clamp_float(float value, float min_val, float max_val);

#ifdef __cplusplus
}
#endif
