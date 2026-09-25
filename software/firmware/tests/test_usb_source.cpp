#include "sensors/usb_source_monitor.h"
#include <array>
#include <cstring>
#include <cstdio>
#include <deque>
#include <functional>
#include <set>
#include <utility>
#include <vector>

namespace {
unsigned passed=0,failed=0;
void check(bool ok,const char *message) {
    std::printf("%s %s\n",ok ? "PASS":"FAIL",message); ok ? ++passed:++failed;
}
struct Bus : system_i2c::Bus {
    std::array<uint8_t,256> regs{};
    std::vector<std::pair<uint8_t,uint8_t>> writes;
    std::set<uint8_t> failed_reads;
    std::deque<std::array<uint8_t,3>> status_script;
    std::function<void(Bus &)> after_ack;
    uint32_t now=1000;
    bool unplugged=false,fail_write=false,ignore_write=false,irq_stuck=false;
    Bus() {
        const uint8_t id[]={0x30,0x32,0x33,0x42,0x53,0x55,0x54,0};
        std::memcpy(regs.data(),id,8); regs[0xa0]=2; regs[9]=0x20;
    }
    void sink(unsigned mode,bool cc2=false,bool irq=true) {
        regs[8]=static_cast<uint8_t>(mode<<4);
        regs[9]=static_cast<uint8_t>(0x80|(cc2?0x20:0)|(irq?0x10:0)|(regs[9]&7));
    }
    bool write(uint8_t address,const uint8_t *data,size_t count) override {
        ++now;
        if(unplugged || fail_write || address!=0x47 || count!=2) return false;
        writes.emplace_back(data[0],data[1]);
        if(ignore_write) return true;
        if(data[0]==9) {
            const uint8_t irq=(regs[9]&0x10) && (!(data[1]&0x10) || irq_stuck) ? 0x10:0;
            regs[9]=static_cast<uint8_t>((regs[9]&0xe8)|(data[1]&7)|irq);
            if((data[1]&0x10) && after_ack) after_ack(*this);
        } else regs[data[0]]=data[1];
        return true;
    }
    bool read(uint8_t,uint8_t *,size_t) override { return false; }
    bool read_register(uint8_t address,uint8_t reg,uint8_t *out,size_t count) override {
        ++now;
        if(unplugged || address!=0x47 || failed_reads.count(reg) || unsigned(reg)+count>256) return false;
        if(reg==8 && count==3 && !status_script.empty()) {
            std::memcpy(out,status_script.front().data(),3); status_script.pop_front(); return true;
        }
        std::memcpy(out,regs.data()+reg,count); return true;
    }
    void delay_ms(uint32_t ms) override { now+=ms; }
    uint32_t now_ms() override { return now; }
};
}
int main() {
    using namespace usb_source;
    Sample s;
    check(Monitor::decode(0,0x81,0,0,s) && s.attached_sink && s.current==Current::UsbDefault && !s.advertised_ma,
          "USB default never implies enumerated500/900mA permission");
    check(Monitor::decode(0x10,0x81,0,0,s) && s.current==Current::A1_5 && s.advertised_ma==1500,
          "CURRENT_MODE_DETECT01 is1.5A");
    check(Monitor::decode(0x30,0x81,0,0,s) && s.current==Current::A3 && s.advertised_ma==3000,
          "CURRENT_MODE_DETECT11 is3A");
    check(!Monitor::decode(0x20,0x81,0,0,s) && (s.faults&UnsupportedAccessory) && !s.advertised_ma,
          "CURRENT_MODE_DETECT10 is unsupported audio charging, never3A");
    check(Monitor::decode(0x30,0x21,0,0,s) && !s.attached_sink && s.current==Current::Unknown && !s.advertised_ma,
          "Detached status discards even a lingering high-current register");
    check(!Monitor::decode(0x30,0x41,0,0,s) && (s.faults&UnsupportedRole),
          "Attached source role cannot authorize sink consumption");
    check(!Monitor::decode(0x30,0xc1,0,0,s) && (s.faults&UnsupportedRole),
          "Accessory attachment cannot authorize ordinary sink consumption");
    for(unsigned accessory=1;accessory<8;++accessory)
        check(!Monitor::decode(static_cast<uint8_t>(0x30|(accessory<<1)),0x81,0,0,s) && !s.advertised_ma,
              "All accessory and reserved accessory codes are withheld");
    check(Monitor::decode(0x31,0xa1,0x10,0,s) && s.orientation==1 && s.current==Current::A3,
          "CC2 orientation and active-cable bit do not corrupt current decoding");
    check(!Monitor::decode(0x30,0x80,0,0,s) && (s.faults&Configuration),
          "Accessory support must be disabled before valid policy observations");
    for(uint8_t bad : {uint8_t(0x20),uint8_t(0x30),uint8_t(1),uint8_t(8),uint8_t(2),uint8_t(6)})
        check(!Monitor::decode(0x30,0x81,bad,0,s),
              "Source/DRP, disabled termination, reset or source-preference config is rejected");
    check(!Monitor::decode(0x30,0x81,0,4,s) && !Monitor::decode(0x30,0x81,0,1,s),
          "REG45 disabled terminations or nondefault reserved fields are rejected");
    check(!Monitor::decode(0xf0,0x81,0,0,s) && !Monitor::decode(0x30,0x89,0,0,s),
          "Unexpected source advertisement or reserved status fields are rejected");

    Bus bus; bus.sink(3,true); Monitor monitor(bus);
    auto first=monitor.poll();
    check(!first.valid && bus.writes.empty(), "Read-only first poll does not configure or clear an interrupt");
    check(monitor.initialize_disabled_policy() && bus.writes.size()==1 && bus.writes[0]==std::make_pair(uint8_t(9),uint8_t(1)) &&
          (bus.regs[9]&0x10), "Caller-disabled initialization sets accessory bit without clearing pending IRQ");
    auto current=monitor.poll(); const auto epoch=current.generation;
    const auto write_count=bus.writes.size();
    bool stable=true;
    for(unsigned i=0;i<100;++i) { const auto sample=monitor.poll(); stable &= sample.valid && sample.generation==epoch && sample.change_latched; }
    check(stable && bus.writes.size()==write_count, "Sticky IRQ is never auto-acknowledged and does not inflate generation on every poll");
    Sample after;
    check(monitor.acknowledge_change(current,after) && !after.change_latched && after.generation==epoch && after.advertised_ma==3000,
          "Explicit disabled-policy acknowledgement verifies same source and cleared IRQ");
    check(bus.writes.back()==std::make_pair(uint8_t(9),uint8_t(0x11)),
          "W1C write contains only control fields plus interrupt bit, never echoed role bits");
    bus.sink(3,true,true); current=monitor.poll();
    check(current.generation==epoch+1, "A new latched event changes generation even when final source class is unchanged");
    const auto high=current; bus.sink(1,false,true); current=monitor.poll();
    check(current.valid && current.current==Current::A1_5 && current.generation>high.generation,
          "Current downgrade and orientation change invalidate prior source observation");
    const auto before_stale_ack=bus.writes.size();
    check(!monitor.acknowledge_change(high,after) && bus.writes.size()==before_stale_ack,
          "Stale expected source cannot clear a new-source interrupt");
    bus.irq_stuck=true;
    check(!monitor.acknowledge_change(current,after) && after.change_latched,
          "Acknowledged but uncleared W1C interrupt cannot qualify a source");
    bus.irq_stuck=false;
    check(monitor.acknowledge_change(after,after), "A later explicit acknowledgement can recover after a stuck event");
    // Passing one object as expected/output must not accidentally replace the
    // expected observation before it has been checked.
    auto alias=after; bus.sink(3,true,true); const auto before_alias=bus.writes.size();
    check(!monitor.acknowledge_change(alias,alias) && bus.writes.size()==before_alias,
          "Aliased expected/output still rejects a stale source observation");
    current=monitor.poll(); bus.after_ack=[](Bus &b){b.sink(1,false,true);};
    check(!monitor.acknowledge_change(current,after) && after.change_latched && after.current==Current::A1_5,
          "Source changes during W1C handling are withheld for a new qualification cycle");
    bus.after_ack={}; current=monitor.poll(); bus.fail_write=true;
    check(!monitor.acknowledge_change(current,after), "Failed W1C transaction never returns permission");
    bus.fail_write=false;
    bus.unplugged=true; auto missing=monitor.poll();
    check(!missing.valid && (missing.faults&Transport) && !missing.advertised_ma && missing.generation>current.generation,
          "Controller unplug replaces previous high-current information immediately");
    bus.unplugged=false; current=monitor.poll();
    check(current.valid && current.generation>missing.generation, "Controller recovery creates a fresh observation generation");
    bus.regs[0]^=1; const auto before_bad_id=bus.writes.size();
    check(!monitor.poll().valid && !monitor.initialize_disabled_policy() && bus.writes.size()==before_bad_id,
          "Incorrect identity prevents configuration writes and source claims");
    bus.regs[0]^=1; bus.regs[0xa0]=3;
    check(!monitor.poll().valid && !monitor.initialize_disabled_policy(), "Unqualified controller revision is withheld");
    bus.regs[0xa0]=2;
    for(uint8_t reg : {uint8_t(0),uint8_t(0xa0),uint8_t(0x45),uint8_t(8)}) {
        bus.failed_reads.insert(reg); auto broken=monitor.poll();
        check(!broken.valid && (broken.faults&Transport) && broken.current==Current::Unknown,
              "Every partial identity/config/status read failure discards source capability");
        bus.failed_reads.clear();
    }
    bus.status_script={{{0x10,0x81,0}},{{0x30,0x81,0}}};
    auto race=monitor.poll();
    check(!race.valid && (race.faults&Incoherent) && !race.advertised_ma,
          "Split current/attachment register observations cannot fabricate a coherent source");
    bus.status_script={{{0x10,0x81,0}},{{0x10,0x91,0}}};
    check(monitor.poll().change_latched, "IRQ assertion between identical state snapshots is retained");
    Bus ignored; ignored.ignore_write=true; Monitor ignored_monitor(ignored);
    check(!ignored_monitor.initialize_disabled_policy() && !ignored_monitor.poll().valid,
          "ACKed but ignored accessory-disable write fails readback qualification");
    ignored.ignore_write=false; ignored.fail_write=true;
    check(!ignored_monitor.initialize_disabled_policy(), "Failed accessory configuration remains unqualified");
    ignored.fail_write=false;
    check(ignored_monitor.initialize_disabled_policy(), "Detached controller may initialize while consumption remains unqualified");
    auto detached=ignored_monitor.poll();
    check(detached.valid && !detached.attached_sink && !detached.advertised_ma,
          "Successful initialization alone does not grant any source current");
    std::printf("USB source results: %u passed, %u failed\n",passed,failed);
    return failed ? 1:0;
}
