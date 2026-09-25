#pragma once
#include <cstdint>
namespace charge_profile {
// This structure is source-controlled, never loaded from UI/NVS. Encoding
// validation is not cell qualification. The production provider is inhibited.
struct Profile {
    uint16_t schema=1;
    uint32_t revision=1, qualification_record=0;
    bool bench_qualified=false;
    uint16_t regulation_mv=0, fast_ma=0, precharge_ma=0, termination_ma=0;
    uint16_t precharge_threshold_mv=0, recharge_mv=0;
    uint8_t safety_hours=0, thermal_regulation_c=0;
    bool double_timer_during_dpm=false;
    uint16_t standby_min_mv=0; // Entry additionally requires100mV margin.
    uint8_t standby_min_soc=0;
};
struct Registers { uint8_t r4=0,r5=0,r6=0,r7=0,r8=0,r9=0; };
const Profile &active();
bool encode(const Profile &profile, Registers &out);
}
