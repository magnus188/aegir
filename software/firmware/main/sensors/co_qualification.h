#pragma once
#include "ze07_co.h"
#include <cmath>
#include <cstdint>

namespace co_qualification {
// Winsen ZE07-CO manual V1.7. These operating checks do not establish
// noncondensing conditions, correct supply, gas compatibility or safety use.
constexpr uint32_t kWarmupMs=300000, kEnvironmentMaxAgeMs=3000;
enum Fault : uint32_t {
    InterfaceUnavailable=1u<<0, Warming=1u<<1, FrameUnavailable=1u<<2,
    EnvironmentUnavailable=1u<<3, EnvironmentStale=1u<<4,
    EnvironmentNonfinite=1u<<5, TemperatureRange=1u<<6, HumidityRange=1u<<7
};
struct Environment {
    bool valid=false;
    uint32_t timestamp_ms=0;
    float temperature_c=NAN, humidity_percent=NAN;
};
struct Result {
    bool valid=false;
    float ppm=NAN;
    uint32_t faults=0;
};
class Monitor {
public:
    // GPIO51 enables UART translation, not module power. Starting the full
    // wait here is conservative; it does not verify the module's supply.
    void interface_available(bool available,uint32_t now_ms);
    bool push(uint8_t byte,uint32_t now_ms);
    Result sample(uint32_t now_ms,const Environment &environment) const;
private:
    Ze07CoDecoder decoder_;
    bool available_=false;
    uint32_t enabled_ms_=0;
};
const char *fault_message(uint32_t faults);
}
