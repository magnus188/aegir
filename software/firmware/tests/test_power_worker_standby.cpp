// Actual ESP system_power worker, USB/BQ/CC/BC drivers, maintenance coordinator,
// standby policy and backlight service. Hardware/RTOS/storage are explicit stubs.
#include "services/system_power.h"
#include "services/maintenance_service.h"
#include "services/backlight_service.h"
#include "services/storage_service.h"
#include "sensors/sensor_hardware.h"
#include <driver/gpio.h>
#include <freertos/task.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <future>
#include <iostream>
#include <map>
#include <thread>

// Compile the unchanged production encoder/provider under a test-local provider
// name. Only this executable substitutes a synthetic active() at the link seam.
// No preprocessor flag, qualified values or test provider enter firmware.
#define active shipped_commissioning_profile
#include "../main/services/charge_profile.cpp"
#undef active
namespace charge_profile {
const Profile &active() {
    static const Profile p=[] {Profile x;x.bench_qualified=true;x.qualification_record=0x54455354;
        x.regulation_mv=4192;x.fast_ma=512;x.precharge_ma=x.termination_ma=64;
        x.precharge_threshold_mv=3000;x.recharge_mv=100;x.safety_hours=5;x.thermal_regulation_c=60;
        x.standby_min_mv=3400;x.standby_min_soc=10;return x;}();return p;
}
}
namespace {
std::atomic<uint32_t> clock_ms{1000};
std::atomic<bool> acquisition{false},heater{false},paused{false};
std::atomic<unsigned> starts{0},kills{0};
std::array<std::atomic<int>,64> pins{};
std::atomic<uint8_t> pwm{48};
void(*worker_fn)(void *)=nullptr;void(*button_fn)(void *)=nullptr;
std::thread::id worker_thread;
std::string scenario;
unsigned passed=0,failed=0,phase=0,off_cycles=0,starts_after_off=0;
unsigned kill_attempts=0;uint32_t irq_release=0;
unsigned standby_input_wakes=0,standby_charge_enables=0,standby_bc_cycles=0;
uint32_t standby_began=0,standby_observation=0;
bool maintenance_launched=false,maintenance_checked=false;
std::future<bool> maint;
struct End{};
void check(bool ok,const char *label) {std::cout<<(ok?"PASS ":"FAIL ")<<label<<'\n';ok?++passed:++failed;}
bool wait_for(const std::function<bool()> &predicate) {
    auto until=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(!predicate()){if(std::chrono::steady_clock::now()>=until)return false;std::this_thread::yield();}return true;
}
void maintenance_during_write() {
    maintenance_launched=true;
    maint=std::async(std::launch::async,[]{return maintenance_begin(MAINTENANCE_RESTART,2500);});
    check(wait_for([]{return maintenance_is_active();}),"External maintenance starts during the actual worker's input/charge transition");
    check(maint.wait_for(std::chrono::milliseconds(2))==std::future_status::timeout && !paused,
          "Maintenance cannot become Held before the worker completes input/charge cancellation");
}
void loop_observe();
struct Bus:system_i2c::Bus {
    std::map<uint8_t,std::array<uint8_t,256>> r;
    bool charger_missing=false;unsigned charger_read_attempts=0,unsafe_held_enables=0;
    unsigned unsafe_held_inputs=0;uint32_t bc_started=0,input_woke=0;
    bool bc_pending=false,source_swapped=false;uint8_t bc_result=0x40,latched_bc_result=0;
    unsigned input_wakes=0,charge_enables=0,bc_cycles=0,cc_acknowledgments=0;
    Bus() {
        auto &b=r[0x6a];b[0x14]=0x39;b[3]=0x1a;b[0]=0x48;b[2]=0x1d;b[4]=0x20;b[5]=0x13;
        b[6]=0x5e;b[7]=0x9d;b[8]=3;b[9]=0x40;b[0xb]=4;
        const uint8_t id[]={0x30,0x32,0x33,0x42,0x53,0x55,0x54,0};
        auto &c=r[0x47];std::memcpy(c.data(),id,8);c[0xa0]=2;c[8]=0x30;c[9]=0x91;
        r[0x5f][1]=2;
    }
    bool write(uint8_t a,const uint8_t *d,size_t n) override {
        ++clock_ms;if((a==0x6a&&charger_missing)||!r.count(a)||n!=2)return false;
        if(a==0x6a && !maintenance_launched &&
           ((scenario=="maintenance" && d[0]==4 && d[1]==8) ||
            (scenario=="maintenance-last" && d[0]==3 && (d[1]&0x30)==0x10)))maintenance_during_write();
        if(a==0x6a && !maintenance_launched && scenario=="hiz-maintenance" && d[0]==0 && !(d[1]&0x80))maintenance_during_write();
        if(a==0x6a && paused && d[0]==3 && (d[1]&0x30)==0x10)++unsafe_held_enables;
        if(a==0x6a && paused && d[0]==0 && !(d[1]&0x80))++unsafe_held_inputs;
        if(a==0x6a && scenario=="hiz-ignored" && phase==0 && d[0]==0 && (d[1]&0x80))return true;
        if(a==0x6a && scenario=="hiz-adc-ignored" && phase==0 && d[0]==2)return true;
        if(a==0x6a && d[0]==0 && (r[a][0]&0x80) && !(d[1]&0x80)) {input_woke=clock_ms;++input_wakes;}
        if(a==0x6a && d[0]==3 && !(r[a][3]&0x10) && (d[1]&0x10))++charge_enables;
        // A completed one-shot belongs to the source present during detection;
        // a later cable swap cannot silently replace its unread status.
        if(a==0x5f && d[0]==1) {bc_pending=d[1]==0x0a;bc_started=clock_ms;if(bc_pending){++bc_cycles;latched_bc_result=bc_result;}}
        if(scenario=="legacy-before-detection" && !source_swapped && a==0x47 && d[0]==9 && (d[1]&0x10)) {
            source_swapped=true;bc_result=0x40;r[a][9]|=0x10;
            check(bc_cycles==0 && !pins[50],"Initial pending CC event is cleared before any BC cycle begins");
        }
        if(a==0x47 && d[0]==9) {if(d[1]&0x10)++cc_acknowledgments;
            r[a][9]=uint8_t((r[a][9]&0xe8)|(d[1]&7)|((r[a][9]&0x10)&&!(d[1]&0x10)?0x10:0));}
        else r[a][d[0]]=d[1];
        if(scenario=="charger-irq" && a==0x6a && d[0]==3 && (d[1]&0x30)==0x10) {pins[49]=0;irq_release=clock_ms+1;}
        return true;
    }
    bool read(uint8_t,uint8_t *,size_t) override{return false;}
    bool read_register(uint8_t a,uint8_t reg,uint8_t *out,size_t n) override {
        ++clock_ms;
        if(a==0x6a){++charger_read_attempts;if(charger_missing)return false;}
        if(a==0x36 && n==2){uint16_t value=reg==8?0x12:reg==2?51200:reg==4?12800:0;out[0]=value>>8;out[1]=value&255;return true;}
        if(!r.count(a)||unsigned(reg)+n>256)return false;
        std::memcpy(out,r[a].data()+reg,n);
        if(a==0x6a && reg==0xb && ((r[a][0]&0x80) || scenario=="hiz-timeout" ||
           ((scenario=="hiz-delay" || scenario=="hiz-maintenance-wait") && uint32_t(clock_ms-input_woke)<2200)))*out&=~4;
        if(a==0x5f && reg==2 && n==1) {
            *out=bc_pending && uint32_t(clock_ms-bc_started)>50 ? latched_bc_result:0;
            if(*out)bc_pending=false;
            if(scenario=="legacy-after-collection" && !source_swapped && *out==0x80) {
                source_swapped=true;bc_result=0x40;r[0x47][9]|=0x10;
                check(!pins[50],"Source changes after DCP status is read while authorization remains LOW");
            }
        }
        return true;
    }
    void delay_ms(uint32_t ms) override {clock_ms+=ms;if(ms==500)loop_observe();else std::this_thread::yield();}
    uint32_t now_ms() override{return clock_ms;}
} bus;
void loop_observe() {
    if(scenario=="legacy-before-detection" || scenario=="legacy-during-detection" || scenario=="legacy-after-collection") {
        const auto source=system_usb_input_sample();
        if(scenario=="legacy-during-detection" && !bus.source_swapped && source.bc.status==bc12::Status::Detecting) {
            check(!source.cc.change_latched && !pins[50],"Fresh BC detection begins only after verified CC interrupt clearance");
            bus.source_swapped=true;bus.bc_result=0x40;bus.r[0x47][9]|=0x10;
        }
        if(source.permission_confirmed || pins[50] || bus.input_wakes) {
            check(false,"Replacement SDP must never inherit DCP authorization or an input-enable edge");throw End{};
        }
        if(bus.source_swapped && source.state==usb_input::State::UsbDefault && source.bc.port==bc12::Port::SDP) {
            check(source.power.input_path==power_monitor::InputPath::Isolated && !source.power.input_enable_command &&
                  !pins[50] && bus.charge_enables==0,"Replacement SDP is classified independently and remains in verified HIZ");
            check(bus.bc_cycles==(scenario=="legacy-before-detection"?1u:2u),
                  "An event after detection begins discards the old cycle; pre-detection events need one fresh cycle");
            throw End{};
        }
        if(clock_ms>20000){check(false,"Attachment-edge recovery reaches bounded SDP isolation");throw End{};}
        return;
    }
    if(scenario.rfind("legacy-",0)==0) {
        const auto source=system_usb_input_sample();
        if(phase==0 && source.permission_confirmed) {button_fn(nullptr);phase=1;}
        else if(phase==1 && system_power_charge_standby()) {
            check(!heater && !acquisition && pwm==0 && pins[50],"Legacy charging port enters dark standby");
            check(bus.cc_acknowledgments==bus.bc_cycles,"Legacy CC acknowledgments occur only before fresh detection, never after its result");
            standby_input_wakes=bus.input_wakes;standby_charge_enables=bus.charge_enables;
            standby_bc_cycles=bus.bc_cycles;standby_began=clock_ms;standby_observation=source.bc.observed_ms;
            phase=2;
        }
        if(phase==2 && clock_ms-standby_began>=120000) {
            if(scenario=="legacy-gap") {clock_ms+=4000;phase=3;return;}
            if(scenario=="legacy-detach") {bus.r[0x47][9]=0x11;phase=3;return;}
            if(scenario=="legacy-replug") {bus.r[0x47][9]|=0x10;phase=3;return;}
            if(scenario=="legacy-reset") {bus.r[0x5f][0]=0;phase=3;return;}
            if(clock_ms-standby_began>=6u*60u*60u*1000u) {
                check(system_power_charge_standby() && !kills && source.permission_confirmed &&
                      !heater && !acquisition && pwm==0,"Stable DCP/CDP stays in standby for six virtual hours");
                check(bus.input_wakes==standby_input_wakes && bus.charge_enables==standby_charge_enables &&
                      bus.bc_cycles==standby_bc_cycles,"Stable attachment never restarts detection, input or charge enable");
                check(source.bc.observed_ms==standby_observation && source.bc.validated_ms>standby_observation &&
                      clock_ms-source.bc.validated_ms<1000,"Old classification timestamp stays honest while attachment health is fresh");
                throw End{};
            }
        }
        if(kills) {
            check(phase==3 && !pins[50] && (bus.r[0x6a][0]&0x80) && !(bus.r[0x6a][3]&0x30),
                  "Lost attachment health revokes permission and isolates charging before shutdown");
            throw End{};
        }
        if(phase<2 && clock_ms>20000) {check(false,"Legacy standby entry is bounded");throw End{};}
        if(phase==3 && clock_ms-standby_began>150000) {check(false,"Loss of attachment health must reach bounded shutdown");throw End{};}
        return;
    }
    if(scenario.rfind("hiz-",0)==0) {
        using State=usb_input::State;using Path=power_monitor::InputPath;
        const auto source=system_usb_input_sample();
        if(scenario.rfind("hiz-maintenance",0)==0 && maintenance_launched && !maintenance_checked) {
            check(maint.wait_for(std::chrono::seconds(2))==std::future_status::ready && maint.get(),
                  "Input-transition race reaches Held only after the actual worker completes isolation readback");
            maintenance_checked=true;
            check(paused && (bus.r[0x6a][0]&0x80) && !pins[50] && !acquisition && !heater &&
                  !system_power_sample().input_enable_command && system_power_sample().input_path==Path::Isolated,
                  "Maintenance barrier sees commanded-off and verified HIZ, not only charge inhibition");
            check(bus.unsafe_held_inputs==0,"Actual BQ HIZ-clear writes cannot occur after maintenance Held");
            maintenance_finish(false);phase=1;return;
        }
        if(scenario=="hiz-ignored" && phase==0 && source.state==State::Fault) {
            check(!pins[50] && !source.power.input_enable_command && source.power.input_path==Path::Enabled &&
                  !(bus.r[0x6a][0]&0x80),"Ignored physical HIZ write reports observed-enabled and LOW authorization without false isolation");
            phase=1;return;
        }
        if(scenario=="hiz-ignored" && phase==1 && source.state==State::UsbDefault) {
            check(source.cc.attached_sink && !source.power.usb_power_good && source.power.input_path==Path::Isolated &&
                  (bus.r[0x6a][0]&0x80) && !pins[50],"Actual worker retries failed HIZ and leaves recognized SDP isolated");throw End{};
        }
        if(scenario=="hiz-adc-ignored" && phase==0 && source.state==State::Fault) {
            check((bus.r[0x6a][0]&0x80) && (bus.r[0x6a][2]&0x40) && !pins[50] &&
                  !source.power.charger_configured && !source.power.bq_adc_idle_confirmed,
                  "Actual worker rejects ignored continuous-ADC clear while input stays HIZ");phase=1;return;
        }
        if(scenario=="hiz-adc-ignored" && phase==1 && source.state==State::Qualified) {
            check(!(bus.r[0x6a][2]&0x40) && source.power.charger_configured && pins[50],
                  "Actual worker can requalify only after inherited ADC mode is cleared and read back");throw End{};
        }
        if(scenario=="hiz-timeout" && source.state==State::Fault) {
            check(source.cc.attached_sink && (bus.r[0x6a][0]&0x80) && !pins[50] &&
                  source.power.input_path==Path::Isolated && clock_ms>=3500 && clock_ms<5000,
                  "Actual recognized-source wake times out with HIZ restored and cable still reported attached");throw End{};
        }
        if((scenario=="hiz-delay" || (scenario.rfind("hiz-maintenance",0)==0 && phase==1)) && source.state==State::Qualified) {
            check(source.power.input_path==Path::Enabled && source.power.input_enable_command && source.power.usb_power_good &&
                  pins[50] && !(bus.r[0x6a][0]&0x80),"Actual worker rechecks PG/input readback before successful delayed or post-maintenance source grant");throw End{};
        }
        if(scenario=="hiz-requalify") {
            if(phase==0 && source.state==State::UsbDefault) {
                check(source.cc.attached_sink && source.bc.port==bc12::Port::SDP && (bus.r[0x6a][0]&0x80) &&
                      source.power.input_path==Path::Isolated && !source.power.usb_power_good && !pins[50],
                      "Actual BC detector classifies SDP independently while BQ HIZ makes PG low");
                bus.r[0x47][9]=0x11;phase=1;return;
            }
            if(phase==1 && source.state==State::Detached) {
                check((bus.r[0x6a][0]&0x80) && !pins[50],"Actual CC detach is observable while the charger is isolated");
                bus.r[0x47][9]=0x91;bus.bc_result=0x80;phase=2;return;
            }
            if(phase==2 && source.state==State::Qualified) {
                check(source.bc.port==bc12::Port::DCP && source.power.input_path==Path::Enabled &&
                      source.power.usb_power_good && pins[50] && !(bus.r[0x6a][3]&0x30),
                      "Fresh actual-driver DCP detection escapes PG-low HIZ trap while battery charging stays inhibited");throw End{};
            }
        }
        if(clock_ms>15000){check(false,"HIZ integration scenario is bounded");throw End{};}return;
    }
    if(maintenance_launched && !maintenance_checked) {
        check(maint.wait_for(std::chrono::seconds(2))==std::future_status::ready && maint.get(),"Actual external maintenance reaches Held only after cancellation/readback");
        maintenance_checked=true;
        check(paused && !(bus.r[0x6a][3]&0x30) && !pins[50] && !acquisition && !heater,
              "Maintenance Held has charge disabled, source permission LOW and acquisition/heater stopped");
        check((bus.r[0x6a][0]&0x80) && bus.unsafe_held_inputs==0,"Charge-standby maintenance also verifies isolated input");
        maintenance_finish(false);
        check(!paused && !maintenance_is_active() && !pins[50] && !acquisition,
              "Releasing another maintenance owner cannot restart an already-pending shutdown");
    }
    if(phase==0 && system_usb_input_sample().permission_confirmed) {
        check(acquisition && heater && pwm==48,"Actual worker starts with analyzer acquisition and saved backlight");
        button_fn(nullptr);phase=1;return;
    }
    if(phase==1 && system_power_charge_standby()) {
        check(!maintenance_is_active() && !paused && !acquisition && !heater && pwm==0 && pins[50] && (bus.r[0x6a][3]&0x30)==0x10,
              "Actual worker enters dark charge standby with source policy alive and storage released");
        check(!system_power_ota_allowed(),"OTA cannot start in intentional standby");
        if(scenario=="normal" || scenario=="charger-irq" || scenario=="kill-retry") {button_fn(nullptr);phase=2;return;}
        if(scenario=="bus-loss") {bus.charger_missing=true;phase=4;return;}
    }
    if(phase==2 && !system_power_charge_standby() && acquisition) {
        check(pwm==48 && heater && !(bus.r[0x6a][3]&0x30),"Actual wake restores brightness and acquisition only after charger inhibition");
        button_fn(nullptr);phase=3;return;
    }
    if(phase==3 && system_power_charge_standby()) {bus.r[0x6a][0xb]=0;phase=4;return;}
    if(kills) {
        if(!off_cycles)starts_after_off=starts;
        ++off_cycles;
        check(!pins[50] && !acquisition && !heater && pwm==0 && maintenance_is_active() && starts==starts_after_off,
              "Successful KILL returning on service power keeps dark screen, LOW permission and no measurement restart");
        if(scenario=="bus-loss" && off_cycles==1) {
            check(!system_power_sample().charge_inhibit_confirmed,"Unavailable charger never becomes a false confirmed electrical inhibit");
            const auto attempts=bus.charger_read_attempts;
            bus.charger_missing=false;
            check(attempts>0,"Worker attempted charger communication before safe-off");
        }
        if(off_cycles>=3) {
            check(!(bus.r[0x6a][3]&0x30) && (bus.r[0x6a][0]&0x80) && bus.unsafe_held_enables==0 && bus.unsafe_held_inputs==0,
                  "Off-state recovery retries charge inhibit and HIZ and never enables either after Held");
            check(kills>=2,"Off retries remain active when hardware cutoff does not kill an externally powered CPU");throw End{};
        }
    }
    if(clock_ms>20000) {check(false,"Scenario must finish within bounded virtual time");throw End{};}
    std::this_thread::yield();
}
}
namespace system_i2c {Bus &shared(){return bus;}bool initialize(){return true;}bool responsive(){return true;}}
namespace environment_monitor {Sample Monitor::poll(){Sample out;out.timestamp_ms=bus_.now_ms();return out;}}
bool storage_pause_writes(bool pause,uint32_t){paused=pause;return true;}
bool sensor_hardware_stop_wait(uint32_t){acquisition=false;heater=false;return true;}
void sensor_hardware_stop(){acquisition=false;heater=false;}
esp_err_t sensor_hardware_start(){++starts;acquisition=true;heater=true;return ESP_OK;}
bool sensor_hardware_service_healthy(){return acquisition;}
extern "C" esp_err_t guition_backlight_set(uint8_t value){pwm=value;return ESP_OK;}
esp_err_t gpio_set_level(gpio_num_t pin,int value){
    if(pin==33&&!value){++kill_attempts;if(scenario=="kill-retry"&&kill_attempts==1)return ESP_FAIL;++kills;}
    pins[pin]=value;if(pin==50)pins[49]=value;return ESP_OK;
}
int gpio_get_level(gpio_num_t pin){
    if(pin==49&&irq_release&&clock_ms>=irq_release){irq_release=0;pins[49]=pins[50].load();}
    return pins[pin];
}
esp_err_t gpio_set_direction(gpio_num_t,int){return ESP_OK;}
esp_err_t gpio_set_pull_mode(gpio_num_t,int){return ESP_OK;}
esp_err_t gpio_install_isr_service(int){return ESP_OK;}
esp_err_t gpio_set_intr_type(gpio_num_t,int){return ESP_OK;}
esp_err_t gpio_isr_handler_add(gpio_num_t,void(*fn)(void *),void *){button_fn=fn;return ESP_OK;}
esp_err_t gpio_isr_handler_remove(gpio_num_t){return ESP_OK;}
TaskHandle_t xTaskGetCurrentTaskHandle(){static thread_local int identity;return &identity;}
BaseType_t xTaskCreate(void(*fn)(void *),const char *,uint32_t,void *,unsigned,TaskHandle_t *){worker_fn=fn;return pdPASS;}
void vTaskDelay(TickType_t ticks){
    if(std::this_thread::get_id()==worker_thread) {
        if(scenario=="hiz-maintenance-wait" && !maintenance_launched && ticks>=20 && ticks<=25 && !(bus.r[0x6a][0]&0x80))maintenance_during_write();
        clock_ms+=ticks;
    } else std::this_thread::sleep_for(std::chrono::milliseconds(1));
}
int64_t esp_timer_get_time(){return int64_t(clock_ms.load())*1000;}
void esp_restart(){throw End{};}
int main(int argc,char **argv) {
    if(argc!=2)return 2;scenario=argv[1];pins[32]=1;worker_thread=std::this_thread::get_id();
    if(scenario.rfind("legacy-",0)==0) {bus.r[0x47][8]=0;bus.bc_result=scenario=="legacy-cdp"?0x20:0x80;}
    if(scenario=="hiz-requalify" || scenario=="hiz-ignored")bus.r[0x47][8]=0;
    if(scenario=="hiz-requalify" || scenario=="hiz-adc-ignored")bus.r[0x6a][2]|=0xc0;
    check(!charge_profile::shipped_commissioning_profile().bench_qualified,"Unchanged production provider still supplies no qualified profile");
    backlight_init();backlight_set(48);check(system_power_start(),"Actual ESP power startup registers real maintenance hooks and worker");
    if(scenario!="probation")system_power_accept_startup();
    try {worker_fn(nullptr);}catch(const End &){}
    if(maint.valid())check(maint.get(),"External maintenance completed");
    if(scenario.rfind("hiz-",0)!=0 && scenario.rfind("legacy-",0)!=0)check(off_cycles>=3,"Actual worker exercised persistent off on retained external host power");
    if(scenario=="kill-retry")check(kill_attempts>kills,"Failed KILL command is actually retried using the completed shutdown hold");
    std::cout<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;
}
