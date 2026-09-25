#include "system_i2c.h"
#include "hardware_contract.h"
#include <mutex>

#if defined(ESP_PLATFORM) && !defined(TRIMIX_SIMULATOR)
#include <driver/i2c_master.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace system_i2c {
namespace {
constexpr uint8_t addresses[]={hardware_contract::kGaugeAddress,hardware_contract::kOxygenAddress,hardware_contract::kHeliumAddress,hardware_contract::kChargerAddress,hardware_contract::kEnvironmentAddress0,hardware_contract::kEnvironmentAddress1,hardware_contract::kUsbCcAddress,hardware_contract::kUsbBc12Address};
class SharedBus final : public Bus {
public:
    bool initialize() { std::lock_guard<std::mutex> guard(mutex_); return init_locked(); }
    bool write(uint8_t addr,const uint8_t *data,size_t count) override {
        std::lock_guard<std::mutex> guard(mutex_);
        auto dev=device_locked(addr);
        return track(dev && data && count && i2c_master_transmit(dev,data,count,100)==ESP_OK);
    }
    bool read(uint8_t addr,uint8_t *data,size_t count) override {
        std::lock_guard<std::mutex> guard(mutex_);
        auto dev=device_locked(addr);
        return track(dev && data && count && i2c_master_receive(dev,data,count,100)==ESP_OK);
    }
    bool read_register(uint8_t addr,uint8_t reg,uint8_t *data,size_t count) override {
        std::lock_guard<std::mutex> guard(mutex_);
        auto dev=device_locked(addr);
        return track(dev && data && count && i2c_master_transmit_receive(dev,&reg,1,data,count,100)==ESP_OK);
    }
    void delay_ms(uint32_t ms) override { vTaskDelay(pdMS_TO_TICKS(ms) ? pdMS_TO_TICKS(ms) : 1); }
    uint32_t now_ms() override { return static_cast<uint32_t>(esp_timer_get_time()/1000); }
    bool responsive() { std::lock_guard<std::mutex> guard(mutex_); return bus_!=nullptr; }
private:
    bool init_locked() {
        if (bus_) return true;
        // I2C0 and GPIO7/8 belong to the existing touch controller.
        i2c_master_bus_config_t cfg{};
        cfg.i2c_port=I2C_NUM_1; cfg.sda_io_num=static_cast<gpio_num_t>(hardware_contract::kSda.gpio); cfg.scl_io_num=static_cast<gpio_num_t>(hardware_contract::kScl.gpio);
        cfg.clk_source=I2C_CLK_SRC_DEFAULT; cfg.glitch_ignore_cnt=7;
        cfg.flags.enable_internal_pullup=false;
        if (i2c_new_master_bus(&cfg,&bus_)!=ESP_OK) { bus_=nullptr; return false; }
        for (unsigned i=0; i<sizeof(addresses)/sizeof(addresses[0]); ++i) {
            i2c_device_config_t dc{}; dc.dev_addr_length=I2C_ADDR_BIT_LEN_7;
            dc.device_address=addresses[i]; dc.scl_speed_hz=hardware_contract::kI2cFrequencyHz;
            if (i2c_master_bus_add_device(bus_,&dc,&devices_[i])!=ESP_OK) {
                for (auto &device:devices_) if (device) { i2c_master_bus_rm_device(device); device=nullptr; }
                i2c_del_master_bus(bus_); bus_=nullptr; return false;
            }
        }
        return true;
    }
    i2c_master_dev_handle_t device_locked(uint8_t addr) {
        if (!init_locked()) return nullptr;
        for (unsigned i=0; i<sizeof(addresses)/sizeof(addresses[0]); ++i) if (addresses[i]==addr) return devices_[i];
        return nullptr;
    }
    bool track(bool ok) {
        if (ok) failures_=0;
        else if (++failures_>=8) {
            // A bounded controller reset attempts bus recovery. A physically
            // held-low line remains a reported fault, never a successful read.
            if (bus_) i2c_master_bus_reset(bus_);
            failures_=0;
        }
        return ok;
    }
    std::mutex mutex_;
    i2c_master_bus_handle_t bus_=nullptr;
    i2c_master_dev_handle_t devices_[sizeof(addresses)/sizeof(addresses[0])]{};
    unsigned failures_=0;
};
SharedBus instance;
}
Bus &shared() { return instance; }
bool initialize() { return instance.initialize(); }
bool responsive() { return instance.responsive(); }
}
#endif
