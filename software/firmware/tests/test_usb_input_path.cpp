// Production BQ driver; only electrical I2C/register behavior is substituted.
#include "sensors/power_monitor.h"
#include <array>
#include <functional>
#include <iostream>
struct Bus:system_i2c::Bus {
    std::array<uint8_t,256> r{};uint32_t now=1000,latency=0;
    bool ignore_set=false,ignore_clear=false,absent=false,read_lost=false,ignore_adc=false;
    uint32_t adc_busy_until=0;
    unsigned writes=0;std::function<void(uint8_t,uint8_t)> after_write;
    Bus(){r[0x14]=0x39;r[0]=0x48;r[2]=0x1d;r[3]=0x1a;r[7]=0x9d;r[0xb]=4;}
    bool write(uint8_t a,const uint8_t *b,size_t n) override {
        now+=latency;if(absent||a!=0x6a||n!=2)return false;++writes;
        if(!(b[0]==0 && ((ignore_set&&(b[1]&0x80))||(ignore_clear&&!(b[1]&0x80)))) && !(b[0]==2&&ignore_adc))r[b[0]]=b[1];
        if(after_write)after_write(b[0],b[1]);return true;
    }
    bool read(uint8_t,uint8_t *,size_t) override{return false;}
    bool read_register(uint8_t a,uint8_t reg,uint8_t *out,size_t n) override {
        now+=latency;if(absent||read_lost)return false;
        if(a==0x6a&&n==1){*out=r[reg];if(reg==0xb&&(r[0]&0x80))*out&=~4;
            if(reg==2&&adc_busy_until&&now<adc_busy_until)*out|=0x80;return true;}
        if(a==0x36&&n==2){uint16_t v=reg==8?0x12:reg==2?51200:reg==4?12800:0;out[0]=v>>8;out[1]=v&255;return true;}
        return false;
    }
    uint32_t now_ms() override{return now;}
    void delay_ms(uint32_t t) override{now+=t;}
};
unsigned passed=0,failed=0;
void check(bool value,const char *name){std::cout<<(value?"PASS ":"FAIL ")<<name<<'\n';value?++passed:++failed;}
bool guard(void *p){return *static_cast<bool *>(p);}
int main(){
    using power_monitor::Monitor;using power_monitor::InputPath;
    Bus b;Monitor m(b);auto s=m.poll();
    check(s.charger_configured && s.input_path==InputPath::Isolated && !s.input_enable_command && !s.usb_power_good && b.r[0]==0xc0,
          "Startup verifies HIZ and low register ceiling before relying on PG");
    check(m.set_input_enabled(true) && !(b.r[0]&0x80),"Recognized caller may clear HIZ even though prior PG was low");
    s=m.poll();check(s.input_enable_command && s.input_path==InputPath::Enabled && s.usb_power_good,"Commanded enable and actual readback are separately exposed");
    check(m.set_input_limit_ma(1400) && m.set_input_enabled(false) && b.r[0]==0xc0,"Isolation also resets the raised current register to100mA");
    s=m.poll();check(!s.input_enable_command && s.input_path==InputPath::Isolated,"Readback confirms isolation while retaining battery-powered driver service");
    b.ignore_clear=true;check(!m.set_input_enabled(true) && m.input_isolated() && !m.input_enable_command(),"Ignored HIZ-clear write cannot be reported as input enabled");
    b.ignore_clear=false;check(m.set_input_enabled(true),"Source may retry after failed HIZ exit");
    b.ignore_set=true;check(!m.set_input_enabled(false) && !m.input_isolated() && !m.input_enable_command() && m.input_path()==InputPath::Enabled,
          "Ignored HIZ-set write exposes commanded-off versus observed-enabled without false isolation");
    b.ignore_set=false;s=m.poll();check(s.charger_configured && s.input_path==InputPath::Isolated,"Failed isolation is retried during charger configuration recovery");
    b.r[0]&=~0x80;s=m.poll();check(s.charger_configured && !s.input_enable_command && b.r[0]==0xc0,"Unexpected EN_HIZ clear is detected and repaired rather than preserved");
    m.set_input_enabled(true);b.r[0]=0x48;b.r[7]|=0x30;s=m.poll();
    check(s.charger_configured && s.input_path==InputPath::Isolated && !s.input_enable_command && !(b.r[7]&0x30),"Simulated register/watchdog reset restores HIZ and disables watchdog again");
    bool allowed=false;check(!m.set_input_enabled(true,guard,&allowed) && m.input_isolated(),"Cancellation before input write preserves confirmed isolation");
    allowed=true;b.after_write=[&](uint8_t reg,uint8_t value){if(reg==0&&!(value&0x80))allowed=false;};
    check(!m.set_input_enabled(true,guard,&allowed) && m.input_isolated() && b.r[0]==0xc0,"Maintenance arriving after enable write forces HIZ before enable is acknowledged");
    b.after_write={};b.read_lost=true;check(!m.set_input_enabled(false) && m.input_path()==InputPath::Unknown,"Unavailable readback remains unknown, not confirmed HIZ");
    b.read_lost=false;m.poll();const auto prior=b.writes;b.r[0x14]=0;
    check(!m.set_input_enabled(false) && b.writes==prior && m.input_path()==InputPath::Unknown,"Wrong charger identity is never written as a supposed isolate command");
    Bus slow;Monitor timed(slow);timed.poll();slow.latency=800;
    check(!timed.set_input_enabled(true) && timed.input_isolated(),"Successful but slow input programming exceeds its bound and re-isolates");
    Bus wrap;Monitor rollover(wrap);rollover.poll();wrap.now=0xfffffffe;wrap.latency=1;
    check(rollover.set_input_enabled(true),"Input-operation timing is valid across uint32 wrap");
    Bus adc;adc.r[2]|=0xc0;adc.adc_busy_until=2000;Monitor quiet(adc);auto starting=quiet.poll();
    check(starting.charger_configured && !(adc.r[2]&0x40) && !starting.bq_adc_idle_confirmed && starting.input_path==InputPath::Isolated,
          "Inherited continuous ADC is explicitly disabled without claiming an in-flight conversion instantly stopped");
    adc.now=1500;check(!quiet.poll().bq_adc_idle_confirmed,"Busy conversion status keeps quiet-current condition unconfirmed");
    adc.now=2001;check(!quiet.poll().bq_adc_idle_confirmed,"First clear ADC status does not skip conservative settling interval");
    adc.now=2501;check(quiet.poll().bq_adc_idle_confirmed,"Clear ADC status after a full quiet interval is separately reported");
    adc.r[2]|=0x40;adc.ignore_adc=true;auto refused=quiet.poll();
    check(!refused.charger_configured && !refused.bq_adc_idle_confirmed && refused.input_path==InputPath::Isolated,
          "Ignored continuous-ADC disable is a configuration fault while input remains HIZ");
    adc.ignore_adc=false;adc.r[2]=0x80;adc.adc_busy_until=adc.now+800;
    auto pending=quiet.poll();check(pending.charger_configured && !pending.bq_adc_idle_confirmed,
          "Pending single conversion is tracked without rewriting CONV_START high or creating a HIZ-exit deadlock");
    std::cout<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;
}
