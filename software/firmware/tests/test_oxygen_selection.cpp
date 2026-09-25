#include "services/oxygen_selection_core.h"
#include "sensors/gas_acquisition_config.h"
#include <cstdio>
#include <cstring>
#include <cmath>

namespace {
unsigned passed = 0, failed = 0;
void check(bool value, const char *name) {
    std::printf("%s %s\n", value ? "PASS" : "FAIL", name);
    value ? ++passed : ++failed;
}
class Memory : public oxygen_selection::Storage {
public:
    oxygen_selection::Record saved{};
    oxygen_selection::ReadResult result = oxygen_selection::ReadResult::Missing;
    bool fail_write = false;
    unsigned writes = 0;
    oxygen_selection::ReadResult read(oxygen_selection::Record &r) override { r = saved; return result; }
    bool write(const oxygen_selection::Record &r) override {
        ++writes;
        if (fail_write) return false;
        saved = r; result = oxygen_selection::ReadResult::Present; return true;
    }
};
}
int main() {
    Memory memory;
    oxygen_selection::Core core(memory, false);
    core.initialize();
    gas_cal_channel_t c = GAS_CAL_HELIUM;
    check(!core.selected(c) && core.status(0).choice == OXYGEN_UNCONFIGURED, "First boot never assumes an oxygen sensor");
    check(core.status(0).calibration_required && !core.status(0).storage_error, "Missing setup is distinct from damaged storage");
    check(core.confirm(OXYGEN_UNCONFIGURED, false, 0) == OXYGEN_SELECT_BAD_CHOICE && memory.writes == 0,
          "Unconfigured cannot be confirmed as a usable sensor");
    check(core.confirm(static_cast<oxygen_selection_t>(99), false, 0) == OXYGEN_SELECT_BAD_CHOICE, "Unknown choices rejected");
    check(core.confirm(OXYGEN_AO2, false, 4) == OXYGEN_SELECT_OK && core.selected(c) && c == GAS_CAL_AO2,
          "Explicit AO2 maps to original calibration ID zero");
    check(!core.usable(GAS_CAL_AO2, 4) && core.usable(GAS_CAL_AO2, 5), "First setup requires a newer calibration than an existing record");
    check(!core.usable(GAS_CAL_JJCCR, 999) && !core.usable(GAS_CAL_HELIUM, 999), "Only selected oxygen identity can supply primary reading");
    check(core.status(5).configured && !core.status(5).calibration_required, "Fresh saved revision clears calibration requirement");
    const auto ao2 = memory.saved;
    oxygen_selection::Core restarted(memory, false); restarted.initialize();
    check(restarted.selected(c) && c == GAS_CAL_AO2 && restarted.usable(c, 5), "Selection and required calibration revision survive restart");
    check(core.confirm(OXYGEN_AO2, false, 5) == OXYGEN_SELECT_OK && memory.saved.generation == ao2.generation,
          "Confirming unchanged installed cell does not erase calibration or rewrite settings");
    check(core.confirm(OXYGEN_AO2, true, 5) == OXYGEN_SELECT_OK && !core.usable(GAS_CAL_AO2, 5) && core.usable(GAS_CAL_AO2, 6),
          "Declared replacement of same cell type requires fresh calibration");
    const auto replacement = memory.saved;
    memory.fail_write = true;
    check(core.confirm(OXYGEN_JJCCR, false, 10) == OXYGEN_SELECT_STORAGE_ERROR &&
          std::memcmp(&replacement, &memory.saved, sizeof(replacement)) == 0 && core.selected(c) && c == GAS_CAL_AO2,
          "Interrupted settings write retains the previous selection and calibration gate");
    memory.fail_write = false;
    check(core.confirm(OXYGEN_JJCCR, false, 10) == OXYGEN_SELECT_OK && core.selected(c) && c == GAS_CAL_JJCCR,
          "JJ selection keeps original calibration ID one");
    check(!core.usable(GAS_CAL_AO2, 999) && !core.usable(GAS_CAL_JJCCR, 10) && core.usable(GAS_CAL_JJCCR, 11),
          "Changing type never reuses the other channel or its old calibration");
    check(core.confirm(OXYGEN_AO2, false, 6) == OXYGEN_SELECT_OK && !core.usable(GAS_CAL_AO2, 6),
          "Changing back also requires a fresh reference calibration");
    Memory wrong_source = memory;
    oxygen_selection::Core simulator(wrong_source, true); simulator.initialize();
    check(!simulator.selected(c) && simulator.status(0).storage_error, "Real configuration cannot load as simulated configuration");
    Memory future = memory; ++future.saved.profile_revision;
    oxygen_selection::Core new_profile(future, false); new_profile.initialize();
    check(!new_profile.selected(c) && new_profile.status(0).storage_error, "Unknown OTA acquisition profile is withheld rather than silently migrated");
    // Simulate a future supported profile while reading this firmware's stored
    // selection, without changing purchased ADC parameters in production.
    Memory upgraded_storage = memory;
    const auto before_upgrade = upgraded_storage.saved;
    const auto prior_writes = upgraded_storage.writes;
    oxygen_selection::Core upgraded(upgraded_storage, false, gas_acquisition::kOxygenProfile.revision + 1);
    upgraded.initialize();
    check(upgraded.selected(c) && c == GAS_CAL_AO2 && !upgraded.status(0).storage_error,
          "OTA profile upgrade preserves the explicitly installed oxygen type");
    check(upgraded.status(0).calibration_required && upgraded.status(0).minimum_calibration_revision == before_upgrade.minimum_calibration_revision,
          "Incompatible archived calibration requires fresh calibration without resetting the replacement gate");
    check(upgraded_storage.writes == prior_writes && std::memcmp(&before_upgrade, &upgraded_storage.saved, sizeof(before_upgrade)) == 0,
          "Profile migration is read-only and preserves persistent generation during OTA probation");
    check(!upgraded.usable(GAS_CAL_AO2, before_upgrade.minimum_calibration_revision - 1) &&
          upgraded.usable(GAS_CAL_AO2, before_upgrade.minimum_calibration_revision),
          "An upgraded profile still enforces the last declared sensor replacement");
    check(upgraded.confirm(OXYGEN_AO2, false, before_upgrade.minimum_calibration_revision) == OXYGEN_SELECT_OK &&
          upgraded_storage.writes == prior_writes,
          "Reconfirming retained type does not silently rewrite settings or reset calibration requirements");
    check(upgraded.confirm(OXYGEN_AO2, true, before_upgrade.minimum_calibration_revision) == OXYGEN_SELECT_OK &&
          upgraded_storage.saved.generation == before_upgrade.generation + 1 &&
          upgraded_storage.saved.minimum_calibration_revision == before_upgrade.minimum_calibration_revision + 1 &&
          upgraded_storage.saved.profile_revision == gas_acquisition::kOxygenProfile.revision + 1,
          "Explicit replacement after profile migration advances both generation and calibration gate");
    Memory zero_profile = memory; zero_profile.saved.profile_revision = zero_profile.saved.acquisition_revision = 0;
    oxygen_selection::Core invalid_zero(zero_profile, false); invalid_zero.initialize();
    check(!invalid_zero.selected(c) && invalid_zero.status(0).storage_error, "Zero profile identity is not a known older acquisition profile");
    Memory bad = memory; bad.saved.choice = 99;
    oxygen_selection::Core invalid(bad, false); invalid.initialize();
    check(!invalid.selected(c), "Malformed persisted choice is unconfigured");
    bad = memory; bad.saved.format_version = 99;
    oxygen_selection::Core unknown_schema(bad, false); unknown_schema.initialize();
    check(!unknown_schema.selected(c), "Unknown selection schema is unconfigured");
    bad = memory; bad.result = oxygen_selection::ReadResult::Error;
    oxygen_selection::Core storage_error(bad, false); storage_error.initialize();
    check(!storage_error.selected(c) && storage_error.status(0).storage_error, "Storage failure cannot select a default sensor");
    check(core.confirm(OXYGEN_JJCCR, true, UINT32_MAX) == OXYGEN_SELECT_STORAGE_ERROR, "Calibration revision overflow is rejected");
    check(gas_acquisition::revision_for(GAS_CAL_AO2) == 1 && gas_acquisition::revision_for(GAS_CAL_JJCCR) == 1 &&
          gas_acquisition::revision_for(GAS_CAL_HELIUM) == 2, "He protection change does not invalidate compatible O2 acquisition profiles");
    check(gas_acquisition::kOxygenProfile.gain == 8 && gas_acquisition::kOxygenProfile.samples_per_second == 20 &&
          gas_acquisition::kOxygenProfile.internal_reference_mv == 2048 && !gas_acquisition::kOxygenProfile.pga_bypass &&
          gas_acquisition::kHeliumProfile.gain == 1 && gas_acquisition::kHeliumProfile.pga_bypass,
          "Versioned acquisition profiles retain deliberate ADC starting configuration");
    gas_raw_sample_t a{}, j{}; a.warmed = j.warmed = a.stable = j.stable = true;
    a.mean_voltage_v = 0.01; j.mean_voltage_v = 0.000001;
    check(oxygen_selection::air_advice(false, &a, &j) == OXYGEN_AIR_CONFIRM_REFERENCE, "Air check requires explicit known-air confirmation");
    check(oxygen_selection::air_advice(true, &a, &j) == OXYGEN_AIR_AMBIGUOUS, "Different mV responses cannot identify a cell without qualified profile limits");
    j.faults = GAS_FAULT_STALE;
    check(oxygen_selection::air_advice(true, &a, &j) == OXYGEN_AIR_WAIT, "Stale diagnostic input cannot suggest a selection");
    j.faults = 0; j.mean_voltage_v = NAN;
    check(oxygen_selection::air_advice(true, &a, &j) == OXYGEN_AIR_WAIT, "Nonfinite diagnostic input cannot suggest a selection");
    check(oxygen_selection::air_advice(true, &a, nullptr) == OXYGEN_AIR_WAIT, "Missing diagnostic input remains manual/unavailable");
    std::printf("Results: %u passed, %u failed\n", passed, failed);
    return failed ? 1 : 0;
}
