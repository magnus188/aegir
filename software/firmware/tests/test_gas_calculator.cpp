#include "gas_calculator.h"

#include <cmath>
#include <cstdio>

namespace {

int tests_passed = 0;
int tests_failed = 0;

void assert_near(float actual, float expected, float tolerance, const char* test_name) {
    if (std::fabs(actual - expected) <= tolerance) {
        std::printf("  PASS %s (%.2f ~= %.2f)\n", test_name, actual, expected);
        tests_passed++;
    } else {
        std::printf("  FAIL %s (got %.2f, expected %.2f)\n", test_name, actual, expected);
        tests_failed++;
    }
}

void assert_true(bool condition, const char* test_name) {
    if (condition) {
        std::printf("  PASS %s\n", test_name);
        tests_passed++;
    } else {
        std::printf("  FAIL %s\n", test_name);
        tests_failed++;
    }
}

}  // namespace

int main() {
    std::printf("Gas Calculator Production Tests\n");
    std::printf("===============================\n\n");

    std::printf("MOD calculations:\n");
    assert_near(calc_mod(100.0f, 1.6f), 6.0f, 0.1f, "100% O2 @ 1.6 PPO2 = 6m");
    assert_near(calc_mod(32.0f, 1.4f), 33.75f, 0.5f, "32% O2 @ 1.4 PPO2 = 33.75m");
    assert_near(calc_mod(21.0f, 1.4f), 56.67f, 0.5f, "21% O2 @ 1.4 PPO2 = 56.67m");
    assert_near(calc_mod(0.0f, 1.4f), 0.0f, 0.01f, "0% O2 clamps MOD to 0m");

    std::printf("\nPPO2 calculations:\n");
    assert_near(calc_ppo2(30.0f, 32.0f), 1.28f, 0.01f, "32% O2 @ 30m = 1.28 PPO2");
    assert_near(calc_ppo2(0.0f, 21.0f), 0.21f, 0.01f, "21% O2 @ surface = 0.21 PPO2");
    assert_near(calc_ppo2(40.0f, 21.0f), 1.05f, 0.01f, "Air @ 40m = 1.05 PPO2");

    std::printf("\nO2 percent for depth/PPO2:\n");
    assert_near(calc_o2_for_depth_ppo2(30.0f, 1.4f), 35.0f, 0.5f, "1.4 PPO2 @ 30m = 35% O2");
    assert_near(calc_o2_for_depth_ppo2(60.0f, 1.4f), 21.0f, 0.01f, "O2 recommendation clamps to 21% minimum");
    assert_near(calc_o2_for_depth_ppo2(0.0f, 1.6f), 100.0f, 0.01f, "O2 recommendation clamps to 100% maximum");

    std::printf("\nEAD calculations:\n");
    assert_near(calc_ead(40.0f, 0.0f), 53.29f, 0.5f, "0% He @ 40m follows current EAD formula");
    assert_near(calc_ead(60.0f, 35.0f), 47.59f, 0.5f, "35% He @ 60m lowers EAD");
    assert_near(calc_ead(40.0f, 100.0f), 0.0f, 0.01f, "100% He clamps EAD to 0m");

    std::printf("\nReverse EAD calculations:\n");
    assert_near(calc_depth_for_ead(30.0f, 35.0f), 38.62f, 0.5f, "Depth for EAD 30m with 35% He");
    assert_near(calc_helium_for_ead(60.0f, 30.0f), 54.86f, 0.5f, "Helium for EAD 30m @ 60m");
    assert_near(clamp_float(150.0f, 0.0f, 100.0f), 100.0f, 0.01f, "clamp upper bound");
    assert_near(clamp_float(-5.0f, 0.0f, 100.0f), 0.0f, 0.01f, "clamp lower bound");

    std::printf("\nBlend top-up calculations:\n");
    blend_topup_result_t topup = calc_blend_topup(50.0f, 200.0f, 20.9f, 0.0f, 18.0f, 45.0f);
    assert_true(topup.valid, "Air to trimix top-up is possible");
    assert_near(topup.oxygen_add_bar, 16.4f, 0.5f, "Trimix top-up O2 addition");
    assert_near(topup.helium_add_bar, 90.0f, 0.5f, "Trimix top-up He addition");
    assert_near(topup.air_add_bar, 43.6f, 0.5f, "Trimix top-up air addition");
    blend_topup_result_t impossible = calc_blend_topup(150.0f, 200.0f, 50.0f, 0.0f, 21.0f, 0.0f);
    assert_true(!impossible.valid, "Impossible top-up requires drain or alternate source");

    std::printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
