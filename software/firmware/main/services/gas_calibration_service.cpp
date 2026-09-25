#include "gas_calibration_internal.h"
#include "gas_calibration_core.h"
#include "gas_calibration_journal.h"
#include "oxygen_selection_service.h"
#include "oxygen_selection_internal.h"
#include "storage_service.h"
#include <cstdio>
#include <cstring>
#include <mutex>
#include <cmath>

#if defined(ESP_PLATFORM) && !defined(TRIMIX_SIMULATOR)
#include <nvs.h>
#include <esp_timer.h>
#define GAS_REAL_HARDWARE 1
#else
#define GAS_REAL_HARDWARE 0
#endif

namespace {
class PlatformSlots : public gas_cal::Slots {
public:
    gas_cal::SelectionResult selected(gas_cal_channel_t c, bool simulated, uint8_t &slot) override {
#if GAS_REAL_HARDWARE
        if (!storage_init() || !storage_ready()) return gas_cal::SelectionResult::Error;
        nvs_handle_t nvs;
        const esp_err_t opened = nvs_open(simulated ? "gas_sim_v1" : "gas_hw_v1", NVS_READONLY, &nvs);
        if (opened == ESP_ERR_NVS_NOT_FOUND) return gas_cal::SelectionResult::Missing;
        if (opened != ESP_OK) return gas_cal::SelectionResult::Error;
        char key[16]; std::snprintf(key, sizeof(key), "c%d_sel", c);
        const esp_err_t result = nvs_get_u8(nvs, key, &slot);
        nvs_close(nvs);
        return result == ESP_OK ? gas_cal::SelectionResult::Present :
            result == ESP_ERR_NVS_NOT_FOUND ? gas_cal::SelectionResult::Missing : gas_cal::SelectionResult::Error;
#else
        if (!simulated || c < 0 || c >= GAS_CAL_CHANNEL_COUNT) return gas_cal::SelectionResult::Error;
        if (!selected_[c]) return gas_cal::SelectionResult::Missing;
        slot = active_[c]; return gas_cal::SelectionResult::Present;
#endif
    }
    bool read(gas_cal_channel_t c, bool simulated, uint8_t slot, gas_cal_record_t &r) override {
#if GAS_REAL_HARDWARE
        if (!storage_ready()) return false;
        nvs_handle_t nvs;
        if (nvs_open(simulated ? "gas_sim_v1" : "gas_hw_v1", NVS_READONLY, &nvs) != ESP_OK) return false;
        char key[16]; std::snprintf(key, sizeof(key), "c%d_%u", c, slot);
        size_t size = sizeof(r);
        const bool ok = nvs_get_blob(nvs, key, &r, &size) == ESP_OK && size == sizeof(r);
        nvs_close(nvs);
        return ok;
#else
        if (!simulated || c < 0 || c >= GAS_CAL_CHANNEL_COUNT || slot > 1) return false;
        r = memory_[c][slot]; return r.valid;
#endif
    }
    bool write(const gas_cal_record_t &r, uint8_t slot) override {
        if (!storage_init()) return false;
        return storage_run_write([&] {
#if GAS_REAL_HARDWARE
        nvs_handle_t nvs;
        if (nvs_open(r.simulated ? "gas_sim_v1" : "gas_hw_v1", NVS_READWRITE, &nvs) != ESP_OK) return false;
        char key[16]; std::snprintf(key, sizeof(key), "c%d_%u", r.channel, slot);
        const bool ok = nvs_set_blob(nvs, key, &r, sizeof(r)) == ESP_OK && nvs_commit(nvs) == ESP_OK;
        nvs_close(nvs); return ok;
#else
        if (!r.simulated || r.channel < 0 || r.channel >= GAS_CAL_CHANNEL_COUNT || slot > 1) return false;
        memory_[r.channel][slot] = r; return true;
#endif
        });
    }
    bool select(gas_cal_channel_t c, bool simulated, uint8_t slot) override {
        return storage_run_write([&] {
#if GAS_REAL_HARDWARE
        nvs_handle_t nvs;
        if (nvs_open(simulated ? "gas_sim_v1" : "gas_hw_v1", NVS_READWRITE, &nvs) != ESP_OK) return false;
        char key[16]; std::snprintf(key, sizeof(key), "c%d_sel", c);
        const bool ok = nvs_set_u8(nvs, key, slot) == ESP_OK && nvs_commit(nvs) == ESP_OK;
        nvs_close(nvs); return ok;
#else
        if (!simulated || c < 0 || c >= GAS_CAL_CHANNEL_COUNT || slot > 1) return false;
        active_[c] = slot; selected_[c] = true; return true;
#endif
        });
    }
private:
#if !GAS_REAL_HARDWARE
    // Simulator records live only in this simulator process; never access device NVS.
    gas_cal_record_t memory_[GAS_CAL_CHANNEL_COUNT][2]{};
    bool selected_[GAS_CAL_CHANNEL_COUNT]{};
    uint8_t active_[GAS_CAL_CHANNEL_COUNT]{};
#endif
};

PlatformSlots slots;
gas_cal::Journal storage(slots);
gas_cal::Core core(storage, !GAS_REAL_HARDWARE);
std::mutex lock;
#if !GAS_REAL_HARDWARE
uint32_t simulated_now = 0;
#endif
void initialize() { core.initialize(); }
}

