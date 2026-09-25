#include "environment_monitor.h"
#include "hardware_contract.h"
#include <cstring>
#include <initializer_list>
#include <algorithm>

namespace environment_monitor {
BME280_INTF_RET_TYPE Monitor::read(uint8_t reg,uint8_t *data,uint32_t len,void *ctx) {
    auto &self=*static_cast<Monitor *>(ctx);
    return self.bus_.read_register(self.address_,reg,data,len) ? 0 : -1;
}
BME280_INTF_RET_TYPE Monitor::write(uint8_t reg,const uint8_t *data,uint32_t len,void *ctx) {
    auto &self=*static_cast<Monitor *>(ctx);
    uint8_t bytes[33]{};
    if (!data || len>32) return -1;
    bytes[0]=reg; std::memcpy(bytes+1,data,len);
    return self.bus_.write(self.address_,bytes,len+1) ? 0 : -1;
}
void Monitor::delay(uint32_t us,void *ctx) {
    static_cast<Monitor *>(ctx)->bus_.delay_ms((us+999)/1000);
}
bool Monitor::configuration_matches() {
    bme280_settings actual{};
    return bme280_get_sensor_settings(&actual,&device_)==BME280_OK &&
        actual.osr_t==settings_.osr_t && actual.osr_p==settings_.osr_p &&
        actual.osr_h==settings_.osr_h && actual.filter==settings_.filter;
}
bool Monitor::initialize() {
    device_={}; device_.intf=BME280_I2C_INTF; device_.intf_ptr=this;
    device_.read=read; device_.write=write; device_.delay_us=delay;
    for (const uint8_t addr : {hardware_contract::kEnvironmentAddress0,hardware_contract::kEnvironmentAddress1}) {
        uint8_t identity=0; address_=addr;
        // A BMP280 (0x58) cannot report humidity. Never silently accept it.
        if (!bus_.read_register(addr,0xd0,&identity,1) || identity!=BME280_CHIP_ID) continue;
        if (bme280_init(&device_)!=BME280_OK || !device_.calib_data.dig_t1 || !device_.calib_data.dig_p1 ||
            device_.calib_data.dig_t1==0xffff || device_.calib_data.dig_p1==0xffff ||
            !device_.calib_data.dig_h2) continue;
        settings_={}; settings_.osr_t=settings_.osr_p=settings_.osr_h=BME280_OVERSAMPLING_1X;
        settings_.filter=BME280_FILTER_COEFF_OFF;
        if (bme280_set_sensor_settings(BME280_SEL_ALL_SETTINGS,&settings_,&device_)==BME280_OK &&
            configuration_matches()) return true;
    }
    address_=0; return false;
}
Sample Monitor::poll() {
    Sample result;
    if (!ready_) ready_=initialize();
    if (ready_) {
        uint32_t us=0; uint8_t mode=0;
        ready_=configuration_matches() && bme280_cal_meas_delay(&us,&settings_)==BME280_OK &&
            bme280_set_sensor_mode(BME280_POWERMODE_FORCED,&device_)==BME280_OK &&
            bme280_get_sensor_mode(&mode,&device_)==BME280_OK && (mode==1 || mode==2);
        if (ready_) {
            bus_.delay_ms((us+999)/1000+2);
            uint8_t status=0, bytes[8]{}; bme280_data data{}; bme280_uncomp_data raw{};
            ready_=bus_.read_register(address_,0xf3,&status,1) && !(status & 9) &&
                bus_.read_register(address_,0xf7,bytes,sizeof(bytes)) && configuration_matches();
            if (ready_) {
                raw.pressure=(uint32_t(bytes[0])<<12) | (uint32_t(bytes[1])<<4) | (bytes[2]>>4);
                raw.temperature=(uint32_t(bytes[3])<<12) | (uint32_t(bytes[4])<<4) | (bytes[5]>>4);
                raw.humidity=(uint32_t(bytes[6])<<8) | bytes[7];
                // Bosch skipped/reset sentinels must be rejected BEFORE the
                // manufacturer's compensator clamps values into nominal ranges.
                const bool constant_invalid=std::all_of(bytes,bytes+sizeof(bytes),[](uint8_t b){return b==0xff;}) ||
                    std::all_of(bytes,bytes+sizeof(bytes),[](uint8_t b){return b==0;});
                ready_=!constant_invalid && raw.pressure!=0x80000 && raw.temperature!=0x80000 && raw.humidity!=0x8000 &&
                    bme280_compensate_data(BME280_ALL,&raw,&data,&device_.calib_data)==BME280_OK;
            }
            if (ready_ && std::isfinite(data.temperature) && std::isfinite(data.humidity) &&
                std::isfinite(data.pressure) && data.temperature>=-40 && data.temperature<=85 &&
                data.pressure>=30000 && data.pressure<=110000 && data.humidity>=0 && data.humidity<=100) {
                result.valid=true; result.temperature_c=data.temperature;
                result.humidity_percent=data.humidity; result.pressure_bar=data.pressure/100000.0;
                result.address=address_;
            }
        }
    }
    result.timestamp_ms=bus_.now_ms(); return result;
}
}
