#pragma once
#include <cstddef>
#include <cstdint>

namespace system_i2c {
// One physical bus for the two ADCs, charger, gauge and environmental module.
class Bus {
public:
    virtual ~Bus() = default;
    virtual bool write(uint8_t address, const uint8_t *data, size_t count) = 0;
    virtual bool read(uint8_t address, uint8_t *data, size_t count) = 0;
    virtual bool read_register(uint8_t address, uint8_t reg, uint8_t *data, size_t count) = 0;
    virtual void delay_ms(uint32_t ms) = 0;
    virtual uint32_t now_ms() = 0;
    bool write_register(uint8_t address, uint8_t reg, uint8_t value) {
        const uint8_t bytes[] = {reg, value}; return write(address, bytes, sizeof(bytes));
    }
};
Bus &shared();
bool initialize();
bool responsive();
}