uint32_t gas_calibration_now_ms() {
#if GAS_REAL_HARDWARE
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
#else
    return simulated_now;
#endif
}
void gas_calibration_publish(gas_cal_channel_t channel, const gas_raw_sample_t &sample) {
    std::lock_guard<std::mutex> guard(lock);
    initialize();
#if !GAS_REAL_HARDWARE
    simulated_now = sample.timestamp_ms;
#endif
    core.ingest(channel, sample);
}
bool gas_calibration_publish_frame(uint32_t generation,
    const gas_raw_sample_t samples[GAS_CAL_CHANNEL_COUNT],
    const bool sampled[GAS_CAL_CHANNEL_COUNT], bool reset_history) {
    if (!samples || !sampled) return false;
    std::lock_guard<std::recursive_mutex> configuration(oxygen_configuration_mutex());
    oxygen_selection_status_t selected{};
    oxygen_selection_get_status(&selected);
    if (selected.generation!=generation) return false;
    std::lock_guard<std::mutex> guard(lock);
    initialize();
    if (reset_history)
        for (int i=0;i<GAS_CAL_CHANNEL_COUNT;++i) core.reset_history(static_cast<gas_cal_channel_t>(i));
    for (int i=0;i<GAS_CAL_CHANNEL_COUNT;++i) if (sampled[i]) {
#if !GAS_REAL_HARDWARE
        simulated_now=samples[i].timestamp_ms;
#endif
        core.ingest(static_cast<gas_cal_channel_t>(i),samples[i]);
    }
    return true;
}
void gas_calibration_reset_history() {
    std::lock_guard<std::mutex> guard(lock);
    for (int i = 0; i < GAS_CAL_CHANNEL_COUNT; ++i) core.reset_history(static_cast<gas_cal_channel_t>(i));
}
bool gas_calibration_convert(gas_cal_channel_t c, double voltage, double &percent) {
    uint32_t ignored_revision;
    return gas_calibration_convert_snapshot(c,voltage,percent,ignored_revision);
}
bool gas_calibration_convert_snapshot(gas_cal_channel_t c, double voltage,
                                      double &percent, uint32_t &applied_revision) {
    applied_revision=0; percent=NAN;
    std::lock_guard<std::recursive_mutex> configuration(oxygen_configuration_mutex());
    std::lock_guard<std::mutex> guard(lock);
    initialize();
    gas_cal_record_t record{};
    if (!core.record(c,record) ||
        (c!=GAS_CAL_HELIUM && !oxygen_selection_calibration_usable(c,record.revision)) ||
        !core.convert(c,voltage,percent)) return false;
    applied_revision=record.revision;
    return true;
}
extern "C" {
bool sensor_get_raw(gas_cal_channel_t c, gas_raw_sample_t *out) {
    if (!out) return false;
    std::lock_guard<std::mutex> guard(lock);
    initialize(); return core.sample(c, gas_calibration_now_ms(), *out);
}
bool gas_calibration_get_record(gas_cal_channel_t c, gas_cal_record_t *out) {
    if (!out) return false;
    std::lock_guard<std::mutex> guard(lock);
    initialize(); return core.record(c, *out);
}
bool gas_calibration_record_is_current(const gas_cal_record_t *record) {
    return record && gas_cal::record_valid(*record, record->channel, !GAS_REAL_HARDWARE);
}
bool gas_calibration_get_point(gas_cal_channel_t c, unsigned p, gas_cal_point_t *out) {
    if (!out) return false;
    std::lock_guard<std::mutex> guard(lock);
    return core.point(c, p, *out);
}
gas_cal_result_t gas_calibration_capture(gas_cal_channel_t c, unsigned p, const gas_cal_reference_t *reference) {
    if (!reference) return GAS_CAL_BAD_ARGUMENT;
    std::lock_guard<std::recursive_mutex> configuration(oxygen_configuration_mutex());
    if (c >= GAS_CAL_AO2 && c <= GAS_CAL_JJCCR && !oxygen_selection_channel_enabled(c)) return GAS_CAL_SELECTION_REQUIRED;
    std::lock_guard<std::mutex> guard(lock);
    initialize(); return core.capture(c, p, *reference, gas_calibration_now_ms());
}
gas_cal_result_t gas_calibration_save(gas_cal_channel_t c) {
    std::lock_guard<std::recursive_mutex> configuration(oxygen_configuration_mutex());
    if (c >= GAS_CAL_AO2 && c <= GAS_CAL_JJCCR && !oxygen_selection_channel_enabled(c)) return GAS_CAL_SELECTION_REQUIRED;
    std::lock_guard<std::mutex> guard(lock);
    initialize(); return core.commit(c, gas_calibration_now_ms());
}
void gas_calibration_cancel(gas_cal_channel_t c) {
    std::lock_guard<std::mutex> guard(lock); core.cancel(c);
}
bool gas_calibration_is_simulated() { return !GAS_REAL_HARDWARE; }
const char *gas_calibration_channel_label(gas_cal_channel_t c) {
    switch (c) { case GAS_CAL_AO2: return "AO2 oxygen"; case GAS_CAL_JJCCR: return "JJ-CCR oxygen";
        case GAS_CAL_HELIUM: return "Helium (MD62)"; default: return "Unknown channel"; }
}
const char *gas_calibration_result_label(gas_cal_result_t r) {
    switch (r) {
        case GAS_CAL_OK: return "Accepted";
        case GAS_CAL_BAD_ARGUMENT: return "Enter valid gas values and a reference ID";
        case GAS_CAL_NO_SAMPLE: return "No measurement available";
        case GAS_CAL_WARMING: return "Wait for sensor warm-up";
        case GAS_CAL_UNSTABLE: return "Wait for a stable 10-second window";
        case GAS_CAL_STALE: return "Measurement or calibration session is too old";
        case GAS_CAL_SENSOR_FAULT: return "Resolve sensor or ADC fault first";
        case GAS_CAL_NEED_TWO_POINTS: return "Capture both reference points";
        case GAS_CAL_INSUFFICIENT_SPAN: return "References need more signal and concentration separation";
        case GAS_CAL_STORAGE_ERROR: return "Save failed; previous calibration retained";
        case GAS_CAL_WRONG_SOURCE: return "Simulation and hardware records cannot be mixed";
        case GAS_CAL_SELECTION_REQUIRED: return "Confirm the installed oxygen sensor in setup first";
        default: return "Calibration failed";
    }
}
}
