#include "storage_service.h"
#include "blob_journal.h"
#include <atomic>
#include <mutex>
#include <cstring>
#include <chrono>
#if defined(ESP_PLATFORM) && !defined(TRIMIX_SIMULATOR)
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_ota_ops.h>
#include <esp_log.h>
#else
#include <map>
#include <string>
#endif

namespace {
std::timed_mutex lock;
std::atomic<bool> ready{false}, accepted{false}, paused{false}, pause_pending{false};
bool attempted = false;
std::atomic<const char *> message{"Storage not initialized"};
class Slots : public persistent::Backend {
public:
    explicit Slots(const char *ns) : ns_(ns) {}
    persistent::Read get(const char *key, std::vector<uint8_t> &bytes) override {
#if defined(ESP_PLATFORM) && !defined(TRIMIX_SIMULATOR)
        nvs_handle_t h;
        esp_err_t e = nvs_open(ns_, NVS_READONLY, &h);
        if (e == ESP_ERR_NVS_NOT_FOUND) return persistent::Read::Missing;
        if (e != ESP_OK) return persistent::Read::Error;
        size_t n = 0;
        e = nvs_get_blob(h, key, nullptr, &n);
        if (e == ESP_ERR_NVS_NOT_FOUND) { nvs_close(h); return persistent::Read::Missing; }
        if (e != ESP_OK || n > 4112) { nvs_close(h); return persistent::Read::Error; }
        bytes.resize(n);
        e = nvs_get_blob(h, key, bytes.data(), &n);
        nvs_close(h);
        return e == ESP_OK ? persistent::Read::Ok : persistent::Read::Error;
#else
        auto &space = memory[ns_];
        auto i = space.find(key);
        if (i == space.end()) return persistent::Read::Missing;
        bytes = i->second; return persistent::Read::Ok;
#endif
    }
    bool put(const char *key, const std::vector<uint8_t> &bytes) override {
#if defined(ESP_PLATFORM) && !defined(TRIMIX_SIMULATOR)
        nvs_handle_t h;
        if (nvs_open(ns_, NVS_READWRITE, &h) != ESP_OK) return false;
        const bool ok = nvs_set_blob(h, key, bytes.data(), bytes.size()) == ESP_OK && nvs_commit(h) == ESP_OK;
        nvs_close(h); return ok;
#else
        memory[ns_][key] = bytes; return true;
#endif
    }
private:
    const char *ns_;
#if !defined(ESP_PLATFORM) || defined(TRIMIX_SIMULATOR)
    static std::map<std::string, std::map<std::string, std::vector<uint8_t>>> memory;
#endif
};
#if !defined(ESP_PLATFORM) || defined(TRIMIX_SIMULATOR)
std::map<std::string, std::map<std::string, std::vector<uint8_t>>> Slots::memory;
#endif
}
bool storage_init() {
    if (ready.load()) return true;
    std::lock_guard<std::timed_mutex> guard(lock);
    if (attempted) return ready;
    attempted = true;
#if defined(ESP_PLATFORM) && !defined(TRIMIX_SIMULATOR)
    const esp_err_t result = nvs_flash_init();
    if (result != ESP_OK) {
        message = "Storage recovery required; existing data preserved";
        ESP_LOGE("STORAGE", "NVS initialization failed (%s); partition was NOT erased", esp_err_to_name(result));
        return false;
    }
    esp_ota_img_states_t state;
    const esp_partition_t *running = esp_ota_get_running_partition();
    accepted = !running || esp_ota_get_state_partition(running, &state) != ESP_OK || state != ESP_OTA_IMG_PENDING_VERIFY;
#else
    accepted = true;
#endif
    ready = true;
    message = accepted ? "Storage ready" : "Update probation: storage read only";
    return true;
}
bool storage_ready() { return ready; }
bool storage_writes_allowed() { return ready && accepted && !paused && !pause_pending; }
const char *storage_status_message() { return message; }
void storage_accept_boot() { accepted = true; if (ready) message = "Storage ready"; }
bool storage_pause_writes(bool value, uint32_t timeout_ms) {
    // Stop new arrivals before waiting for the in-flight transaction, so a busy
    // calibration/settings writer cannot starve shutdown or commit after Held.
    if (value) pause_pending = true;
    std::unique_lock<std::timed_mutex> guard(lock, std::defer_lock);
    if (!guard.try_lock_for(std::chrono::milliseconds(timeout_ms))) {
        if (value) pause_pending = false;
        return false;
    }
    paused = value;
    pause_pending = false;
    return true;
}
bool storage_run_write(const std::function<bool()> &operation, uint32_t timeout_ms) {
    if (!operation || !storage_writes_allowed()) return false;
    std::unique_lock<std::timed_mutex> guard(lock, std::defer_lock);
    if (!guard.try_lock_for(std::chrono::milliseconds(timeout_ms)) || !storage_writes_allowed()) return false;
    return operation();
}
storage_read_result_t storage_read_blob(const char *ns, uint32_t schema, void *out, size_t bytes) {
    if (!ns || !*ns || std::strlen(ns) > 15 || !storage_init()) return STORAGE_ERROR;
    std::lock_guard<std::timed_mutex> guard(lock);
    Slots slots(ns); persistent::Journal journal(slots);
    const auto result = journal.load(schema, out, bytes);
    return result == persistent::Read::Ok ? STORAGE_OK : result == persistent::Read::Missing ? STORAGE_MISSING : STORAGE_ERROR;
}
bool storage_write_blob(const char *ns, uint32_t schema, const void *data, size_t bytes) {
    if (!ns || !*ns || std::strlen(ns) > 15 || !storage_init() || !storage_writes_allowed()) return false;
    return storage_run_write([&] {
#if defined(ESP_PLATFORM) && !defined(TRIMIX_SIMULATOR)
    nvs_stats_t stats{};
    // Reserve a spare page's entries for calibration and NVS garbage collection.
    if (nvs_get_stats(nullptr, &stats) != ESP_OK || stats.free_entries < (bytes + 47) / 32 + 128) {
        message = "Storage capacity low; save rejected, previous data retained"; return false;
    }
#endif
    Slots slots(ns); persistent::Journal journal(slots);
    const bool ok = journal.save(schema, data, bytes);
    message = ok ? "Storage ready" : "Save failed; previous data retained";
    return ok;
    });
}
