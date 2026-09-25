#pragma once
#include "system_i2c.h"
#include "hardware_contract.h"
#include "services/charge_profile.h"
#include <cmath>

namespace power_monitor {
constexpr uint8_t kGaugeAddress = hardware_contract::kGaugeAddress;
constexpr uint8_t kChargerAddress = hardware_contract::kChargerAddress;
enum class InputPath { Unknown, Isolated, Enabled };
struct Sample {
    uint32_t timestamp_ms = 0;
    bool gauge_valid = false, charger_valid = false, charger_configured = false;
    float voltage_mv = NAN, soc_percent = NAN, rate_percent_hour = NAN;
    bool usb_power_good = false, charging = false;
    uint8_t charger_state = 0, faults_latched = 0, faults_current = 0;
    uint16_t input_limit_ma = 100;
    bool input_enable_command = false;
    InputPath input_path = InputPath::Unknown; // Last verified EN_HIZ readback, not GPIO permission.
    bool bq_adc_idle_confirmed=false;
    bool charge_enabled=false, charge_inhibit_confirmed=false, charge_fault_latched=false;
    uint32_t charge_profile_revision=0;
};
// Production active() is permanently CommissioningInhibited until an explicit
// source-reviewed physical qualification profile is added. J104 stays OPEN.
// This API cannot alter cold defaults while the host is absent.
class Monitor {
public:
    explicit Monitor(system_i2c::Bus &bus, const charge_profile::Profile &profile=charge_profile::active()) : bus_(bus),profile_(profile) {}
    Sample poll();
    // Caller holds the independent hardware permission command low while
    // changing this setting. Does not enable charging, OTG or D+/D- negotiation.
    bool set_input_limit_ma(uint16_t milliamps);
    // Worker-owned. Enable additionally requires a verified fresh higher-current
    // source; caller supplies that physical permission, never an NVS/UI switch.
    using ChargePermit=bool(*)(void *);
    // Source-policy owner only. Disable is always attempted; enable requires a
    // fresh permitted source and a cancellation guard supplied by the adapter.
    bool set_input_enabled(bool enabled,ChargePermit permit=nullptr,void *context=nullptr);
    bool input_isolated() const { return input_path_==InputPath::Isolated; }
    InputPath input_path() const { return input_path_; }
    bool input_enable_command() const { return input_enable_command_; }
    // Production passes a lock-free cancellation/source guard. The driver
    // checks it between transactions; an already-started bus write must finish
    // before a verified inhibit can be acknowledged to maintenance.
    bool request_charging(bool enabled,ChargePermit permit=nullptr,void *context=nullptr);
    bool charge_inhibited() const { return charge_inhibit_confirmed_; }
    static bool decode_gauge(uint16_t version, uint16_t voltage, uint16_t soc,
                             uint16_t rate, Sample &out);
    static bool ota_allowed(const Sample &sample, uint32_t now);
private:
    bool configure_inhibited();
    bool config_matches();
    bool inhibit_charge();
    bool isolate_input();
    bool disable_adc();
    void observe_adc(uint8_t reg02);
    bool profile_matches();
    bool fail_charge();
    bool update(uint8_t reg, uint8_t mask, uint8_t value);
    bool read8(uint8_t reg, uint8_t &value);
    bool charge_operation_allowed();
    system_i2c::Bus &bus_;
    charge_profile::Profile profile_;
    bool configured_ = false;
    bool profile_applied_=false,charge_enabled_=false,charge_inhibit_confirmed_=false,inhibit_recovered_=false;
    bool charge_fault_latched_=false,last_power_good_=false,last_sample_valid_=false;
    uint32_t last_sample_ms_=0;
    uint16_t input_limit_ma_ = 100;
    bool input_enable_command_=false,input_request_active_=false;
    InputPath input_path_=InputPath::Unknown;
    bool adc_off_known_=false,adc_idle_confirmed_=false;
    uint32_t adc_off_since_=0;
    uint32_t input_request_start_=0;
    ChargePermit input_permit_=nullptr;
    void *input_context_=nullptr;
    bool charge_request_active_=false;
    uint32_t charge_request_start_=0;
    ChargePermit charge_permit_=nullptr;
    void *charge_context_=nullptr;
};
}
