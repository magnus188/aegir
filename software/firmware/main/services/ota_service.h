#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// OTA update states
typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_CHECKING,
    OTA_STATE_UPDATE_AVAILABLE,
    OTA_STATE_NO_UPDATE,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_INSTALLING,
    OTA_STATE_SUCCESS,
    OTA_STATE_ERROR
} ota_state_t;

// Update info structure
typedef struct {
    char version[32];
    char release_notes[256];
    char download_url[256];
    uint32_t file_size;
    bool is_newer;
} ota_update_info_t;

// Progress callback type
typedef void (*ota_progress_cb_t)(int progress_percent, const char* status_msg);

/**
 * Initialize OTA service
 */
void ota_service_init(void);

/**
 * Check for updates from GitHub releases
 * Non-blocking - updates state when complete
 */
void ota_check_for_update(void);

/**
 * Start OTA update download and installation
 * @param progress_cb Callback for progress updates (can be NULL)
 */
void ota_start_update(ota_progress_cb_t progress_cb);

/**
 * Get current OTA state
 */
ota_state_t ota_get_state(void);

/**
 * Get update info (valid when state is OTA_STATE_UPDATE_AVAILABLE)
 */
const ota_update_info_t* ota_get_update_info(void);

/**
 * Get last error message
 */
const char* ota_get_error_message(void);

/**
 * Get current version string
 */
const char* ota_get_current_version(void);

/**
 * Cancel ongoing OTA operation
 */
void ota_cancel(void);

/**
 * Reboot device (call after successful OTA)
 */
void ota_reboot(void);

#ifdef __cplusplus
}
#endif
