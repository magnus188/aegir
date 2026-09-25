#include "ota_service.h"
#include "ota_core.h"
#include "maintenance_service.h"
#include "storage_service.h"
#include <esp_app_desc.h>
#include <mbedtls/sha256.h>
#include "../version.h"
#include <esp_log.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_chip_info.h>
#include <esp_crt_bundle.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cJSON.h>
#include <atomic>
#include <cstdio>
#include <cstring>

static const char* TAG = "OTA_SERVICE";

namespace {

// State
std::atomic<ota_state_t> g_state{OTA_STATE_IDLE};
ota_update_info_t g_update_info = {};
char g_error_message[128] = {0};
ota_progress_cb_t g_progress_cb = nullptr;
std::atomic<bool> g_operation_busy{false};
std::atomic<bool> g_cancel_requested{false};

// Bounded response accepts both Content-Length and chunked transfer encoding.
ota::Response g_response;
ota::Release g_release;

void set_state(ota_state_t state) {
    g_state.store(state, std::memory_order_release);
}

ota_state_t get_state() {
    return g_state.load(std::memory_order_acquire);
}

void set_error(const char* message) {
    snprintf(g_error_message, sizeof(g_error_message), "%s", message);
    set_state(OTA_STATE_ERROR);
}

void finish_task() {
    g_operation_busy.store(false, std::memory_order_release);
    vTaskDelete(nullptr);
}

esp_err_t http_event_handler(esp_http_client_event_t* evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA &&
        (evt->data_len < 0 || !g_response.append(static_cast<const char *>(evt->data), evt->data_len))) return ESP_FAIL;
    return ESP_OK;
}
bool parse_release_json() {
    esp_chip_info_t chip{}; esp_chip_info(&chip);
    const esp_partition_t *slot = esp_ota_get_next_update_partition(nullptr);
    std::string error;
    if (g_response.overflowed() || !slot || !ota::parse_release(g_response.body().data(), g_response.body().size(),
        chip.revision >= 300 ? ota::Family::V3 : ota::Family::Pre3, TRIMIX_ANALYZER_VERSION,
        "https://github.com/" GITHUB_OWNER "/" GITHUB_REPO, slot ? slot->size : 0, g_release, error)) {
        set_error(g_response.overflowed() ? "Release metadata exceeds 32 KiB" : !slot ? "No OTA partition available" : error.c_str());
        return false;
    }
    std::snprintf(g_update_info.version, sizeof(g_update_info.version), "%s", g_release.version.c_str());
    std::snprintf(g_update_info.release_notes, sizeof(g_update_info.release_notes), "%s", g_release.notes.c_str());
    std::snprintf(g_update_info.download_url, sizeof(g_update_info.download_url), "%s", g_release.url.c_str());
    g_update_info.file_size = g_release.size; g_update_info.is_newer = g_release.newer;
    return true;
}
bool verify_download(const esp_partition_t *partition, uint32_t size) {
    if (!partition || size != g_release.size || size > partition->size) return false;
    mbedtls_sha256_context hash; mbedtls_sha256_init(&hash);
    bool ok = mbedtls_sha256_starts(&hash, 0) == 0;
    uint8_t data[1024], digest[32];
    for (uint32_t offset = 0; ok && offset < size;) {
        const size_t count = size - offset < sizeof(data) ? size - offset : sizeof(data);
        ok = esp_partition_read(partition, offset, data, count) == ESP_OK && mbedtls_sha256_update(&hash, data, count) == 0;
        offset += count;
    }
    ok = ok && mbedtls_sha256_finish(&hash, digest) == 0 && std::memcmp(digest, g_release.sha256, sizeof(digest)) == 0;
    mbedtls_sha256_free(&hash); return ok;
}

// Check for update task
void check_update_task(void* param) {
    (void)param;

    ESP_LOGI(TAG, "Checking for updates...");
    set_state(OTA_STATE_CHECKING);
    g_response.clear();
    
    // Configure HTTP client
    esp_http_client_config_t config = {};
    config.url = GITHUB_API_URL;
    config.event_handler = http_event_handler;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = 10000;
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        set_error("HTTP init failed");
        finish_task();
        return;
    }
    
    // Set GitHub API headers
    esp_http_client_set_header(client, "Accept", "application/vnd.github.v3+json");
    esp_http_client_set_header(client, "User-Agent", "Aegir");
    
    // Perform request
    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    
    if (err == ESP_OK && status_code == 200) {
        if (parse_release_json()) {
            if (g_update_info.is_newer && strlen(g_update_info.download_url) > 0) {
                set_state(OTA_STATE_UPDATE_AVAILABLE);
                ESP_LOGI(TAG, "Update available: %s", g_update_info.version);
            } else {
                set_state(OTA_STATE_NO_UPDATE);
                ESP_LOGI(TAG, "No update available");
            }
        } else {
            // parse_release_json preserves the concrete rejection reason.
        }
    } else {
        snprintf(g_error_message, sizeof(g_error_message), 
                 "HTTP error: %d (code %d)", err, status_code);
        set_state(OTA_STATE_ERROR);
        ESP_LOGE(TAG, "%s", g_error_message);
    }
    
    esp_http_client_cleanup(client);
    
    finish_task();
}

