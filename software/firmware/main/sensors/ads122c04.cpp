#include "ads122c04.h"

namespace ads122 {
static_assert(gas_acquisition::kOxygenProfile.samples_per_second==20 &&
              gas_acquisition::kHeliumProfile.samples_per_second==20 &&
              gas_acquisition::kOxygenProfile.internal_reference_mv==2048 &&
              gas_acquisition::kHeliumProfile.internal_reference_mv==2048,
              "Update register encoding and scaling when changing the acquisition profile");
bool Adc::registers(Config c, uint8_t out[4]) {
    uint8_t gain_index = 0;
    for (uint16_t gain = 1; gain < c.gain && gain_index < 7; gain <<= 1) ++gain_index;
    if (!out || c.mux > 0xe || c.gain != (1u << gain_index) || (c.bypass && c.gain > 4) ||
        (c.mux >= 8 && c.mux <= 0xd && (!c.bypass || c.gain > 4))) return false;
    out[0] = static_cast<uint8_t>((c.mux << 4) | (gain_index << 1) | (c.bypass ? 1 : 0));
    // 20 SPS, normal mode, single-shot, internal 2.048 V reference, temperature off.
    out[1] = 0;
    // No IDAC, no burnout current, no counter and no CRC payload. Passive O2 cells are never excited.
    out[2] = 0;
    out[3] = 0;
    return true;
}
int32_t Adc::signed_code(const uint8_t b[3]) {
    const uint32_t value = (static_cast<uint32_t>(b[0]) << 16) | (static_cast<uint32_t>(b[1]) << 8) | b[2];
    return value & 0x800000u ? static_cast<int32_t>(value) - 0x1000000 : static_cast<int32_t>(value);
}
bool Adc::command(uint8_t c) { return io_.write(address_, &c, 1); }
bool Adc::read_registers(uint8_t out[4]) {
    // rr selects one register; unlike ADS1220 there is no count field in RREG.
    for (uint8_t reg = 0; reg < 4; ++reg)
        if (!command(static_cast<uint8_t>(0x20 | (reg << 2))) || !io_.read(address_, out + reg, 1)) return false;
    return true;
}
Result Adc::initialize() {
    initialized_ = false;
    if (address_ != kOxygenAddress && address_ != kHeliumAddress) return Result::InvalidConfig;
    if (!command(0x06)) return Result::Transport;
    io_.delay_ms(2);
    uint8_t registers[4]{};
    if (!read_registers(registers)) return Result::Transport;
    if (registers[0] || registers[1] || (registers[2] & 0x7f) || registers[3]) return Result::ConfigMismatch;
    initialized_ = true; return Result::Ok;
}
Result Adc::measure(Config config, Sample &out) {
    out = {};
    uint8_t desired[4]{};
    if (!registers(config, desired)) return Result::InvalidConfig;
    if (!initialized_) return Result::Transport;
    for (uint8_t reg = 0; reg < 4; ++reg) {
        const uint8_t write[2] = {static_cast<uint8_t>(0x40 | (reg << 2)), desired[reg]};
        if (!io_.write(address_, write, sizeof(write))) return Result::Transport;
    }
    uint8_t actual[4]{};
    if (!read_registers(actual)) return Result::Transport;
    if (actual[0] != desired[0] || actual[1] != desired[1] || (actual[2] & 0x7f) != desired[2] || actual[3] != desired[3])
        return Result::ConfigMismatch;
    // Allow 100 ms after a mux/gain change before the new integration. This exceeds
    // ten nominal 10 ms bias-filter time constants; unknown sensor impedance and
    // real channel-switch settling still require bench verification.
    io_.delay_ms(100);
    // Clear any old DRDY/data, including a conversion that completed after a previous timeout.
    uint8_t bytes[3]{};
    if (!command(0x10) || !io_.read(address_, bytes, 3) || !command(0x08)) return Result::Transport;
    const uint32_t start = io_.now_ms();
    for (;;) {
        io_.delay_ms(5);
        if (!read_registers(actual)) return Result::Transport;
        if (actual[0] != desired[0] || actual[1] != desired[1] || (actual[2] & 0x7f) != desired[2] || actual[3] != desired[3])
            return Result::ConfigMismatch;
        if (actual[2] & 0x80) break;
        if (io_.now_ms() - start >= 150) return Result::Timeout;
    }
    if (!command(0x10) || !io_.read(address_, bytes, 3)) return Result::Transport;
    out.code = signed_code(bytes);
    out.volts = out.code * (2.048 / (static_cast<double>(config.gain) * 8388608.0));
    out.gain = config.gain;
    out.timestamp_ms = io_.now_ms();
    if (out.code == 8388607 || out.code == -8388608) return Result::Clipped;
    return Result::Ok;
}
}
