#include "co_qualification.h"

namespace co_qualification {
void Monitor::interface_available(bool available,uint32_t now) {
    if(available==available_)return;
    available_=available;enabled_ms_=now;decoder_=Ze07CoDecoder{};
}
bool Monitor::push(uint8_t byte,uint32_t now) {
    return available_ && decoder_.push(byte,now);
}
Result Monitor::sample(uint32_t now,const Environment &env) const {
    Result out;
    if(!available_)out.faults|=InterfaceUnavailable;
    else if(uint32_t(now-enabled_ms_)<kWarmupMs)out.faults|=Warming;
    float ppm=NAN;
    if(!decoder_.latest(now,ppm))out.faults|=FrameUnavailable;
    if(!env.valid)out.faults|=EnvironmentUnavailable;
    else {
        if(uint32_t(now-env.timestamp_ms)>kEnvironmentMaxAgeMs)out.faults|=EnvironmentStale;
        if(!std::isfinite(env.temperature_c) || !std::isfinite(env.humidity_percent))out.faults|=EnvironmentNonfinite;
        else {
            if(env.temperature_c < -10.0f || env.temperature_c > 55.0f)out.faults|=TemperatureRange;
            if(env.humidity_percent < 15.0f || env.humidity_percent > 90.0f)out.faults|=HumidityRange;
        }
    }
    out.valid=out.faults==0;
    if(out.valid)out.ppm=ppm;
    return out;
}
const char *fault_message(uint32_t faults) {
    if(faults&InterfaceUnavailable)return "CO UART interface unavailable";
    if(faults&Warming)return "CO warm-up incomplete";
    if(faults&FrameUnavailable)return "CO frame missing or stale";
    if(faults&EnvironmentUnavailable)return "CO chamber environment unavailable";
    if(faults&EnvironmentStale)return "CO chamber environment stale";
    if(faults&EnvironmentNonfinite)return "CO chamber environment invalid";
    if(faults&TemperatureRange)return "CO temperature outside -10..55 C";
    if(faults&HumidityRange)return "CO humidity outside 15..90 percent RH";
    return faults ? "CO qualification fault":"CO operating checks passed; physical qualification pending";
}
}
