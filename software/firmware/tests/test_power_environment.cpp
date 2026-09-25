#include "sensors/power_monitor.h"
#include "sensors/environment_monitor.h"
#include <array>
#include <set>
#include <deque>
#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include <tuple>
#include <vector>

unsigned passed=0, failed=0;
void check(bool ok,const char *name) { std::cout << (ok ? "PASS " : "FAIL ") << name << "\n"; ok ? ++passed : ++failed; }

struct FakeBus : system_i2c::Bus {
    std::map<unsigned,std::array<uint8_t,256>> registers;
    std::vector<std::tuple<unsigned,unsigned,unsigned>> writes;
    uint32_t now=1000;
    bool fail=false, ignore_writes=false;
    bool complete_bme=true, ignore_forced=false;
    std::set<std::pair<unsigned,unsigned>> fail_reads;
    std::map<std::pair<unsigned,unsigned>,std::deque<uint8_t>> scripted_bytes;
    std::map<unsigned,uint32_t> forced_due;
    unsigned forced_conversions=0;
    std::vector<std::tuple<unsigned,unsigned,unsigned>> reads;
    bool write(uint8_t address,const uint8_t *data,size_t count) override {
        if (fail || !registers.count(address) || count<2) return false;
        writes.emplace_back(address,data[0],data[1]);
        const bool forcing=(address==0x76 || address==0x77) && data[0]==0xf4 && (data[1]&3);
        if (!ignore_writes && !(ignore_forced && forcing)) {
            for(size_t i=1;i<count;++i) registers[address][uint8_t(data[0]+i-1)]=data[i];
            if (forcing) {
                forced_due[address]=now+10; registers[address][0xf3]|=8; ++forced_conversions;
            }
        }
        return true;
    }
    bool read(uint8_t,uint8_t *,size_t) override { return false; }
    bool read_register(uint8_t address,uint8_t reg,uint8_t *out,size_t count) override {
        reads.emplace_back(address,reg,count);
        if (fail || fail_reads.count({address,reg}) || !registers.count(address) || size_t(reg)+count>256) return false;
        auto &script=scripted_bytes[{address,reg}];
        if (count==1 && !script.empty()) { *out=script.front(); script.pop_front(); return true; }
        std::memcpy(out,registers[address].data()+reg,count); return true;
    }
    void delay_ms(uint32_t ms) override {
        now+=ms;
        if (complete_bme) for(auto &entry:forced_due) if (entry.second && now>=entry.second) {
            registers[entry.first][0xf3]&=~8; registers[entry.first][0xf4]&=~3; entry.second=0;
        }
    }
    uint32_t now_ms() override { return now; }
    void word(uint8_t address,uint8_t reg,uint16_t val) {
        registers[address][reg]=val>>8; registers[address][reg+1]=val&255;
    }
};
void bme_fixture(FakeBus &bus,uint8_t address) {
    auto &r=bus.registers[address]; r.fill(0); r[0xd0]=0x60;
    const int32_t trim[]={27504,26435,-1000,36477,-10685,3024,2855,140,-7,15500,-14600,6000};
    for (unsigned i=0;i<12;++i) { const uint16_t value=static_cast<uint16_t>(trim[i]); r[0x88+2*i]=value&255; r[0x89+2*i]=value>>8; }
    // Synthetic humidity coefficients with a closed-form independent oracle:
    // H1=0,H2=256,H3=0,H4=-100,H5=65,H6=0. Packed signed H4/H5 share E5.
    // Bosch Appendix A reduces to (raw+6400-65*(t_fine-76800)/16384)/256.
    // For rawH=6400 and t_fine=128422 this is49.200003147125244% RH.
    r[0xa1]=0; r[0xe1]=0; r[0xe2]=1; r[0xe3]=0;
    r[0xe4]=0xf9; r[0xe5]=0x1c; r[0xe6]=0x04; r[0xe7]=0;
    const uint32_t pressure=415148,temperature=519888;
    r[0xf7]=uint8_t(pressure>>12); r[0xf8]=uint8_t(pressure>>4); r[0xf9]=uint8_t(pressure<<4);
    r[0xfa]=uint8_t(temperature>>12); r[0xfb]=uint8_t(temperature>>4); r[0xfc]=uint8_t(temperature<<4);
    r[0xfd]=0x19; r[0xfe]=0;
}
void raw20(FakeBus &bus,uint8_t reg,uint32_t value,uint8_t address=0x76) {
    auto &r=bus.registers[address]; r[reg]=uint8_t(value>>12); r[reg+1]=uint8_t(value>>4); r[reg+2]=uint8_t(value<<4);
}
int main() {
    using namespace power_monitor;
    FakeBus bus;
    bus.word(0x36,0x08,0x0012); bus.word(0x36,0x02,51200);
    bus.word(0x36,0x04,50*256+128); bus.word(0x36,0x16,0xfff6);
    auto &chg=bus.registers[0x6a]; chg[0x14]=0x39; chg[0x03]=0x1a;
    chg[0x00]=0x48; chg[0x02]=0x1d; chg[0x07]=0x9d; chg[0x0b]=4;
    Monitor monitor(bus);
    auto result=monitor.poll();
    check(result.gauge_valid && result.charger_valid && result.charger_configured, "Existing driver check 1");
    check(result.voltage_mv==4000 && result.soc_percent==50.5f, "Existing driver check 2");
    check(std::abs(result.rate_percent_hour+2.08f)<0.0001f, "Existing driver check 3");
    check(!result.charging && result.usb_power_good, "Existing driver check 4");
    check(bus.writes.front()==std::make_tuple(0x6au,0x00u,0xc0u), "Charger input is isolated before the rest of host configuration");
    check((chg[0x00]&0x7f)==0x40 && !(chg[0x03]&0x30) && !(chg[0x07]&0x30), "Existing driver check 6");
    check(Monitor::ota_allowed(result,bus.now), "Existing driver check 7");
    check(!Monitor::ota_allowed(result,bus.now+3001), "Existing driver check 8");
    result.soc_percent=29; check(!Monitor::ota_allowed(result,bus.now), "Existing driver check 9");
    result.soc_percent=50; result.voltage_mv=NAN; check(!Monitor::ota_allowed(result,bus.now), "Existing driver check 10");
    bus.fail=true; result=monitor.poll();
    check(!result.gauge_valid && !result.charger_valid && std::isnan(result.voltage_mv), "Existing driver check 11");
    bus.fail=false; chg[0x03]|=0x30; bus.ignore_writes=true;
    result=monitor.poll(); check(!result.charger_configured && !Monitor::ota_allowed(result,bus.now), "Existing driver check 12");
    bus.ignore_writes=false; result=monitor.poll(); check(result.charger_configured, "Existing driver check 13");
    chg[0x14]=0; result=monitor.poll(); check(!result.charger_valid, "Existing driver check 14");
    Sample wrong; check(!Monitor::decode_gauge(0xffff,51200,12800,0,wrong), "Existing driver check 15");
    check(!Monitor::decode_gauge(0x12,0xffff,12800,0,wrong), "Existing driver check 16");
    check(!Monitor::decode_gauge(0x12,1,12800,0,wrong), "Existing driver check 17");
    // BMP280 is the commonly substituted module: both addresses must reject it.
    FakeBus environmental;
    environmental.registers[0x76][0xd0]=0x58;
    environmental.registers[0x77][0xd0]=0x58;
    environment_monitor::Monitor env(environmental);
    check(!env.poll().valid, "Existing driver check 18");
    environmental.registers[0x76][0xd0]=0x60; // No factory trim coefficients.
    check(!env.poll().valid, "Existing driver check 19");
    check(std::isnan(env.poll().humidity_percent), "Existing driver check 20");
    // Published Bosch compensation example: checks real manufacturer code with
    // independent expected outputs, not a copied compensator implementation.
    bme280_calib_data calibration{};
    calibration.dig_t1=27504; calibration.dig_t2=26435; calibration.dig_t3=-1000;
    calibration.dig_p1=36477; calibration.dig_p2=-10685; calibration.dig_p3=3024;
    calibration.dig_p4=2855; calibration.dig_p5=140; calibration.dig_p6=-7;
    calibration.dig_p7=15500; calibration.dig_p8=-14600; calibration.dig_p9=6000;
    bme280_uncomp_data raw{}; raw.temperature=519888; raw.pressure=415148;
    bme280_data compensated{};
    check(bme280_compensate_data(BME280_TEMP|BME280_PRESS,&raw,&compensated,&calibration)==BME280_OK, "Existing driver check 21");
    check(std::abs(compensated.temperature-25.0825)<0.001, "Existing driver check 22");
    check(std::abs(compensated.pressure-100653.27)<0.1, "Existing driver check 23");

    // MAX17048 full words are big endian. A fractional SOC over100 is saturated
    // for presentation, while invalid all-ones words are not a valid gauge.
    Sample gauge;
    check(Monitor::decode_gauge(0x001f,51200,100*256+128,0x000a,gauge) && gauge.soc_percent==100 &&
          std::abs(gauge.rate_percent_hour-2.08f)<0.0001f, "Gauge supports family version and clips model SOC to100%");
    check(Monitor::decode_gauge(0x0012,32000,0,0x8000,gauge) && gauge.voltage_mv==2500 && gauge.soc_percent==0 &&
          gauge.rate_percent_hour<0, "Zero SOC and signed minimum rate are decoded without underflow");
    check(!Monitor::decode_gauge(0x0012,31999,0,0,gauge) && !Monitor::decode_gauge(0x0012,57601,0,0,gauge),
          "Gauge voltage plausibility bounds reject both sides");
    check(Monitor::decode_gauge(0x0012,57600,100*256,0,gauge) && gauge.voltage_mv==4500,
          "Gauge upper voltage boundary remains inclusive");
    bus.word(0x36,0x08,0x1200); result=monitor.poll();
    check(!result.gauge_valid, "Swapped version bytes cannot identify the gauge");
    bus.word(0x36,0x08,0x0012); chg[0x14]=0x39;
    bus.fail_reads.insert({0x36,0x16}); result=monitor.poll();
    check(!result.gauge_valid && result.charger_valid, "Partial gauge read failure does not inherit prior gauge values");
    bus.fail_reads.clear(); result=monitor.poll();
    check(result.gauge_valid && result.charger_valid, "Independent gauge and charger recover after read failures");
    bus.scripted_bytes[{0x6a,0x0c}]={0x80,0}; result=monitor.poll();
    check(result.faults_latched==0x80 && result.faults_current==0 && !result.charger_configured &&
          !Monitor::ota_allowed(result,bus.now), "Latched watchdog event invalidates configuration even after current fault clears");
    result=monitor.poll();
    check(result.charger_configured && Monitor::ota_allowed(result,bus.now), "Configuration is reverified after watchdog history");
    bus.scripted_bytes[{0x6a,0x0c}]={0x20,0x08}; result=monitor.poll();
    check(result.faults_latched==0x20 && result.faults_current==0x08 && !Monitor::ota_allowed(result,bus.now),
          "Separate latched and current fault reads are preserved and current fault blocks OTA");
    bus.fail_reads.insert({0x6a,0x0c}); result=monitor.poll();
    check(!result.charger_valid && !Monitor::ota_allowed(result,bus.now), "Fault-register communication failure blocks OTA");
    bus.fail_reads.clear(); chg[0x00]=0xC8; chg[0x03]=0x3A; chg[0x07]=0xBD;
    result=monitor.poll();
    check(result.charger_configured && (chg[0x00]&0x80) && (chg[0x07]&0x08) && !(chg[0x03]&0x30),
          "Inhibited configuration preserves high-impedance mode and safety-timer enable");
    result.timestamp_ms=0xfffffff0; check(Monitor::ota_allowed(result,0x20), "OTA freshness arithmetic survives timer wrap");
    for (uint8_t state=0;state<4;++state) {
        chg[0x0b]=static_cast<uint8_t>(4|(state<<3)); result=monitor.poll();
        check(result.charger_state==state && result.charging==(state==1 || state==2), "Charging state decoder distinguishes idle/precharge/fast/done");
    }
    auto before_writes=bus.writes.size(); chg[0x14]=0x28; result=monitor.poll();
    check(!result.charger_valid && bus.writes.size()==before_writes, "Wrong charger part number prevents configuration writes");
    chg[0x14]=0x39; result=monitor.poll();
    check(result.charger_valid && result.charger_configured, "Correct charger recovers after wrong-ID observation");

    check(!monitor.set_input_limit_ma(99) && !monitor.set_input_limit_ma(1450) && !monitor.set_input_limit_ma(125),
          "Input current API rejects unsupported or excessive limits");
    check(monitor.set_input_limit_ma(1400), "Qualified source can program1400mA with readback");
    result=monitor.poll();
    check(result.charger_configured && result.input_limit_ma==1400 && (chg[0x00]&0x7f)==0x5a && !(chg[0x03]&0x30),
          "Raised input ceiling persists without enabling charge or OTG");
    bus.ignore_writes=true;
    check(!monitor.set_input_limit_ma(100), "Ignored current-limit write is rejected");
    bus.ignore_writes=false; result=monitor.poll();
    check(result.charger_configured && result.input_limit_ma==100 && (chg[0x00]&0x7f)==0x40,
          "Failed current configuration recovers to100mA baseline");
    check(monitor.set_input_limit_ma(1400), "Source limit can be requalified after recovery");
    bus.scripted_bytes[{0x6a,0x0c}]={0x80,0}; result=monitor.poll();
    check(!result.charger_configured, "Watchdog history invalidates raised limit permission");
    result=monitor.poll();
    check(result.charger_configured && result.input_limit_ma==100, "Watchdog recovery cannot inherit a raised limit");

    FakeBus full; bme_fixture(full,0x76); environment_monitor::Monitor full_env(full);
    auto env_result=full_env.poll();
    check(env_result.valid && env_result.address==0x76 && std::abs(env_result.temperature_c-25.08247793f)<0.001f &&
          std::abs(env_result.pressure_bar-1.0065327f)<0.000002f, "Full BME register path reproduces independent temperature/pressure example");
    check(env_result.valid && std::abs(env_result.humidity_percent-49.20000315f)<0.0001f,
          "Packed signed humidity trim and raw humidity yield the independent closed-form result");
    check(full.forced_conversions==1 && full.now>=1010 && (full.registers[0x76][0xf4]&3)==0,
          "Environmental acquisition triggers one forced measurement and waits for completion");
    check(full.registers[0x76][0xf2]==1 && (full.registers[0x76][0xf4]&0xfc)==0x24,
          "All three channels are configured for1x oversampling");
    full.registers.erase(0x76); env_result=full_env.poll();
    check(!env_result.valid && std::isnan(env_result.humidity_percent) && std::isnan(env_result.pressure_bar),
          "Unplug immediately replaces the prior environmental sample with unavailable values");
    bme_fixture(full,0x76); env_result=full_env.poll();
    check(env_result.valid, "Environment acquisition reinitializes after reconnection");
    full.fail_reads.insert({0x76,0xf7}); env_result=full_env.poll();
    check(!env_result.valid, "Failed data burst does not become a valid environmental sample");
    full.fail_reads.clear(); env_result=full_env.poll(); check(env_result.valid, "Failed environmental data burst can recover");
    full.complete_bme=false; env_result=full_env.poll();
    check(!env_result.valid, "Still-busy BME conversion is not published as fresh");
    full.complete_bme=true; full.delay_ms(20); env_result=full_env.poll(); check(env_result.valid, "Busy conversion recovers through reinitialization");
    FakeBus alternate; alternate.registers[0x76][0xd0]=0x58; bme_fixture(alternate,0x77);
    environment_monitor::Monitor alternate_env(alternate);
    check(alternate_env.poll().valid && alternate_env.poll().address==0x77, "Valid BME at0x77 works when0x76 is a BMP280");
    raw20(full,0xfa,0x80000); env_result=full_env.poll();
    check(!env_result.valid, "Skipped temperature sentinel is rejected before compensation");
    bme_fixture(full,0x76); full_env.poll(); raw20(full,0xf7,0x80000); env_result=full_env.poll();
    check(!env_result.valid, "Skipped pressure sentinel is rejected before compensation");
    bme_fixture(full,0x76); full_env.poll(); full.word(0x76,0xfd,0x8000); env_result=full_env.poll();
    check(!env_result.valid, "Skipped humidity sentinel is rejected before compensation");
    FakeBus no_humidity; bme_fixture(no_humidity,0x76);
    no_humidity.registers[0x76][0xa1]=0;
    for (unsigned reg=0xe1;reg<=0xe7;++reg) no_humidity.registers[0x76][reg]=0;
    environment_monitor::Monitor missing_humidity_trim(no_humidity);
    check(!missing_humidity_trim.poll().valid, "Missing humidity trim cannot produce a fabricated valid zero RH");
    FakeBus erased; bme_fixture(erased,0x76); erased.registers[0x76][0x88]=erased.registers[0x76][0x89]=0xff;
    environment_monitor::Monitor erased_trim(erased);
    check(!erased_trim.poll().valid, "Erased temperature trim is not compensated into a plausible environment");
    FakeBus ignored; bme_fixture(ignored,0x76); ignored.ignore_writes=true;
    environment_monitor::Monitor ignored_config(ignored);
    check(!ignored_config.poll().valid, "Ignored environmental configuration writes cannot publish stale raw values as a fresh reading");
    FakeBus no_trigger; bme_fixture(no_trigger,0x76); no_trigger.ignore_forced=true;
    environment_monitor::Monitor ignored_forced(no_trigger);
    check(!ignored_forced.poll().valid && no_trigger.registers[0x76][0xf2]==1 &&
          (no_trigger.registers[0x76][0xf4]&0xfc)==0x24,
          "ACKed but ignored forced command cannot publish the otherwise valid old raw sample");
    no_trigger.ignore_forced=false;
    check(ignored_forced.poll().valid, "Environmental acquisition recovers after forced-command failure");
    no_trigger.registers[0x76][0xf2]=0;
    check(!ignored_forced.poll().valid, "Unexpected loss of humidity configuration invalidates the current sample");
    check(ignored_forced.poll().valid, "Environmental configuration is restored after mismatch rejection");
    FakeBus erased_data; bme_fixture(erased_data,0x76);
    for (unsigned reg=0xf7;reg<=0xfe;++reg) erased_data.registers[0x76][reg]=0xff;
    environment_monitor::Monitor erased_raw(erased_data);
    auto erased_result=erased_raw.poll();
    check(!erased_result.valid, "All-ones environmental raw burst cannot become a valid clamped measurement");
    for (unsigned reg=0xf7;reg<=0xfe;++reg) erased_data.registers[0x76][reg]=0;
    check(!erased_raw.poll().valid, "All-zero environmental raw burst cannot become a valid clamped measurement");
    bme_fixture(erased_data,0x76);
    check(erased_raw.poll().valid, "Valid environmental data recovers after an erased burst");
    FakeBus dry; bme_fixture(dry,0x76); dry.word(0x76,0xfd,0);
    dry.registers[0x76][0xe4]=dry.registers[0x76][0xe5]=dry.registers[0x76][0xe6]=0;
    environment_monitor::Monitor dry_env(dry); const auto dry_result=dry_env.poll();
    check(dry_result.valid && dry_result.humidity_percent==0,
          "Legitimate zero humidity is not confused with an invalid complete data burst");
    std::cout << "Power/environment results: " << passed << " passed, " << failed << " failed\n";
    return failed ? 1 : 0;
}
