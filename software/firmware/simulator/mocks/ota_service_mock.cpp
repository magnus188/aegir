#include "services/ota_service.h"
#include "version.h"

#include <cstring>

namespace {

ota_state_t g_state = OTA_STATE_IDLE;
ota_update_info_t g_update_info = {};
char g_error[128] = {};

void set_mock_update_info() {
    std::strncpy(g_update_info.version, "v1.1.0", sizeof(g_update_info.version) - 1);
    std::strncpy(
        g_update_info.release_notes,
        "Simulator mock release with UI-only update flow.",
        sizeof(g_update_info.release_notes) - 1);
    std::strncpy(
        g_update_info.download_url,
        "https://example.invalid/trimix-simulator.bin",
        sizeof(g_update_info.download_url) - 1);
    g_update_info.file_size = 2 * 1024 * 1024;
    g_update_info.is_newer = true;
}

}  // namespace

void ota_service_init(void) {
    g_state = OTA_STATE_IDLE;
    g_error[0] = '\0';
    std::memset(&g_update_info, 0, sizeof(g_update_info));
}

void ota_check_for_update(void) {
    set_mock_update_info();
    g_state = OTA_STATE_UPDATE_AVAILABLE;
}

void ota_start_update(ota_progress_cb_t progress_cb) {
    if (g_state != OTA_STATE_UPDATE_AVAILABLE) return;

    g_state = OTA_STATE_DOWNLOADING;
    if (progress_cb) {
        progress_cb(0, "Connecting...");
        progress_cb(25, "Downloading... 25%");
        progress_cb(75, "Installing...");
        progress_cb(100, "Update complete!");
    }
    g_state = OTA_STATE_SUCCESS;
}

ota_state_t ota_get_state(void) {
    return g_state;
}

const ota_update_info_t* ota_get_update_info(void) {
    return &g_update_info;
}

const char* ota_get_error_message(void) {
    return g_error;
}

const char* ota_get_current_version(void) {
    return TRIMIX_ANALYZER_VERSION;
}

void ota_cancel(void) {
    g_state = OTA_STATE_IDLE;
}

void ota_reboot(void) {
    g_state = OTA_STATE_IDLE;
}
