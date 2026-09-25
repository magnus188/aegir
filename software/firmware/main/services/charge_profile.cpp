#include "charge_profile.h"
namespace charge_profile {
const Profile &active() {
    static constexpr Profile commissioning_inhibited{};
    return commissioning_inhibited;
}
bool encode(const Profile &p, Registers &out) {
    out={};
    if(p.schema!=1 || !p.revision || !p.qualification_record || !p.bench_qualified ||
       p.regulation_mv<3840 || p.regulation_mv>4608 || (p.regulation_mv-3840)%16 ||
       p.fast_ma<128 || p.fast_ma>5056 || p.fast_ma%64 ||
       p.precharge_ma<64 || p.precharge_ma>1024 || p.precharge_ma%64 ||
       p.termination_ma<64 || p.termination_ma>1024 || p.termination_ma%64 ||
       p.precharge_ma>p.fast_ma || p.termination_ma>=p.fast_ma ||
       (p.precharge_threshold_mv!=2800 && p.precharge_threshold_mv!=3000) ||
       (p.recharge_mv!=100 && p.recharge_mv!=200) ||
       p.standby_min_mv<3300 || p.standby_min_mv+100>=p.regulation_mv ||
       p.standby_min_soc<5 || p.standby_min_soc>90) return false;
    uint8_t timer=0,thermal=0;
    switch(p.safety_hours) {case 5:timer=0;break;case 8:timer=1;break;case 12:timer=2;break;case 20:timer=3;break;default:return false;}
    switch(p.thermal_regulation_c) {case 60:thermal=0;break;case 80:thermal=1;break;case 100:thermal=2;break;case 120:thermal=3;break;default:return false;}
    out.r4=static_cast<uint8_t>(p.fast_ma/64); // PUMPX off.
    out.r5=static_cast<uint8_t>(((p.precharge_ma/64-1)<<4)|(p.termination_ma/64-1));
    out.r6=static_cast<uint8_t>(((p.regulation_mv-3840)/16<<2)|
                             (p.precharge_threshold_mv==3000 ? 2:0)|(p.recharge_mv==200 ? 1:0));
    out.r7=static_cast<uint8_t>(0x88|(timer<<1)); // Termination+safety timer ON; watchdog OFF.
    out.r8=thermal; // IR compensation and clamp disabled.
    out.r9=p.double_timer_during_dpm ? 0x40:0;
    return true;
}
}
