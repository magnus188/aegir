#include "sensors/bc12_monitor.h"
#include <array>
#include <cstdio>
#include <functional>
#include <utility>
#include <vector>

namespace {
unsigned passed = 0, failed = 0;
void check(bool ok, const char *message) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", message);
    ok ? ++passed : ++failed;
}
struct Bus : system_i2c::Bus {
    std::array<uint8_t, 4> regs{};
    std::vector<std::pair<uint8_t, uint8_t>> writes;
    std::vector<uint8_t> reads;
    std::function<void(Bus &, uint8_t)> after_read;
    uint32_t now = 1000;
    unsigned read_count = 0, write_count = 0, fail_read_n = 0, fail_write_n = 0;
    unsigned cycles = 0;
    bool unplugged = false, ignore_writes = false, ignore_start = false;
    bool stuck_client = false;
    int ignored_reg = -1;
    bool write(uint8_t address, const uint8_t *data, size_t count) override {
        ++now; ++write_count;
        if (unplugged || address != 0x5f || count != 2 || data[0] > 1 || write_count == fail_write_n) return false;
        writes.emplace_back(data[0], data[1]);
        if (ignore_writes || data[0] == ignored_reg || (ignore_start && data[0] == 1 && (data[1] & 8))) return true;
        if (data[0] == 1 && !(regs[1] & 8) && (data[1] & 8) && regs[0] == 8) ++cycles;
        regs[data[0]] = data[1];
        return true;
    }
    bool read(uint8_t, uint8_t *, size_t) override { return false; }
    bool read_register(uint8_t address, uint8_t reg, uint8_t *out, size_t count) override {
        ++now; ++read_count;
        if (unplugged || address != 0x5f || reg > 3 || count != 1 || read_count == fail_read_n) return false;
        reads.push_back(reg);
        *out = regs[reg];
        if (reg >= 2 && !(reg == 2 && stuck_client)) regs[reg] = 0;
        if (after_read) after_read(*this, reg);
        return true;
    }
    void delay_ms(uint32_t ms) override { now += ms; }
    uint32_t now_ms() override { return now; }
    unsigned status_reads() const {
        unsigned result = 0;
        for (auto r : reads) if (r >= 2) ++result;
        return result;
    }
};
}
int main() {
    using namespace bc12;
    uint32_t fault = UINT32_MAX;
    check(Monitor::decode(0, fault) == Port::Unknown && !fault, "Zero result is pending, not an inferred standard USB port");
    check(Monitor::decode(0x80, fault) == Port::DCP && !fault, "DCP one-hot bit7 decodes without a numeric current grant");
    check(Monitor::decode(0x40, fault) == Port::SDP && !fault, "SDP one-hot bit6 does not imply USB enumeration");
    check(Monitor::decode(0x20, fault) == Port::CDP && !fault, "CDP one-hot bit5 decodes independently of SDP/DCP");
    for (uint8_t raw : {uint8_t(8), uint8_t(4), uint8_t(2)})
        check(Monitor::decode(raw, fault) == Port::Proprietary && fault == Unsupported,
              "Proprietary charger bit is explicitly unsupported");
    check(Monitor::decode(1, fault) == Port::Unknown && fault == Unsupported, "Other charger bit is explicitly unsupported");
    for (uint8_t raw : {uint8_t(0x10), uint8_t(0xc0), uint8_t(0xa0), uint8_t(0x60), uint8_t(0x81), uint8_t(3), uint8_t(0xff)})
        check(Monitor::decode(raw, fault) == Port::Unknown && fault == Ambiguous,
              "Reserved or multiple completion bits cannot fabricate a supported charger");

    Bus bus; Monitor monitor(bus);
    check(monitor.poll(1).status == Status::Idle && bus.reads.empty() && bus.writes.empty(),
          "Uninitialized monitor has no cached classification or I2C side effects");
    // Stale power-up/client/host flags are read-cleared before a new cycle.
    bus.regs = {8, 10, 0x80, 7};
    check(monitor.begin_detection(100) && bus.cycles == 1, "Begin verifies a new LOW-to-HIGH detection cycle after stopping the previous state");
    const std::vector<std::pair<uint8_t, uint8_t>> expected{{1,2},{0,0},{0,8},{1,10}};
    check(bus.writes == expected && bus.regs[1] == 10, "Only power-down/client are written and transceiver auto-on stays disabled");
    auto pending = monitor.poll(100);
    check(pending.status == Status::Detecting && !pending.valid && pending.source_generation == 100 && pending.detection_generation == 1,
          "New pending detection is bound to its CC source generation");
    check(monitor.collect_detection(100).status == Status::Detecting,
          "Cleared stale DCP/host flags cannot qualify the new detection cycle");
    bus.regs[2] = 0x80;
    const auto old_status_reads = bus.status_reads();
    const auto old_writes = bus.writes.size();
    check(monitor.poll(100).status == Status::Detecting && bus.regs[2] == 0x80 &&
          bus.status_reads() == old_status_reads && bus.writes.size() == old_writes,
          "Read-only poll does not acknowledge even a pending detector interrupt");
    auto ready = monitor.collect_detection(100);
    check(ready.valid && ready.status == Status::Ready && ready.port == Port::DCP && ready.raw_client_status == 0x80 && bus.regs[2] == 0,
          "Explicit caller-disabled collection accepts fresh one-hot DCP and verifies READ-CLEAR");
    const auto observed = ready.observed_ms;
    const auto ready_status_reads = bus.status_reads();
    bool retained = true;
    for (unsigned i = 0; i < 100; ++i) {
        const auto s = monitor.poll(100);
        retained &= s.valid && s.observed_ms == observed && s.detection_generation == 1;
    }
    check(retained && bus.status_reads() == ready_status_reads && bus.writes.size() == old_writes,
          "Repeated read-only polls preserve observation age without acknowledging or rerunning detection");
    check(monitor.collect_detection(100).valid && bus.status_reads() == ready_status_reads,
          "Collection of an already completed cycle does not destructively reread the cleared result");
    const auto before_mismatch_reads = bus.reads.size();
    auto changed = monitor.poll(101);
    check(!changed.valid && changed.port == Port::Unknown && (changed.faults & SourceChanged) && bus.reads.size() == before_mismatch_reads,
          "A changed CC generation immediately invalidates cached DCP without any interrupt acknowledgement");
    check(!monitor.poll(100).valid, "Returning the old source generation cannot resurrect an invalidated result");
    check(monitor.begin_detection(101) && bus.cycles == 2, "Recovery requires another observed detection edge");
    bus.regs[2] = 0x20; auto cdp = monitor.collect_detection(101);
    check(cdp.valid && cdp.port == Port::CDP && cdp.source_generation == 101 && cdp.detection_generation == 2,
          "New source receives only its own fresh CDP result");
    const auto no_io_before = bus.reads.size() + bus.writes.size();
    monitor.invalidate();
    check(!monitor.poll(101).valid && bus.reads.size() + bus.writes.size() == no_io_before,
          "Worker-side invalidation removes capability without touching the bus");
    check(monitor.begin_detection(102), "Detector can begin again after explicit invalidation");
    bus.regs[2] = 0x40;
    check(monitor.collect_detection(102).port == Port::SDP, "Fresh SDP remains a classification distinct from charging-port permission");
    check(monitor.stop() && bus.regs[1] == 2 && bus.regs[0] == 0 && !monitor.poll(102).valid,
          "Detach stops detection before power-down and clears cached capability");
    check(bus.writes[bus.writes.size()-2] == std::make_pair(uint8_t(1),uint8_t(2)) &&
          bus.writes.back() == std::make_pair(uint8_t(0),uint8_t(0)), "Detach write ordering follows the manufacturer USB-C instruction");

    for (uint8_t raw : {uint8_t(8), uint8_t(4), uint8_t(2), uint8_t(1), uint8_t(0x10), uint8_t(0xc0)}) {
        Bus b; Monitor m(b); m.begin_detection(1); b.regs[2] = raw;
        auto s = m.collect_detection(1);
        check(!s.valid && s.status == Status::Rejected && s.faults && s.raw_client_status == raw,
              "Unsupported/ambiguous completed results stay visible as rejected, never ready");
    }
    Bus expiry; Monitor expiring(expiry, 1000); expiring.begin_detection(1); expiry.regs[2] = 0x80;
    auto timed = expiring.collect_detection(1); expiry.now = timed.observed_ms + 1000;
    auto stale = expiring.poll(1);
    check(!stale.valid && (stale.faults & Stale) && stale.observed_ms == timed.observed_ms,
          "Missed health interval revokes capability without changing the detection timestamp");
    check(!expiring.collect_detection(1).valid, "Expired result cannot be made fresh by another collection");
    Bus wrap; wrap.now = UINT32_MAX - 60; Monitor wrapping(wrap, 1000); wrapping.begin_detection(2); wrap.regs[2] = 0x20;
    auto wrapped = wrapping.collect_detection(2); wrap.now = wrapped.observed_ms + 900;
    auto fresh_wrap = wrapping.poll(2);
    check(fresh_wrap.valid && fresh_wrap.observed_ms == wrapped.observed_ms,
          "Verified attachment health remains valid across the uint32 clock wrap");
    wrap.now = fresh_wrap.validated_ms + 1000;
    check(!wrapping.poll(2).valid, "Missed health interval expires correctly across the uint32 clock wrap");
    Bus continuous; Monitor attached(continuous,1000); attached.begin_detection(3); continuous.regs[2]=0x80;
    const auto original=attached.collect_detection(3);
    const auto original_reads=continuous.status_reads();
    const auto original_writes=continuous.writes.size();
    bool continuously_verified=true;
    for(unsigned i=0;i<240;++i) {
        continuous.now+=500;
        const auto s=attached.poll(3);
        continuously_verified &= s.valid && s.observed_ms==original.observed_ms &&
            s.validated_ms==continuous.now && s.detection_generation==original.detection_generation;
    }
    check(continuously_verified && continuous.status_reads()==original_reads && continuous.writes.size()==original_writes,
          "Two-minute verified attachment preserves classification without re-detection or fake observation timestamps");
    Bus slow; Monitor delayed(slow,1000); delayed.begin_detection(4); slow.regs[2]=0x80;
    const auto before_slow=delayed.collect_detection(4);
    slow.now=before_slow.validated_ms+995;
    slow.after_read=[](Bus &b,uint8_t){b.now+=2;};
    check(!delayed.poll(4).valid && (delayed.poll(4).faults&Stale),
          "Control reads crossing the health deadline cannot renew an expired capability");
    Bus timeout; Monitor waiting(timeout); waiting.begin_detection(1);
    auto wait_sample = waiting.poll(1); timeout.now = wait_sample.started_ms + kDetectionTimeoutMs;
    timeout.regs[2] = 0x80; auto late = waiting.collect_detection(1);
    check(!late.valid && (late.faults & DetectionTimeout) && timeout.regs[2] == 0x80,
          "A late result is rejected before clearing its interrupt and requires a new cycle");
    Bus bad_source; Monitor unbound(bad_source);
    check(!unbound.begin_detection(0) && bad_source.writes.empty(), "Generation zero cannot start an unbound detection");
    for (uint32_t ttl : {uint32_t(0), UINT32_MAX}) {
        Bus b; Monitor m(b, ttl);
        check(!m.begin_detection(1) && b.writes.empty(), "Zero or wrap-ambiguous TTL cannot disable freshness protection");
    }
    Bus ignored; ignored.ignore_writes = true; Monitor ignored_monitor(ignored);
    check(!ignored_monitor.begin_detection(1), "Acknowledged but ignored configuration writes fail readback");
    Bus ignored_start; ignored_start.ignore_start = true; Monitor stopped(ignored_start);
    check(!stopped.begin_detection(1) && ignored_start.cycles == 0,
          "Acknowledged but ignored start command cannot establish a fresh detection edge");
    Bus uncleared; uncleared.regs[2] = 0x80; uncleared.stuck_client = true; Monitor stuck(uncleared);
    check(!stuck.begin_detection(1) && uncleared.cycles == 0, "A stale result that does not clear prevents a new cycle");
    Bus fresh_stuck; Monitor fresh_monitor(fresh_stuck); fresh_monitor.begin_detection(1);
    fresh_stuck.regs[2] = 0x80; fresh_stuck.stuck_client = true;
    check(!fresh_monitor.collect_detection(1).valid && (fresh_monitor.poll(1).faults & UnclearedStatus),
          "A completion result that does not clear is withheld");
    Bus host; Monitor wrong_role(host); wrong_role.begin_detection(1); host.regs[2] = 0x80; host.regs[3] = 2;
    check(!wrong_role.collect_detection(1).valid && (wrong_role.poll(1).faults & UnexpectedHostEvent),
          "Unexpected host-mode event invalidates even a simultaneous DCP indication");
    Bus mutate; Monitor mutating(mutate); mutating.begin_detection(1); mutate.regs[2] = 0x80;
    mutate.after_read = [](Bus &b, uint8_t reg) { if (reg == 2) b.regs[1] = 2; };
    check(!mutating.collect_detection(1).valid && (mutating.poll(1).faults & Configuration),
          "A detector reset/control change during collection cannot combine with a stale result");
    Bus repeat; Monitor repeating(repeat); repeating.begin_detection(1); repeat.regs[2] = 0x80;
    repeat.after_read = [](Bus &b, uint8_t reg) { if (reg == 2) b.regs[2] = 0x20; };
    check(!repeating.collect_detection(1).valid && (repeating.poll(1).faults & UnclearedStatus),
          "A second different completion between READ-CLEAR checks is rejected");
    Bus disconnect; Monitor missing(disconnect); missing.begin_detection(1); disconnect.regs[2] = 0x80;
    check(missing.collect_detection(1).valid, "Unplug fixture first establishes a supported charger");
    disconnect.unplugged = true;
    check(!missing.poll(1).valid && !missing.stop() && missing.poll(1).port == Port::Unknown,
          "Controller unplug and failed power-down still discard previous capability");
    disconnect.unplugged = false;
    check(!missing.poll(1).valid, "Controller recovery alone cannot restore the previous classification");
    check(missing.begin_detection(1) && !missing.poll(1).valid, "Recovery requires a fresh pending detection cycle");
    for (unsigned nth = 1; nth <= 4; ++nth) {
        Bus b; b.fail_write_n = nth; Monitor m(b);
        check(!m.begin_detection(1) && !m.poll(1).valid, "Every failed setup write prevents a usable result");
    }
    // Every individual I2C read in a successful begin and successful collection
    // is independently fault-injected, including reads after clearing status.
    Bus baseline; Monitor baseline_monitor(baseline); baseline_monitor.begin_detection(1);
    const auto setup_read_count = baseline.read_count;
    bool setup_fail_closed = true;
    for (unsigned nth = 1; nth <= setup_read_count; ++nth) {
        Bus b; b.fail_read_n = nth; Monitor m(b);
        setup_fail_closed &= !m.begin_detection(1) && !m.poll(1).valid;
    }
    check(setup_fail_closed && setup_read_count >= 12, "All setup read failures, including status clearing and start readback, fail closed");
    baseline.regs[2] = 0x80; baseline_monitor.collect_detection(1);
    const auto collection_read_count = baseline.read_count - setup_read_count;
    bool collection_fail_closed = true;
    for (unsigned nth = 1; nth <= collection_read_count; ++nth) {
        Bus b; Monitor m(b); m.begin_detection(1); b.regs[2] = 0x80;
        b.fail_read_n = b.read_count + nth;
        collection_fail_closed &= !m.collect_detection(1).valid && !m.poll(1).valid;
    }
    check(collection_fail_closed && collection_read_count >= 12,
          "All collection read failures withhold capability, including failure after destructive result read");
    for (uint8_t reg : {uint8_t(0), uint8_t(1)}) {
        Bus b; Monitor m(b); m.begin_detection(1); b.regs[2] = 0x80; m.collect_detection(1);
        b.regs[reg] ^= 0x80;
        check(!m.poll(1).valid && (m.poll(1).faults & Configuration),
              "Reserved/control corruption invalidates a cached supported port");
    }
    std::printf("BC1.2 source results: %u passed, %u failed\n", passed, failed);
    return failed ? 1 : 0;
}
