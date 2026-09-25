#include "oxygen_selection_service.h"
#include "oxygen_selection_core.h"
#include "oxygen_selection_internal.h"
#include "gas_calibration_internal.h"
#include "storage_service.h"
#include <mutex>
#include <atomic>

#if defined(ESP_PLATFORM) && !defined(TRIMIX_SIMULATOR)
constexpr bool kSimulated = false;
#else
constexpr bool kSimulated = true;
#endif
namespace {
class PlatformStorage : public oxygen_selection::Storage {
public:
    oxygen_selection::ReadResult read(oxygen_selection::Record &record) override {
        if (!storage_init() || !storage_ready()) return oxygen_selection::ReadResult::Error;
        const auto result = storage_read_blob(kSimulated ? "o2_demo_cfg" : "o2_hw_cfg", oxygen_selection::kSchema, &record, sizeof(record));
        return result == STORAGE_OK ? oxygen_selection::ReadResult::Present :
            result == STORAGE_MISSING ? oxygen_selection::ReadResult::Missing : oxygen_selection::ReadResult::Error;
    }
    bool write(const oxygen_selection::Record &record) override {
        return storage_writes_allowed() && storage_write_blob(kSimulated ? "o2_demo_cfg" : "o2_hw_cfg",
            oxygen_selection::kSchema, &record, sizeof(record));
    }
};
PlatformStorage storage;
oxygen_selection::Core core(storage, kSimulated);
std::recursive_mutex configuration_lock;
std::atomic<bool> probe_active{false};
}
std::recursive_mutex &oxygen_configuration_mutex() { return configuration_lock; }
extern "C" {
void oxygen_selection_set_probe_active(bool active) { probe_active.store(active); }
bool oxygen_selection_probe_active() { return probe_active.load(); }
void oxygen_selection_get_status(oxygen_selection_status_t *out) {
    if (!out) return;
    std::lock_guard<std::recursive_mutex> guard(configuration_lock);
    core.initialize();
    gas_cal_channel_t channel;
    gas_cal_record_t record{};
    if (core.selected(channel)) gas_calibration_get_record(channel, &record);
    *out = core.status(gas_calibration_record_is_current(&record) ? record.revision : 0);
}
bool oxygen_selection_selected_channel(gas_cal_channel_t *out) {
    if (!out) return false;
    std::lock_guard<std::recursive_mutex> guard(configuration_lock);
    core.initialize(); return core.selected(*out);
}
bool oxygen_selection_calibration_usable(gas_cal_channel_t channel, uint32_t revision) {
    std::lock_guard<std::recursive_mutex> guard(configuration_lock);
    core.initialize(); return core.usable(channel, revision);
}
bool oxygen_selection_channel_enabled(gas_cal_channel_t channel) {
    if (channel == GAS_CAL_HELIUM) return true;
    gas_cal_channel_t selected;
    return oxygen_selection_selected_channel(&selected) && channel == selected;
}
oxygen_selection_result_t oxygen_selection_confirm(oxygen_selection_t choice, bool replacement) {
    std::lock_guard<std::recursive_mutex> guard(configuration_lock);
    gas_cal_channel_t channel;
    if (!oxygen_selection::channel_for(choice, channel)) return OXYGEN_SELECT_BAD_CHOICE;
    gas_cal_record_t previous{};
    gas_calibration_get_record(channel, &previous);
    core.initialize();
    const auto before = core.status(previous.valid ? previous.revision : 0).generation;
    const auto result = core.confirm(choice, replacement, previous.valid ? previous.revision : 0);
    if (result == OXYGEN_SELECT_OK && core.status(0).generation != before) {
        gas_calibration_cancel(GAS_CAL_AO2);
        gas_calibration_cancel(GAS_CAL_JJCCR);
        gas_calibration_reset_history();
    }
    return result;
}
const char *oxygen_selection_label(oxygen_selection_t choice) {
    switch (choice) { case OXYGEN_AO2: return "AO2"; case OXYGEN_JJCCR: return "JJ-CCR"; default: return "Unconfigured"; }
}
oxygen_air_advice_t oxygen_selection_air_advice(bool known_air_confirmed) {
    gas_raw_sample_t ao2{}, jj{};
    const bool a = sensor_get_raw(GAS_CAL_AO2, &ao2), j = sensor_get_raw(GAS_CAL_JJCCR, &jj);
    return oxygen_selection::air_advice(known_air_confirmed, a ? &ao2 : nullptr, j ? &jj : nullptr);
}
const char *oxygen_selection_air_advice_label(oxygen_air_advice_t advice) {
    switch (advice) {
        case OXYGEN_AIR_CONFIRM_REFERENCE: return "Apply known fresh air and confirm below. This check cannot identify the physical sensor.";
        case OXYGEN_AIR_WAIT: return "Input check unavailable: wait for stable, fault-free signals. Select the installed type manually.";
        case OXYGEN_AIR_CHECK_AO2_INPUT: return "An electrical response is present only at the AO2 input. Check its wiring and confirm the installed type manually.";
        case OXYGEN_AIR_CHECK_JJ_INPUT: return "An electrical response is present only at the JJ-CCR input. Check its wiring and confirm the installed type manually.";
        default: return "Input check is inconclusive. Select the installed type manually; no automatic change was made.";
    }
}
}
