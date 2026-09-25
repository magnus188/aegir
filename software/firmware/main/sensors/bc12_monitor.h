#pragma once
#include "system_i2c.h"
#include "hardware_contract.h"

namespace bc12 {
constexpr uint8_t kAddress = hardware_contract::kUsbBc12Address; // ADDR tied to GND.
constexpr uint32_t kDetectionTimeoutMs = 5000;
constexpr uint32_t kDefaultHealthTtlMs = 3000;
enum class Port : uint8_t { Unknown, SDP, CDP, DCP, Proprietary };
enum class Status : uint8_t { Idle, Detecting, Ready, Rejected, Fault };
enum Fault : uint32_t {
    Transport = 1u << 0, Configuration = 1u << 1, SourceChanged = 1u << 2,
    DetectionTimeout = 1u << 3, Stale = 1u << 4, Ambiguous = 1u << 5,
    Unsupported = 1u << 6, UnexpectedHostEvent = 1u << 7,
    GenerationExhausted = 1u << 8, UnclearedStatus = 1u << 9
};
struct Sample {
    Status status = Status::Idle;
    Port port = Port::Unknown;
    bool valid = false; // Ready AND supported SDP/CDP/DCP. Never a current grant.
    uint32_t source_generation = 0, detection_generation = 0, faults = 0;
    uint32_t timestamp_ms = 0, started_ms = 0, observed_ms = 0, validated_ms = 0;
    uint8_t raw_client_status = 0, raw_host_status = 0;
};
class Monitor {
public:
    // Single worker owns this object and serializes all methods with the shared
    // bus. An ISR must signal that worker, never call these methods directly.
    // Health TTL and detection timeout are conservative software policy, not promises
    // that the IC detects physical unplug or finishes within a guaranteed time.
    explicit Monitor(system_i2c::Bus &bus, uint32_t health_ttl_ms = kDefaultHealthTtlMs)
        : bus_(bus), health_ttl_ms_(health_ttl_ms) {}
    // Strictly read-only on the bus: inspect controls and the cached result.
    // Does not read/clear REG02/03 or refresh the result's observation time.
    // A completed capability belongs to one continuously verified attachment,
    // not a periodic charge cycle. A missed health interval revokes it.
    Sample poll(uint32_t source_generation);
    // Caller MUST hold external ILIM permission GPIO LOW throughout all three
    // operations below, and independently verify an attached sink / 5 V source
    // generation before AND after. These methods cannot inspect physical GPIO.
    bool begin_detection(uint32_t source_generation);
    Sample collect_detection(uint32_t source_generation); // REG02/03 READ-CLEAR.
    bool stop(); // On detach: detection bit LOW, then power-down mode.
    // No I2C. The worker removes cached capability when handling a CC event.
    void invalidate();
    static Port decode(uint8_t raw_client_status, uint32_t &faults);
private:
    bool controls_match(uint8_t control1, uint8_t control2);
    bool read_status(uint8_t &client, uint8_t &host);
    bool clear_old_status();
    Sample fail(uint32_t fault);
    bool bound(uint32_t source_generation);
    system_i2c::Bus &bus_;
    const uint32_t health_ttl_ms_;
    Sample sample_{};
    uint32_t detection_generation_ = 0;
};
}
