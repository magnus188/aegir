#include "gas_calibration_core.h"
#include "sensors/gas_acquisition_config.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace gas_cal {
namespace {
bool valid_channel(gas_cal_channel_t c) { return c >= GAS_CAL_AO2 && c < GAS_CAL_CHANNEL_COUNT; }
uint8_t expected_gain(gas_cal_channel_t c) { return c == GAS_CAL_HELIUM ? gas_acquisition::kHeliumGain : gas_acquisition::kOxygenGain; }
double reference_value(gas_cal_channel_t c, const gas_cal_reference_t &r) {
    return c == GAS_CAL_HELIUM ? r.helium_percent : r.oxygen_percent;
}
bool valid_reference(const gas_cal_reference_t &r) {
    return std::isfinite(r.oxygen_percent) && std::isfinite(r.helium_percent) &&
        r.oxygen_percent >= 0 && r.helium_percent >= 0 && r.oxygen_percent + r.helium_percent <= 100 &&
        (!r.uncertainty_known || (std::isfinite(r.uncertainty_pp) && r.uncertainty_pp > 0 && r.uncertainty_pp <= 100)) &&
        r.reference_id[0] && std::memchr(r.reference_id, '\0', sizeof(r.reference_id));
}
gas_cal_result_t acceptable(const gas_raw_sample_t &s, uint32_t now, bool simulated) {
    if (s.simulated != simulated) return GAS_CAL_WRONG_SOURCE;
    if (now - s.timestamp_ms > kStaleMs) return GAS_CAL_STALE;
    if (s.faults || !std::isfinite(s.voltage_v) || !std::isfinite(s.mean_voltage_v)) return GAS_CAL_SENSOR_FAULT;
    if (!s.warmed) return GAS_CAL_WARMING;
    if (!s.stable) return GAS_CAL_UNSTABLE;
    return GAS_CAL_OK;
}
}

