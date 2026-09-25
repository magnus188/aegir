#include "sensors/acquisition_engine.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>

struct Pins : gas_acquisition::Outputs {
    bool on=false, halt=false;
    unsigned enables=0;
    void heater(bool enabled) override { on=enabled; if(enabled)++enables; }
    bool stopping() const override { return halt; }
};
struct Bus : ads122::Transport {
    struct Device { uint8_t regs[4]{},command=0; bool conversion=false; uint32_t start=0; unsigned conversions[16]{}; } devices[2];
    uint32_t now=0;
    bool fail_o2=false,fail_he=false,never_ready=false;
    double excitation=3.0,bias=1.65;
    Pins *pins=nullptr; uint32_t halt_after=0;
    bool write(uint8_t address,const uint8_t *data,size_t size) override {
        if(address<0x40 || address>0x41 || (address==0x40?fail_o2:fail_he))return false;
        auto &d=devices[address-0x40]; d.command=data[0];
        if(d.command==6) { std::memset(d.regs,0,4);d.conversion=false; }
        else if((d.command&0xf0)==0x40) { if(size!=2)return false;d.regs[(d.command>>2)&3]=data[1]; }
        else if(d.command==8) { d.conversion=true;d.start=now;++d.conversions[d.regs[0]>>4]; }
        return true;
    }
    bool read(uint8_t address,uint8_t *out,size_t size) override {
        if(address<0x40 || address>0x41 || (address==0x40?fail_o2:fail_he))return false;
        auto &d=devices[address-0x40];
        if((d.command&0xf0)==0x20 && size==1) {
            auto reg=(d.command>>2)&3; *out=d.regs[reg];
            if(reg==2 && d.conversion && !never_ready && now-d.start>=50)*out|=0x80;
            return true;
        }
        if(d.command==0x10 && size==3) {
            unsigned mux=d.regs[0]>>4, gain=1u<<((d.regs[0]>>1)&7);
            double volts=address==0x40 ? (mux==0?0.012:0.010) : (mux==0xa?excitation/2:(mux==0xb?bias:0.025));
            int32_t code=static_cast<int32_t>(std::llround(volts*gain/2.048*8388608));
            uint32_t bits=static_cast<uint32_t>(code);out[0]=bits>>16;out[1]=bits>>8;out[2]=bits;
            d.conversion=false;return true;
        }
        return false;
    }
    void delay_ms(uint32_t ms) override { now+=ms; if(pins && halt_after && now>=halt_after)pins->halt=true; }
    uint32_t now_ms() override { return now; }
};
int main() {
    Pins pins; Bus bus;
    gas_acquisition::Engine engine(bus,pins);
    gas_acquisition::Request request{GAS_CAL_JJCCR,false,1};
    auto f=engine.step(request);
    assert(f.reset_history && !f.sampled[0] && f.sampled[1] && f.sampled[2]);
    assert(pins.on && !f.channels[1].faults && !f.channels[1].warmed);
    assert(!bus.devices[0].conversions[0] && bus.devices[0].conversions[6]==1);
    assert(f.channels[1].timestamp_ms<f.channels[2].timestamp_ms);
    assert(std::abs(f.channels[1].voltage_v-.01)<1e-7);
    bus.now+=180000;
    f=engine.step(request); assert(f.channels[1].warmed && f.channels[2].warmed && !f.reset_history);
    request.oxygen_channel=GAS_CAL_AO2;++request.generation;
    f=engine.step(request); assert(f.reset_history && f.sampled[0] && !f.sampled[1] && !f.channels[0].warmed);
    request.probe=true;f=engine.step(request);assert(f.sampled[0] && f.sampled[1] && f.reset_history);
    request.probe=false;bus.fail_o2=true;
    f=engine.step(request);assert(f.channels[0].faults&GAS_FAULT_TRANSPORT);assert(!f.channels[2].faults && pins.on);
    bus.fail_o2=false;bus.now+=5000;f=engine.step(request);assert(!f.channels[0].faults && f.reset_history);
    bus.excitation=2.8;f=engine.step(request);
    assert(f.excitation_latched && !pins.on && (f.channels[2].faults&GAS_FAULT_EXCITATION));
    unsigned enables=pins.enables;bus.now+=6000;bus.excitation=3;
    f=engine.step(request);assert(!pins.on && pins.enables==enables && !f.channels[0].faults);
    assert(f.channels[2].faults&GAS_FAULT_EXCITATION);
    bus.fail_he=true;f=engine.step(request);assert(f.channels[0].faults && !pins.on);
    bus.fail_he=false;bus.now+=5000;f=engine.step(request);assert(!f.channels[0].faults && !pins.on);
    pins.halt=true;f=engine.step(request);assert(!pins.on && !f.sampled[0] && !f.sampled[2]);
    // Stop arriving during reset/integration must never be followed by heater enable.
    Pins interrupted;Bus stopping;stopping.pins=&interrupted;stopping.halt_after=1;
    gas_acquisition::Engine halted(stopping,interrupted);halted.step(request);assert(!interrupted.on && interrupted.enables==0);
    // Initial bus allocation/transport failures recover, with independent ADCs.
    Pins recovering;Bus failed;failed.fail_o2=failed.fail_he=true;
    gas_acquisition::Engine recovery(failed,recovering);f=recovery.step(request);assert(f.channels[0].faults && !recovering.on);
    failed.fail_o2=failed.fail_he=false;failed.now+=5000;f=recovery.step(request);assert(!f.channels[0].faults && recovering.on);
    // Clock rollover and >8 virtual hours use the same production state machine.
    Pins soak_pins;Bus soak;soak.now=0xfffffff0u;
    gas_acquisition::Engine long_run(soak,soak_pins);
    const uint32_t start=soak.now;
    for(unsigned i=0;i<40000;++i) {
        auto sample=long_run.step({GAS_CAL_JJCCR,false,3});
        assert(!sample.channels[1].faults && !sample.channels[2].faults && soak_pins.on);
        soak.now+=200;
    }
    assert(soak.now-start>8*60*60*1000u);
    long_run.stop();assert(!soak_pins.on);
    std::cout<<"Acquisition lifecycle/fault cases and "<<(soak.now-start)/1000<<" seconds virtual soak passed\n";
}
