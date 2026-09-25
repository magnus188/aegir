#include "usb_source_monitor.h"
#include <cstring>

namespace usb_source {
namespace {
constexpr uint8_t identity[8] = {0x30,0x32,0x33,0x42,0x53,0x55,0x54,0x00};
constexpr uint8_t kInterrupt = 0x10;
bool safe_configuration(uint8_t r08,uint8_t r09,uint8_t r0a,uint8_t r45,bool accessories_disabled) {
    // Keep hardware-strapped sink or explicit UFP; never accept source/DRP,
    // disabled terminations, reset in progress, or a nondefault source preference.
    const uint8_t mode = r0a & 0x3f;
    return !(r08 & 0xc0) && !(r09 & 0x08) &&
        (mode == 0 || mode == 0x10) && r45 == 0 &&
        (!accessories_disabled || (r09 & 1));
}
}
bool Monitor::decode(uint8_t r08,uint8_t r09,uint8_t r0a,uint8_t r45,Sample &out) {
    out = {};
    out.raw08=r08; out.raw09=r09; out.raw0a=r0a; out.raw45=r45;
    out.change_latched=(r09 & kInterrupt)!=0;
    out.orientation=(r09 >> 5) & 1;
    if (!safe_configuration(r08,r09,r0a,r45,true)) out.faults|=Configuration;
    const uint8_t role=(r09 >> 6) & 3, accessory=(r08 >> 1) & 7;
    const uint8_t current=(r08 >> 4) & 3;
    if (accessory || (role==2 && current==2)) out.faults|=UnsupportedAccessory;
    if (role==1 || role==3) out.faults|=UnsupportedRole;
    if (out.faults) return false;
    out.valid=true;
    if (role!=2) return true; // Detached is valid information, never a current grant.
    out.attached_sink=true;
    if (current==0) out.current=Current::UsbDefault;
    else if (current==1) { out.current=Current::A1_5; out.advertised_ma=1500; }
    else if (current==3) { out.current=Current::A3; out.advertised_ma=3000; }
    return true;
}
bool Monitor::read_snapshot(Sample &out) {
    out={};
    uint8_t id[8]{},rev=0,termination=0,a[3]{},b[3]{};
    if (!bus_.read_register(kAddress,0,id,sizeof(id)) ||
        !bus_.read_register(kAddress,0xa0,&rev,1)) { out.faults=Transport; return false; }
    out.revision=rev;
    if (std::memcmp(id,identity,sizeof(id)) || rev!=0x02) { out.faults=Identity; return false; }
    if (!bus_.read_register(kAddress,0x45,&termination,1) ||
        !bus_.read_register(kAddress,0x08,a,sizeof(a)) ||
        !bus_.read_register(kAddress,0x08,b,sizeof(b))) { out.faults=Transport; return false; }
    // State can change between bytes/transactions. Do not combine one source's
    // current advertisement with another source's attachment. IRQ is sticky and
    // may assert between snapshots without changing the final state.
    const bool coherent=a[0]==b[0] && ((a[1]^b[1]) & ~kInterrupt)==0 && a[2]==b[2];
    decode(b[0],static_cast<uint8_t>(b[1]|(a[1]&kInterrupt)),b[2],termination,out);
    out.revision=rev;
    if (!coherent) { out.faults|=Incoherent; out.valid=false; }
    if (!out.valid) { out.attached_sink=false; out.current=Current::Unknown; out.advertised_ma=0; }
    return out.valid;
}
bool Monitor::equivalent(const Sample &a,const Sample &b) {
    return a.valid==b.valid && a.faults==b.faults && a.revision==b.revision &&
        a.raw08==b.raw08 && ((a.raw09^b.raw09)&~kInterrupt)==0 &&
        a.raw0a==b.raw0a && a.raw45==b.raw45;
}
Sample Monitor::finish(Sample out) {
    if (!seen_ || !equivalent(out,previous_) || (out.change_latched && !previous_.change_latched)) {
        if (generation_!=UINT32_MAX) ++generation_;
        else { out.faults|=GenerationExhausted; out.valid=out.attached_sink=false; out.current=Current::Unknown; out.advertised_ma=0; }
    }
    out.timestamp_ms=bus_.now_ms(); out.generation=generation_;
    previous_=out; seen_=true;
    return out;
}
Sample Monitor::poll() { Sample out; read_snapshot(out); return finish(out); }
bool Monitor::initialize_disabled_policy() {
    Sample before; read_snapshot(before);
    // With accessories still enabled the snapshot may deliberately report a
    // Configuration fault, but its ID/config must be readable and sink-compatible.
    if ((before.faults & (Transport|Identity|Incoherent)) ||
        !safe_configuration(before.raw08,before.raw09,before.raw0a,before.raw45,false)) return false;
    if (!(before.raw09&1) && !bus_.write_register(kAddress,0x09,static_cast<uint8_t>((before.raw09&7)|1))) return false;
    const auto after=poll();
    return after.valid && (after.raw09&1);
}
bool Monitor::acknowledge_change(const Sample &expected,Sample &after) {
    const Sample requested=expected; // Permit caller to reuse one snapshot object.
    after=poll();
    if (!requested.valid || !after.valid || requested.generation!=after.generation || !equivalent(requested,after)) return false;
    const auto before=after;
    // Only the writable control bits are carried forward; bit4 is W1C. Never
    // echo role/orientation/read-only fields or clear interrupt implicitly in poll.
    if (!bus_.write_register(kAddress,0x09,static_cast<uint8_t>((before.raw09&7)|kInterrupt))) { after=poll(); return false; }
    after=poll();
    return after.valid && !after.change_latched && after.generation==before.generation && equivalent(before,after);
}
}
