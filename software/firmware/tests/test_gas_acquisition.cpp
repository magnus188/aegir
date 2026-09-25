#include "sensors/ads122c04.h"
#include "sensors/ze07_co.h"
#include "services/gas_calibration_core.h"
#include "services/gas_calibration_journal.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {
unsigned failures = 0;
void check(bool ok, const char *name) { std::printf("%s %s\n", ok ? "PASS" : "FAIL", name); if (!ok) ++failures; }
class MockI2c : public ads122::Transport {
public:
    uint8_t reg[4]{}; uint8_t command = 0;
    int32_t code = 123456, old_code = -999;
    uint32_t now = 0, started = 0;
    bool converting = false, never_ready = false, mismatch = false, fail = false;
    unsigned starts = 0, data_reads = 0;
    std::vector<std::vector<uint8_t>> writes;
    bool write(uint8_t address, const uint8_t *data, size_t size) override {
        if (fail || (address != 0x40 && address != 0x41)) return false;
        writes.emplace_back(data, data + size);
        command = data[0];
        if ((command & 0xf0) == 0x40) {
            if (size != 2) return false; // ADS122C04 has per-register commands, not ADS1220 count fields.
            reg[(command >> 2) & 3] = data[1];
        } else if (command == 6) { std::memset(reg, 0, sizeof(reg)); converting = false; }
        else if (command == 8) { started = now; converting = true; ++starts; }
        return true;
    }
    bool read(uint8_t, uint8_t *out, size_t size) override {
        if (fail) return false;
        if ((command & 0xf0) == 0x20) {
            if (size != 1) return false;
            const unsigned index = (command >> 2) & 3;
            *out = reg[index];
            if (index == 2 && converting && !never_ready && now - started >= 50) *out |= 0x80;
            if (mismatch && index == 1) *out ^= 0x02;
            return true;
        }
        if (command == 0x10 && size == 3) {
            const uint32_t v = static_cast<uint32_t>(converting && !never_ready && now - started >= 50 ? code : old_code);
            out[0] = v >> 16; out[1] = v >> 8; out[2] = v;
            converting = false; ++data_reads; return true;
        }
        return false;
    }
    void delay_ms(uint32_t ms) override { now += ms; }
    uint32_t now_ms() override { return now; }
};
class Memory : public gas_cal::Storage {
public:
    gas_cal_record_t saved[2][GAS_CAL_CHANNEL_COUNT]{};
    bool fail = false;
    bool load(gas_cal_channel_t c, bool sim, gas_cal_record_t &r) override { r = saved[sim][c]; return r.valid; }
    bool save(const gas_cal_record_t &r) override { if (fail) return false; saved[r.simulated][r.channel] = r; return true; }
};
class FlashSlots : public gas_cal::Slots {
public:
    gas_cal_record_t slots[2]{};
    bool has_selection = false, fail_write = false, corrupt_readback = false, fail_select = false;
    uint8_t active = 0;
    gas_cal::SelectionResult selected(gas_cal_channel_t, bool, uint8_t &slot) override {
        if (!has_selection) return gas_cal::SelectionResult::Missing;
        slot = active; return gas_cal::SelectionResult::Present;
    }
    bool read(gas_cal_channel_t, bool, uint8_t slot, gas_cal_record_t &record) override {
        record = slots[slot];
        if (corrupt_readback && (!has_selection || slot != active)) record.zero_voltage_v += 1;
        return record.valid;
    }
    bool write(const gas_cal_record_t &record, uint8_t slot) override {
        if (fail_write) return false;
        slots[slot] = record; return true;
    }
    bool select(gas_cal_channel_t, bool, uint8_t slot) override {
        if (fail_select) return false;
        active = slot; has_selection = true; return true;
    }
};
uint32_t sequence = 0, now = 0;
void stable(gas_cal::Core &core, gas_cal_channel_t c, double voltage, bool sim = true) {
    for (unsigned i = 0; i < 12; ++i) {
        gas_raw_sample_t sample{};
        sample.sequence = ++sequence; sample.timestamp_ms = now += 1000;
        sample.simulated = sim; sample.warmed = true; sample.voltage_v = voltage;
        sample.gain = c == GAS_CAL_HELIUM ? 1 : 8;
        core.ingest(c, sample);
    }
}
gas_cal_reference_t reference(float oxygen, float helium = 0) {
    gas_cal_reference_t r{};
    r.oxygen_percent = oxygen; r.helium_percent = helium;
    r.uncertainty_known = true; r.uncertainty_pp = 0.02f;
    std::snprintf(r.reference_id, sizeof(r.reference_id), "test reference"); return r;
}
}
int main() {
    uint8_t regs[4]{};
    check(ads122::Adc::registers(ads122::kOxygenA, regs) && regs[0] == 0x06 && !regs[1] && !regs[2] && !regs[3], "O2 A gain8, PGA on, 20SPS internal reference and excitation currents off");
    check(ads122::Adc::registers(ads122::kOxygenB, regs) && regs[0] == 0x66, "O2 B differential mux2-3");
    check(ads122::Adc::registers(ads122::kHelium, regs) && regs[0] == 0x01, "He gain1 bypass explicit");
    check(ads122::Adc::registers(ads122::kExcitation, regs) && regs[0] == 0xa1, "Excitation single-ended uses bypass gain1");
    check(ads122::Adc::registers(ads122::kBias, regs) && regs[0] == 0xb1, "O2 bias single-ended uses bypass gain1");
    check(!ads122::Adc::registers({8, 8, false}, regs) && !ads122::Adc::registers({0, 3, false}, regs), "Invalid single-ended PGA and non-power-of-two gain rejected");
    const uint8_t negative[3] = {0xff, 0xff, 0xff}, minimum[3] = {0x80, 0, 0};
    check(ads122::Adc::signed_code(negative) == -1 && ads122::Adc::signed_code(minimum) == -8388608, "24-bit sign extension preserves negative differential values");
    MockI2c transport; ads122::Adc adc(transport, 0x40); ads122::Sample sample;
    check(adc.initialize() == ads122::Result::Ok, "Reset and register readback succeeds");
    check(adc.measure(ads122::kOxygenA, sample) == ads122::Result::Ok && sample.code == transport.code && transport.data_reads == 2 && transport.starts == 1, "Old data discarded; only completed new conversion returned");
    check(std::fabs(sample.volts - transport.code * (0.256 / 8388608)) < 1e-12 && sample.gain == 8, "Configured gain determines voltage scaling");
    transport.code = -123456;
    check(adc.measure(ads122::kHelium, sample) == ads122::Result::Ok && sample.volts < 0, "Negative He voltage supported");
    transport.code = 8388607;
    check(adc.measure(ads122::kHelium, sample) == ads122::Result::Clipped, "Positive clipping rejected");
    transport.code = -8388608;
    check(adc.measure(ads122::kHelium, sample) == ads122::Result::Clipped, "Negative clipping rejected");
    transport.never_ready = true;
    check(adc.measure(ads122::kHelium, sample) == ads122::Result::Timeout, "DRDY timeout does not return stale data");
    transport.never_ready = false; transport.mismatch = true;
    check(adc.measure(ads122::kHelium, sample) == ads122::Result::ConfigMismatch, "Reference/configuration mismatch is a fault");
    transport.mismatch = false; transport.fail = true;
    check(adc.measure(ads122::kHelium, sample) == ads122::Result::Transport, "I2C failure reported");

    Memory storage; gas_cal::Core core(storage, true); core.initialize();
    check(core.capture(GAS_CAL_AO2, 0, reference(20.9f), now) == GAS_CAL_NO_SAMPLE, "Calibration requires actual samples");
    stable(core, GAS_CAL_AO2, 0.01046);
    gas_raw_sample_t raw{};
    check(core.sample(GAS_CAL_AO2, now, raw) && raw.stable, "Stable history requires a full observed time window");
    check(core.capture(GAS_CAL_AO2, 0, reference(20.9f), now) == GAS_CAL_OK, "Air reference captured");
    stable(core, GAS_CAL_AO2, 0.01601);
    check(core.capture(GAS_CAL_AO2, 1, reference(32), now) == GAS_CAL_OK && core.commit(GAS_CAL_AO2, now) == GAS_CAL_OK, "Independent second reference saves calibration");
    double concentration = 0;
    check(core.convert(GAS_CAL_AO2, 0.02501, concentration) && std::fabs(concentration - 50) < 0.001, "Saved zero and sensitivity applied");
    gas_cal_record_t original{}, record{}; core.record(GAS_CAL_AO2, original);
    check(original.reference_budget_met && !original.characterization_validated, "Reference budget never becomes physical accuracy qualification");
    FlashSlots flash; gas_cal::Journal journal(flash);
    check(journal.save(original) && journal.load(GAS_CAL_AO2, true, record) && record.crc32 == original.crc32, "Journal selects first record only after verified inactive-slot write");
    auto replacement = original; ++replacement.revision; replacement.crc32 = gas_cal::record_crc(replacement);
    flash.fail_write = true;
    check(!journal.save(replacement) && journal.load(GAS_CAL_AO2, true, record) && record.crc32 == original.crc32, "Failed inactive-slot write preserves active record");
    flash.fail_write = false; flash.fail_select = true;
    check(!journal.save(replacement), "Interrupted selector commit reports failure");
    gas_cal::Journal after_power_loss(flash);
    check(after_power_loss.load(GAS_CAL_AO2, true, record) && record.crc32 == original.crc32, "Restart after draft write uses old committed selection");
    flash.fail_select = false; flash.corrupt_readback = true;
    check(!journal.save(replacement) && journal.load(GAS_CAL_AO2, true, record) && record.crc32 == original.crc32, "Corrupted draft never replaces valid calibration");
    flash.corrupt_readback = false;
    check(journal.save(replacement) && journal.load(GAS_CAL_AO2, true, record) && record.revision == replacement.revision, "Successful verified selector commit publishes complete replacement");
    gas_cal::Core rebooted(storage, true); rebooted.initialize();
    check(rebooted.record(GAS_CAL_AO2, record) && record.crc32 == original.crc32, "Complete validated record reloads after restart");
    gas_raw_sample_t different_gain{};
    different_gain.voltage_v = 0.01046; different_gain.gain = 16; different_gain.warmed = true; different_gain.simulated = true;
    different_gain.sequence = ++sequence; different_gain.timestamp_ms = now;
    rebooted.ingest(GAS_CAL_AO2, different_gain);
    check(!rebooted.convert(GAS_CAL_AO2, different_gain.voltage_v, concentration), "Rebooted gain8 calibration cannot apply to a gain16 sample");
    auto changed_revision = original; ++changed_revision.acquisition_revision; changed_revision.crc32 = gas_cal::record_crc(changed_revision);
    check(!gas_cal::record_valid(changed_revision, GAS_CAL_AO2, true), "Different analog acquisition revision invalidates old calibration");
    gas_cal::Core physical(storage, false); physical.initialize();
    check(!physical.record(GAS_CAL_AO2, record), "Hardware cannot load simulator calibration");
    stable(core, GAS_CAL_AO2, 0.01046);
    check(core.capture(GAS_CAL_AO2, 0, reference(20.9f), now + 3000) == GAS_CAL_STALE, "Stale capture rejected");
    auto invalid_ref = reference(20.9f); invalid_ref.helium_percent = NAN;
    check(core.capture(GAS_CAL_AO2, 0, invalid_ref, now) == GAS_CAL_BAD_ARGUMENT, "Nonfinite gas composition rejected");
    check(core.capture(GAS_CAL_AO2, 0, reference(20.9f), now) == GAS_CAL_OK, "Replacement starts as draft");
    stable(core, GAS_CAL_AO2, 0.01601);
    check(core.capture(GAS_CAL_AO2, 1, reference(32), now) == GAS_CAL_OK, "Replacement second point accepted");
    storage.fail = true;
    check(core.commit(GAS_CAL_AO2, now) == GAS_CAL_STORAGE_ERROR && core.record(GAS_CAL_AO2, record) && record.crc32 == original.crc32, "Failed storage retains previous valid calibration");
    storage.fail = false;
    raw = {}; raw.sequence = ++sequence; raw.timestamp_ms = now += 1000; raw.simulated = true; raw.warmed = true; raw.voltage_v = NAN;
    core.ingest(GAS_CAL_AO2, raw);
    check(core.capture(GAS_CAL_AO2, 1, reference(32), now) == GAS_CAL_SENSOR_FAULT && core.commit(GAS_CAL_AO2, now) == GAS_CAL_SENSOR_FAULT, "Nonfinite/fault blocks capture and save without erasing previous data");
    raw.voltage_v = 0.01; raw.faults = GAS_FAULT_CLIPPED; raw.sequence = ++sequence; core.ingest(GAS_CAL_AO2, raw);
    check(core.capture(GAS_CAL_AO2, 0, reference(20.9f), now) == GAS_CAL_SENSOR_FAULT, "Clipped sample blocks calibration");
    raw.faults = 0; raw.warmed = false; raw.sequence = ++sequence; core.ingest(GAS_CAL_AO2, raw);
    check(core.capture(GAS_CAL_AO2, 0, reference(20.9f), now) == GAS_CAL_WARMING, "Warm-up blocks calibration");
    raw.warmed = true;
    for (int i = 0; i < 12; ++i) { raw.voltage_v = 0.01 + (i % 2) * 0.001; raw.sequence = ++sequence; raw.timestamp_ms = now += 1000; core.ingest(GAS_CAL_AO2, raw); }
    check(core.capture(GAS_CAL_AO2, 0, reference(20.9f), now) == GAS_CAL_UNSTABLE, "Unstable window rejected");
    stable(core, GAS_CAL_HELIUM, 0.030);
    check(core.capture(GAS_CAL_HELIUM, 0, reference(20.9f), now) == GAS_CAL_OK, "He zero captures complete gas composition");
    stable(core, GAS_CAL_HELIUM, -0.006);
    auto he_ref = reference(18, 45); he_ref.uncertainty_known = false;
    check(core.capture(GAS_CAL_HELIUM, 1, he_ref, now) == GAS_CAL_OK && core.commit(GAS_CAL_HELIUM, now) == GAS_CAL_OK, "Measured negative helium sensitivity permitted without inventing a factory curve");
    core.record(GAS_CAL_HELIUM, record);
    check(!record.reference_budget_met && !record.characterization_validated, "Unknown reference uncertainty remains comparison only");
    check(original.acquisition_revision == 1 && gas_cal::record_valid(original, GAS_CAL_AO2, true) && record.acquisition_revision == 2,
          "He revision2 preserves compatible saved O2 revision1 calibration");
    auto prior_he = record; prior_he.acquisition_revision = 1; prior_he.crc32 = gas_cal::record_crc(prior_he);
    check(!gas_cal::record_valid(prior_he, GAS_CAL_HELIUM, true), "He input-filter revision1 calibration cannot apply to the revised protection network");
    prior_he.revision = 7; prior_he.crc32 = gas_cal::record_crc(prior_he);
    FlashSlots archived_flash; archived_flash.has_selection = true; archived_flash.slots[0] = prior_he;
    gas_cal::Journal migrated_journal(archived_flash);
    check(migrated_journal.load(GAS_CAL_HELIUM, true, record) && record.revision == 7 &&
          gas_cal::record_integrity_valid(record, GAS_CAL_HELIUM, true) && !gas_cal::record_valid(record, GAS_CAL_HELIUM, true),
          "Journal retains an intact prior acquisition profile as inactive calibration history");
    check(!migrated_journal.save(prior_he), "An archived profile cannot be saved as a current calibration");
    gas_cal::Core migrated(migrated_journal, true); migrated.initialize();
    stable(migrated, GAS_CAL_HELIUM, 0.030);
    check(migrated.record(GAS_CAL_HELIUM, record) && record.revision == 7 &&
          !migrated.convert(GAS_CAL_HELIUM, 0.012, concentration),
          "Restart retains the prior successful revision but never converts with incompatible coefficients");
    check(migrated.capture(GAS_CAL_HELIUM, 0, reference(20.9f), now) == GAS_CAL_OK,
          "Fresh reference capture remains available after an acquisition profile change");
    stable(migrated, GAS_CAL_HELIUM, -0.006);
    check(migrated.capture(GAS_CAL_HELIUM, 1, reference(18,45), now) == GAS_CAL_OK,
          "Second fresh reference belongs to the current acquisition profile");
    archived_flash.fail_select = true;
    check(migrated.commit(GAS_CAL_HELIUM, now) == GAS_CAL_STORAGE_ERROR &&
          migrated.record(GAS_CAL_HELIUM, record) && record.crc32 == prior_he.crc32,
          "Failed recalibration after profile change preserves the archived successful record");
    archived_flash.fail_select = false;
    check(migrated.commit(GAS_CAL_HELIUM, now) == GAS_CAL_OK && migrated.record(GAS_CAL_HELIUM, record) &&
          record.revision == 8 && record.acquisition_revision == 2 && gas_cal::record_valid(record, GAS_CAL_HELIUM, true),
          "Successful profile recalibration advances the existing revision instead of resetting its identity");
    check(migrated.convert(GAS_CAL_HELIUM, 0.012, concentration) && std::abs(concentration - 22.5) < 0.001,
          "Only newly saved compatible coefficients restore conversion after profile migration");
    gas_cal::Core migrated_restart(migrated_journal, true); migrated_restart.initialize();
    check(migrated_restart.record(GAS_CAL_HELIUM, record) && record.revision == 8 && record.acquisition_revision == 2,
          "Updated acquisition profile and monotonic calibration revision survive restart");
    auto old_gain = original; old_gain.points[0].sample.gain = old_gain.points[1].sample.gain = 16;
    old_gain.crc32 = gas_cal::record_crc(old_gain);
    check(gas_cal::record_integrity_valid(old_gain, GAS_CAL_AO2, true) && !gas_cal::record_valid(old_gain, GAS_CAL_AO2, true),
          "Intact old ADC gain is reviewable history but is incompatible with the current profile");
    old_gain.points[1].sample.gain = 3; old_gain.crc32 = gas_cal::record_crc(old_gain);
    check(!gas_cal::record_integrity_valid(old_gain, GAS_CAL_AO2, true),
          "Malformed archive capture gains are rejected even when the checksum is intact");
    storage.saved[true][GAS_CAL_AO2].zero_voltage_v += 1;
    gas_cal::Core corrupted(storage, true); corrupted.initialize();
    check(!corrupted.record(GAS_CAL_AO2, record), "Corrupt persistent record discarded by CRC");

    uint8_t frame[9] = {0xff, 0x04, 0x03, 0x01, 0, 0x25, 0x13, 0x88, 0x38}; float co = 0;
    check(Ze07CoDecoder::decode(frame, co) && std::fabs(co - 3.7f) < 0.001, "Manufacturer CO frame decoded in ppm, not CO2");
    Ze07CoDecoder decoder; decoder.push(0x99, 100);
    for (uint8_t byte : frame) decoder.push(byte, 100);
    check(decoder.latest(1000, co) && !decoder.latest(3200, co), "CO parser resynchronizes and expires old data");
    frame[8] ^= 1;
    check(!Ze07CoDecoder::decode(frame, co), "CO checksum failure rejected");
    frame[1] = 0x09;
    check(!Ze07CoDecoder::decode(frame, co), "Other gas identifiers are not interpreted as CO");
    std::printf("Gas acquisition failures: %u\n", failures);
    return failures ? 1 : 0;
}
