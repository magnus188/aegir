#pragma once
#include "usb_source_monitor.h"
#include "bc12_monitor.h"
#include "power_monitor.h"

namespace usb_input {
enum class State { Detached, Checking, UsbDefault, Qualified, Fault, Maintenance };
struct Result {
    State state=State::Detached;
    power_monitor::Sample power;
    usb_source::Sample cc;
    bc12::Sample bc;
    uint16_t requested_limit_ma=100;
    bool permission_command=false; // Host command, not a current measurement.
    bool permission_confirmed=false; // Command HIGH and shared Q/charger feedback observed HIGH.
};
class Actions {
public:
    virtual ~Actions()=default;
    virtual power_monitor::Sample power()=0;
    virtual usb_source::Sample cc()=0;
    virtual bool configure_cc()=0;
    virtual bool acknowledge_cc(const usb_source::Sample &,usb_source::Sample &)=0;
    virtual bc12::Sample bc(uint32_t source_generation)=0;
    virtual bool begin_bc(uint32_t source_generation)=0;
    virtual bc12::Sample collect_bc(uint32_t source_generation)=0;
    virtual void invalidate_bc()=0;
    virtual bool stop_bc()=0;
    virtual bool current_limit(uint16_t ma)=0;
    // Enable only after classification, with the physical adapter's maintenance
    // guard. Disable must verify EN_HIZ; it is distinct from charge inhibition.
    virtual bool input_enabled(bool enabled)=0;
    // A physical adapter must refuse high while maintenance/startup inhibits it.
    virtual bool permission(bool enabled)=0;
    // Incremented whenever another owner or this policy forces permission LOW.
    // A HIGH command does not change the epoch.
    virtual uint32_t permission_epoch()=0;
    // HIGH means the hardware authorization latch is set and charger IRQ is
    // released. LOW can mean either lost authorization or a short charger IRQ.
    virtual bool permission_feedback_high()=0;
    virtual uint32_t now_ms()=0;
    virtual void delay_ms(uint32_t)=0;
};
class Policy {
public:
    explicit Policy(Actions &actions):io_(actions) { io_.permission(false); }
    Result step(bool maintenance);
    static uint16_t budget(const usb_source::Sample &,const bc12::Sample &);
private:
    bool lower(Result &out);
    bool stop_detection();
    bool same_source(const usb_source::Sample &,const usb_source::Sample &);
    Actions &io_;
    uint32_t generation_=0,last_detection_ms_=0,armed_epoch_=0;
    bool armed_=false,detected_attempt_=false,bc_running_=false;
};
}