class EspImageTransport final : public ota::ImageTransport {
public:
    bool begin() override {
        partition_ = esp_ota_get_next_update_partition(nullptr);
        if (!partition_ || g_release.size > partition_->size) return false;
        esp_http_client_config_t http{};
        http.url = g_release.url.c_str(); http.crt_bundle_attach = esp_crt_bundle_attach;
        http.timeout_ms = 30000; http.keep_alive_enable = true;
        http.buffer_size = 1024; http.buffer_size_tx = 1024; http.max_redirection_count = 10;
        esp_https_ota_config_t config{}; config.http_config = &http;
        config.bulk_flash_erase = false;
        if (esp_https_ota_begin(&config, &handle_) != ESP_OK) return false;
        const int expected = esp_https_ota_get_image_size(handle_);
        return expected <= 0 || static_cast<uint32_t>(expected) == g_release.size;
    }
    bool identity(ota::ImageIdentity &out) override {
        esp_app_desc_t descriptor{};
        if (!handle_ || esp_https_ota_get_img_desc(handle_, &descriptor) != ESP_OK) return false;
        static_assert(sizeof(out.project) == sizeof(descriptor.project_name));
        static_assert(sizeof(out.version) == sizeof(descriptor.version));
        std::memcpy(out.project, descriptor.project_name, sizeof(out.project));
        std::memcpy(out.version, descriptor.version, sizeof(out.version));
        return true;
    }
    ota::TransferStep transfer() override {
        const esp_err_t result = esp_https_ota_perform(handle_);
        if (result != ESP_OK && result != ESP_ERR_HTTPS_OTA_IN_PROGRESS) return ota::TransferStep::Error;
        const unsigned progress = static_cast<unsigned>((uint64_t(received()) * 100) / g_release.size);
        if (progress != last_progress_ && g_progress_cb) {
            char status[64]; std::snprintf(status, sizeof(status), "Downloading... %u%%", progress);
            g_progress_cb(progress > 100 ? 100 : progress, status);
            last_progress_ = progress;
        }
        return result == ESP_OK ? ota::TransferStep::Complete : ota::TransferStep::More;
    }
    uint32_t received() override {
        const int length = handle_ ? esp_https_ota_get_image_len_read(handle_) : -1;
        return length >= 0 ? static_cast<uint32_t>(length) : UINT32_MAX;
    }
    bool complete() override {
        set_state(OTA_STATE_INSTALLING);
        if (g_progress_cb) g_progress_cb(100, "Verifying application...");
        return esp_https_ota_is_complete_data_received(handle_);
    }
    bool digest_matches() override { return verify_download(partition_, received()); }
    bool activate() override {
        const esp_err_t result = esp_https_ota_finish(handle_);
        handle_ = nullptr; // ESP-IDF releases its handle on both finish outcomes.
        return result == ESP_OK;
    }
    void abort() override { if (handle_) { esp_https_ota_abort(handle_); handle_ = nullptr; } }
    bool cancelled() override { return g_cancel_requested.load(std::memory_order_acquire); }
    uint32_t now_ms() override { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }
private:
    const esp_partition_t *partition_ = nullptr;
    esp_https_ota_handle_t handle_ = nullptr;
    unsigned last_progress_ = UINT32_MAX;
};
void ota_update_task(void*) {
    if (!storage_writes_allowed() || !maintenance_begin(MAINTENANCE_OTA, 3000)) {
        set_error("Update blocked: check power, storage and active measurements");
        finish_task(); return;
    }
    set_state(OTA_STATE_DOWNLOADING);
    if (g_progress_cb) g_progress_cb(0, "Connecting...");
    EspImageTransport transport;
    const auto result = ota::install(transport, esp_app_get_description()->project_name, g_release);
    if (result == ota::InstallResult::Ok) {
        maintenance_finish(true);
        set_state(OTA_STATE_SUCCESS);
        if (g_progress_cb) g_progress_cb(100, "Update complete!");
    } else {
        set_error(ota::install_message(result));
        maintenance_finish(false);
    }
    g_cancel_requested.store(false, std::memory_order_release);
    finish_task();
}

}  // namespace

void ota_service_init(void) {
    ESP_LOGI(TAG, "OTA service initialized (current version: %s)", TRIMIX_ANALYZER_VERSION);
    if (!g_operation_busy) set_state(OTA_STATE_IDLE);
}

void ota_check_for_update(void) {
    if (g_operation_busy.exchange(true)) {
        ESP_LOGW(TAG, "OTA operation already in progress");
        return;
    }
    
    set_state(OTA_STATE_CHECKING);
    memset(&g_update_info, 0, sizeof(g_update_info));
    memset(g_error_message, 0, sizeof(g_error_message));
    
    BaseType_t created = xTaskCreate(check_update_task, "ota_check", 8192, nullptr, 5, nullptr);
    if (created != pdPASS) {
        g_operation_busy = false;
        set_error("Failed to start update check");
    }
}

void ota_start_update(ota_progress_cb_t progress_cb) {
    if (get_state() != OTA_STATE_UPDATE_AVAILABLE) {
        ESP_LOGE(TAG, "No update available to install");
        return;
    }
    
    if (g_operation_busy.exchange(true)) {
        ESP_LOGW(TAG, "OTA operation already in progress");
        return;
    }
    
    g_progress_cb = progress_cb;
    g_cancel_requested.store(false, std::memory_order_release);
    
    BaseType_t created = xTaskCreate(ota_update_task, "ota_update", 8192, nullptr, 5, nullptr);
    if (created != pdPASS) {
        g_operation_busy = false;
        set_error("Failed to start OTA task");
    }
}

ota_state_t ota_get_state(void) {
    return get_state();
}

const ota_update_info_t* ota_get_update_info(void) {
    return &g_update_info;
}

const char* ota_get_error_message(void) {
    return g_error_message;
}

const char* ota_get_current_version(void) {
    return TRIMIX_ANALYZER_VERSION;
}

void ota_cancel(void) {
    if (get_state() == OTA_STATE_DOWNLOADING) {
        g_cancel_requested.store(true, std::memory_order_release);
    }
}

void ota_reboot(void) {
    ESP_LOGI(TAG, "Rebooting...");
    if (!maintenance_restart()) set_error("Restart blocked: measurements did not stop");
}
