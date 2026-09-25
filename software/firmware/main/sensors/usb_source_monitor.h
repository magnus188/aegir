#pragma once
#include "system_i2c.h"
#include "hardware_contract.h"

namespace usb_source {
constexpr uint8_t kAddress = hardware_contract::kUsbCcAddress; // ADDR low; PORT low (sink).
enum class Current : uint8_t { Unknown, UsbDefault, A1_5, A3 };
enum Fault : uint32_t {
    Transport = 1u << 0, Identity = 1u << 1, Configuration = 1u << 2,
    UnsupportedRole = 1u << 3, UnsupportedAccessory = 1u << 4,
    Incoherent = 1u << 5, GenerationExhausted = 1u << 6
};
struct Sample {
    uint32_t timestamp_ms = 0, generation = 0, faults = 0;
    bool valid = false, attached_sink = false, change_latched = false;
    Current current = Current::Unknown;
    // Zero for USB default: this driver does not assume enumeration or BC1.2.
    uint16_t advertised_ma = 0;
    uint8_t orientation = 0; // 0=CC1, 1=CC2; meaningful only while attached.
    uint8_t raw08 = 0, raw09 = 0, raw0a = 0, raw45 = 0, revision = 0;
};
class Monitor {
public:
    explicit Monitor(system_i2c::Bus &bus) : bus_(bus) {}
    // Strictly read-only. Never clears INT_N or arms current/charging hardware.
    Sample poll();
    // CALLER MUST HOLD THE EXTERNAL ILIM PERMISSION COMMAND LOW throughout
    // either operation. These APIs cannot verify GPIO or permission-latch state.
    // Initialization sets only DISABLE_UFP_ACCESSORY, preserving pending IRQ.
    bool initialize_disabled_policy();
    // Check expected identity/config/source, clear W1C event, verify it cleared
    // and that the same observation remains. Failure is not current permission.
    bool acknowledge_change(const Sample &expected, Sample &after);
    static bool decode(uint8_t r08, uint8_t r09, uint8_t r0a, uint8_t r45, Sample &out);
private:
    bool read_snapshot(Sample &out);
    Sample finish(Sample out);
    static bool equivalent(const Sample &a, const Sample &b);
    system_i2c::Bus &bus_;
    Sample previous_{};
    bool seen_ = false;
    uint32_t generation_ = 0;
};
}