uint32_t record_crc(const gas_cal_record_t &record) {
    uint32_t crc = 0xffffffffu;
    const auto *bytes = reinterpret_cast<const uint8_t *>(&record);
    for (size_t i = 0; i < offsetof(gas_cal_record_t, crc32); ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

bool record_integrity_valid(const gas_cal_record_t &r, gas_cal_channel_t c, bool simulated) {
    if (!valid_channel(c) || r.format_version != kVersion || !r.acquisition_revision || r.channel != c || r.simulated != simulated ||
        !r.valid || r.characterization_validated || !r.revision || r.crc32 != record_crc(r) ||
        !std::isfinite(r.zero_voltage_v) || !std::isfinite(r.percent_per_volt) || r.percent_per_volt == 0) return false;
    for (const auto &p : r.points) {
        if (!p.captured || !valid_reference(p.reference) || !std::isfinite(p.sample.mean_voltage_v) ||
            p.sample.simulated != simulated || !p.sample.gain || p.sample.gain > 128 ||
            (p.sample.gain & (p.sample.gain - 1)) || p.sample.gain != r.points[0].sample.gain || p.sample.faults || !p.sample.warmed || !p.sample.stable) return false;
    }
    return true;
}

bool record_valid(const gas_cal_record_t &r, gas_cal_channel_t c, bool simulated) {
    return record_integrity_valid(r, c, simulated) &&
        r.acquisition_revision == gas_acquisition::revision_for(c) &&
        r.points[0].sample.gain == expected_gain(c);
}

Core::Core(Storage &storage, bool simulated) : storage_(storage), simulated_(simulated) {}
void Core::initialize() {
    if (initialized_) return;
    for (int i = 0; i < GAS_CAL_CHANNEL_COUNT; ++i) {
        gas_cal_record_t loaded{};
        const auto channel = static_cast<gas_cal_channel_t>(i);
        if (storage_.load(channel, simulated_, loaded) && record_integrity_valid(loaded, channel, simulated_)) channels_[i].saved = loaded;
    }
    initialized_ = true;
}
void Core::reset_history(gas_cal_channel_t c) {
    if (!valid_channel(c)) return;
    channels_[c].count = 0;
    channels_[c].present = false;
}
void Core::ingest(gas_cal_channel_t c, const gas_raw_sample_t &input) {
    if (!valid_channel(c)) return;
    auto &ch = channels_[c];
    if (ch.present && input.sequence == ch.latest.sequence) return;
    const bool gap = ch.present && (input.timestamp_ms - ch.latest.timestamp_ms > kStaleMs || input.gain != ch.latest.gain);
    ch.latest = input;
    ch.present = true;
    ch.latest.stable = false;
    ch.latest.mean_voltage_v = input.voltage_v;
    ch.latest.peak_to_peak_v = 0;
    if (!std::isfinite(input.voltage_v)) ch.latest.faults |= GAS_FAULT_NONFINITE;
    if (input.simulated != simulated_) ch.latest.faults |= GAS_FAULT_TRANSPORT;
    if (gap || ch.latest.faults || !input.warmed) ch.count = 0;
    if (ch.latest.faults || !input.warmed) return;
    // Keep a bounded window and always retain its sample just before the 10 s boundary.
    while (ch.count > 1 && input.timestamp_ms - ch.history[1].timestamp_ms >= kHistoryMs) {
        std::memmove(ch.history, ch.history + 1, (--ch.count) * sizeof(ch.history[0]));
    }
    if (ch.count == kHistoryCapacity) {
        std::memmove(ch.history, ch.history + 1, (kHistoryCapacity - 1) * sizeof(ch.history[0]));
        --ch.count;
    }
    ch.history[ch.count++] = ch.latest;
    double low = input.voltage_v, high = input.voltage_v, sum = 0, early = 0, late = 0;
    for (size_t i = 0; i < ch.count; ++i) {
        const double v = ch.history[i].voltage_v;
        low = std::min(low, v); high = std::max(high, v); sum += v;
        if (i < ch.count / 2) early += v; else late += v;
    }
    ch.latest.mean_voltage_v = sum / ch.count;
    ch.latest.peak_to_peak_v = high - low;
    // Provisional electrical stability gates, not gas accuracy specifications.
    const double limit = c == GAS_CAL_HELIUM ? 30e-6 : 10e-6;
    if (ch.count >= 10 && input.timestamp_ms - ch.history[0].timestamp_ms >= kHistoryMs) {
        const double drift = std::fabs(early / (ch.count / 2) - late / (ch.count - ch.count / 2));
        ch.latest.stable = high - low <= limit && drift <= limit / 2;
    }
}
bool Core::sample(gas_cal_channel_t c, uint32_t now, gas_raw_sample_t &out) const {
    if (!valid_channel(c) || !channels_[c].present) return false;
    out = channels_[c].latest;
    if (now - out.timestamp_ms > kStaleMs) { out.faults |= GAS_FAULT_STALE; out.stable = false; }
    return true;
}
bool Core::record(gas_cal_channel_t c, gas_cal_record_t &out) const {
    if (!valid_channel(c) || !channels_[c].saved.valid) return false;
    out = channels_[c].saved;
    return true;
}
bool Core::point(gas_cal_channel_t c, unsigned index, gas_cal_point_t &out) const {
    if (!valid_channel(c) || index > 1 || !channels_[c].pending[index].captured) return false;
    out = channels_[c].pending[index]; return true;
}
gas_cal_result_t Core::capture(gas_cal_channel_t c, unsigned index, const gas_cal_reference_t &reference, uint32_t now) {
    if (!valid_channel(c) || index > 1 || !valid_reference(reference)) return GAS_CAL_BAD_ARGUMENT;
    auto &ch = channels_[c];
    if (!ch.present) return GAS_CAL_NO_SAMPLE;
    const auto status = acceptable(ch.latest, now, simulated_);
    if (status != GAS_CAL_OK) return status;
    ch.pending[index] = {};
    ch.pending[index].reference = reference;
    ch.pending[index].sample = ch.latest;
    ch.pending[index].captured = true;
    return GAS_CAL_OK;
}
gas_cal_result_t Core::commit(gas_cal_channel_t c, uint32_t now) {
    if (!valid_channel(c)) return GAS_CAL_BAD_ARGUMENT;
    auto &ch = channels_[c];
    if (!ch.pending[0].captured || !ch.pending[1].captured) return GAS_CAL_NEED_TWO_POINTS;
    if (!ch.present) return GAS_CAL_NO_SAMPLE;
    const auto status = acceptable(ch.latest, now, simulated_);
    if (status != GAS_CAL_OK) return status;
    for (const auto &p : ch.pending) {
        if (now - p.sample.timestamp_ms > 30u * 60u * 1000u) return GAS_CAL_STALE;
        if (p.sample.gain != ch.latest.gain) return GAS_CAL_SENSOR_FAULT;
    }
    const double dc = reference_value(c, ch.pending[1].reference) - reference_value(c, ch.pending[0].reference);
    const double dv = ch.pending[1].sample.mean_voltage_v - ch.pending[0].sample.mean_voltage_v;
    const double noise = std::max(ch.pending[0].sample.peak_to_peak_v, ch.pending[1].sample.peak_to_peak_v);
    if (!std::isfinite(dc) || !std::isfinite(dv) || std::fabs(dc) < 5 || std::fabs(dv) <= std::max(10e-6, 20 * noise) ||
        (c != GAS_CAL_HELIUM && dc / dv <= 0)) return GAS_CAL_INSUFFICIENT_SPAN;
    gas_cal_record_t candidate{};
    candidate.format_version = kVersion;
    candidate.acquisition_revision = gas_acquisition::revision_for(c);
    candidate.revision = ch.saved.revision + 1;
    if (!candidate.revision) return GAS_CAL_STORAGE_ERROR;
    candidate.channel = c; candidate.simulated = simulated_; candidate.valid = true;
    candidate.percent_per_volt = dc / dv;
    candidate.zero_voltage_v = ch.pending[0].sample.mean_voltage_v - reference_value(c, ch.pending[0].reference) / candidate.percent_per_volt;
    candidate.reference_budget_met = true;
    const float budget = c == GAS_CAL_HELIUM ? 0.125f : 0.05f;
    for (unsigned i = 0; i < 2; ++i) {
        candidate.points[i] = ch.pending[i];
        candidate.reference_budget_met &= ch.pending[i].reference.uncertainty_known && ch.pending[i].reference.uncertainty_pp <= budget;
    }
    candidate.crc32 = record_crc(candidate);
    if (!record_valid(candidate, c, simulated_) || !storage_.save(candidate)) return GAS_CAL_STORAGE_ERROR;
    ch.saved = candidate;
    cancel(c);
    return GAS_CAL_OK;
}
void Core::cancel(gas_cal_channel_t c) {
    if (valid_channel(c)) for (auto &p : channels_[c].pending) p = {};
}
bool Core::convert(gas_cal_channel_t c, double voltage, double &percent) const {
    if (!valid_channel(c) || !record_valid(channels_[c].saved, c, simulated_) || !std::isfinite(voltage) || !channels_[c].present ||
        channels_[c].latest.gain != expected_gain(c)) return false;
    const auto &r = channels_[c].saved;
    percent = (voltage - r.zero_voltage_v) * r.percent_per_volt;
    return std::isfinite(percent);
}
}
