#pragma once
#include <cstddef>
#include <cstdint>
#include "gas_acquisition_config.h"
#include "hardware_contract.h"

namespace ads122 {
constexpr uint8_t kOxygenAddress = hardware_contract::kOxygenAddress;
constexpr uint8_t kHeliumAddress = hardware_contract::kHeliumAddress;
enum class Result { Ok, InvalidConfig, Transport, ConfigMismatch, Timeout, Clipped };
struct Config { uint8_t mux; uint8_t gain; bool bypass; };
constexpr Config kOxygenA{0x0, gas_acquisition::kOxygenProfile.gain, gas_acquisition::kOxygenProfile.pga_bypass};
constexpr Config kOxygenB{0x6, gas_acquisition::kOxygenProfile.gain, gas_acquisition::kOxygenProfile.pga_bypass};
constexpr Config kHelium{0x0, gas_acquisition::kHeliumProfile.gain, gas_acquisition::kHeliumProfile.pga_bypass};
constexpr Config kExcitation{0xa, 1, true};
constexpr Config kBias{0xb, 1, true};
struct Sample { int32_t code = 0; double volts = 0; uint8_t gain = 0; uint32_t timestamp_ms = 0; };
class Transport {
public:
    virtual ~Transport() = default;
    virtual bool write(uint8_t address, const uint8_t *data, size_t size) = 0;
    virtual bool read(uint8_t address, uint8_t *data, size_t size) = 0;
    virtual void delay_ms(uint32_t ms) = 0;
    virtual uint32_t now_ms() = 0;
};
class Adc {
public:
    Adc(Transport &transport, uint8_t address) : io_(transport), address_(address) {}
    Result initialize();
    Result measure(Config config, Sample &out);
    static bool registers(Config config, uint8_t out[4]);
    static int32_t signed_code(const uint8_t bytes[3]);
private:
    bool command(uint8_t command);
    bool read_registers(uint8_t out[4]);
    Transport &io_;
    uint8_t address_;
    bool initialized_ = false;
};
}
