#include "oxygen_selection_core.h"
#include "sensors/gas_acquisition_config.h"
#include <cmath>
#include <limits>
#include <algorithm>

namespace oxygen_selection {
bool channel_for(oxygen_selection_t choice, gas_cal_channel_t &channel) {
    if (choice == OXYGEN_AO2) { channel = GAS_CAL_AO2; return true; }
    if (choice == OXYGEN_JJCCR) { channel = GAS_CAL_JJCCR; return true; }
    return false;
}
bool record_valid(const Record &r, bool simulated, uint32_t supported_profile_revision) {
    gas_cal_channel_t unused;
    return r.format_version == kSchema && r.generation &&
        channel_for(static_cast<oxygen_selection_t>(r.choice), unused) &&
        r.acquisition_revision == r.profile_revision &&
        r.profile_revision && r.profile_revision <= supported_profile_revision &&
        r.minimum_calibration_revision && r.simulated == static_cast<uint32_t>(simulated) && !r.reserved;
}
void Core::initialize() {
    if (initialized_) return;
    Record loaded{};
    const auto result = storage_.read(loaded);
    // A known older acquisition profile does not change which physical type
    // the owner installed. Preserve that choice and its replacement gate; the
    // calibration service separately rejects incompatible transfer coefficients.
    if (result == ReadResult::Present && record_valid(loaded, simulated_, profile_revision_)) record_ = loaded;
    else storage_error_ = result != ReadResult::Missing;
    initialized_ = true;
}
bool Core::selected(gas_cal_channel_t &channel) const {
    return !storage_error_ && record_valid(record_, simulated_, profile_revision_) &&
        channel_for(static_cast<oxygen_selection_t>(record_.choice), channel);
}
bool Core::usable(gas_cal_channel_t channel, uint32_t revision) const {
    gas_cal_channel_t selected_channel;
    return selected(selected_channel) && channel == selected_channel &&
        revision >= record_.minimum_calibration_revision;
}
oxygen_selection_status_t Core::status(uint32_t calibration_revision) const {
    oxygen_selection_status_t out{};
    gas_cal_channel_t channel;
    out.configured = selected(channel);
    out.choice = out.configured ? static_cast<oxygen_selection_t>(record_.choice) : OXYGEN_UNCONFIGURED;
    out.calibration_required = !out.configured || !usable(channel, calibration_revision);
    out.storage_error = storage_error_;
    out.simulated = simulated_;
    out.generation = record_.generation;
    out.profile_revision = profile_revision_;
    out.minimum_calibration_revision = record_.minimum_calibration_revision;
    return out;
}
oxygen_selection_result_t Core::confirm(oxygen_selection_t choice, bool replacement, uint32_t previous_revision) {
    initialize();
    gas_cal_channel_t channel;
    if (!channel_for(choice, channel)) return OXYGEN_SELECT_BAD_CHOICE;
    if (!storage_error_ && record_.choice == static_cast<uint32_t>(choice) && !replacement) return OXYGEN_SELECT_OK;
    if (record_.generation == UINT32_MAX || previous_revision == UINT32_MAX) return OXYGEN_SELECT_STORAGE_ERROR;
    Record candidate{};
    candidate.format_version = kSchema;
    candidate.generation = record_.generation + 1;
    candidate.choice = choice;
    candidate.acquisition_revision = profile_revision_;
    candidate.profile_revision = profile_revision_;
    candidate.minimum_calibration_revision = previous_revision + 1;
    candidate.simulated = simulated_;
    if (!storage_.write(candidate)) return OXYGEN_SELECT_STORAGE_ERROR;
    record_ = candidate;
    storage_error_ = false;
    return OXYGEN_SELECT_OK;
}
oxygen_air_advice_t air_advice(bool known_air, const gas_raw_sample_t *ao2, const gas_raw_sample_t *jj) {
    if (!known_air) return OXYGEN_AIR_CONFIRM_REFERENCE;
    const auto ready = [](const gas_raw_sample_t *s) {
        return s && !s->faults && s->warmed && s->stable &&
            std::isfinite(s->mean_voltage_v) && std::isfinite(s->peak_to_peak_v) && s->peak_to_peak_v >= 0;
    };
    if (!ready(ao2) || !ready(jj)) return OXYGEN_AIR_WAIT;
    // Neither installed sensor profile has qualified air response bounds.
    // Display both raw responses for wiring review; do not invent mV limits
    // or use voltage to infer a physical cell type.
    return OXYGEN_AIR_AMBIGUOUS;
}
}
