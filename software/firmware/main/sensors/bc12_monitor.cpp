#include "bc12_monitor.h"

namespace bc12 {
namespace {
constexpr uint8_t kClientMode = 0x08;
constexpr uint8_t kSwitchOff = 0x02;
constexpr uint8_t kDetect = 0x08;
}
Port Monitor::decode(uint8_t raw, uint32_t &faults) {
    faults = 0;
    if (!raw) return Port::Unknown;
    if ((raw & 0x10) || (raw & (raw - 1))) { faults = Ambiguous; return Port::Unknown; }
    if (raw == 0x80) return Port::DCP;
    if (raw == 0x40) return Port::SDP;
    if (raw == 0x20) return Port::CDP;
    faults = Unsupported;
    return raw == 1 ? Port::Unknown : Port::Proprietary;
}
Sample Monitor::fail(uint32_t fault) {
    sample_.status = Status::Fault;
    sample_.valid = false;
    sample_.port = Port::Unknown;
    sample_.faults |= fault;
    sample_.timestamp_ms = bus_.now_ms();
    return sample_;
}
void Monitor::invalidate() {
    sample_ = {};
    sample_.detection_generation = detection_generation_;
    sample_.timestamp_ms = bus_.now_ms();
}
bool Monitor::bound(uint32_t generation) {
    if (!generation || generation != sample_.source_generation) { fail(SourceChanged); return false; }
    return true;
}
bool Monitor::controls_match(uint8_t c1, uint8_t c2) {
    // Single-byte reads avoid depending on unspecified register auto-increment.
    // Both copies must match exact policy, including reserved/default fields.
    uint8_t a0 = 0, a1 = 0, b0 = 0, b1 = 0;
    if (!bus_.read_register(kAddress, 0, &a0, 1) ||
        !bus_.read_register(kAddress, 1, &a1, 1) ||
        !bus_.read_register(kAddress, 0, &b0, 1) ||
        !bus_.read_register(kAddress, 1, &b1, 1)) { fail(Transport); return false; }
    if (a0 != c1 || b0 != c1 || a1 != c2 || b1 != c2) { fail(Configuration); return false; }
    return true;
}
bool Monitor::read_status(uint8_t &client, uint8_t &host) {
    // Reading either register ACKNOWLEDGES its interrupt. Never call from poll.
    if (!bus_.read_register(kAddress, 2, &client, 1) ||
        !bus_.read_register(kAddress, 3, &host, 1)) { fail(Transport); return false; }
    return true;
}
bool Monitor::clear_old_status() {
    uint8_t client = 0, host = 0;
    if (!read_status(client, host) || !read_status(client, host)) return false;
    if (client || host) { fail(UnclearedStatus); return false; }
    return true;
}
bool Monitor::begin_detection(uint32_t generation) {
    invalidate();
    if (detection_generation_ == UINT32_MAX) { fail(GenerationExhausted); return false; }
    sample_.detection_generation = ++detection_generation_;
    sample_.source_generation = generation;
    if (!bound(generation)) return false;
    if (!health_ttl_ms_ || health_ttl_ms_ > INT32_MAX) { fail(Configuration); return false; }
    // Power/enable readiness is specified below 500 us; wait 1 ms before access.
    // Caller has already dropped current permission and verified ATTACHED.SNK.
    bus_.delay_ms(1);
    // Stop the previous cycle before discarding its READ-CLEAR result. Program
    // only client mode; never host/USB-path-on or a proprietary voltage protocol.
    const bool stopped = bus_.write_register(kAddress, 1, kSwitchOff);
    const bool down = bus_.write_register(kAddress, 0, 0);
    if (!stopped || !down) { fail(Transport); return false; }
    if (!controls_match(0, kSwitchOff) || !clear_old_status()) return false;
    if (!bus_.write_register(kAddress, 0, kClientMode)) { fail(Transport); return false; }
    if (!controls_match(kClientMode, kSwitchOff)) return false;
    // An observed LOW control bit followed by this verified HIGH establishes a
    // fresh detection cycle. No result from before the transition is retained.
    if (!bus_.write_register(kAddress, 1, kSwitchOff | kDetect)) { fail(Transport); return false; }
    if (!controls_match(kClientMode, kSwitchOff | kDetect)) return false;
    sample_.status = Status::Detecting;
    sample_.started_ms = sample_.timestamp_ms = bus_.now_ms();
    return true;
}
Sample Monitor::poll(uint32_t generation) {
    sample_.timestamp_ms = bus_.now_ms();
    if (sample_.status == Status::Idle || sample_.status == Status::Fault) return sample_;
    if (!bound(generation)) return sample_;
    if (sample_.status == Status::Detecting &&
        uint32_t(bus_.now_ms() - sample_.started_ms) >= kDetectionTimeoutMs) return fail(DetectionTimeout);
    if (sample_.status == Status::Ready &&
        uint32_t(bus_.now_ms() - sample_.validated_ms) >= health_ttl_ms_) return fail(Stale);
    if (!controls_match(kClientMode, kSwitchOff | kDetect)) return sample_;
    sample_.timestamp_ms = bus_.now_ms();
    // Check after I2C latency too. A late read cannot resurrect a missed lease.
    // PI3USB9201 Rev3-2 p8 binds detection to ATTACHED.SNK. Keep its original
    // read-clear observation, while independently refreshing control health.
    // The caller must provide the freshly polled CC attachment generation.
    if (sample_.status == Status::Ready &&
        uint32_t(sample_.timestamp_ms - sample_.validated_ms) >= health_ttl_ms_) return fail(Stale);
    if (sample_.status == Status::Detecting &&
        uint32_t(sample_.timestamp_ms - sample_.started_ms) >= kDetectionTimeoutMs) return fail(DetectionTimeout);
    if (sample_.status == Status::Ready) sample_.validated_ms = sample_.timestamp_ms;
    return sample_;
}
Sample Monitor::collect_detection(uint32_t generation) {
    // A completed/rejected cycle is never silently rerun or re-acknowledged.
    if (poll(generation).status != Status::Detecting) return sample_;
    uint8_t client = 0, host = 0;
    if (!read_status(client, host)) return sample_;
    sample_.raw_client_status = client;
    sample_.raw_host_status = host;
    if (host) return fail(UnexpectedHostEvent);
    if (!controls_match(kClientMode, kSwitchOff | kDetect)) return sample_;
    if (uint32_t(bus_.now_ms() - sample_.started_ms) >= kDetectionTimeoutMs) return fail(DetectionTimeout);
    if (!client) { sample_.timestamp_ms = bus_.now_ms(); return sample_; }
    // A read-clear result must actually clear. A second/new event or stuck
    // status is ambiguous and requires a new caller-disabled detection cycle.
    uint8_t cleared_client = 0, cleared_host = 0;
    if (!read_status(cleared_client, cleared_host)) return sample_;
    if (cleared_client || cleared_host) return fail(UnclearedStatus);
    if (!controls_match(kClientMode, kSwitchOff | kDetect)) return sample_;
    if (uint32_t(bus_.now_ms() - sample_.started_ms) >= kDetectionTimeoutMs) return fail(DetectionTimeout);
    sample_.port = decode(client, sample_.faults);
    sample_.status = sample_.faults ? Status::Rejected : Status::Ready;
    sample_.valid = sample_.status == Status::Ready;
    sample_.observed_ms = sample_.validated_ms = sample_.timestamp_ms = bus_.now_ms();
    return sample_;
}
bool Monitor::stop() {
    invalidate(); // Drop the cached capability even when the bus is unavailable.
    const bool stopped = bus_.write_register(kAddress, 1, kSwitchOff);
    const bool down = bus_.write_register(kAddress, 0, 0);
    if (!stopped || !down) { fail(Transport); return false; }
    return controls_match(0, kSwitchOff);
}
}
