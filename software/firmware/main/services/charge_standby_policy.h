#pragma once
#include <cstdint>
namespace charge_standby {
enum class Mode { Running, Preparing, Qualifying, Standby, Waking, OffPending };
enum class Action { None, Enter, EnableCharge, Wake, Off };
struct Inputs {
    uint32_t now_ms=0;
    bool button=false, maintenance=false, startup_accepted=false,force_off=false;
    bool profile_eligible=false, power_healthy=false, usb_present=false;
    bool source_qualified=false, source_fault=false;
    bool battery_entry_ok=false, battery_continue_ok=false;
};
class Policy {
public:
    static constexpr uint32_t kTransitionMs=3000,kRequalifyMs=3000,kOffRetryMs=1000;
    Action step(const Inputs &in);
    void complete(Action action,bool success,uint32_t now);
    Mode mode() const { return mode_; }
    bool measurements_expected() const { return mode_==Mode::Running; }
private:
    Mode mode_=Mode::Running;
    Action pending_=Action::None;
    uint32_t began_=0,last_off_=0;
    bool off_sent_=false;
    Action off(uint32_t now);
};
}
