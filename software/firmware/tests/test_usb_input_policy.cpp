#include "sensors/usb_input_policy.h"
#include <cassert>
#include <functional>
#include <iostream>
#include <vector>

struct IO:usb_input::Actions {
    uint32_t now=1000,acked_at=0,permission_revision=0;
    bool high=false,fail_limit=false,fail_ack=false,deny_high=false,deny_low=false,fail_stop=false;
    bool hardware_q=false,hardware_reset=false,feedback_stuck_low=false;
    bool pg_can_start=true,fail_isolate=false,fail_enable=false;
    uint32_t enabled_at=0,pg_delay=0;
    unsigned input_enables=0,input_isolations=0;
    uint32_t irq_started=0,irq_duration=0;
    unsigned edges=0,bc_begins=0,bc_stops=0,cc_reads=0,cc_acks=0;
    bool mutated_high=false;
    power_monitor::Sample p;
    usb_source::Sample c;
    bc12::Sample b;
    bc12::Port detected=bc12::Port::DCP;
    std::function<void(IO &)> during_delay,after_limit;
    std::function<void(IO &)> after_high;
    std::function<void(IO &)> after_input_enable;
    std::function<void(IO &)> after_feedback_sample;
    std::function<void(IO &,uint32_t)> on_delay;
    IO() { p.charger_valid=p.charger_configured=true;p.input_path=power_monitor::InputPath::Isolated;
        c.valid=c.attached_sink=c.change_latched=true;c.generation=1;c.current=usb_source::Current::A3; }
    power_monitor::Sample power() override{
        p.usb_power_good=p.input_path==power_monitor::InputPath::Enabled && pg_can_start && now-enabled_at>=pg_delay;
        return p;
    }
    usb_source::Sample cc() override{++cc_reads;return c;}
    bool configure_cc() override{mutated_high|=high;return false;}
    bool acknowledge_cc(const usb_source::Sample &,usb_source::Sample &out) override {
        ++cc_acks;mutated_high|=high;if(fail_ack)return false;c.change_latched=false;out=c;acked_at=now;return true;
    }
    bc12::Sample bc(uint32_t) override{return b;}
    bool begin_bc(uint32_t gen) override {
        assert(!c.change_latched && cc_acks>0);
        mutated_high|=high;++bc_begins;b={};b.status=bc12::Status::Detecting;b.source_generation=gen;return true;
    }
    bc12::Sample collect_bc(uint32_t gen) override{
        mutated_high|=high;b.status=bc12::Status::Ready;b.valid=true;b.port=detected;b.source_generation=gen;return b;
    }
    void invalidate_bc() override{b={};}
    bool stop_bc() override{mutated_high|=high;++bc_stops;b={};return !fail_stop;}
    bool current_limit(uint16_t ma) override{
        mutated_high|=high;if(fail_limit)return false;p.input_limit_ma=ma;
        if(ma==1400 && after_limit)after_limit(*this);return true;
    }
    bool input_enabled(bool enabled) override {
        mutated_high|=high;p.input_enable_command=enabled;
        if((enabled&&fail_enable)||(!enabled&&fail_isolate))return false;
        p.input_path=enabled ? power_monitor::InputPath::Enabled:power_monitor::InputPath::Isolated;
        if(enabled) {++input_enables;enabled_at=now;if(after_input_enable)after_input_enable(*this);}
        else {++input_isolations;p.input_limit_ma=100;}
        return true;
    }
    bool permission(bool enabled) override{
        if(enabled){assert(now-acked_at>=40);if(deny_high)return false;
            if(!high){++edges;if(!hardware_reset)hardware_q=true;}}
        else if(deny_low)return false;
        else ++permission_revision;
        high=enabled;if(enabled && after_high)after_high(*this);return true;
    }
    uint32_t permission_epoch() override{return permission_revision;}
    bool permission_feedback_high() override {
        const bool sensed=hardware_q && !hardware_reset && !feedback_stuck_low &&
                          !(irq_duration && now-irq_started<irq_duration);
        if(after_feedback_sample)after_feedback_sample(*this);
        return sensed;
    }
    uint32_t now_ms() override{return now;}
    void delay_ms(uint32_t ms) override{
        now+=ms;if(during_delay)during_delay(*this);if(on_delay)on_delay(*this,ms);
    }
};
int main(){
    using namespace usb_input;
    IO io;Policy policy(io);auto result=policy.step(false);
    assert(result.state==State::Qualified && io.high && io.edges==1 && io.p.input_limit_ma==1400 && !io.mutated_high);
    for(int i=0;i<1000;++i){io.now+=500;result=policy.step(false);assert(result.state==State::Qualified && io.edges==1);}
    result=policy.step(true);assert(result.state==State::Maintenance && !io.high && io.p.input_limit_ma==100);
    result=policy.step(false);assert(io.high && io.edges==2 && !io.mutated_high);
    io.c.current=usb_source::Current::UsbDefault;++io.c.generation;io.c.change_latched=true;
    result=policy.step(false);assert(!io.high && io.p.input_limit_ma==100 && result.state==State::Checking);
    result=policy.step(false);assert(io.high && result.state==State::Qualified && io.bc_begins==1 && !io.mutated_high);
    io.b.status=bc12::Status::Fault;io.b.valid=false;io.now+=30000;
    result=policy.step(false);assert(!io.high && result.state==State::Checking && io.bc_begins==2);
    for(auto port:{bc12::Port::SDP,bc12::Port::Proprietary,bc12::Port::Unknown}){
        IO host;host.c.current=usb_source::Current::UsbDefault;host.detected=port;
        Policy safe(host);safe.step(false);auto out=safe.step(false);
        assert(!host.high && host.p.input_limit_ma==100 && out.state==State::UsbDefault && !host.mutated_high);
    }
    for(auto port:{bc12::Port::CDP,bc12::Port::DCP}){
        IO adapter;adapter.c.current=usb_source::Current::UsbDefault;adapter.detected=port;
        Policy safe(adapter);safe.step(false);auto out=safe.step(false);assert(adapter.high && out.requested_limit_ma==1400);
        adapter.c.attached_sink=false;out=safe.step(false);assert(!adapter.high && out.state==State::Detached);
    }
    IO changed;changed.during_delay=[](IO &x){++x.c.generation;x.c.change_latched=true;};Policy reject(changed);
    result=reject.step(false);assert(result.state==State::Fault && !changed.high && changed.p.input_limit_ma==100);
    IO late;late.after_limit=[](IO &x){++x.c.generation;x.c.change_latched=true;};Policy reject_late(late);
    result=reject_late.step(false);assert(!late.high && late.p.input_limit_ma==100);
    IO blocked;blocked.deny_high=true;Policy stop(blocked);result=stop.step(false);
    assert(!blocked.high && blocked.p.input_limit_ma==100 && result.state==State::Fault);
    IO failed;failed.fail_limit=true;Policy bad_write(failed);result=bad_write.step(false);
    assert(!failed.high && result.state==State::Fault);
    IO stuck;stuck.fail_ack=true;Policy no_ack(stuck);result=no_ack.step(false);assert(!stuck.high && !stuck.mutated_high);
    IO fault;Policy current(fault);current.step(false);fault.p.faults_current=8;result=current.step(false);
    assert(!fault.high && fault.p.input_limit_ma==100 && result.state==State::Fault);
    IO lost;Policy transport(lost);transport.step(false);lost.c.valid=false;result=transport.step(false);
    assert(!lost.high && lost.p.input_limit_ma==100);
    IO wrap;wrap.now=0xfffffff0;Policy rollover(wrap);result=rollover.step(false);assert(wrap.high && !wrap.mutated_high);
    std::cout<<"USB current policy sequencing, source changes, failures, maintenance and1000 stable polls passed\n";
    unsigned extra_pass=0,extra_fail=0;
    auto check=[&](bool good,const char *name){
        std::cout<<(good ? "PASS ":"FAIL ")<<name<<'\n';good ? ++extra_pass:++extra_fail;
    };
    IO legacy_ack_failure;legacy_ack_failure.c.current=usb_source::Current::UsbDefault;
    legacy_ack_failure.fail_ack=true;Policy failed_detection_ack(legacy_ack_failure);
    auto ack_result=failed_detection_ack.step(false);
    check(ack_result.state==State::Fault && legacy_ack_failure.bc_begins==0 &&
          !legacy_ack_failure.high && legacy_ack_failure.p.input_path==power_monitor::InputPath::Isolated,
          "A failed pre-detection CC acknowledgment cannot begin BC or authorize input");
    IO legacy_ack_once;legacy_ack_once.c.current=usb_source::Current::UsbDefault;
    Policy single_ack(legacy_ack_once);single_ack.step(false);
    const unsigned detection_acks=legacy_ack_once.cc_acks;
    check(single_ack.step(false).state==State::Qualified && detection_acks==1 && legacy_ack_once.cc_acks==detection_acks,
          "A completed BC capability is validated without another destructive CC acknowledgment");
    IO late_pg;late_pg.after_limit=[](IO &x){x.pg_can_start=false;};Policy lost_pg(late_pg);
    auto pg_result=lost_pg.step(false);
    check(!late_pg.high && pg_result.state!=State::Qualified,
          "Power-good lost during the input-limit transaction prevents final permission");
    IO late_fault;late_fault.after_limit=[](IO &x){x.p.faults_current=0x10;};Policy new_fault(late_fault);
    auto fault_result=new_fault.step(false);
    check(!late_fault.high && fault_result.state==State::Fault,
          "New charger fault after initial observation prevents final permission");
    IO late_reset;late_reset.after_limit=[](IO &x){x.p.charger_configured=false;x.p.input_limit_ma=100;};Policy reset(late_reset);
    auto reset_result=reset.step(false);
    check(!late_reset.high && reset_result.state==State::Fault,
          "Charger reset/readback invalidation after the write prevents final permission");
    IO stale_bc;stale_bc.c.current=usb_source::Current::UsbDefault;
    stale_bc.after_limit=[](IO &x){x.b.status=bc12::Status::Fault;x.b.faults=bc12::Stale;x.b.valid=false;};
    Policy expires(stale_bc);expires.step(false);auto expiry_result=expires.step(false);
    check(!stale_bc.high && expiry_result.state!=State::Qualified,
          "BC1.2 result that expires during input-limit programming is rechecked before permission");
    IO cc_detached;cc_detached.c.current=usb_source::Current::UsbDefault;Policy unplug(cc_detached);
    unplug.step(false);cc_detached.c.attached_sink=false;++cc_detached.c.generation;
    auto detach_result=unplug.step(false);
    check(!cc_detached.high && cc_detached.bc_stops==1 && detach_result.state==State::Detached,
          "CC detach powers down an active BC1.2 cycle even while BQ power-good is still high");
    IO low_failure;Policy cannot_lower(low_failure);cannot_lower.step(false);
    const auto old_edges=low_failure.edges;
    const auto old_acks=low_failure.cc_acks;
    low_failure.deny_low=true;low_failure.c.change_latched=true;++low_failure.c.generation;
    auto low_result=cannot_lower.step(false);
    check(low_result.state==State::Fault && low_failure.edges==old_edges && low_failure.cc_acks==old_acks,
          "Failed LOW command never acknowledges CC or attempts another HIGH edge");
    IO late_source;late_source.c.current=usb_source::Current::UsbDefault;Policy old_source(late_source);
    old_source.step(false);late_source.after_limit=[](IO &x){++x.c.generation;x.c.change_latched=true;};
    check(old_source.step(false).state==State::Fault && !late_source.high,
          "Fresh DCP classification cannot survive a later CC generation change");
    IO native_after_bc;native_after_bc.c.current=usb_source::Current::UsbDefault;Policy transition(native_after_bc);
    transition.step(false);native_after_bc.c.current=usb_source::Current::A1_5;++native_after_bc.c.generation;
    auto transition_result=transition.step(false);
    check(native_after_bc.bc_stops==1 && transition_result.state==State::Qualified && !native_after_bc.mutated_high,
          "Switching from pending BC1.2 to native CC current stops the detector while LOW");
    IO failed_stop;failed_stop.c.current=usb_source::Current::UsbDefault;Policy stop_error(failed_stop);
    stop_error.step(false);failed_stop.c.current=usb_source::Current::A1_5;++failed_stop.c.generation;failed_stop.fail_stop=true;
    auto stop_result=stop_error.step(false);
    check(!failed_stop.high && stop_result.state==State::Fault,
          "Failed BC1.2 power-down cannot precede a new native-current permission");
    usb_source::Sample default_cc;default_cc.valid=default_cc.attached_sink=true;
    default_cc.current=usb_source::Current::UsbDefault;default_cc.generation=10;
    bc12::Sample wrong_generation;wrong_generation.valid=true;wrong_generation.status=bc12::Status::Ready;
    wrong_generation.port=bc12::Port::DCP;wrong_generation.source_generation=9;
    check(Policy::budget(default_cc,wrong_generation)==100,
          "Budget refuses a valid DCP result belonging to another connection");
    IO missed_maintenance;Policy maintenance_gap(missed_maintenance);maintenance_gap.step(false);
    const auto pre_maintenance_acks=missed_maintenance.cc_acks;
    const auto pre_maintenance_edges=missed_maintenance.edges;
    // The GPIO wrapper may lower outside this worker, and a failed/cancelled
    // maintenance can finish before the policy observes step(true).
    missed_maintenance.high=false;++missed_maintenance.permission_revision;
    auto resumed=maintenance_gap.step(false);
    check(missed_maintenance.high && resumed.state==State::Qualified &&
          missed_maintenance.cc_acks==pre_maintenance_acks+1 && missed_maintenance.edges==pre_maintenance_edges+1 &&
          !missed_maintenance.mutated_high,
          "External LOW during an unobserved maintenance interval requires fresh ack, delay and permission edge");
    IO async_clear;Policy recover_hardware(async_clear);recover_hardware.step(false);
    const auto clear_edges=async_clear.edges,clear_acks=async_clear.cc_acks;
    // Q alone is reset: the software epoch, host command, CC status and BQ
    // power-good/current register all remain unchanged.
    async_clear.hardware_q=false;
    auto recovered=recover_hardware.step(false);
    check(recovered.state==State::Qualified && recovered.permission_command && recovered.permission_confirmed &&
          async_clear.hardware_q && async_clear.edges==clear_edges+1 && async_clear.cc_acks==clear_acks+1 &&
          !async_clear.mutated_high,
          "Hardware-only authorization loss requires a fresh LOW, source check, delay and successful new edge");
    IO short_irq;Policy pulse_tolerant(short_irq);pulse_tolerant.step(false);
    const auto irq_edges=short_irq.edges,irq_acks=short_irq.cc_acks;
    short_irq.irq_started=short_irq.now;short_irq.irq_duration=1; // Greater than nominal256us.
    auto irq_result=pulse_tolerant.step(false);
    check(irq_result.state==State::Qualified && irq_result.permission_confirmed &&
          short_irq.edges==irq_edges && short_irq.cc_acks==irq_acks,
          "Short charger interrupt while armed does not cycle healthy input permission");
    IO grant_irq;grant_irq.after_high=[](IO &x){x.irq_started=x.now;x.irq_duration=1;};
    Policy grant_pulse(grant_irq);auto grant_irq_result=grant_pulse.step(false);
    check(grant_irq_result.state==State::Qualified && grant_irq_result.permission_confirmed,
          "Coincident short charger interrupt releases during bounded post-grant settling");
    IO stuck_feedback;stuck_feedback.feedback_stuck_low=true;Policy detect_stuck(stuck_feedback);
    auto stuck_result=detect_stuck.step(false);
    check(stuck_result.state==State::Fault && !stuck_result.permission_confirmed &&
          !stuck_feedback.high && stuck_feedback.p.input_limit_ma==100,
          "A GPIO HIGH write without feedback confirmation returns Fault and restores LOW");
    IO reset_held;reset_held.hardware_reset=true;Policy no_latch(reset_held);
    bool rejected_all=true;
    for(unsigned i=0;i<20;++i){
        reset_held.now+=500;auto out=no_latch.step(false);
        rejected_all &= out.state==State::Fault && !out.permission_command && !out.permission_confirmed &&
                        !reset_held.high && !reset_held.hardware_q && reset_held.p.input_limit_ma==100;
    }
    check(rejected_all && !reset_held.mutated_high,
          "Repeated failed hardware rearm attempts remain LOW and never report Qualified");
    reset_held.hardware_reset=false;auto retry_ok=no_latch.step(false);
    check(retry_ok.state==State::Qualified && retry_ok.permission_confirmed && reset_held.hardware_q,
          "After persistent reset releases a new qualification attempt can recover");
    IO grant_clear;grant_clear.after_high=[](IO &x){x.hardware_q=false;};Policy clear_at_edge(grant_clear);
    auto edge_clear_result=clear_at_edge.step(false);
    check(edge_clear_result.state==State::Fault && !grant_clear.high && !edge_clear_result.permission_confirmed,
          "Hardware clear coincident with the new grant cannot be cached as authorization");
    IO maintenance_verify;
    maintenance_verify.on_delay=[](IO &x,uint32_t ms){
        if(ms==2 && x.high){x.high=false;++x.permission_revision;}
    };
    Policy interrupted_verify(maintenance_verify);auto maintenance_result=interrupted_verify.step(false);
    check(maintenance_result.state==State::Fault && !maintenance_verify.high &&
          !maintenance_result.permission_confirmed && maintenance_verify.p.input_limit_ma==100,
          "Maintenance LOW during post-grant verification invalidates the epoch despite latch Q staying high");
    IO maintenance_after_sample;
    maintenance_after_sample.after_feedback_sample=[](IO &x){if(x.high){x.high=false;++x.permission_revision;}};
    Policy interrupted_sample(maintenance_after_sample);auto interrupted_sample_result=interrupted_sample.step(false);
    check(interrupted_sample_result.state==State::Fault && !maintenance_after_sample.high &&
          !interrupted_sample_result.permission_confirmed,
          "Epoch is checked after the physical feedback sample so concurrent LOW invalidates old HIGH evidence");
    IO maintenance_pulse;Policy interrupted_pulse(maintenance_pulse);interrupted_pulse.step(false);
    maintenance_pulse.irq_started=maintenance_pulse.now;maintenance_pulse.irq_duration=1;
    maintenance_pulse.on_delay=[](IO &x,uint32_t ms){
        if(ms==2){x.high=false;++x.permission_revision;x.deny_high=true;}
    };
    auto maintenance_pulse_result=interrupted_pulse.step(false);
    check(maintenance_pulse_result.state==State::Fault && !maintenance_pulse.high &&
          !maintenance_pulse_result.permission_confirmed,
          "Maintenance during IRQ deglitch cannot reuse the old armed cache");
    IO feedback_wrap;Policy wrap_feedback(feedback_wrap);wrap_feedback.step(false);
    feedback_wrap.now=0xffffffffu;feedback_wrap.irq_started=feedback_wrap.now;feedback_wrap.irq_duration=1;
    auto wrapped_irq=wrap_feedback.step(false);
    feedback_wrap.hardware_q=false;auto wrapped_clear=wrap_feedback.step(false);
    check(wrapped_irq.state==State::Qualified && wrapped_irq.permission_confirmed &&
          wrapped_clear.state==State::Qualified && wrapped_clear.permission_confirmed && !feedback_wrap.mutated_high,
          "IRQ deglitch and hardware-clear recovery work across millisecond timer wrap");
    IO confirmed_stable;Policy stable_feedback(confirmed_stable);stable_feedback.step(false);
    const auto stable_edges=confirmed_stable.edges,stable_acks=confirmed_stable.cc_acks;
    bool stable_all=true;
    for(unsigned i=0;i<1000;++i){confirmed_stable.now+=500;auto out=stable_feedback.step(false);
        stable_all &= out.state==State::Qualified && out.permission_confirmed;}
    check(stable_all && confirmed_stable.edges==stable_edges && confirmed_stable.cc_acks==stable_acks,
          "1000 healthy feedback polls retain authorization without periodic current interruptions");
    using Path=power_monitor::InputPath;
    IO hiz_default;hiz_default.c.current=usb_source::Current::UsbDefault;hiz_default.detected=bc12::Port::SDP;
    Policy isolated_sdp(hiz_default);auto detecting=isolated_sdp.step(false);auto sdp=isolated_sdp.step(false);
    check(detecting.state==State::Checking && sdp.state==State::UsbDefault && sdp.cc.attached_sink &&
          !hiz_default.p.usb_power_good && !sdp.power.input_enable_command && sdp.power.input_path==Path::Isolated &&
          hiz_default.input_enables==0 && !hiz_default.high,
          "Attached SDP is positively isolated even with PG low; cable attachment remains distinct");
    hiz_default.c.attached_sink=false;++hiz_default.c.generation;
    check(isolated_sdp.step(false).state==State::Detached,"Independent CC detach is still recognized while HIZ suppresses PG");
    hiz_default.c.attached_sink=true;++hiz_default.c.generation;hiz_default.c.change_latched=true;hiz_default.detected=bc12::Port::DCP;
    isolated_sdp.step(false);auto dcp_after_sdp=isolated_sdp.step(false);
    check(dcp_after_sdp.state==State::Qualified && dcp_after_sdp.power.input_enable_command &&
          dcp_after_sdp.power.input_path==Path::Enabled && hiz_default.input_enables==1,
          "Fresh DCP classification after isolated SDP can wake BQ and qualify without the PG-low deadlock");
    IO ignored_hiz;Policy ignored_isolation(ignored_hiz);ignored_isolation.step(false);ignored_hiz.fail_isolate=true;
    auto cannot_isolate=ignored_isolation.step(true);
    check(cannot_isolate.state==State::Fault && !ignored_hiz.high && !cannot_isolate.power.input_enable_command &&
          cannot_isolate.power.input_path==Path::Unknown && ignored_hiz.p.input_path==Path::Enabled,
          "Failed HIZ readback reports commanded isolation versus unknown actual mode and keeps AUTH LOW");
    IO ignored_wake;ignored_wake.fail_enable=true;Policy ignored_enable(ignored_wake);auto not_awake=ignored_enable.step(false);
    check(not_awake.state==State::Fault && !ignored_wake.high && not_awake.power.input_path==Path::Isolated,
          "Failed HIZ exit cannot grant permission and reasserts isolated input");
    IO delayed_pg;delayed_pg.pg_delay=2200;Policy delayed_start(delayed_pg);auto started_late=delayed_start.step(false);
    check(started_late.state==State::Qualified && delayed_pg.high && delayed_pg.now<4000,
          "Recognized source allows bounded PG startup beyond a poor-source retry interval");
    IO absent_pg;absent_pg.pg_can_start=false;Policy timeout_start(absent_pg);auto timed_out=timeout_start.step(false);
    check(timed_out.state==State::Fault && !absent_pg.high && timed_out.power.input_path==Path::Isolated &&
          absent_pg.now>=3500 && absent_pg.now<=3600,
          "Missing PG after a recognized source times out, restores HIZ and does not claim cable detach");
    IO wake_change;wake_change.pg_delay=100;wake_change.on_delay=[](IO &x,uint32_t ms){
        if(ms==20){x.c.current=usb_source::Current::UsbDefault;++x.c.generation;x.c.change_latched=true;}
    };Policy changes_during_wake(wake_change);auto lost_during_wake=changes_during_wake.step(false);
    check(lost_during_wake.state==State::Fault && !wake_change.high && lost_during_wake.power.input_path==Path::Isolated,
          "CC decrease while awaiting PG re-isolates before any HIGH permission");
    IO higher_despite_bc;higher_despite_bc.b.status=bc12::Status::Rejected;
    higher_despite_bc.b.port=bc12::Port::SDP;higher_despite_bc.c.current=usb_source::Current::A1_5;
    Policy native_priority(higher_despite_bc);auto native_ok=native_priority.step(false);
    check(native_ok.state==State::Qualified && higher_despite_bc.bc_begins==0,
          "Failed or SDP BC discovery never overrides valid native Type-C1.5A advertisement");
    higher_despite_bc.c.current=usb_source::Current::UsbDefault;++higher_despite_bc.c.generation;higher_despite_bc.c.change_latched=true;
    auto downgraded=native_priority.step(false);
    check(!higher_despite_bc.high && downgraded.power.input_path==Path::Isolated && downgraded.state==State::Checking,
          "Higher Type-C current downgraded to unclassified Default immediately returns to HIZ");
    IO input_drift;Policy drifted_mode(input_drift);drifted_mode.step(false);input_drift.p.input_path=Path::Isolated;
    auto recovered_mode=drifted_mode.step(false);
    check(recovered_mode.state==State::Qualified && input_drift.input_enables==2,
          "Unexpected HIZ readback cannot reuse an armed cache and requires complete requalification");
    IO wake_wrap;wake_wrap.now=0xffffff00;wake_wrap.pg_delay=2200;Policy long_wrap(wake_wrap);
    check(long_wrap.step(false).state==State::Qualified,"HIZ wake/PG deadline remains correct across uint32 timer wrap");
    IO waiting_maintenance;waiting_maintenance.pg_delay=2200;
    waiting_maintenance.on_delay=[](IO &x,uint32_t ms){if(ms==20){x.permission(false);x.fail_enable=true;}};
    Policy cancelled_wait(waiting_maintenance);auto cancelled_pg=cancelled_wait.step(false);
    check(cancelled_pg.state==State::Fault && cancelled_pg.power.input_path==Path::Isolated && !waiting_maintenance.high &&
          waiting_maintenance.now<1200,"Maintenance during PG wait restores HIZ without waiting for the full startup deadline");
    std::cout<<"Additional USB policy assertions: "<<extra_pass<<" passed, "<<extra_fail<<" failed\n";
    return extra_fail ? 1:0;
}
