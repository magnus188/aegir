#include "ui/screens/dive_planner/gas_calculator.h"
#include "analysis/analysis_calculator.h"
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <limits>

namespace {
int failed = 0;
void check(bool condition, const char* message) {
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", message);
    if (!condition) ++failed;
}
bool near(float a, float b, float tolerance = 0.001f) {
    return std::isfinite(a) && std::isfinite(b) && std::fabs(a - b) <= tolerance;
}
uint32_t seed = 0xA361;
unsigned next(unsigned max) { seed = 1664525U * seed + 1013904223U; return seed % max; }
}

int main() {
    const float bad[] = {NAN, INFINITY, -INFINITY};
    bool rejects_nonfinite = true;
    for (float value : bad) for (int slot = 0; slot < 6; ++slot) {
        float args[] = {50, 200, 21, 0, 18, 45}; args[slot] = value;
        const auto plan = calc_blend_topup(args[0], args[1], args[2], args[3], args[4], args[5]);
        rejects_nonfinite &= !plan.valid && plan.status[0] &&
            std::isfinite(plan.oxygen_add_bar) && std::isfinite(plan.helium_add_bar) && std::isfinite(plan.air_add_bar);
    }
    check(rejects_nonfinite, "All 18 nonfinite blend input cases are rejected without NaN additions");
    check(!calc_blend_topup(-1, 200, 21, 0, 18, 45).valid &&
          !calc_blend_topup(200, 200, 21, 0, 18, 45).valid &&
          !calc_blend_topup(201, 200, 21, 0, 18, 45).valid,
          "Negative, equal, and reversed fill pressures are rejected");
    check(!calc_blend_topup(0, 200, 21, 0, 70, 70).valid &&
          !calc_blend_topup(50, 200, 70, 70, 18, 45).valid,
          "Invalid source and target fractions are rejected");
    check(std::isnan(calc_ead(40, 80, 80)) && std::isnan(calc_ead(40, NAN, 21)) &&
          std::isnan(calc_depth_for_ead(30, 79, 21)) && std::isnan(calc_mod(NAN, 1.4f)) &&
          std::isnan(calc_o2_for_depth_ppo2(-10, 1.4f)),
          "Impossible mixtures, nonfinite inputs, and undefined inverse calculations fail explicitly");
    check(near(calc_ead(40, 0, 21), 40) && near(calc_ead(40, 0, 32), 33.03797f) &&
          near(calc_ead(60, 45, 18), 22.78481f), "Air, nitrox, and trimix EAD use nitrogen fraction");

    bool consistent = true, inverses = true;
    unsigned compositions = 0;
    for (int oxygen = 1; oxygen <= 100; oxygen += 3) {
        for (int helium = 0; helium <= 100 - oxygen; helium += 5) {
            for (int depth = 0; depth <= 150; depth += 5) {
                analysis_input_t input{};
                input.readings.oxygen_percent = oxygen;
                input.readings.helium_percent = helium;
                input.readings.status = SENSOR_STATUS_STABLE;
                input.readings.source = SENSOR_SOURCE_SIMULATED;
                input.manual_he_percent = -1;
                input.planned_depth_m = depth;
                input.limits = analysis_default_limits();
                const auto result = analysis_calculate(&input);
                const float ead = calc_ead(depth, helium, oxygen);
                consistent &= result.valid && near(ead, result.ead_m) &&
                    near(calc_ppo2(depth, oxygen), result.ppo2_at_depth) &&
                    near(calc_mod(oxygen, 1.4f), result.mod_working_m, 0.01f);
                if (ead > 0.01f) inverses &= near(calc_depth_for_ead(ead, helium, oxygen), depth) &&
                    near(calc_helium_for_ead(depth, ead, oxygen), helium);
                ++compositions;
            }
        }
    }
    std::printf("Checked %u depth/composition combinations\n", compositions);
    check(consistent, "Planner EAD, PPO2, and MOD agree with production analysis across the input grid");
    check(inverses, "Equivalent-depth inverse calculations recover feasible source values");

    bool conserves = true;
    unsigned valid_plans = 0;
    for (int i = 0; i < 10000; ++i) {
        const float start = next(200), final = 200 + next(101);
        const float o2 = next(101), he = next(101 - static_cast<unsigned>(o2));
        const float target_o2 = next(101), target_he = next(101 - static_cast<unsigned>(target_o2));
        const auto plan = calc_blend_topup(start, final, o2, he, target_o2, target_he);
        if (!plan.valid) continue;
        ++valid_plans;
        conserves &= near(plan.oxygen_add_bar + plan.helium_add_bar + plan.air_add_bar, final - start, 0.15f) &&
            near((start * o2 / 100 + plan.oxygen_add_bar + plan.air_add_bar * 0.21f) / final * 100, target_o2, 0.05f) &&
            near((start * he / 100 + plan.helium_add_bar) / final * 100, target_he, 0.05f) &&
            plan.oxygen_add_bar >= 0 && plan.helium_add_bar >= 0 && plan.air_add_bar >= 0;
    }
    std::printf("Checked 10000 blend candidates (%u feasible plans)\n", valid_plans);
    check(conserves && valid_plans > 1000, "Feasible top-ups conserve fill pressure and oxygen/helium fractions");
    return failed ? 1 : 0;
}
