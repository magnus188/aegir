#include "analysis_history.h"
#include "analysis_history_format.h"
#include "storage_service.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory>

#ifndef TRIMIX_SIMULATOR
#include <nvs.h>
#include <nvs_flash.h>
#include <esp_log.h>
#endif

namespace {

#ifndef TRIMIX_SIMULATOR
constexpr const char* kNvsNamespace = "analysis_hist";
constexpr const char* kNvsKeyRecords = "records_v2";
constexpr const char* kNvsKeyLegacyRecords = "records";
constexpr const char* kNvsKeyCount = "count_v2";
constexpr const char* kNvsKeyLegacyCount = "count";
const char* TAG = "ANALYSIS_HISTORY";
#endif

analysis_history_record_t g_records[ANALYSIS_HISTORY_CAPACITY] = {};
uint8_t g_count = 0;
bool g_initialized = false;

#ifndef TRIMIX_SIMULATOR
void normalize_count() {
    if (g_count > ANALYSIS_HISTORY_CAPACITY) {
        g_count = ANALYSIS_HISTORY_CAPACITY;
    }
}

struct HistorySnapshot { uint32_t count; analysis_history_record_t records[ANALYSIS_HISTORY_CAPACITY]; };
void load_from_nvs() {
    if (!storage_ready()) return;
    auto snapshot = std::make_unique<HistorySnapshot>();
    const auto result = storage_read_blob("hist_v3", 3, snapshot.get(), sizeof(*snapshot));
    if (result == STORAGE_OK) {
        if (snapshot->count <= ANALYSIS_HISTORY_CAPACITY) {
            std::memcpy(g_records, snapshot->records, sizeof(g_records)); g_count = snapshot->count;
        }
        return;
    }
    if (result == STORAGE_ERROR) return;
    nvs_handle_t handle;
    if (nvs_open(kNvsNamespace, NVS_READONLY, &handle) != ESP_OK) {
        return;
    }

    auto legacy_v2 = std::make_unique<analysis_history_format::LegacyV2Record[]>(ANALYSIS_HISTORY_CAPACITY);
    size_t required = sizeof(analysis_history_format::LegacyV2Record) * ANALYSIS_HISTORY_CAPACITY;
    esp_err_t err = nvs_get_blob(handle, kNvsKeyRecords, legacy_v2.get(), &required);
    uint8_t count = 0;
    if (nvs_get_u8(handle, err == ESP_ERR_NVS_NOT_FOUND ? kNvsKeyLegacyCount : kNvsKeyCount, &count) == ESP_OK)
        g_count = std::min<uint8_t>(count, ANALYSIS_HISTORY_CAPACITY);
    if (err == ESP_OK && required == sizeof(analysis_history_format::LegacyV2Record) * ANALYSIS_HISTORY_CAPACITY) {
        analysis_history_format::decode_v2(legacy_v2.get(), required, g_count, g_records);
    } else {
        auto legacy = std::make_unique<analysis_history_format::LegacyRecord[]>(ANALYSIS_HISTORY_CAPACITY);
        required = sizeof(analysis_history_format::LegacyRecord) * ANALYSIS_HISTORY_CAPACITY;
        std::memset(g_records, 0, sizeof(g_records));
        if (!(err == ESP_ERR_NVS_NOT_FOUND &&
            nvs_get_blob(handle, kNvsKeyLegacyRecords, legacy.get(), &required) == ESP_OK &&
            analysis_history_format::decode_legacy(legacy.get(), required, g_count, g_records))) {
            g_count = 0;
        }
    }
    normalize_count();
    nvs_close(handle);
}

bool save_to_nvs() {
    HistorySnapshot snapshot{}; snapshot.count = g_count;
    std::memcpy(snapshot.records, g_records, sizeof(g_records));
    return storage_write_blob("hist_v3", 3, &snapshot, sizeof(snapshot));
}
#else
void load_from_nvs() {}
bool save_to_nvs() { return true; }
#endif

}  // namespace

extern "C" {

void analysis_history_init(void) {
    if (g_initialized) {
        return;
    }
#ifndef TRIMIX_SIMULATOR
    if (!storage_init()) ESP_LOGE(TAG, "%s", storage_status_message());
#endif
    load_from_nvs();
    g_initialized = true;
}

esp_err_t analysis_history_add(const analysis_history_record_t* record) {
    if (!record) {
        return ESP_ERR_INVALID_ARG;
    }
    analysis_history_init();

    const auto previous_count = g_count;
    analysis_history_record_t previous[ANALYSIS_HISTORY_CAPACITY]; std::memcpy(previous, g_records, sizeof(previous));
    if (g_count > 0) {
        const uint8_t move_count = std::min<uint8_t>(g_count, ANALYSIS_HISTORY_CAPACITY - 1);
        std::memmove(&g_records[1], &g_records[0], move_count * sizeof(g_records[0]));
    }
    g_records[0] = *record;
    if (g_count < ANALYSIS_HISTORY_CAPACITY) {
        ++g_count;
    }
    if (!save_to_nvs()) { std::memcpy(g_records, previous, sizeof(previous)); g_count = previous_count; return ESP_FAIL; }
    return ESP_OK;
}

uint8_t analysis_history_count(void) {
    analysis_history_init();
    return g_count;
}

bool analysis_history_get(uint8_t index, analysis_history_record_t* out) {
    if (!out) {
        return false;
    }
    analysis_history_init();
    if (index >= g_count) {
        return false;
    }
    *out = g_records[index];
    return true;
}

void analysis_history_clear(void) {
    analysis_history_init();
    const auto previous_count = g_count;
    analysis_history_record_t previous[ANALYSIS_HISTORY_CAPACITY]; std::memcpy(previous, g_records, sizeof(previous));
    std::memset(g_records, 0, sizeof(g_records)); g_count = 0;
    if (!save_to_nvs()) { std::memcpy(g_records, previous, sizeof(previous)); g_count = previous_count; }
}

analysis_history_record_t analysis_history_record_from_result(const sensor_readings_t* readings,
                                                              const analysis_result_t* result) {
    analysis_history_record_t record = {};
    if (!readings || !result) {
        return record;
    }
    record.timestamp_ms = readings->timestamp_ms;
    record.sequence = readings->sequence;
    std::snprintf(record.mix_label, sizeof(record.mix_label), "%s", result->mix_label);
    record.oxygen_percent = result->oxygen_percent;
    record.helium_percent = result->helium_percent;
    record.nitrogen_percent = result->nitrogen_percent;
    record.co2_ppm = result->co2_ppm;
    record.planned_depth_m = result->planned_depth_m;
    record.mod_working_m = result->mod_working_m;
    record.mod_secondary_m = result->mod_secondary_m;
    record.ppo2_at_depth = result->ppo2_at_depth;
    record.ead_m = result->ead_m;
    record.end_m = result->end_m;
    record.gas_density_g_l = result->gas_density_g_l;
    record.gas_mode = result->gas_mode;
    record.severity = result->severity;
    record.co_ppm = readings->co_valid ? readings->co_ppm : NAN;
    record.co_valid = readings->co_valid && std::isfinite(readings->co_ppm) && readings->co_ppm >= 0.0f;
    record.source_known = true;
    record.source = readings->source;
    record.oxygen_selection = readings->oxygen_selection;
    record.oxygen_selection_generation = readings->oxygen_selection_generation;
    record.oxygen_calibration_revision = readings->oxygen_calibration_revision;
    return record;
}

}  // extern "C"
