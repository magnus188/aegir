#include "power_monitor.h"
#include <algorithm>

namespace power_monitor {
bool Monitor::decode_gauge(uint16_t version, uint16_t voltage, uint16_t soc,
                           uint16_t rate, Sample &out) {
    // MAX17048 Rev7 table2: VCELL 78.125uV/LSB, SOC 1/256%, signed CRATE .208%/h.
    // Reject wrong production family and impossible voltage; never invent SOC.
    if ((version & 0xfff0u) != 0x0010u || voltage == 0xffffu || soc == 0xffffu) return false;
    const float mv = voltage * 0.078125f;
    if (mv < 2500 || mv > 4500) return false;
    out.voltage_mv = mv;
    out.soc_percent = std::min(100.0f, soc / 256.0f);
    const int32_t signed_rate = rate & 0x8000u ? static_cast<int32_t>(rate) - 65536 : rate;
    out.rate_percent_hour = signed_rate * 0.208f;
    out.gauge_valid = true;
    return true;
}
bool Monitor::read8(uint8_t reg, uint8_t &v) {
    return charge_operation_allowed() && bus_.read_register(kChargerAddress, reg, &v, 1) && charge_operation_allowed();
}
bool Monitor::charge_operation_allowed() {
    // This bounds successful-but-slow programming too, not just bus failures.
    // Each underlying physical transaction still has the shared bus timeout.
    return (!charge_request_active_ || (bus_.now_ms()-charge_request_start_<1500 &&
           (!charge_permit_ || charge_permit_(charge_context_)))) &&
           (!input_request_active_ || (bus_.now_ms()-input_request_start_<1500 &&
           (!input_permit_ || input_permit_(input_context_))));
}
bool Monitor::update(uint8_t reg, uint8_t mask, uint8_t value) {
    uint8_t old = 0, actual = 0;
    if (!read8(reg, old)) return false;
    const uint8_t desired = static_cast<uint8_t>((old & ~mask) | (value & mask));
    return bus_.write_register(kChargerAddress, reg, desired) && read8(reg, actual) &&
        (actual & mask) == (desired & mask);
}
bool Monitor::configure_inhibited() {
    input_limit_ma_=100;
    charge_enabled_=false; profile_applied_=false; charge_inhibit_confirmed_=false;
    // TI BQ25895 RevC. Disable charge/OTG FIRST, then disable autonomous input
    // negotiation/optimization. Retain hardware ILIM. Do not reset safety timers.
    return isolate_input() && inhibit_charge() &&
        disable_adc() &&
        update(0x04, 0x80, 0x00) &&
        update(0x00, 0xff, 0xc0) && // HIZ, EN_ILIM=1, 100mA ceiling; no source entitlement.
        update(0x07, 0x30, 0x00);   // Disable watchdog after inhibited host configuration.
}
bool Monitor::config_matches() {
    uint8_t r0=0, r2=0, r3=0, r4=0, r7=0;
    input_path_=InputPath::Unknown;
    if(!read8(0x00,r0))return false;
    input_path_=(r0&0x80) ? InputPath::Isolated:InputPath::Enabled;
    if(!read8(0x02,r2)) {adc_idle_confirmed_=false;return false;}
    observe_adc(r2);
    return read8(0x03,r3) && read8(0x04,r4) && read8(0x07,r7) &&
        r0 == ((input_enable_command_ ? 0:0x80) | 0x40 | ((input_limit_ma_-100)/50)) &&
        !(r2 & 0x7f) && (r3 & 0x30)==(charge_enabled_ ? 0x10:0) &&
        !(r4 & 0x80) && !(r7 & 0x30) && (!profile_applied_ || profile_matches());
}
void Monitor::observe_adc(uint8_t reg02) {
    if(reg02&0x40)adc_off_known_=false;
    // CONV_START also indicates input detection. Observing busy resets the
    // quiet interval but must not make every legitimate HIZ exit a config fault.
    if(reg02&0xc0)adc_off_since_=bus_.now_ms();
    adc_idle_confirmed_=adc_off_known_ && !(reg02&0xc0) && bus_.now_ms()-adc_off_since_>=1000;
}
bool Monitor::disable_adc() {
    adc_off_known_=adc_idle_confirmed_=false;
    uint8_t actual=0;
    // Do not echo CONV_START=1 from read/modify/write, which can start a new
    // one-shot. Writing0 is not assumed to instantly abort an existing sample.
    if(!bus_.write_register(kChargerAddress,0x02,0) || !read8(0x02,actual) || (actual&0x7f))return false;
    adc_off_known_=true;adc_off_since_=bus_.now_ms();return true;
}
bool Monitor::isolate_input() {
    input_request_active_=false; // Cancellation must never prevent the safe write.
    input_enable_command_=false;input_path_=InputPath::Unknown;
    // Exact REG00 is deliberate: safe even if the initial read failed. A read
    // failure/ignored write cannot become a confirmed electrical isolation.
    uint8_t actual=0;
    const bool written=bus_.write_register(kChargerAddress,0x00,0xc0);
    const bool read=read8(0x00,actual);
    if(read)input_path_=(actual&0x80) ? InputPath::Isolated:InputPath::Enabled;
    if(!written || !read || actual!=0xc0)return false;
    input_limit_ma_=100;return true;
}
bool Monitor::set_input_enabled(bool enabled,ChargePermit permit,void *context) {
    input_enable_command_=enabled;
    uint8_t id=0;
    if(!read8(0x14,id)||(id&0x38)!=0x38) {
        input_path_=InputPath::Unknown;configured_=false;return false;
    }
    if(!enabled) {
        if(input_path_==InputPath::Isolated && input_limit_ma_==100 && configured_ && config_matches())return true;
        const bool ok=isolate_input();if(!ok)configured_=false;return ok;
    }
    // A source grant must start with the isolated, low-current configuration.
    // Do not require PG here: source detection deliberately operates during HIZ.
    input_enable_command_=false;
    if(!configured_ || input_limit_ma_!=100 || !config_matches() || !input_isolated()) {
        isolate_input();configured_=false;return false;
    }
    input_request_start_=bus_.now_ms();input_permit_=permit;input_context_=context;
    input_request_active_=true;
    const bool ok=update(0x00,0x80,0);
    input_request_active_=false;
    if(!ok) {isolate_input();return false;}
    input_enable_command_=true;input_path_=InputPath::Enabled;
    return true;
}
bool Monitor::inhibit_charge() {
    charge_request_active_=false; // Cancellation must never block a safe inhibit.
    charge_enabled_=false;inhibit_recovered_=false;
    charge_inhibit_confirmed_=update(0x03,0x30,0);
    if(!charge_inhibit_confirmed_) {
        inhibit_recovered_=true;
        // Best-effort recovery after a failed read/modify/write. Exact REG03
        // keeps SYS_MIN=3.5V and disables charge/OTG; never resets safety timers.
        uint8_t actual=0;
        charge_inhibit_confirmed_=bus_.write_register(kChargerAddress,0x03,0x0a) &&
                                 read8(0x03,actual) && !(actual&0x30);
    }
    return charge_inhibit_confirmed_;
}
bool Monitor::fail_charge() {
    charge_fault_latched_=true; configured_=false; profile_applied_=false;
    inhibit_charge(); // Failure cannot be reported as a confirmed disable.
    return false;
}
bool Monitor::profile_matches() {
    charge_profile::Registers expected;
    if(!charge_profile::encode(profile_,expected))return false;
    constexpr uint8_t regs[]={0x04,0x05,0x06,0x07,0x08,0x09};
    const uint8_t values[]={expected.r4,expected.r5,expected.r6,expected.r7,expected.r8,expected.r9};
    constexpr uint8_t masks[]={0xff,0xff,0xff,0xfe,0xff,0x60};
    for(unsigned i=0;i<6;++i) {uint8_t v=0;if(!read8(regs[i],v)||(v&masks[i])!=values[i])return false;}
    return true;
}
bool Monitor::request_charging(bool enabled,ChargePermit permit,void *context) {
    uint8_t id=0;
    if(!read8(0x14,id)||(id&0x38)!=0x38) {
        if(charge_enabled_ || profile_applied_)charge_fault_latched_=true;
        charge_enabled_=false;charge_inhibit_confirmed_=false;configured_=false;
        return false; // Do not write to an unidentified part.
    }
    if(!enabled) {
        if(charge_inhibit_confirmed_ && !charge_enabled_ && configured_ && config_matches())return true;
        if(!inhibit_charge()) {configured_=false;if(profile_applied_)charge_fault_latched_=true;return false;}
        return true;
    }
    charge_request_start_=bus_.now_ms();charge_permit_=permit;charge_context_=context;
    charge_request_active_=true;
    struct Clear {bool &flag;~Clear(){flag=false;}} clear{charge_request_active_};
    charge_profile::Registers r;
    if(!charge_profile::encode(profile_,r) || charge_fault_latched_ || !configured_ ||
       !last_sample_valid_ || !last_power_good_ || bus_.now_ms()-last_sample_ms_>3000 ||
       input_limit_ma_!=1400 || !input_enable_command_ || input_path_!=InputPath::Enabled) {
        // An ineligible request never leaves an earlier charge grant active.
        inhibit_charge();return false;
    }
    if(!config_matches())return fail_charge();
    if(charge_enabled_)return true; // Do not reset a healthy charge cycle/timer.
    if(!inhibit_charge() || inhibit_recovered_)return fail_charge();
    charge_request_active_=true;
    uint8_t shipping=0;
    if(!read8(0x09,shipping)||(shipping&0x20))return fail_charge(); // Never clear a BATFET protection latch.
    if(!update(0x04,0xff,r.r4) || !update(0x05,0xff,r.r5) ||
       !update(0x06,0xff,r.r6) || !update(0x07,0xfe,r.r7) ||
       !update(0x08,0xff,r.r8) || !update(0x09,0x40,r.r9))return fail_charge();
    profile_applied_=true;
    if(!profile_matches() || !update(0x03,0x30,0x10))return fail_charge();
    charge_enabled_=true;charge_inhibit_confirmed_=false;
    if(!config_matches())return fail_charge();
    return true;
}
bool Monitor::set_input_limit_ma(uint16_t milliamps) {
    if (milliamps<100 || milliamps>1400 || (milliamps-100)%50) return false;
    uint8_t id=0;
    if (!configured_ || !read8(0x14,id) || (id&0x38)!=0x38 || !config_matches()) return false;
    const uint8_t value=static_cast<uint8_t>(0x40 | ((milliamps-100)/50));
    if (!update(0x00,0x7f,value)) { configured_=false; input_limit_ma_=100; return false; }
    input_limit_ma_=milliamps;
    return true;
}
Sample Monitor::poll() {
    Sample result;
    uint8_t b[2]{};
    uint16_t words[4]{};
    constexpr uint8_t regs[] = {0x08, 0x02, 0x04, 0x16};
    bool ok = true;
    for (unsigned i=0; i<4; ++i) {
        if (!bus_.read_register(kGaugeAddress, regs[i], b, 2)) { ok=false; break; }
        words[i] = static_cast<uint16_t>((uint16_t(b[0]) << 8) | b[1]);
    }
    if (ok) decode_gauge(words[0],words[1],words[2],words[3],result);
    uint8_t identity=0, status=0, latched=0, current=0;
    // REG0C: first read reports latched faults; second reports current status.
    ok = read8(0x14,identity) && (identity & 0x38) == 0x38;
    if (ok) {
        if (!configured_ || !config_matches()) {
            if(profile_applied_ || charge_enabled_)charge_fault_latched_=true;
            configured_=configure_inhibited();
        }
        ok=read8(0x0b,status) && read8(0x0c,latched) && read8(0x0c,current);
    } else {
        if(profile_applied_ || charge_enabled_)charge_fault_latched_=true;
        charge_enabled_=false;charge_inhibit_confirmed_=false;configured_=false;input_path_=InputPath::Unknown;
        adc_off_known_=adc_idle_confirmed_=false;
    }
    if(!ok && (profile_applied_ || charge_enabled_)) {
        if((identity&0x38)==0x38)fail_charge();
        else charge_fault_latched_=true;
    }
    charge_profile::Registers candidate;
    if(ok && (latched || current) &&
       (profile_applied_ || charge_enabled_ || charge_profile::encode(profile_,candidate)))fail_charge();
    last_sample_valid_=ok && configured_ && !latched && !current;
    last_power_good_=ok && (status&4);last_sample_ms_=bus_.now_ms();
    result.charger_configured=configured_;
    result.input_limit_ma=input_limit_ma_;
    result.input_enable_command=input_enable_command_;
    result.input_path=input_path_;
    result.bq_adc_idle_confirmed=adc_idle_confirmed_;
    if (ok) {
        result.charger_valid=true; result.usb_power_good=(status & 4)!=0;
        result.charger_state=(status >> 3) & 3;
        result.charging=result.charger_state==1 || result.charger_state==2;
        result.faults_latched=latched; result.faults_current=current;
        if (latched & 0x80) {
            configured_=false; result.charger_configured=false; input_limit_ma_=100;
        } // Verify/reapply after watchdog/reset evidence before authorizing OTA.
    }
    result.charge_enabled=charge_enabled_;
    result.charge_inhibit_confirmed=charge_inhibit_confirmed_;
    result.charge_fault_latched=charge_fault_latched_;
    result.charge_profile_revision=profile_applied_ ? profile_.revision:0;
    result.timestamp_ms=bus_.now_ms();
    return result;
}
bool Monitor::ota_allowed(const Sample &s, uint32_t now) {
    // Cable presence does not prove an input grant or adequate power for the
    // display. Require fresh battery reserve even while USB is attached.
    return now-s.timestamp_ms <= 3000 && s.gauge_valid && s.charger_valid &&
        s.charger_configured && !s.faults_current && std::isfinite(s.soc_percent) &&
        s.soc_percent >= 30 && std::isfinite(s.voltage_mv) && s.voltage_mv >= 3600;
}
}
