#include "services/charge_profile.h"
#include "services/charge_standby_policy.h"
#include "sensors/power_monitor.h"
#include <array>
#include <vector>
#include <tuple>
#include <iostream>
#include <functional>
#include <cstring>
unsigned passed=0,failed=0;
void check(bool ok,const char *name) {std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';ok?++passed:++failed;}
// Synthetic register fixture, NOT a recommendation or physical cell profile.
charge_profile::Profile fake_profile() {
    charge_profile::Profile p;p.bench_qualified=true;p.qualification_record=0x54455354;
    p.regulation_mv=4192;p.fast_ma=512;p.precharge_ma=64;p.termination_ma=64;
    p.precharge_threshold_mv=3000;p.recharge_mv=100;p.safety_hours=5;p.thermal_regulation_c=60;
    p.standby_min_mv=3400;p.standby_min_soc=10;return p;
}
struct Bus:system_i2c::Bus {
    std::array<uint8_t,256> r{};uint32_t now=1000;
    unsigned writes=0,fail_write=0,ignore_write=0;bool disconnected=false;
    uint32_t transaction_ms=0;std::function<void()> after_write;
    std::vector<std::pair<uint8_t,uint8_t>> written;
    Bus() {r[0x14]=0x39;r[3]=0x1a;r[0]=0x48;r[2]=0x1d;r[4]=0x20;r[5]=0x13;r[6]=0x5e;r[7]=0x9d;r[8]=3;r[9]=0x40;r[0xb]=4;}
    bool write(uint8_t a,const uint8_t *d,size_t n) override {
        if(disconnected || a!=0x6a || n!=2)return false;
        now+=transaction_ms;
        ++writes;written.push_back({d[0],d[1]});if(writes==fail_write)return false;
        if(writes!=ignore_write)r[d[0]]=d[1];if(after_write)after_write();return true;
    }
    bool read(uint8_t,uint8_t*,size_t) override {return false;}
    bool read_register(uint8_t a,uint8_t reg,uint8_t *out,size_t n) override {
        if(disconnected)return false;
        now+=transaction_ms;
        if(a==0x6a && n==1){*out=r[reg];return true;}
        if(a==0x36 && n==2) {
            uint16_t v=reg==8?0x12:reg==2?51200:reg==4?12800:0;
            out[0]=v>>8;out[1]=v&255;return true;
        }
        return false;
    }
    uint32_t now_ms() override{return now;}
    void delay_ms(uint32_t ms) override{now+=ms;}
};
void ready(Bus &b,power_monitor::Monitor &m) {m.poll();check(m.set_input_enabled(true) && m.set_input_limit_ma(1400),"Fixture recognized source clears HIZ then grants1400mA register ceiling");m.poll();b.writes=0;b.written.clear();}
int main() {
    using namespace charge_profile;Registers regs;
    check(!active().bench_qualified && !active().qualification_record && !encode(active(),regs),"Production immutable profile is inhibited; no fake qualification shipped");
    auto p=fake_profile();check(encode(p,regs) && regs.r4==8 && regs.r5==0 && regs.r6==0x5a && regs.r7==0x88 && regs.r8==0 && regs.r9==0,"Independent expected BQ register bytes for synthetic test profile");
    for(const auto &mutate:std::vector<std::function<void(Profile&)>>{
        [](Profile &x){x.schema=2;},[](Profile &x){x.revision=0;},[](Profile &x){x.qualification_record=0;},
        [](Profile &x){x.bench_qualified=false;},[](Profile &x){x.regulation_mv=4200;},
        [](Profile &x){x.fast_ma=100;},[](Profile &x){x.precharge_ma=63;},
        [](Profile &x){x.termination_ma=x.fast_ma;},[](Profile &x){x.safety_hours=0;},
        [](Profile &x){x.thermal_regulation_c=70;},[](Profile &x){x.recharge_mv=0;},
        [](Profile &x){x.precharge_threshold_mv=2900;},[](Profile &x){x.standby_min_mv=3000;},
        [](Profile &x){x.standby_min_soc=0;}}) {
        auto invalid=p;mutate(invalid);check(!encode(invalid,regs),"Invalid/incomplete/future-schema profile rejected");
    }
    Bus production;power_monitor::Monitor inhibit(production);ready(production,inhibit);
    check(!inhibit.request_charging(true) && !(production.r[3]&0x30) && inhibit.charge_inhibited(),"Default runtime cannot enable charging even after source grant");
    Bus b;power_monitor::Monitor monitor(b,p);ready(b,monitor);
    check(monitor.request_charging(true) && (b.r[3]&0x30)==0x10 && !monitor.charge_inhibited(),"Synthetic qualified profile enables only after inhibit/program/readback");
    check((b.written.front().second&0x30)==0 && b.written.back()==std::make_pair(uint8_t(3),uint8_t(0x1a)),"Charge inhibit is first write and enable is last write");
    auto enabled=monitor.poll();check(enabled.charge_enabled && enabled.charge_profile_revision==1 && !enabled.charge_fault_latched,"Applied version and verified charge state are reported separately from charging status");
    unsigned stable_writes=b.writes;
    for(int i=0;i<1000;++i){b.now+=500;monitor.poll();if(!monitor.request_charging(true))++failed;}
    check(b.writes==stable_writes,"1000 stable enabled polls do not rewrite charge settings or restart timers");
    check(monitor.request_charging(false) && !(b.r[3]&0x30) && monitor.charge_inhibited(),"Verified charge inhibit on exit");
    const unsigned stopped_writes=b.writes;monitor.request_charging(false);
    check(b.writes==stopped_writes,"Repeated inhibit preserves a healthy stopped configuration");
    for(unsigned i=1;i<=8;++i) {
        Bus fault;power_monitor::Monitor m(fault,p);ready(fault,m);fault.fail_write=i;
        check(!m.request_charging(true) && !(fault.r[3]&0x30) && m.charge_inhibited(),"Each programming write failure rolls back to verified charge inhibit");
        auto result=m.poll();check(result.charge_fault_latched && !m.request_charging(true),"Failed profile cannot silently re-enable on next clean poll");
    }
    for(unsigned i=2;i<=8;++i) {
        Bus ignored;power_monitor::Monitor m(ignored,p);ready(ignored,m);ignored.ignore_write=i;
        check(!m.request_charging(true) && !(ignored.r[3]&0x30),"Ignored profile/enable write rejected by readback");
    }
    Bus stale;power_monitor::Monitor sm(stale,p);ready(stale,sm);stale.now+=3001;
    check(!sm.request_charging(true) && sm.charge_inhibited(),"Stale power sample cannot enable charge");
    Bus low;power_monitor::Monitor lm(low,p);lm.poll();
    check(!lm.request_charging(true) && !(low.r[3]&0x30),"100mA register ceiling cannot enable standby charging");
    Bus shipping;power_monitor::Monitor ship(shipping,p);ready(shipping,ship);shipping.r[9]|=0x20;
    check(!ship.request_charging(true) && (shipping.r[9]&0x20),"BATFET protection/ship latch is never cleared to force charging");
    Bus reset;power_monitor::Monitor rm(reset,p);ready(reset,rm);rm.request_charging(true);reset.r[6]^=4;
    auto result=rm.poll();check(result.charge_fault_latched && !result.charge_enabled && !(reset.r[3]&0x30),"Unexpected profile mutation inhibits and latches rather than automatic re-enable");
    Bus timer;power_monitor::Monitor tm(timer,p);ready(timer,tm);tm.request_charging(true);timer.r[0xc]=0x30;
    result=tm.poll();timer.r[0xc]=0;tm.poll();check(result.charge_fault_latched && !tm.request_charging(true),"Safety timer fault remains latched after status clears");
    Bus cold_fault;cold_fault.r[0xc]=0x30;power_monitor::Monitor cf(cold_fault,p);
    result=cf.poll();cold_fault.r[0xc]=0;ready(cold_fault,cf);
    check(result.charge_fault_latched && !cf.request_charging(true),"Timer fault seen before first profile application cannot be cleared by a new charge session");
    Bus drift;power_monitor::Monitor dm(drift,p);ready(drift,dm);drift.r[2]|=1;
    check(!dm.request_charging(true) && !(drift.r[3]&0x30),"Register drift between sample and enable rejects charging");
    check(dm.poll().charge_fault_latched && !dm.request_charging(true),"Pre-enable register drift remains latched on next healthy poll");
    for(unsigned i=1;i<=8;++i) {
        Bus cancelled;power_monitor::Monitor cm(cancelled,p);ready(cancelled,cm);bool allowed=true;
        cancelled.after_write=[&]{if(cancelled.writes==i)allowed=false;};
        check(!cm.request_charging(true,[](void *v){return *static_cast<bool *>(v);},&allowed) &&
              cm.charge_inhibited() && !(cancelled.r[3]&0x30),"Cancellation at each charge-programming write finishes in verified inhibit");
    }
    Bus slow;power_monitor::Monitor sl(slow,p);ready(slow,sl);slow.transaction_ms=90;auto began=slow.now;
    check(!sl.request_charging(true) && sl.charge_inhibited() && !(slow.r[3]&0x30) && slow.now-began<2200,
          "Successful but slow I2C traffic cannot grant charge after programming deadline");
    Bus lost;power_monitor::Monitor gone(lost,p);ready(lost,gone);gone.request_charging(true);lost.disconnected=true;
    check(!gone.request_charging(false) && !gone.charge_inhibited(),"Lost bus never claims a verified electrical charging disable");
    lost.disconnected=false;result=gone.poll();check(result.charge_fault_latched && !result.charge_enabled && !(lost.r[3]&0x30),"Communication recovery stays inhibited and retains charge failure");

    using namespace charge_standby;
    auto good=[] {Inputs i;i.now_ms=100;i.startup_accepted=i.profile_eligible=i.power_healthy=i.usb_present=i.source_qualified=i.battery_entry_ok=i.battery_continue_ok=true;return i;};
    Policy mode;auto in=good();in.button=true;
    check(mode.step(in)==Action::Enter && !mode.measurements_expected(),"Normal short press enters preparing state with acquisition suppressed");
    mode.complete(Action::Enter,true,120);in.button=false;in.now_ms=121;in.source_qualified=false;
    check(mode.step(in)==Action::None && mode.mode()==Mode::Qualifying,"Transition releases maintenance then waits for fresh source qualification");
    in.now_ms=500;in.source_qualified=true;check(mode.step(in)==Action::EnableCharge,"Fresh source requests explicit charge enable");
    mode.complete(Action::EnableCharge,true,550);check(mode.mode()==Mode::Standby && !mode.measurements_expected(),"Successful charge-only mode keeps acquisition suppressed");
    for(int i=0;i<1000;++i){in.now_ms+=500;if(mode.step(in)!=Action::None)++failed;}
    check(mode.mode()==Mode::Standby,"Stable standby does not cycle current permission or restart measurements");
    in.button=true;check(mode.step(in)==Action::Wake && !mode.measurements_expected(),"Wake remains suppressed until transition succeeds");
    mode.complete(Action::Wake,true,in.now_ms+50);check(mode.measurements_expected(),"Successful wake restores normal acquisition expectation");
    Policy probation;auto waiting=good();waiting.startup_accepted=false;
    for(unsigned i=0;i<1000;++i){waiting.now_ms+=500;if(probation.step(waiting)!=Action::None)++failed;}
    check(probation.measurements_expected(),"Waiting startup health keeps acquisition running and never enters standby by itself");
    waiting.maintenance=true;waiting.button=true;
    check(probation.step(waiting)==Action::None && probation.measurements_expected(),"Short press cannot steal maintenance ownership before startup acceptance");
    waiting.maintenance=false;waiting.startup_accepted=true;
    check(probation.step(waiting)==Action::Enter,"Explicit startup acceptance permits the otherwise qualified transition");
    for(const auto &mutate:std::vector<std::function<void(Inputs&)>>{
        [](Inputs &i){i.startup_accepted=false;},[](Inputs &i){i.profile_eligible=false;},
        [](Inputs &i){i.power_healthy=false;},[](Inputs &i){i.usb_present=false;},
        [](Inputs &i){i.source_qualified=false;},[](Inputs &i){i.source_fault=true;},
        [](Inputs &i){i.battery_entry_ok=false;}}) {
        Policy m;auto x=good();x.button=true;mutate(x);check(m.step(x)==Action::Off,"Ineligible standby entry keeps ordinary true-off behavior");
    }
    auto enter=[&](Policy &m,Inputs &x){x.button=true;m.step(x);m.complete(Action::Enter,true,x.now_ms+10);x.button=false;x.now_ms+=20;m.step(x);m.complete(Action::EnableCharge,true,x.now_ms+10);x.now_ms+=20;};
    for(const auto &mutate:std::vector<std::function<void(Inputs&)>>{
        [](Inputs &i){i.power_healthy=false;},[](Inputs &i){i.usb_present=false;},
        [](Inputs &i){i.source_qualified=false;},[](Inputs &i){i.source_fault=true;},
        [](Inputs &i){i.profile_eligible=false;},[](Inputs &i){i.battery_continue_ok=false;},
        [](Inputs &i){i.maintenance=true;}}) {
        Policy m;auto x=good();enter(m,x);mutate(x);
        check(m.step(x)==Action::Off && m.mode()==Mode::OffPending,"Standby detach/downgrade/latched-source/fault/low/stale/maintenance requests true off");
        x.now_ms+=500;check(m.step(x)==Action::None,"Failed off request is rate-limited");
        x.now_ms+=500;check(m.step(x)==Action::Off,"Failed off request is retried without restoring acquisition");
    }
    for(auto a:{Action::Enter,Action::EnableCharge,Action::Wake}) {
        Policy m;auto x=good();x.button=true;m.step(x);
        if(a!=Action::Enter){m.complete(Action::Enter,true,110);x.button=false;x.now_ms=120;m.step(x);}
        if(a==Action::Wake){m.complete(Action::EnableCharge,true,130);x.button=true;x.now_ms=140;m.step(x);}
        m.complete(a,false,150);check(m.mode()==Mode::OffPending && !m.measurements_expected(),"Failed transition or charge write never restores a running sensor mode");
    }
    Policy wait;in=good();in.button=true;wait.step(in);wait.complete(Action::Enter,true,110);
    in.button=false;in.source_qualified=false;in.now_ms=3110;check(wait.step(in)==Action::Off,"Source requalification timeout goes to off");
    Policy hung;in=good();in.button=true;hung.step(in);in.now_ms+=3000;
    check(hung.step(in)==Action::Off,"Unfinished transition has bounded deadline");hung.complete(Action::Enter,true,in.now_ms);
    check(hung.mode()==Mode::OffPending,"Late transition success cannot undo timeout");
    Policy wrap;in=good();in.now_ms=0xfffffff0;in.button=true;wrap.step(in);wrap.complete(Action::Enter,true,0x10);
    in.button=false;in.source_qualified=false;in.now_ms=0x20;check(wrap.step(in)==Action::None,"Transition timing is correct across uint32 wrap");
    in.now_ms=0x10+3000;check(wrap.step(in)==Action::Off,"Requalification timeout remains bounded across wrap");
    Policy external;in=good();in.force_off=true;
    check(external.step(in)==Action::Off,"Critical or external shutdown enters persistent off policy");
    external.complete(Action::Off,true,in.now_ms);in.force_off=false;in.button=true;in.now_ms+=1000;
    check(external.step(in)==Action::Off && !external.measurements_expected(),"KILL returning successfully never restarts host kept alive by external service power");
    std::cout<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;
}
