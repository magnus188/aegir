#include "update_screen.h"
#include "../../styles/styles.h"
#include "../../components/navbar.h"
#include "../../images/ziphius_logo.h"
#include "../../../services/ota_service.h"
#include "../../../services/wifi_service.h"
#include "../screen_manager.h"
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <cstring>
#include <cstdio>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static const char* TAG = "UPDATE_SCREEN";

namespace {

constexpr int SCREEN_WIDTH = 480;
constexpr int SCREEN_HEIGHT = 800;
constexpr int NAVBAR_HEIGHT = 70;
constexpr int CONTENT_START_Y = NAVBAR_HEIGHT;

// Thread-safe progress state (updated from OTA task, read from LVGL task)
static std::atomic<int> g_ota_progress{0};
static char g_ota_status[64] = "Installing update...";
static char g_last_status[64] = "";
static SemaphoreHandle_t g_status_mutex = nullptr;
static esp_timer_handle_t g_restart_timer = nullptr;

// Screen state
struct UpdateScreenState {
    lv_obj_t* screen = nullptr;
    lv_obj_t* current_version_label = nullptr;
    lv_obj_t* status_label = nullptr;
    lv_obj_t* new_version_label = nullptr;
    lv_obj_t* release_notes = nullptr;
    lv_obj_t* progress_bar = nullptr;
    lv_obj_t* progress_label = nullptr;
    lv_obj_t* check_btn = nullptr;
    lv_obj_t* update_btn = nullptr;
    lv_obj_t* reboot_btn = nullptr;
    lv_obj_t* update_panel = nullptr;
    lv_timer_t* state_timer = nullptr;
    // Fullscreen update overlay
    lv_obj_t* update_overlay = nullptr;
    lv_obj_t* overlay_logo = nullptr;
    lv_obj_t* overlay_progress = nullptr;
    lv_obj_t* overlay_status = nullptr;
    lv_obj_t* overlay_percent = nullptr;
    int last_displayed_progress = -1;  // Track last displayed value
    ota_state_t last_ota_state = OTA_STATE_IDLE;  // Track state changes
    bool success_handled = false;  // Track if success state was handled
};

UpdateScreenState g_state;

// Forward declarations
void on_check_click(lv_event_t* e);
void on_update_click(lv_event_t* e);
void on_reboot_click(lv_event_t* e);
void update_ui_state();
void ota_progress_callback(int progress, const char* status);
void show_update_overlay();
void hide_update_overlay();
void update_overlay_progress();

void restart_timer_cb(void* arg) {
    (void)arg;
    ota_reboot();
}

void schedule_restart() {
    if (!g_restart_timer) {
        const esp_timer_create_args_t restart_args = {
            .callback = restart_timer_cb,
            .arg = nullptr,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "ota_restart",
            .skip_unhandled_events = true
        };
        ESP_ERROR_CHECK(esp_timer_create(&restart_args, &g_restart_timer));
    }

    if (!esp_timer_is_active(g_restart_timer)) {
        ESP_ERROR_CHECK(esp_timer_start_once(g_restart_timer, 2000000));
    }
}

// Back button callback
void update_back_cb(lv_event_t* e) {
    screen_manager_show(SCREEN_SETTINGS);
}

// Timer to poll OTA state and update overlay progress
void state_timer_cb(lv_timer_t* timer) {
    update_overlay_progress();  // Update progress bar from stored values
    update_ui_state();
}

void screen_visibility_cb(lv_event_t* event) {
    if (!g_state.state_timer) return;

    if (lv_event_get_code(event) == LV_EVENT_SCREEN_LOADED) {
        update_overlay_progress();
        update_ui_state();
        lv_timer_reset(g_state.state_timer);
        lv_timer_resume(g_state.state_timer);
        return;
    }

    ota_state_t state = ota_get_state();
    if (state != OTA_STATE_DOWNLOADING && state != OTA_STATE_INSTALLING) {
        lv_timer_pause(g_state.state_timer);
    }
}

// Create fullscreen update overlay with logo and progress
void show_update_overlay() {
    if (g_state.update_overlay) return;  // Already showing
    
    // Create fullscreen dark overlay
    g_state.update_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(g_state.update_overlay, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(g_state.update_overlay, 0, 0);
    lv_obj_set_style_bg_color(g_state.update_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(g_state.update_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_state.update_overlay, 0, 0);
    lv_obj_set_style_radius(g_state.update_overlay, 0, 0);
    lv_obj_clear_flag(g_state.update_overlay, LV_OBJ_FLAG_SCROLLABLE);
    
    // Logo image
    g_state.overlay_logo = lv_image_create(g_state.update_overlay);
    lv_image_set_src(g_state.overlay_logo, &ziphius_logo);
    lv_obj_align(g_state.overlay_logo, LV_ALIGN_CENTER, 0, -140);
    
    // Status label
    g_state.overlay_status = lv_label_create(g_state.update_overlay);
    lv_label_set_text(g_state.overlay_status, "Installing update...");
    lv_obj_set_style_text_font(g_state.overlay_status, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(g_state.overlay_status, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_align(g_state.overlay_status, LV_ALIGN_CENTER, 0, 60);
    
    // Progress bar
    g_state.overlay_progress = lv_bar_create(g_state.update_overlay);
    lv_obj_set_size(g_state.overlay_progress, SCREEN_WIDTH - 100, 12);
    lv_obj_align(g_state.overlay_progress, LV_ALIGN_CENTER, 0, 100);
    lv_bar_set_range(g_state.overlay_progress, 0, 100);
    lv_bar_set_value(g_state.overlay_progress, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(g_state.overlay_progress, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_color(g_state.overlay_progress, lv_color_hex(0x298ACA), LV_PART_INDICATOR);
    lv_obj_set_style_radius(g_state.overlay_progress, 6, 0);
    lv_obj_set_style_radius(g_state.overlay_progress, 6, LV_PART_INDICATOR);
    lv_obj_set_style_anim_duration(g_state.overlay_progress, 300, 0);
    
    // Percent label
    g_state.overlay_percent = lv_label_create(g_state.update_overlay);
    lv_label_set_text(g_state.overlay_percent, "0%");
    lv_obj_set_style_text_font(g_state.overlay_percent, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(g_state.overlay_percent, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_align(g_state.overlay_percent, LV_ALIGN_CENTER, 0, 150);
    
    // "Do not turn off" warning
    lv_obj_t* warning = lv_label_create(g_state.update_overlay);
    lv_label_set_text(warning, "Do not turn off the device");
    lv_obj_set_style_text_font(warning, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(warning, lv_color_hex(STYLE_COLOR_WARNING), 0);
    lv_obj_align(warning, LV_ALIGN_BOTTOM_MID, 0, -60);
    
    ESP_LOGI(TAG, "Update overlay shown");
}

void hide_update_overlay() {
    if (g_state.update_overlay) {
        lv_obj_delete(g_state.update_overlay);
        g_state.update_overlay = nullptr;
        g_state.overlay_logo = nullptr;
        g_state.overlay_progress = nullptr;
        g_state.overlay_status = nullptr;
        g_state.overlay_percent = nullptr;
        ESP_LOGI(TAG, "Update overlay hidden");
    }
}

// Progress callback from OTA service (called from OTA task - NOT LVGL safe!)
// Just store values - the timer will update the UI
void ota_progress_callback(int progress, const char* status) {
    // Store progress atomically
    g_ota_progress.store(progress);
    
    // Store status with mutex protection
    if (g_status_mutex) {
        if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            strncpy(g_ota_status, status, sizeof(g_ota_status) - 1);
            g_ota_status[sizeof(g_ota_status) - 1] = '\0';
            xSemaphoreGive(g_status_mutex);
        }
    }
}

// Update overlay UI from LVGL context (called by timer)
void update_overlay_progress() {
    if (!g_state.update_overlay) return;
    
    int progress = g_ota_progress.load();
    
    // Only update progress bar if progress changed
    if (progress != g_state.last_displayed_progress) {
        g_state.last_displayed_progress = progress;
        
        if (g_state.overlay_progress) {
            lv_bar_set_value(g_state.overlay_progress, progress, LV_ANIM_OFF);  // No animation to reduce glitching
        }
        if (g_state.overlay_percent) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d%%", progress);
            lv_label_set_text(g_state.overlay_percent, buf);
        }
    }
    
    // Update status text only if changed
    if (g_state.overlay_status && g_status_mutex) {
        char status_copy[64];
        if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            strncpy(status_copy, g_ota_status, sizeof(status_copy));
            xSemaphoreGive(g_status_mutex);
            
            // Only update if status actually changed
            if (strcmp(status_copy, g_last_status) != 0) {
                strncpy(g_last_status, status_copy, sizeof(g_last_status));
                lv_label_set_text(g_state.overlay_status, status_copy);
            }
        }
    }
}

void update_ui_state() {
    ota_state_t state = ota_get_state();
    const ota_update_info_t* info = ota_get_update_info();
    
    // Only update UI if state changed
    if (state == g_state.last_ota_state && state != OTA_STATE_DOWNLOADING && state != OTA_STATE_INSTALLING) {
        return;  // No change, skip UI update
    }
    g_state.last_ota_state = state;
    
    // Hide all dynamic elements first
    lv_obj_add_flag(g_state.update_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_state.progress_bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_state.progress_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_state.update_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_state.reboot_btn, LV_OBJ_FLAG_HIDDEN);
    
    switch (state) {
        case OTA_STATE_IDLE:
            lv_label_set_text(g_state.status_label, "Tap 'Check for Updates' to begin");
            lv_obj_clear_flag(g_state.check_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_state(g_state.check_btn, LV_STATE_DISABLED);
            break;
            
        case OTA_STATE_CHECKING:
            lv_label_set_text(g_state.status_label, "Checking for updates...");
            lv_obj_add_state(g_state.check_btn, LV_STATE_DISABLED);
            break;
            
        case OTA_STATE_NO_UPDATE:
            lv_label_set_text(g_state.status_label, "You're up to date!");
            lv_obj_set_style_text_color(g_state.status_label, 
                lv_color_hex(STYLE_COLOR_SUCCESS), 0);
            lv_obj_clear_flag(g_state.check_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_state(g_state.check_btn, LV_STATE_DISABLED);
            break;
            
        case OTA_STATE_UPDATE_AVAILABLE:
            lv_label_set_text(g_state.status_label, "Update available!");
            lv_obj_set_style_text_color(g_state.status_label, 
                lv_color_hex(STYLE_COLOR_PRIMARY), 0);
            
            // Show update panel with info
            lv_obj_clear_flag(g_state.update_panel, LV_OBJ_FLAG_HIDDEN);
            
            char version_text[64];
            snprintf(version_text, sizeof(version_text), "Version %s", info->version);
            lv_label_set_text(g_state.new_version_label, version_text);
            
            if (strlen(info->release_notes) > 0) {
                lv_label_set_text(g_state.release_notes, info->release_notes);
            } else {
                lv_label_set_text(g_state.release_notes, "No release notes");
            }
            
            lv_obj_clear_flag(g_state.update_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(g_state.check_btn, LV_OBJ_FLAG_HIDDEN);
            break;
            
        case OTA_STATE_DOWNLOADING:
        case OTA_STATE_INSTALLING:
            // Show fullscreen overlay during download/install
            show_update_overlay();
            if (g_state.overlay_status) {
                lv_label_set_text(g_state.overlay_status, 
                    state == OTA_STATE_DOWNLOADING ? "Downloading update..." : "Installing...");
            }
            lv_label_set_text(g_state.status_label, 
                state == OTA_STATE_DOWNLOADING ? "Downloading update..." : "Installing...");
            lv_obj_clear_flag(g_state.progress_bar, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_state.progress_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(g_state.check_btn, LV_OBJ_FLAG_HIDDEN);
            break;
            
        case OTA_STATE_SUCCESS:
            // Only handle success once
            if (!g_state.success_handled) {
                g_state.success_handled = true;
                
                // Update overlay to show success
                if (g_state.overlay_status) {
                    lv_label_set_text(g_state.overlay_status, "Update complete! Restarting...");
                    lv_obj_set_style_text_color(g_state.overlay_status, 
                        lv_color_hex(STYLE_COLOR_SUCCESS), 0);
                }
                if (g_state.overlay_percent) {
                    lv_label_set_text(g_state.overlay_percent, LV_SYMBOL_OK);
                    lv_obj_set_style_text_color(g_state.overlay_percent, 
                        lv_color_hex(STYLE_COLOR_SUCCESS), 0);
                }
                
                lv_label_set_text(g_state.status_label, "Update complete!");
                lv_obj_set_style_text_color(g_state.status_label, 
                    lv_color_hex(STYLE_COLOR_SUCCESS), 0);
                lv_obj_add_flag(g_state.check_btn, LV_OBJ_FLAG_HIDDEN);
                
                // Auto-restart after 2 seconds
                ESP_LOGI(TAG, "OTA successful, auto-restarting in 2 seconds...");
                schedule_restart();
            }
            break;
            
        case OTA_STATE_ERROR:
            // Hide overlay on error
            hide_update_overlay();
            lv_label_set_text(g_state.status_label, ota_get_error_message());
            lv_obj_set_style_text_color(g_state.status_label, 
                lv_color_hex(STYLE_COLOR_ERROR), 0);
            lv_obj_clear_flag(g_state.check_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_state(g_state.check_btn, LV_STATE_DISABLED);
            break;
    }
}

void on_check_click(lv_event_t* e) {
    // Check WiFi connection first
    if (!wifi_service_is_connected()) {
        lv_label_set_text(g_state.status_label, "Please connect to WiFi first");
        lv_obj_set_style_text_color(g_state.status_label, 
            lv_color_hex(STYLE_COLOR_WARNING), 0);
        return;
    }
    
    ESP_LOGI(TAG, "Checking for updates...");
    lv_obj_set_style_text_color(g_state.status_label, 
        lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    ota_check_for_update();
}

void on_update_click(lv_event_t* e) {
    ESP_LOGI(TAG, "Starting update...");
    show_update_overlay();  // Show fullscreen update UI
    lv_bar_set_value(g_state.progress_bar, 0, LV_ANIM_OFF);
    if (g_state.overlay_progress) {
        lv_bar_set_value(g_state.overlay_progress, 0, LV_ANIM_OFF);
    }
    ota_start_update(ota_progress_callback);
}

void on_reboot_click(lv_event_t* e) {
    ESP_LOGI(TAG, "Rebooting...");
    ota_reboot();
}

lv_obj_t* create_button(lv_obj_t* parent, const char* text, uint32_t color, 
                        lv_event_cb_t cb, int width = 200, int height = 50) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, width, height);
    lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
    lv_obj_set_style_bg_color(btn, lv_color_darken(lv_color_hex(color), 30), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 25, 0);
    lv_obj_set_style_shadow_width(btn, 8, 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_30, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
    lv_obj_center(label);
    
    return btn;
}

}  // namespace

lv_obj_t* update_screen_create(void) {
    ESP_LOGI(TAG, "Creating update screen");
    
    // Initialize mutex for thread-safe status updates
    if (!g_status_mutex) {
        g_status_mutex = xSemaphoreCreateMutex();
    }
    
    // Reset progress tracking
    g_ota_progress.store(0);
    g_state.last_displayed_progress = -1;
    g_state.last_ota_state = OTA_STATE_IDLE;
    g_state.success_handled = false;
    g_last_status[0] = '\0';
    
    // Initialize OTA service
    ota_service_init();
    
    // Create screen
    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen, lv_color_hex(STYLE_COLOR_BACKGROUND), 0);
    g_state.screen = screen;
    
    // Navbar
    navbar_create_with_back(screen, "Software Update", update_back_cb);
    
    // Content container
    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, SCREEN_WIDTH, SCREEN_HEIGHT - NAVBAR_HEIGHT);
    lv_obj_set_pos(content, 0, CONTENT_START_Y);
    lv_obj_set_style_pad_all(content, 24, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    
    // Current version card
    lv_obj_t* version_card = lv_obj_create(content);
    lv_obj_set_size(version_card, SCREEN_WIDTH - 48, 100);
    lv_obj_align(version_card, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(version_card, lv_color_hex(STYLE_COLOR_SURFACE), 0);
    lv_obj_set_style_radius(version_card, 16, 0);
    lv_obj_set_style_border_width(version_card, 0, 0);
    lv_obj_clear_flag(version_card, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t* version_title = lv_label_create(version_card);
    lv_label_set_text(version_title, "Current Version");
    lv_obj_set_style_text_font(version_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(version_title, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_align(version_title, LV_ALIGN_TOP_LEFT, 16, 16);
    
    g_state.current_version_label = lv_label_create(version_card);
    char ver_text[32];
    snprintf(ver_text, sizeof(ver_text), "v%s", ota_get_current_version());
    lv_label_set_text(g_state.current_version_label, ver_text);
    lv_obj_set_style_text_font(g_state.current_version_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(g_state.current_version_label, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_align(g_state.current_version_label, LV_ALIGN_LEFT_MID, 16, 10);
    
    // Status label
    g_state.status_label = lv_label_create(content);
    lv_label_set_text(g_state.status_label, "Tap 'Check for Updates' to begin");
    lv_obj_set_style_text_font(g_state.status_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(g_state.status_label, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_set_style_text_align(g_state.status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(g_state.status_label, SCREEN_WIDTH - 64);
    lv_obj_align(g_state.status_label, LV_ALIGN_TOP_MID, 0, 120);
    
    // Update available panel (hidden by default)
    g_state.update_panel = lv_obj_create(content);
    lv_obj_set_size(g_state.update_panel, SCREEN_WIDTH - 48, 200);
    lv_obj_align(g_state.update_panel, LV_ALIGN_TOP_MID, 0, 160);
    lv_obj_set_style_bg_color(g_state.update_panel, lv_color_hex(STYLE_COLOR_SURFACE), 0);
    lv_obj_set_style_radius(g_state.update_panel, 16, 0);
    lv_obj_set_style_border_width(g_state.update_panel, 2, 0);
    lv_obj_set_style_border_color(g_state.update_panel, lv_color_hex(STYLE_COLOR_PRIMARY), 0);
    lv_obj_add_flag(g_state.update_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(g_state.update_panel, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t* new_title = lv_label_create(g_state.update_panel);
    lv_label_set_text(new_title, "New Version Available");
    lv_obj_set_style_text_font(new_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(new_title, lv_color_hex(STYLE_COLOR_PRIMARY), 0);
    lv_obj_align(new_title, LV_ALIGN_TOP_LEFT, 16, 12);
    
    g_state.new_version_label = lv_label_create(g_state.update_panel);
    lv_label_set_text(g_state.new_version_label, "Version X.X.X");
    lv_obj_set_style_text_font(g_state.new_version_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(g_state.new_version_label, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_align(g_state.new_version_label, LV_ALIGN_TOP_LEFT, 16, 36);
    
    lv_obj_t* notes_title = lv_label_create(g_state.update_panel);
    lv_label_set_text(notes_title, "What's New:");
    lv_obj_set_style_text_font(notes_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(notes_title, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_align(notes_title, LV_ALIGN_TOP_LEFT, 16, 70);
    
    g_state.release_notes = lv_label_create(g_state.update_panel);
    lv_label_set_text(g_state.release_notes, "");
    lv_obj_set_style_text_font(g_state.release_notes, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_state.release_notes, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_set_width(g_state.release_notes, SCREEN_WIDTH - 96);
    lv_label_set_long_mode(g_state.release_notes, LV_LABEL_LONG_WRAP);
    lv_obj_align(g_state.release_notes, LV_ALIGN_TOP_LEFT, 16, 90);
    
    // Progress bar (hidden by default)
    g_state.progress_bar = lv_bar_create(content);
    lv_obj_set_size(g_state.progress_bar, SCREEN_WIDTH - 80, 20);
    lv_obj_align(g_state.progress_bar, LV_ALIGN_TOP_MID, 0, 380);
    lv_bar_set_range(g_state.progress_bar, 0, 100);
    lv_bar_set_value(g_state.progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(g_state.progress_bar, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_color(g_state.progress_bar, lv_color_hex(STYLE_COLOR_PRIMARY), LV_PART_INDICATOR);
    lv_obj_set_style_radius(g_state.progress_bar, 10, 0);
    lv_obj_set_style_radius(g_state.progress_bar, 10, LV_PART_INDICATOR);
    lv_obj_add_flag(g_state.progress_bar, LV_OBJ_FLAG_HIDDEN);
    
    g_state.progress_label = lv_label_create(content);
    lv_label_set_text(g_state.progress_label, "");
    lv_obj_set_style_text_font(g_state.progress_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_state.progress_label, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_align(g_state.progress_label, LV_ALIGN_TOP_MID, 0, 410);
    lv_obj_add_flag(g_state.progress_label, LV_OBJ_FLAG_HIDDEN);
    
    // Buttons
    g_state.check_btn = create_button(content, "Check for Updates", STYLE_COLOR_PRIMARY, on_check_click, 240, 56);
    lv_obj_align(g_state.check_btn, LV_ALIGN_BOTTOM_MID, 0, -80);
    
    g_state.update_btn = create_button(content, "Install Update", STYLE_COLOR_SUCCESS, on_update_click, 240, 56);
    lv_obj_align(g_state.update_btn, LV_ALIGN_BOTTOM_MID, 0, -80);
    lv_obj_add_flag(g_state.update_btn, LV_OBJ_FLAG_HIDDEN);
    
    g_state.reboot_btn = create_button(content, "Restart Now", STYLE_COLOR_WARNING, on_reboot_click, 240, 56);
    lv_obj_align(g_state.reboot_btn, LV_ALIGN_BOTTOM_MID, 0, -80);
    lv_obj_add_flag(g_state.reboot_btn, LV_OBJ_FLAG_HIDDEN);
    
    // Start state polling timer
    g_state.state_timer = lv_timer_create(state_timer_cb, 500, nullptr);
    lv_timer_pause(g_state.state_timer);
    lv_obj_add_event_cb(screen, screen_visibility_cb, LV_EVENT_SCREEN_LOADED, nullptr);
    lv_obj_add_event_cb(screen, screen_visibility_cb, LV_EVENT_SCREEN_UNLOADED, nullptr);
    
    return screen;
}

void update_screen_refresh(void) {
    if (wifi_service_is_connected()) {
        ota_check_for_update();
    }
}
