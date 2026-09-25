#include "charge_standby_policy.h"
namespace charge_standby {
Action Policy::off(uint32_t now) {
    mode_=Mode::OffPending;pending_=Action::None;
    if(off_sent_ && now-last_off_<kOffRetryMs) return Action::None;
    last_off_=now;off_sent_=true;return Action::Off;
}
Action Policy::step(const Inputs &in) {
    if(mode_==Mode::OffPending) return off(in.now_ms);
    if(in.force_off)return off(in.now_ms);
    if(pending_!=Action::None) {
        if(in.now_ms-began_>=kTransitionMs)return off(in.now_ms);
        return Action::None;
    }
    if(mode_==Mode::Running) {
        if(!in.button)return Action::None;
        if(in.maintenance)return Action::None; // Independent held-button cutoff remains available.
        if(!(in.startup_accepted && in.profile_eligible && in.power_healthy &&
             in.usb_present && in.source_qualified && !in.source_fault && in.battery_entry_ok))return off(in.now_ms);
        mode_=Mode::Preparing;pending_=Action::Enter;began_=in.now_ms;return pending_;
    }
    // A transient maintenance owner cannot retain a charge-only mode indefinitely.
    // Off retries remain bounded while that owner finishes or is forced off.
    if(in.maintenance || !in.profile_eligible || !in.power_healthy || !in.usb_present ||
       in.source_fault || !in.battery_continue_ok)return off(in.now_ms);
    if(in.button) {mode_=Mode::Waking;pending_=Action::Wake;began_=in.now_ms;return pending_;}
    if(mode_==Mode::Qualifying) {
        if(in.source_qualified) {pending_=Action::EnableCharge;began_=in.now_ms;return pending_;}
        if(in.now_ms-began_>=kRequalifyMs)return off(in.now_ms);
    } else if(mode_==Mode::Standby && !in.source_qualified)return off(in.now_ms);
    return Action::None;
}
void Policy::complete(Action action,bool success,uint32_t now) {
    if(pending_!=action || action==Action::None)return;
    if(!success || now-began_>=kTransitionMs) {mode_=Mode::OffPending;pending_=Action::None;off_sent_=false;return;}
    pending_=Action::None;
    if(action==Action::Enter) {mode_=Mode::Qualifying;began_=now;}
    else if(action==Action::EnableCharge)mode_=Mode::Standby;
    else if(action==Action::Wake)mode_=Mode::Running;
}
}
