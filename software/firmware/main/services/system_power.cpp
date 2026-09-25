#include "system_power.h"
#include "hardware_contract.h"
#include "maintenance_service.h"
#include "charge_profile.h"
#include "charge_standby_policy.h"
#include "backlight_service.h"
#include "sd_log_service.h"
#include "sensors/sensor_hardware.h"
#include <atomic>
#include <mutex>

#if defined(ESP_PLATFORM) && !defined(TRIMIX_SIMULATOR)
#include <driver/gpio.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
namespace {
constexpr auto kill_pin=static_cast<gpio_num_t>(hardware_contract::kPowerKill.gpio), interrupt_pin=static_cast<gpio_num_t>(hardware_contract::kPowerInterrupt.gpio);
constexpr auto usb_permission_pin=static_cast<gpio_num_t>(hardware_contract::kUsbPermission.gpio);
constexpr auto usb_feedback_pin=static_cast<gpio_num_t>(hardware_contract::kChargerAlert.gpio);
std::mutex power_lock;
std::mutex input_permission_lock;
std::atomic<bool> input_inhibited{true};
std::atomic<uint32_t> input_permission_epoch{0};
std::atomic<bool> input_command_high{false}, input_command_healthy{true};
std::atomic<bool> input_feedback_ready{false};
std::atomic<charge_standby::Mode> operating_mode{charge_standby::Mode::Running};
std::atomic<bool> startup_accepted{false},charge_stop_confirmed{false},shutdown_hold{false};
std::atomic<bool> input_stop_confirmed{false},input_enable_command{false};
std::atomic<power_monitor::InputPath> input_path{power_monitor::InputPath::Unknown};
std::atomic<bool> off_requested{false};
std::atomic<TaskHandle_t> power_owner{nullptr};
power_monitor::Monitor *owner_monitor=nullptr; // Dereferenced only by power_owner.
bool measurements_expected() {return operating_mode.load()==charge_standby::Mode::Running;}
power_monitor::Sample power_cache;
environment_monitor::Sample environment_cache;
usb_input::Result usb_cache;
std::atomic<bool> started{false}, pulse_valid{false};
std::atomic<uint32_t> pulse{0};
std::atomic<bool> interrupt_armed{false}, button_pending{false};
uint32_t now_ms() { return static_cast<uint32_t>(esp_timer_get_time()/1000); }
void observe_input(power_monitor::Monitor &power) {
    input_enable_command.store(power.input_enable_command());
    input_path.store(power.input_path());
    input_stop_confirmed.store(power.input_isolated());
}
bool input_permitted(void *) {
    return !input_inhibited.load() && !maintenance_is_active() && !off_requested.load();
}
bool input_permission(bool enabled) {
    std::lock_guard<std::mutex> guard(input_permission_lock);
    const bool allowed=!enabled || (!input_inhibited.load() && !maintenance_is_active());
    const bool ok=gpio_set_level(usb_permission_pin,enabled && allowed)==ESP_OK;
    input_command_healthy.store(ok);
    if (ok) input_command_high.store(enabled && allowed);
    if (ok && (!enabled || !allowed)) input_permission_epoch.fetch_add(1);
    return allowed && ok;
}
bool stop_for_maintenance(uint32_t timeout) {
    input_inhibited.store(true);
    const uint32_t start=now_ms();
    const bool low=input_permission(false);
    if(power_owner.load()==xTaskGetCurrentTaskHandle() && owner_monitor) {
        owner_monitor->set_input_enabled(false);observe_input(*owner_monitor);
        charge_stop_confirmed.store(owner_monitor->request_charging(false));
    }
    auto remaining=[&] {const uint32_t elapsed=now_ms()-start;return elapsed>=timeout ? 0u:timeout-elapsed;};
    const bool stopped=sensor_hardware_stop_wait(remaining());
    // External maintenance cannot write charger registers. The power worker
    // acknowledges a verified inhibit before the barrier may become Held.
    const auto mode=operating_mode.load();
    const bool need_confirm=mode!=charge_standby::Mode::OffPending ||
                            power_owner.load()!=xTaskGetCurrentTaskHandle();
    while(need_confirm && (!charge_stop_confirmed.load() || !input_stop_confirmed.load()) && remaining())vTaskDelay(1);
    // OffPending must still remove host power if the charger cannot be reached.
    // Source permission is already LOW; never claim charging itself was disabled.
    return stopped && low && (!need_confirm || (charge_stop_confirmed.load() && input_stop_confirmed.load()));
}
void resume() {
    if(off_requested.load() || operating_mode.load()==charge_standby::Mode::OffPending) {
        input_inhibited.store(true);input_permission(false);return;
    }
    input_inhibited.store(false);
    // A timed-out worker may still be exiting. Never start a second ADC owner.
    if (!measurements_expected())return;
    sd_log_resume();
    if (!sensor_hardware_stop_wait(1500) || sensor_hardware_start()!=ESP_OK)
        ESP_LOGE("POWER","Acquisition resume pending worker recovery");
}
void button_isr(void *) {
    if (interrupt_armed.load(std::memory_order_relaxed)) button_pending.store(true,std::memory_order_relaxed);
}
bool charge_permitted(void *) {
    // No lock acquisition: enable holds input_permission_lock and invokes this
    // between BQ transactions. Maintenance publishes Preparing before its hook.
    auto command_ok=[] {return !input_inhibited.load() && !maintenance_is_active() &&
                              input_command_high.load() && input_feedback_ready.load();};
    if(!command_ok())return false;
    if(gpio_get_level(usb_feedback_pin))return true;
    // Enabling charge can itself issue a normal256-us BQ IRQ. As in the USB
    // source policy, distinguish this pulse from persistent lost latch feedback.
    vTaskDelay(static_cast<TickType_t>((uint64_t(2)*configTICK_RATE_HZ+999)/1000+1));
    return command_ok() && gpio_get_level(usb_feedback_pin);
}
class UsbActions final:public usb_input::Actions {
public:
    UsbActions(system_i2c::Bus &bus,power_monitor::Monitor &power):bus_(bus),power_(power),cc_(bus),bc_(bus) {}
    power_monitor::Sample power() override{auto out=power_.poll();observe_input(power_);return out;}
    usb_source::Sample cc() override{return cc_.poll();}
    bool configure_cc() override{return cc_.initialize_disabled_policy();}
    bool acknowledge_cc(const usb_source::Sample &s,usb_source::Sample &after) override{return cc_.acknowledge_change(s,after);}
    bc12::Sample bc(uint32_t gen) override{return bc_.poll(gen);}
    bool begin_bc(uint32_t gen) override{return bc_.begin_detection(gen);}
    bc12::Sample collect_bc(uint32_t gen) override{return bc_.collect_detection(gen);}
    void invalidate_bc() override{bc_.invalidate();}
    bool stop_bc() override{return bc_.stop();}
    bool current_limit(uint16_t ma) override{return power_.set_input_limit_ma(ma);}
    bool input_enabled(bool enabled) override {
        // Maintenance sets its Preparing state before waiting on this lock.
        // It cannot become Held until a cancelled enable finishes and HIZ is
        // read back by this worker. Never let another task own charger I2C.
        std::lock_guard<std::mutex> guard(input_permission_lock);
        if(enabled)input_stop_confirmed.store(false);
        const bool ok=power_.set_input_enabled(enabled,input_permitted,nullptr);
        observe_input(power_);return ok;
    }
    bool permission(bool enabled) override{return input_permission(enabled);}
    uint32_t permission_epoch() override{return input_permission_epoch.load();}
    bool permission_feedback_high() override {
        return input_feedback_ready.load() && gpio_get_level(usb_feedback_pin)!=0;
    }
    uint32_t now_ms() override{return bus_.now_ms();}
    void delay_ms(uint32_t ms) override {
        // vTaskDelay(1) can finish at an imminent tick, so simple floor/min-one
        // conversion is not a minimum elapsed wait. Round up and add the phase
        // tick for both supervisor release and shared-IRQ pulse settling.
        const auto ticks=static_cast<TickType_t>((uint64_t(ms)*configTICK_RATE_HZ+999)/1000+1);
        vTaskDelay(ticks);
    }
private:
    system_i2c::Bus &bus_;power_monitor::Monitor &power_;
    usb_source::Monitor cc_;bc12::Monitor bc_;
};
void worker(void *) {
    auto &bus=system_i2c::shared();
    power_monitor::Monitor power(bus);
    environment_monitor::Monitor environment(bus);
    owner_monitor=&power;power_owner.store(xTaskGetCurrentTaskHandle());
    UsbActions usb_actions(bus,power);
    usb_input::Policy usb(usb_actions);
    charge_standby::Policy mode;
    const auto &profile=charge_profile::active();charge_profile::Registers unused;
    const bool profile_eligible=charge_profile::encode(profile,unused);
    unsigned low_count=0;
    for (;;) {
        auto source=usb.step(maintenance_is_active() || input_inhibited.load());
        charge_stop_confirmed.store(power.charge_inhibited());
        if((maintenance_is_active() || input_inhibited.load() || mode.mode()!=charge_standby::Mode::Standby) &&
           !power.charge_inhibited())charge_stop_confirmed.store(power.request_charging(false));
        auto p=source.power;
        if(charge_stop_confirmed.load()) {p.charge_enabled=false;p.charge_inhibit_confirmed=true;}
        auto e=environment.poll();
        {
            std::lock_guard<std::mutex> guard(power_lock);
            power_cache=p; environment_cache=e; usb_cache=source;
        }
        pulse.store(now_ms()); pulse_valid.store(true);
        if (measurements_expected() && !maintenance_is_active() && !sensor_hardware_service_healthy()) sensor_hardware_start();
        // LTC2954 already debounces PB; INT pulses can be much shorter than
        // an I2C poll cycle. The ISR latches an event even for a short press.
        if (gpio_get_level(interrupt_pin)) interrupt_armed.store(true);
        charge_standby::Inputs in;
        in.now_ms=now_ms();in.button=button_pending.exchange(false);
        // Provisional normal-run threshold; standby has stricter profile limits.
        if(measurements_expected() && p.gauge_valid && p.voltage_mv<=3300)++low_count;else low_count=0;
        in.force_off=off_requested.load() || low_count>=3;
        in.maintenance=maintenance_is_active();in.startup_accepted=startup_accepted.load();
        in.profile_eligible=profile_eligible;
        in.power_healthy=in.now_ms-p.timestamp_ms<=3000 && p.charger_valid && p.charger_configured &&
                         !p.faults_current && !p.charge_fault_latched && p.gauge_valid &&
                         std::isfinite(p.voltage_mv) && std::isfinite(p.soc_percent);
        in.usb_present=source.cc.valid && source.cc.attached_sink;
        in.source_qualified=source.state==usb_input::State::Qualified && source.permission_confirmed &&
                            source.requested_limit_ma==1400;
        in.source_fault=source.state==usb_input::State::Fault || source.state==usb_input::State::UsbDefault;
        in.battery_continue_ok=p.voltage_mv>profile.standby_min_mv && p.soc_percent>=profile.standby_min_soc;
        in.battery_entry_ok=p.voltage_mv>=profile.standby_min_mv+100 && in.battery_continue_ok;
        auto action=mode.step(in);operating_mode.store(mode.mode());
        bool completed=true;
        if(action==charge_standby::Action::Enter || action==charge_standby::Action::Wake) {
            completed=maintenance_begin(MAINTENANCE_MODE_CHANGE,2500);
            if(completed) {
                if(action==charge_standby::Action::Enter)completed=backlight_set_standby(true);
                // This is a bounded transition, not persistent maintenance.
                // resume() releases source policy while mode suppresses acquisition.
                maintenance_finish(false);
                completed=completed && !maintenance_is_active();
                if(action==charge_standby::Action::Wake && completed)
                    completed=sensor_hardware_start()==ESP_OK && backlight_set_standby(false);
                if(action==charge_standby::Action::Wake && completed)sd_log_resume();
                if(!completed)sensor_hardware_stop();
            }
        } else if(action==charge_standby::Action::EnableCharge) {
            std::lock_guard<std::mutex> guard(input_permission_lock);
            completed=charge_permitted(nullptr);
            if(completed) {
                charge_stop_confirmed.store(false);
                completed=power.request_charging(true,charge_permitted);
                if(!charge_permitted(nullptr)) {power.request_charging(false);completed=false;}
                charge_stop_confirmed.store(power.charge_inhibited());
            }
            if(!completed) {
                // Complete cancellation under the same input lock, so an
                // external maintenance barrier cannot observe only the charge
                // bit rollback while the input remains connected to SYS.
                power.set_input_enabled(false);observe_input(power);
            }
        } else if(action==charge_standby::Action::Off) {
            input_inhibited.store(true);input_permission(false);sensor_hardware_stop();
            // A service USB cable can keep Guition alive after main KILL. Keep
            // the visible display dark and retry safe-off without restarting it.
            backlight_set_standby(true);
            completed=system_power_shutdown(2500);
            if(!completed)ESP_LOGE("POWER","Orderly off remains pending; held-button cutoff is independent");
        }
        mode.complete(action,completed,now_ms());operating_mode.store(mode.mode());
        // A failed standby transition can return to Running after its resume
        // hook ran while the transitional mode still suppressed measurements.
        if(mode.mode()==charge_standby::Mode::Running && !maintenance_is_active() && !off_requested.load())sd_log_resume();
        if(mode.mode()==charge_standby::Mode::OffPending) {input_inhibited.store(true);input_permission(false);sensor_hardware_stop();}
        bus.delay_ms(500);
    }
}
}
bool system_power_start() {
    bool expected=false;
    if (!started.compare_exchange_strong(expected,true)) return true;
    input_feedback_ready.store(false);
    // Open-drain release before configuring output; never drive KILL high.
    gpio_set_level(kill_pin,1);
    gpio_set_level(usb_permission_pin,0);
    if (gpio_set_direction(kill_pin,GPIO_MODE_OUTPUT_OD)!=ESP_OK ||
        gpio_set_direction(usb_permission_pin,GPIO_MODE_OUTPUT)!=ESP_OK ||
        gpio_set_direction(usb_feedback_pin,GPIO_MODE_INPUT)!=ESP_OK ||
        gpio_set_direction(interrupt_pin,GPIO_MODE_INPUT)!=ESP_OK) { started=false; return false; }
    // External HOST_3V3 pull-up. Q112 combines lost latch authorization with
    // the charger's open-drain IRQ; do not drive or internally pull this net.
    if (gpio_set_pull_mode(usb_feedback_pin,GPIO_FLOATING)!=ESP_OK) { started=false; return false; }
    input_feedback_ready.store(true);
    gpio_set_pull_mode(interrupt_pin,GPIO_FLOATING); // external pull-up, host3V3
    const esp_err_t isr=gpio_install_isr_service(0);
    if (isr!=ESP_OK && isr!=ESP_ERR_INVALID_STATE) { started=false; return false; }
    interrupt_armed.store(gpio_get_level(interrupt_pin)!=0);
    if (gpio_set_intr_type(interrupt_pin,GPIO_INTR_NEGEDGE)!=ESP_OK ||
        gpio_isr_handler_add(interrupt_pin,button_isr,nullptr)!=ESP_OK) { started=false; return false; }
    maintenance_hooks_t hooks;
    hooks.stop_measurements=stop_for_maintenance; hooks.resume_measurements=resume;
    hooks.power_ok=system_power_ota_allowed;
    hooks.flush_state=sd_log_pause;
    maintenance_register_hooks(hooks);
    system_i2c::initialize(); // device absence is degraded service, not a fake success value
    if (sensor_hardware_start()!=ESP_OK) {
        gpio_isr_handler_remove(interrupt_pin); started=false; return false;
    }
    const BaseType_t created=xTaskCreate(worker,"system_power",6144,nullptr,4,nullptr);
    if (created!=pdPASS) {
        sensor_hardware_stop(); gpio_isr_handler_remove(interrupt_pin); started=false; return false;
    }
    input_inhibited.store(false);
    return true;
}
bool system_power_ready() { return started && pulse_valid && now_ms()-pulse.load()<5000; }
void system_power_accept_startup() {startup_accepted.store(true);}
bool system_power_charge_standby() {return operating_mode.load()==charge_standby::Mode::Standby;}
power_monitor::Sample system_power_sample() {
    std::lock_guard<std::mutex> guard(power_lock);
    auto out=power_cache;
    out.input_enable_command=input_enable_command.load();
    out.input_path=input_path.load();
    if (now_ms()-out.timestamp_ms>3000 || !pulse_valid) {
        out.gauge_valid=out.charger_valid=out.charger_configured=false;
        out.voltage_mv=out.soc_percent=out.rate_percent_hour=NAN;
        out.charging=false;
        out.charge_inhibit_confirmed=false;
        out.input_path=power_monitor::InputPath::Unknown;
        out.bq_adc_idle_confirmed=false;
    }
    return out;
}
environment_monitor::Sample system_environment_sample() {
    std::lock_guard<std::mutex> guard(power_lock);
    auto out=environment_cache;
    if (now_ms()-out.timestamp_ms>3000 || !pulse_valid) {
        out.valid=false; out.temperature_c=out.humidity_percent=out.pressure_bar=NAN;
    }
    return out;
}
usb_input::Result system_usb_input_sample() {
    std::lock_guard<std::mutex> guard(power_lock);
    auto out=usb_cache;
    out.power.input_enable_command=input_enable_command.load();
    out.power.input_path=input_path.load();
    out.permission_command=input_command_high.load();
    if (!out.permission_command || !input_feedback_ready.load() || !gpio_get_level(usb_feedback_pin) ||
        out.power.input_path!=power_monitor::InputPath::Enabled) {
        out.permission_confirmed=false;
        if (out.state==usb_input::State::Qualified) out.state=usb_input::State::Checking;
    }
    if (!pulse_valid || now_ms()-out.power.timestamp_ms>3000 || !input_command_healthy.load()) {
        // Stale evidence invalidates authorization, but does not prove the
        // last successful GPIO command was LOW. Preserve that diagnostic fact.
        out.state=usb_input::State::Fault;
        out.permission_confirmed=false;
        out.power.input_path=power_monitor::InputPath::Unknown;
        out.power.bq_adc_idle_confirmed=false;
        out.requested_limit_ma=100; out.cc.valid=out.bc.valid=false;
    }
    return out;
}
bool system_power_ota_allowed() {
    return measurements_expected() && power_monitor::Monitor::ota_allowed(system_power_sample(),now_ms());
}
bool system_power_shutdown(uint32_t timeout) {
    off_requested.store(true);input_inhibited.store(true);
    if(!shutdown_hold.load()) {
        if(!maintenance_begin(MAINTENANCE_SHUTDOWN,timeout))return false;
        shutdown_hold.store(true);
    }
    // Retrying a failed KILL write reuses only our own completed shutdown hold.
    // The hardware controller's held-button timeout remains independent of this.
    return gpio_set_level(kill_pin,0)==ESP_OK;
}
#else
bool system_power_start() { return false; }
void system_power_accept_startup() {}
bool system_power_charge_standby() {return false;}
bool system_power_ready() { return false; }
bool system_power_ota_allowed() { return false; }
bool system_power_shutdown(uint32_t) { return false; }
power_monitor::Sample system_power_sample() { return {}; }
environment_monitor::Sample system_environment_sample() { return {}; }
usb_input::Result system_usb_input_sample() { return {}; }
#endif
