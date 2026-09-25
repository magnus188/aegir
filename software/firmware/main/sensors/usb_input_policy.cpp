#include "usb_input_policy.h"

namespace usb_input {
uint16_t Policy::budget(const usb_source::Sample &cc,const bc12::Sample &bc) {
    if (!cc.valid || !cc.attached_sink) return 100;
    if (cc.current==usb_source::Current::A1_5 || cc.current==usb_source::Current::A3) return 1400;
    if (cc.current==usb_source::Current::UsbDefault && bc.valid && bc.status==bc12::Status::Ready &&
        bc.source_generation==cc.generation && (bc.port==bc12::Port::CDP || bc.port==bc12::Port::DCP)) return 1400;
    // No enumeration occurs. 100mA is only the register ceiling; unclassified,
    // SDP and proprietary inputs must stay in HIZ, not draw this indefinitely.
    return 100;
}
bool Policy::same_source(const usb_source::Sample &a,const usb_source::Sample &b) {
    return a.valid && b.valid && a.attached_sink && b.attached_sink &&
        a.generation==b.generation && a.current==b.current && a.orientation==b.orientation;
}
bool Policy::lower(Result &out) {
    armed_=false; out.permission_command=false; out.permission_confirmed=false; out.requested_limit_ma=100;
    const bool low=io_.permission(false);
    // Try electrical input isolation even if the GPIO LOW write failed. A
    // failed HIZ transaction must not be presented as confirmed isolation.
    const bool isolated=io_.input_enabled(false);
    out.power.input_enable_command=false;
    out.power.input_path=isolated ? power_monitor::InputPath::Isolated:power_monitor::InputPath::Unknown;
    if(isolated)out.power.input_limit_ma=100;
    return low && isolated;
}
bool Policy::stop_detection() {
    const bool stopped=!bc_running_ || io_.stop_bc();
    if (stopped) bc_running_=false;
    io_.invalidate_bc(); detected_attempt_=false;
    return stopped;
}
Result Policy::step(bool maintenance) {
    Result out;
    out.power=io_.power(); out.cc=io_.cc();
    if (maintenance) {
        const bool stopped=lower(out) && stop_detection();
        out.state=stopped ? State::Maintenance:State::Fault; return out;
    }
    if (!out.power.charger_valid || !out.power.charger_configured || out.power.faults_current) {
        if (lower(out)) stop_detection();
        out.state=State::Fault; return out;
    }
    if (!out.cc.valid) {
        if (!lower(out)) { out.state=State::Fault; return out; }
        io_.invalidate_bc(); detected_attempt_=false;
        if (!io_.configure_cc()) { out.state=State::Fault; return out; }
        out.cc=io_.cc();
    }
    if (!out.cc.valid || !out.cc.attached_sink) {
        const bool stopped=lower(out) && stop_detection();
        generation_=0;
        out.state=stopped && out.cc.valid ? State::Detached:State::Fault; return out;
    }
    if (out.cc.generation!=generation_) {
        if (!lower(out)) { out.state=State::Fault; return out; }
        io_.invalidate_bc(); detected_attempt_=false; generation_=out.cc.generation;
    }
    const bool legacy=out.cc.current==usb_source::Current::UsbDefault;
    if (legacy) out.bc=io_.bc(generation_);
    const uint16_t wanted=budget(out.cc,out.bc);
    if (armed_ && armed_epoch_==io_.permission_epoch() && !out.cc.change_latched &&
        wanted==1400 && out.power.input_limit_ma==1400 && out.power.usb_power_good &&
        out.power.input_enable_command && out.power.input_path==power_monitor::InputPath::Enabled) {
        // Q can clear asynchronously without changing CC/BQ registers or the
        // command epoch. Its inverted output holds the shared charger IRQ LOW
        // until a fresh authorization edge succeeds. A normal BQ IRQ is a
        // 256-us pulse; give it a short opportunity to release without cycling
        // a healthy source. Persistent LOW always takes the full LOW/recheck path.
        bool feedback=io_.permission_feedback_high();
        if (!feedback) { io_.delay_ms(2); feedback=io_.permission_feedback_high(); }
        if (feedback && armed_epoch_==io_.permission_epoch()) {
            out.state=State::Qualified; out.permission_command=true; out.permission_confirmed=true;
            out.requested_limit_ma=1400; return out;
        }
    }
    if (!lower(out)) { out.state=State::Fault; return out; }
    if (legacy && wanted==100) {
        if (out.bc.status==bc12::Status::Detecting) out.bc=io_.collect_bc(generation_);
        else if (out.bc.status!=bc12::Status::Ready && out.bc.status!=bc12::Status::Rejected &&
                 (!detected_attempt_ || io_.now_ms()-last_detection_ms_>=5000)) {
            detected_attempt_=true; last_detection_ms_=io_.now_ms();
            // Clear the current attachment event BEFORE starting one-shot BC
            // detection. Otherwise a second detach/replug with identical final
            // CC can hide under an already-set sticky bit and let the first
            // source's read-clear result authorize the replacement source.
            // Subsequent events now create a new CC generation. Ordinary poll
            // remains read-only; both this ACK and detection run with AUTH LOW.
            usb_source::Sample detection_source;
            if (!io_.acknowledge_cc(out.cc,detection_source) ||
                !same_source(out.cc,detection_source) || detection_source.change_latched) {
                io_.invalidate_bc(); out.bc=io_.bc(generation_);
                out.state=State::Fault; return out;
            }
            out.cc=detection_source;
            bc_running_=io_.begin_bc(generation_);
            out.bc=io_.bc(generation_);
        }
        if (budget(out.cc,out.bc)==100) {
            out.state=out.bc.status==bc12::Status::Detecting ? State::Checking:
                out.bc.status==bc12::Status::Fault ? State::Fault:State::UsbDefault;
            return out;
        }
    } else if (!legacy && bc_running_) {
        if (!io_.stop_bc()) { out.state=State::Fault; return out; }
        bc_running_=false;
    }
    // Legacy detection already began after a verified interrupt clearance.
    // Never W1C after observing BC capability: that could erase a new event
    // arriving between the ACK verification read and its write. Require the
    // same still-clear attachment using only reads. Native CC authorization
    // does not carry a previous source's BC result and can ACK its own event.
    usb_source::Sample confirmed;
    if (legacy) {
        confirmed=io_.cc();
        if (!same_source(out.cc,confirmed) || confirmed.change_latched) { out.state=State::Fault; return out; }
    } else if (!io_.acknowledge_cc(out.cc,confirmed)) { out.state=State::Fault; return out; }
    // Wait beyond TPS3808 CT-open maximum28ms release before a clock edge;
    // recheck the source after the delay and again after charger writes.
    io_.delay_ms(40);
    auto after=io_.cc();
    if (!same_source(confirmed,after) || after.change_latched) { out.state=State::Fault; return out; }
    if (legacy) out.bc=io_.bc(after.generation);
    const uint32_t wake_epoch=io_.permission_epoch();
    if (budget(after,out.bc)!=1400 || !io_.input_enabled(true)) {
        lower(out);out.state=State::Fault;return out;
    }
    // HIZ may suppress BQ power-good. Independent CC/BC detection above is
    // therefore allowed while isolated; only a recognized source can wake the
    // input. Keep AUTH LOW while waiting a bounded time for the charger.
    const uint32_t wake=io_.now_ms();
    for(;;) {
        if(io_.permission_epoch()!=wake_epoch) {lower(out);out.state=State::Fault;return out;}
        out.power=io_.power();
        auto observed=io_.cc();
        if(legacy)out.bc=io_.bc(observed.generation);
        if(!same_source(after,observed) || observed.change_latched || budget(observed,out.bc)!=1400 ||
           !out.power.charger_valid || !out.power.charger_configured || out.power.faults_current ||
           !out.power.input_enable_command || out.power.input_path!=power_monitor::InputPath::Enabled ||
           io_.permission_epoch()!=wake_epoch || io_.now_ms()-wake>=2500) {
            lower(out);out.state=State::Fault;return out;
        }
        if(out.power.usb_power_good)break;
        io_.delay_ms(20);
    }
    if (!io_.current_limit(1400)) { lower(out);out.state=State::Fault;return out; }
    out.power.input_limit_ma=1400;
    // Evidence collected before the delay/write cannot authorize the new edge.
    // Poll again: a reset, fault or expiring BC result invalidates this grant.
    out.power=io_.power();
    if (legacy) out.bc=io_.bc(after.generation);
    if (!out.power.charger_valid || !out.power.charger_configured ||
        out.power.faults_current || !out.power.usb_power_good ||
        !out.power.input_enable_command || out.power.input_path!=power_monitor::InputPath::Enabled ||
        out.power.input_limit_ma!=1400 || budget(after,out.bc)!=1400) {
        lower(out); out.state=State::Fault; return out;
    }
    auto final=io_.cc();
    const uint32_t grant_epoch=io_.permission_epoch();
    if (!same_source(after,final) || final.change_latched || !io_.permission(true)) {
        lower(out); out.state=State::Fault; return out;
    }
    // A successful GPIO write cannot prove the external latch accepted the
    // edge. Wait for its output and a coincident short charger IRQ to settle.
    // Maintenance may lower the command while waiting; its epoch invalidates
    // this grant even if the external latch itself still contains a one.
    io_.delay_ms(2);
    if (!io_.permission_feedback_high() || io_.permission_epoch()!=grant_epoch) {
        lower(out); out.state=State::Fault; return out;
    }
    armed_=true; armed_epoch_=grant_epoch; out.cc=final; out.state=State::Qualified;
    out.permission_command=true; out.permission_confirmed=true; out.requested_limit_ma=1400;
    return out;
}
}
