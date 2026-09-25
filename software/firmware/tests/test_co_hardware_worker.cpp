// Actual ESP acquisition adapter, ADC engine, CO decoder/gate and OTA health
// core. UART/I2C/time/GPIO/calibration services are explicit host boundaries.
#include "sensors/sensor_hardware.h"
#include "sensors/co_qualification.h"
#include "services/system_power.h"
#include "services/ota_core.h"
#include "services/gas_calibration_internal.h"
#include <driver/gpio.h>
#include <driver/uart.h>
#include <freertos/task.h>
#include <cstring>
#include <iostream>
namespace {
uint32_t clock_ms=1000,interface_ms=0;unsigned phase=0,passed=0,failed=0;
bool missing_uart=false,interface_on=false;void(*worker)(void *)=nullptr;
uint32_t stored_revision[GAS_CAL_CHANNEL_COUNT]{7,11,13};
uint32_t converted_revision[GAS_CAL_CHANNEL_COUNT]{};
environment_monitor::Sample environment;
struct Done{};
void check(bool ok,const char *s){std::cout<<(ok?"PASS ":"FAIL ")<<s<<'\n';ok?++passed:++failed;}
void observe(){
    sensor_readings_t r{};sensor_hardware_read(&r);
    if(phase==0) {
        check(!r.co_valid && std::isnan(r.co_ppm),"Actual worker publishes no CO before interface warm-up/environment qualification");
        check(sensor_hardware_service_healthy() && ota::boot_healthy(4,sensor_hardware_service_healthy(),true),
              "Missing optional CO/environment does not fail OTA worker acceptance");
        clock_ms+=3000;phase=1;return;
    }
    if(phase==1) {
        check(!r.co_valid,"Actual worker retains warm-up even when UART frames are present");
        if(missing_uart){check(!interface_on && sensor_hardware_service_healthy(),"Failed optional UART setup leaves acquisition service responsive");throw Done{};}
        check(interface_on,"Actual CO UART-enable command succeeds before qualification clock starts");
        clock_ms=interface_ms+co_qualification::kWarmupMs+100;phase=2;return;
    }
    if(phase==2) {
        check(!r.co_valid && std::isnan(r.co_ppm),"Fresh warmed UART is still invalid with missing chamber environment");
        environment.valid=true;environment.temperature_c=20;environment.humidity_percent=50;environment.pressure_bar=1;
        environment.timestamp_ms=clock_ms;phase=3;return;
    }
    if(phase==3) {
        check(r.co_valid && r.co_ppm==2.5f,"Actual publish path accepts qualified CO and preserves its value");
        environment.humidity_percent=10;environment.timestamp_ms=clock_ms;phase=4;return;
    }
    if(phase==4) {
        check(!r.co_valid && std::isnan(r.co_ppm),"Previously valid CO is actively cleared on low chamber humidity");
        check(r.oxygen_calibrated && r.helium_calibrated && r.oxygen_percent==20.9f && r.helium_percent==0,
              "CO-only rejection preserves independent oxygen/helium publication");
        check(r.oxygen_calibration_revision==converted_revision[GAS_CAL_AO2] &&
              r.helium_calibration_revision==converted_revision[GAS_CAL_HELIUM] &&
              stored_revision[GAS_CAL_AO2]>r.oxygen_calibration_revision &&
              stored_revision[GAS_CAL_HELIUM]>r.helium_calibration_revision,
              "Actual worker retains both applied revisions when saves occur immediately after conversion");
        environment.humidity_percent=50;environment.timestamp_ms=clock_ms-4000;phase=5;return;
    }
    if(phase==5) {
        check(!r.co_valid && std::isnan(r.co_ppm),"Actual gate rejects stale environment even when provider leaves valid flag set");
        check(sensor_hardware_service_healthy() && ota::boot_healthy(4,sensor_hardware_service_healthy(),true),
              "Environment fault remains data validity, not a rollback-triggering worker failure");
        throw Done{};
    }
}
struct Bus:system_i2c::Bus {
    bool write(uint8_t,const uint8_t *,size_t) override{return false;}
    bool read(uint8_t,uint8_t *,size_t) override{return false;}
    bool read_register(uint8_t,uint8_t,uint8_t *,size_t) override{return false;}
    void delay_ms(uint32_t ms) override{clock_ms+=ms;if(ms==200)observe();}
    uint32_t now_ms() override{return clock_ms;}
} bus;
}
namespace system_i2c {Bus &shared(){return bus;}}
environment_monitor::Sample system_environment_sample(){return environment;}
void oxygen_selection_get_status(oxygen_selection_status_t *s){*s={};s->choice=OXYGEN_AO2;s->configured=true;s->generation=1;}
bool oxygen_selection_selected_channel(gas_cal_channel_t *c){*c=GAS_CAL_AO2;return true;}
bool oxygen_selection_probe_active(){return false;}
bool gas_calibration_publish_frame(uint32_t,const gas_raw_sample_t *,const bool *,bool){return true;}
void gas_calibration_reset_history(){}
bool gas_calibration_get_record(gas_cal_channel_t c,gas_cal_record_t *r){*r={};r->revision=stored_revision[c];return true;}
bool sensor_get_raw(gas_cal_channel_t,gas_raw_sample_t *r){*r={};r->warmed=r->stable=true;r->mean_voltage_v=.01;return true;}
bool gas_calibration_convert_snapshot(gas_cal_channel_t c,double,double &v,uint32_t &revision){
    v=c==GAS_CAL_HELIUM?0:20.9;revision=stored_revision[c];converted_revision[c]=revision;
    ++stored_revision[c]; // deterministic save at the old convert/get_record boundary
    return true;
}
bool gas_calibration_convert(gas_cal_channel_t c,double voltage,double &v){uint32_t revision;return gas_calibration_convert_snapshot(c,voltage,v,revision);}
esp_err_t gpio_set_level(gpio_num_t pin,int value){if(pin==51){if(value&&!interface_on)interface_ms=clock_ms;interface_on=value;}return ESP_OK;}
esp_err_t gpio_set_direction(gpio_num_t,int){return ESP_OK;}
esp_err_t uart_param_config(uart_port_t,const uart_config_t *){return missing_uart?ESP_FAIL:ESP_OK;}
esp_err_t uart_set_pin(uart_port_t,int,int,int,int){return ESP_OK;}
esp_err_t uart_driver_install(uart_port_t,int,int,int,void *,int){return ESP_OK;}
int uart_read_bytes(uart_port_t,void *out,size_t size,unsigned){
    if(!interface_on||size<9)return 0;uint8_t f[]={0xff,4,3,1,0,25,0x13,0x88,0};
    for(unsigned i=1;i<8;++i)f[8]-=f[i];std::memcpy(out,f,9);return 9;
}
esp_err_t uart_driver_delete(uart_port_t){return ESP_OK;}
BaseType_t xTaskCreate(void(*fn)(void *),const char *,uint32_t,void *,unsigned,TaskHandle_t *){worker=fn;return pdPASS;}
void vTaskDelay(TickType_t ms){clock_ms+=ms;}
void vTaskDelete(TaskHandle_t){throw Done{};}
int64_t esp_timer_get_time(){return int64_t(clock_ms)*1000;}
int main(int argc,char **argv){
    missing_uart=argc>1 && std::string(argv[1])=="missing-uart";
    check(sensor_hardware_start()==ESP_OK,"Actual acquisition task is created without requiring optional hardware");
    try{worker(nullptr);}catch(const Done &){}
    std::cout<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;
}
