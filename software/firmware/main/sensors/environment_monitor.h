#pragma once
#include "system_i2c.h"
#include "third_party/bme280/bme280.h"
#include <cmath>

namespace environment_monitor {
struct Sample {
    bool valid=false;
    uint32_t timestamp_ms=0;
    float temperature_c=NAN, humidity_percent=NAN, pressure_bar=NAN;
    uint8_t address=0;
};
class Monitor {
public:
    explicit Monitor(system_i2c::Bus &bus) : bus_(bus) {}
    Sample poll();
private:
    static BME280_INTF_RET_TYPE read(uint8_t reg, uint8_t *data, uint32_t len, void *context);
    static BME280_INTF_RET_TYPE write(uint8_t reg, const uint8_t *data, uint32_t len, void *context);
    static void delay(uint32_t us, void *context);
    bool initialize();
    bool configuration_matches();
    system_i2c::Bus &bus_;
    bme280_dev device_{};
    bme280_settings settings_{};
    uint8_t address_=0;
    bool ready_=false;
};
}
