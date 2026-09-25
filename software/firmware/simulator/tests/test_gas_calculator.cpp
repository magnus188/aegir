#include "gas_calculator.h"

#include <cmath>
#include <cstdio>

namespace {

int g_failed = 0;

void expect_near(const char* name, float actual, float expected, float tolerance) {
    if (std::fabs(actual - expected) > tolerance) {
        std::fprintf(stderr, "%s: got %.4f, expected %.4f\n", name, actual, expected);
        ++g_failed;
    }
}

}  // namespace

int main() {
    expect_near("MOD 32% @ 1.4", calc_mod(32.0f, 1.4f), 33.75f, 0.01f);
    expect_near("PPO2 32% @ 30m", calc_ppo2(30.0f, 32.0f), 1.28f, 0.01f);
    expect_near("O2 for 30m @ 1.4", calc_o2_for_depth_ppo2(30.0f, 1.4f), 35.0f, 0.01f);
    expect_near("O2 clamps to air", calc_o2_for_depth_ppo2(60.0f, 1.4f), 21.0f, 0.01f);

    const float ead = calc_ead(60.0f, 35.0f);
    expect_near("Depth/EAD inverse", calc_depth_for_ead(ead, 35.0f), 60.0f, 0.01f);
    expect_near("Helium/EAD inverse", calc_helium_for_ead(60.0f, ead), 35.0f, 0.01f);

    expect_near("Clamp lower", clamp_float(-5.0f, 0.0f, 10.0f), 0.0f, 0.01f);
    expect_near("Clamp upper", clamp_float(15.0f, 0.0f, 10.0f), 10.0f, 0.01f);

    blend_topup_result_t topup = calc_blend_topup(50.0f, 200.0f, 20.9f, 0.0f, 18.0f, 45.0f);
    if (!topup.valid) {
        std::fprintf(stderr, "Expected top-up plan to be valid\n");
        ++g_failed;
    }
    expect_near("Top-up O2", topup.oxygen_add_bar, 16.4f, 0.5f);
    expect_near("Top-up He", topup.helium_add_bar, 90.0f, 0.5f);

    return g_failed == 0 ? 0 : 1;
}
