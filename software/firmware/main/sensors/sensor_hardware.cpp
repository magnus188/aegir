#include "sensor_hardware.h"
#include "acquisition_engine.h"
#include "system_i2c.h"
#include "hardware_contract.h"
#include "co_qualification.h"
#include "services/gas_calibration_internal.h"
#include "services/oxygen_selection_service.h"
#include "services/system_power.h"
#include "services/maintenance_service.h"
#include "services/sd_log_service.h"
#include <cmath>
#include <mutex>

#if defined(ESP_PLATFORM) && !defined(TRIMIX_SIMULATOR)
#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
namespace {
constexpr gpio_num_t heater_pin=static_cast<gpio_num_t>(hardware_contract::kHeliumEnable.gpio),co_enable_pin=static_cast<gpio_num_t>(hardware_contract::kCoEnable.gpio);
constexpr uart_port_t co_uart=UART_NUM_1;
std::mutex state_lock;
sensor_readings_t cache{};
std::atomic<bool> running{false},stopping{false},heartbeat_valid{false},excitation_latched{false};
std::atomic<uint32_t> heartbeat{0};
uint32_t now_ms() { return static_cast<uint32_t>(esp_timer_get_time()/1000); }
sensor_readings_t empty() {
    sensor_readings_t r{}; r.source=SENSOR_SOURCE_HARDWARE; r.status=SENSOR_STATUS_WARMING;
    r.oxygen_percent=r.oxygen_jj_percent=r.helium_percent=r.co_ppm=r.co2_ppm=NAN;
    r.temperature_c=r.humidity_pct=r.pressure_bar=NAN;
    r.calibration_unvalidated=true; r.oxygen_configuration_required=true;
    return r;
}
class Transport : public ads122::Transport {
public:
    bool write(uint8_t addr,const uint8_t *data,size_t size) override { return system_i2c::shared().write(addr,data,size); }
    bool read(uint8_t addr,uint8_t *data,size_t size) override { return system_i2c::shared().read(addr,data,size); }
    void delay_ms(uint32_t ms) override { system_i2c::shared().delay_ms(ms); }
    uint32_t now_ms() override { return ::now_ms(); }
};
class Outputs : public gas_acquisition::Outputs {
public:
    void heater(bool enabled) override {
        std::lock_guard<std::mutex> guard(state_lock);
        gpio_set_level(heater_pin,enabled && !::stopping.load());
    }
    bool stopping() const override { return ::stopping.load(); }
};
void characterize(gas_cal_channel_t channel,const gas_raw_sample_t &r,const oxygen_selection_status_t &selection) {
    gas_cal_record_t record{}; const bool saved=gas_calibration_get_record(channel,&record);
    sd_log_raw(channel,r,selection,saved ? record.revision:0,gas_acquisition::revision_for(channel));
    // Serial capture keeps raw evidence available before any gas-fit validation.
    // Payload starts CSV1; scripts/extract_characterization.py strips the log prefix.
    ESP_LOGI("GAS_RAW","CSV1,%lu,%d,%d,%lu,%lu,%ld,%.9f,%u,%.6f,%.6f,%.4f,%.4f,%.6f,%u,%lu,%lu,%u,%u",
        static_cast<unsigned long>(r.timestamp_ms),int(channel),int(selection.choice),
        static_cast<unsigned long>(r.sequence),static_cast<unsigned long>(selection.generation),
        static_cast<long>(r.adc_code),r.voltage_v,unsigned(r.gain),r.excitation_v,r.bias_v,
        r.temperature_c,r.humidity_pct,r.pressure_bar,unsigned(r.environment_valid),
        static_cast<unsigned long>(saved ? record.revision:0),static_cast<unsigned long>(r.faults),
        unsigned(r.warmed),unsigned(gas_acquisition::revision_for(channel)));
}
void publish(const gas_acquisition::Frame &frame,const oxygen_selection_status_t &selection,
             const co_qualification::Result &co,const environment_monitor::Sample &env) {
    auto out=empty(); out.timestamp_ms=now_ms(); out.sequence=frame.sequence;
    out.oxygen_selection=selection.choice; out.oxygen_selection_generation=selection.generation;
    out.oxygen_configuration_required=!selection.configured;
    out.oxygen_calibration_required=selection.calibration_required;
    out.co_valid=co.valid; out.co_ppm=co.valid ? co.ppm:NAN;
    out.environment_valid=env.valid;
    if (env.valid) { out.temperature_c=env.temperature_c; out.humidity_pct=env.humidity_percent; out.pressure_bar=env.pressure_bar; }
    out.status=selection.configured ? SENSOR_STATUS_STABLE:SENSOR_STATUS_FAULT;
    gas_cal_channel_t selected=GAS_CAL_AO2;
    const bool configured=oxygen_selection_selected_channel(&selected);
    for (int i=0;i<GAS_CAL_CHANNEL_COUNT;++i) {
        auto channel=static_cast<gas_cal_channel_t>(i);
        if (channel!=GAS_CAL_HELIUM && (!configured || channel!=selected)) continue;
        gas_raw_sample_t r{};
        if (!sensor_get_raw(channel,&r) || r.faults) { out.status=SENSOR_STATUS_FAULT; continue; }
        if (!r.warmed) { if(out.status!=SENSOR_STATUS_FAULT)out.status=SENSOR_STATUS_WARMING; continue; }
        if (!r.stable) { if(out.status==SENSOR_STATUS_STABLE)out.status=SENSOR_STATUS_STABILIZING; continue; }
        double percent=NAN; uint32_t applied_revision=0;
        if (!gas_calibration_convert_snapshot(channel,r.mean_voltage_v,percent,applied_revision) || !std::isfinite(percent) || percent<0 || percent>100) {
            out.status=SENSOR_STATUS_FAULT; continue;
        }
        if (channel==GAS_CAL_HELIUM) { out.helium_percent=percent; out.helium_calibrated=true; out.helium_calibration_revision=applied_revision; }
        else {
            out.oxygen_percent=percent; out.oxygen_calibrated=true;
            out.oxygen_calibration_revision=applied_revision;
            if (channel==GAS_CAL_JJCCR) { out.oxygen_jj_percent=percent; out.oxygen_jj_valid=true; }
        }
    }
    if (out.oxygen_calibrated && out.helium_calibrated && out.oxygen_percent+out.helium_percent>100) {
        out.status=SENSOR_STATUS_FAULT; out.oxygen_calibrated=out.helium_calibrated=false;
        out.oxygen_percent=out.helium_percent=NAN;
    }
    std::lock_guard<std::mutex> guard(state_lock); cache=out;
}
void acquisition_task(void *) {
    Transport io; Outputs outputs;
    gas_acquisition::Engine engine(io,outputs,excitation_latched.load());
    const uint32_t boot=io.now_ms();
    uart_config_t uart{};
    uart.baud_rate=hardware_contract::kCoBaud; uart.data_bits=UART_DATA_8_BITS; uart.parity=UART_PARITY_DISABLE;
    uart.stop_bits=UART_STOP_BITS_1; uart.flow_ctrl=UART_HW_FLOWCTRL_DISABLE; uart.source_clk=UART_SCLK_DEFAULT;
    const bool uart_ok=uart_param_config(co_uart,&uart)==ESP_OK &&
        uart_set_pin(co_uart,hardware_contract::kCoTx.gpio,hardware_contract::kCoRx.gpio,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE)==ESP_OK &&
        uart_driver_install(co_uart,256,0,0,nullptr,0)==ESP_OK;
    co_qualification::Monitor co_monitor;
    uint32_t previous_co_faults=UINT32_MAX;
    while(!stopping.load()) {
        oxygen_selection_status_t selected{}; oxygen_selection_get_status(&selected);
        gas_cal_channel_t channel;
        gas_acquisition::Request req;
        if(oxygen_selection_selected_channel(&channel)) req.oxygen_channel=int(channel);
        req.probe=oxygen_selection_probe_active(); req.generation=selected.generation;
        auto frame=engine.step(req);
        excitation_latched.store(frame.excitation_latched || excitation_latched.load());
        if(stopping.load()) break;
        const auto env=system_environment_sample();
        for(int i=0;i<GAS_CAL_CHANNEL_COUNT;++i) if(frame.sampled[i]) {
            auto &r=frame.channels[i];
            r.environment_valid=env.valid;
            if(env.valid) { r.temperature_c=env.temperature_c; r.humidity_pct=env.humidity_percent; r.pressure_bar=env.pressure_bar; }
        }
        if (!gas_calibration_publish_frame(selected.generation,frame.channels,
                                          frame.sampled,frame.reset_history)) continue;
        for(int i=0;i<GAS_CAL_CHANNEL_COUNT;++i) if(frame.sampled[i])
            characterize(static_cast<gas_cal_channel_t>(i),frame.channels[i],selected);
        const uint32_t now=io.now_ms();
        if(uart_ok && now-boot>=2000) {
            std::lock_guard<std::mutex> guard(state_lock);
            const bool enabled=!stopping.load();
            const bool configured=gpio_set_level(co_enable_pin,enabled)==ESP_OK;
            co_monitor.interface_available(enabled && configured,now);
        }
        if(uart_ok) {
            uint8_t bytes[64]; const int count=uart_read_bytes(co_uart,bytes,sizeof(bytes),0);
            for(int i=0;i<count;++i) co_monitor.push(bytes[i],now);
        }
        const auto co=co_monitor.sample(io.now_ms(),{env.valid,env.timestamp_ms,env.temperature_c,env.humidity_percent});
        if(co.faults!=previous_co_faults) {
            ESP_LOGW("CO_STATUS","%s; faults=0x%lx, environment_ms=%lu, temperature_C=%.2f, humidity_RH=%.2f",
                co_qualification::fault_message(co.faults),static_cast<unsigned long>(co.faults),
                static_cast<unsigned long>(env.timestamp_ms),env.temperature_c,env.humidity_percent);
            previous_co_faults=co.faults;
        }
        publish(frame,selected,co,env);
        heartbeat.store(now); heartbeat_valid.store(true);
        io.delay_ms(200);
    }
    engine.stop(); gpio_set_level(co_enable_pin,0);
    if(uart_ok) uart_driver_delete(co_uart);
    gas_calibration_reset_history();
    {
        std::lock_guard<std::mutex> guard(state_lock);
        cache=empty(); cache.status=SENSOR_STATUS_FAULT; running.store(false); heartbeat_valid.store(false);
    }
    vTaskDelete(nullptr);
}
}
esp_err_t sensor_hardware_start() {
    std::lock_guard<std::mutex> guard(state_lock);
    if(running.load()) return stopping.load() ? ESP_ERR_INVALID_STATE:ESP_OK;
    gpio_set_level(heater_pin,0); gpio_set_level(co_enable_pin,0);
    if(gpio_set_direction(heater_pin,GPIO_MODE_OUTPUT)!=ESP_OK || gpio_set_direction(co_enable_pin,GPIO_MODE_OUTPUT)!=ESP_OK) return ESP_FAIL;
    stopping.store(false); heartbeat_valid.store(false); cache=empty();
    running.store(true);
    const BaseType_t created=xTaskCreate(acquisition_task,"gas_acquire",8192,nullptr,4,nullptr);
    if(created!=pdPASS) { running.store(false); cache.status=SENSOR_STATUS_FAULT; return ESP_FAIL; }
    return ESP_OK;
}
esp_err_t sensor_hardware_read(sensor_readings_t *out) {
    if(!out) return ESP_ERR_INVALID_ARG;
    // Startup is explicit; UI reads never restart work during maintenance.
    oxygen_selection_status_t selection{}; oxygen_selection_get_status(&selection);
    std::lock_guard<std::mutex> guard(state_lock); *out=cache;
    if(!running || stopping || now_ms()-cache.timestamp_ms>2500 || selection.generation!=cache.oxygen_selection_generation) {
        *out=empty(); out->status=SENSOR_STATUS_FAULT;
        out->oxygen_selection=selection.choice; out->oxygen_selection_generation=selection.generation;
        out->oxygen_configuration_required=!selection.configured;
        out->oxygen_calibration_required=selection.calibration_required;
    }
    return ESP_OK;
}
void sensor_hardware_stop() {
    stopping.store(true);
    std::lock_guard<std::mutex> guard(state_lock);
    gpio_set_level(heater_pin,0); gpio_set_level(co_enable_pin,0);
}
bool sensor_hardware_stop_wait(uint32_t timeout) {
    sensor_hardware_stop(); const uint32_t start=now_ms();
    while(running.load()) {
        if(now_ms()-start>=timeout) return false;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return true;
}
bool sensor_hardware_service_healthy() { return running && heartbeat_valid && now_ms()-heartbeat.load()<5000; }
#else
esp_err_t sensor_hardware_read(sensor_readings_t *) { return ESP_ERR_INVALID_STATE; }
esp_err_t sensor_hardware_start() { return ESP_ERR_INVALID_STATE; }
void sensor_hardware_stop() {}
bool sensor_hardware_stop_wait(uint32_t) { return true; }
bool sensor_hardware_service_healthy() { return false; }
#endif
