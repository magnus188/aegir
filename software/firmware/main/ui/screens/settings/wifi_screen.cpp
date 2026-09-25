#include "wifi_screen.h"
#include "../../styles/styles.h"
#include "../../components/navbar.h"
#include "../../components/status_icons.h"
#include "../../../services/wifi_service.h"
#include "../screen_manager.h"
#include <esp_wifi.h>
#include <esp_log.h>
#include <cstring>

static const char* TAG = "WIFI_SCREEN";

namespace {

constexpr int SCREEN_WIDTH = 480;
constexpr int SCREEN_HEIGHT = 800;
constexpr int NAVBAR_HEIGHT = 70;
constexpr int CONTENT_START_Y = NAVBAR_HEIGHT;
constexpr int CONTENT_HEIGHT = SCREEN_HEIGHT - NAVBAR_HEIGHT;
constexpr int MAX_NETWORKS = 20;
constexpr int ITEM_HEIGHT = 70;
constexpr int ITEM_PADDING = 12;

// Portrait keyboard: unlike LVGL's desktop-oriented default, no letter row
// also has to share its width with mode and editing controls.  The widest row
// is ten keys, while shift/backspace/space/confirm receive extra width.
constexpr lv_buttonmatrix_ctrl_t KEY_1 = static_cast<lv_buttonmatrix_ctrl_t>(
    LV_BUTTONMATRIX_CTRL_POPOVER | LV_BUTTONMATRIX_CTRL_WIDTH_1);
constexpr lv_buttonmatrix_ctrl_t KEY_6 = static_cast<lv_buttonmatrix_ctrl_t>(
    LV_BUTTONMATRIX_CTRL_WIDTH_6);
constexpr lv_buttonmatrix_ctrl_t ACTION_2 = static_cast<lv_buttonmatrix_ctrl_t>(
    LV_KEYBOARD_CTRL_BUTTON_FLAGS | LV_BUTTONMATRIX_CTRL_WIDTH_2);
constexpr lv_buttonmatrix_ctrl_t BACKSPACE_2 = static_cast<lv_buttonmatrix_ctrl_t>(
    LV_BUTTONMATRIX_CTRL_CHECKED | LV_BUTTONMATRIX_CTRL_WIDTH_2);

const char* const PASSWORD_KB_LOWER_MAP[] = {
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
    "a", "s", "d", "f", "g", "h", "j", "k", "l", "\n",
    "ABC", "z", "x", "c", "v", "b", "n", "m", LV_SYMBOL_BACKSPACE, "\n",
    "1#", "@", " ", ".", LV_SYMBOL_OK, ""
};

const lv_buttonmatrix_ctrl_t PASSWORD_KB_LOWER_CTRL[] = {
    KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1,
    KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1,
    ACTION_2, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, BACKSPACE_2,
    ACTION_2, KEY_1, KEY_6, KEY_1, ACTION_2
};

const char* const PASSWORD_KB_UPPER_MAP[] = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", "\n",
    "abc", "Z", "X", "C", "V", "B", "N", "M", LV_SYMBOL_BACKSPACE, "\n",
    "1#", "@", " ", ".", LV_SYMBOL_OK, ""
};

const lv_buttonmatrix_ctrl_t PASSWORD_KB_UPPER_CTRL[] = {
    KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1,
    KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1,
    ACTION_2, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, BACKSPACE_2,
    ACTION_2, KEY_1, KEY_6, KEY_1, ACTION_2
};

const char* const PASSWORD_KB_SYMBOL_MAP[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
    "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "\n",
    "-", "_", "+", "=", "/", "\\", ":", ";", "'", "\"", "\n",
    ",", ".", "<", ">", "?", "[", "]", "{", "}", "`", "\n",
    "abc", "~", " ", LV_SYMBOL_BACKSPACE, LV_SYMBOL_OK, ""
};

const lv_buttonmatrix_ctrl_t PASSWORD_KB_SYMBOL_CTRL[] = {
    KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1,
    KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1,
    KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1,
    KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1, KEY_1,
    ACTION_2, KEY_1, KEY_6, BACKSPACE_2, ACTION_2
};

static_assert(sizeof(PASSWORD_KB_LOWER_CTRL) / sizeof(PASSWORD_KB_LOWER_CTRL[0]) == 33);
static_assert(sizeof(PASSWORD_KB_UPPER_CTRL) / sizeof(PASSWORD_KB_UPPER_CTRL[0]) == 33);
static_assert(sizeof(PASSWORD_KB_SYMBOL_CTRL) / sizeof(PASSWORD_KB_SYMBOL_CTRL[0]) == 45);

// Screen state
struct WifiScreenState {
    lv_obj_t* screen = nullptr;
    lv_obj_t* network_list = nullptr;
    lv_obj_t* scan_btn = nullptr;
    lv_obj_t* status_label = nullptr;
    lv_obj_t* connected_panel = nullptr;
    lv_obj_t* password_modal = nullptr;
    lv_obj_t* password_ta = nullptr;
    char selected_ssid[33] = {0};
    lv_timer_t* scan_check_timer = nullptr;
    lv_timer_t* connection_check_timer = nullptr;
    uint8_t connection_checks = 0;
};

WifiScreenState g_state;

// Get signal icon based on RSSI
const char* get_signal_icon(int8_t rssi) {
    int bars = wifi_service_rssi_to_bars(rssi);
    switch (bars) {
        case WIFI_SIGNAL_EXCELLENT: return LV_SYMBOL_WIFI;
        case WIFI_SIGNAL_GOOD: return LV_SYMBOL_WIFI;
        case WIFI_SIGNAL_FAIR: return LV_SYMBOL_WIFI;
        case WIFI_SIGNAL_WEAK: return LV_SYMBOL_WIFI;
        default: return LV_SYMBOL_WIFI;
    }
}

uint32_t get_signal_color(int8_t rssi) {
    int bars = wifi_service_rssi_to_bars(rssi);
    switch (bars) {
        case WIFI_SIGNAL_EXCELLENT: return STYLE_COLOR_SUCCESS;
        case WIFI_SIGNAL_GOOD: return STYLE_COLOR_SUCCESS;
        case WIFI_SIGNAL_FAIR: return STYLE_COLOR_WARNING;
        case WIFI_SIGNAL_WEAK: return STYLE_COLOR_ERROR;
        default: return STYLE_COLOR_TEXT_DIM;
    }
}

// Forward declarations
void on_network_click(lv_event_t* e);
void on_scan_click(lv_event_t* e);
void on_disconnect_click(lv_event_t* e);
void on_password_ok(lv_event_t* e);
void on_password_cancel(lv_event_t* e);
void on_keyboard_event(lv_event_t* e);
void on_screen_loaded(lv_event_t* e);
void on_network_item_delete(lv_event_t* e);
void update_network_list();
void update_connected_panel();
void show_password_modal(const char* ssid);
void hide_password_modal();
void start_scan();
void start_connection_watch();

// Scan check timer callback
void scan_check_cb(lv_timer_t* timer) {
    // Safety check - timer might fire after screen is destroyed
    if (!g_state.screen || !g_state.status_label) {
        lv_timer_del(timer);
        g_state.scan_check_timer = nullptr;
        return;
    }
    
    if (!wifi_service_is_scanning()) {
        ESP_LOGI(TAG, "Scan complete, updating list (count: %d)", wifi_service_get_scan_count());
        update_network_list();
        if (g_state.status_label) {
            uint16_t count = wifi_service_get_scan_count();
            if (count == 0) {
                lv_label_set_text(g_state.status_label, "No networks found");
            } else {
                lv_label_set_text(g_state.status_label, "");
            }
        }
        lv_timer_del(timer);
        g_state.scan_check_timer = nullptr;
    }
}

// Helper to start scanning with proper state management
void start_scan() {
    // Don't start if already scanning
    if (wifi_service_is_scanning()) {
        ESP_LOGI(TAG, "Scan already in progress");
        return;
    }
    
    // Check if WiFi service is ready
    if (!wifi_service_is_ready()) {
        ESP_LOGW(TAG, "WiFi service not ready, will retry");
        if (g_state.status_label) {
            lv_label_set_text(g_state.status_label, "WiFi initializing...");
        }
        // Retry after a short delay
        lv_timer_create([](lv_timer_t* t) {
            lv_timer_del(t);
            start_scan();
        }, 500, nullptr);
        return;
    }
    
    ESP_LOGI(TAG, "Starting WiFi scan");
    
    // Clean up any existing timer first
    if (g_state.scan_check_timer) {
        lv_timer_del(g_state.scan_check_timer);
        g_state.scan_check_timer = nullptr;
    }
    
    // Update UI
    if (g_state.status_label) {
        lv_label_set_text(g_state.status_label, "Scanning...");
    }
    
    // Start the actual scan
    wifi_service_start_scan();
    
    // Create timer to check for completion (check every 300ms for up to 15 seconds)
    g_state.scan_check_timer = lv_timer_create(scan_check_cb, 300, nullptr);
}

void connection_check_cb(lv_timer_t* timer) {
    if (!g_state.screen || !g_state.status_label) {
        lv_timer_del(timer);
        g_state.connection_check_timer = nullptr;
        return;
    }

    if (wifi_service_is_connected()) {
        lv_label_set_text(g_state.status_label, "Connected");
        update_connected_panel();
        lv_timer_del(timer);
        g_state.connection_check_timer = nullptr;
        return;
    }

    if (++g_state.connection_checks >= 60) {
        lv_label_set_text(g_state.status_label, "Connection failed - check the password");
        lv_timer_del(timer);
        g_state.connection_check_timer = nullptr;
    }
}

void start_connection_watch() {
    if (g_state.connection_check_timer) {
        lv_timer_del(g_state.connection_check_timer);
    }
    g_state.connection_checks = 0;
    g_state.connection_check_timer = lv_timer_create(connection_check_cb, 500, nullptr);
}

// Screen loaded event - auto scan when entering the screen
void on_screen_loaded(lv_event_t* e) {
    ESP_LOGI(TAG, "WiFi screen loaded - starting auto scan");
    
    // Update connected panel first
    update_connected_panel();
    
    // Start scan automatically
    start_scan();
}

void update_connected_panel() {
    if (!g_state.connected_panel) return;
    
    if (wifi_service_is_connected()) {
        char ssid[33];
        char ip[16];
        wifi_service_get_connected_ssid(ssid);
        wifi_service_get_ip(ip);
        int8_t rssi = wifi_service_get_rssi();
        
        // Clear and rebuild panel content
        lv_obj_clean(g_state.connected_panel);
        
        // Title
        lv_obj_t* title = lv_label_create(g_state.connected_panel);
        lv_label_set_text(title, "Connected");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(title, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, ITEM_PADDING, 8);
        
        // Signal icon
        lv_obj_t* signal = lv_label_create(g_state.connected_panel);
        lv_label_set_text(signal, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_font(signal, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(signal, lv_color_hex(get_signal_color(rssi)), 0);
        lv_obj_align(signal, LV_ALIGN_TOP_LEFT, ITEM_PADDING, 28);
        
        // SSID
        lv_obj_t* ssid_label = lv_label_create(g_state.connected_panel);
        lv_label_set_text(ssid_label, ssid);
        lv_obj_set_style_text_font(ssid_label, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(ssid_label, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
        lv_obj_align(ssid_label, LV_ALIGN_TOP_LEFT, ITEM_PADDING + 32, 28);
        
        // IP address
        lv_obj_t* ip_label = lv_label_create(g_state.connected_panel);
        char ip_text[32];
        snprintf(ip_text, sizeof(ip_text), "IP: %s", ip);
        lv_label_set_text(ip_label, ip_text);
        lv_obj_set_style_text_font(ip_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(ip_label, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
        lv_obj_align(ip_label, LV_ALIGN_TOP_LEFT, ITEM_PADDING, 56);
        
        // Disconnect button
        lv_obj_t* disconnect_btn = lv_btn_create(g_state.connected_panel);
        lv_obj_set_size(disconnect_btn, 100, 36);
        lv_obj_align(disconnect_btn, LV_ALIGN_TOP_RIGHT, -ITEM_PADDING, 30);
        lv_obj_set_style_bg_color(disconnect_btn, lv_color_hex(STYLE_COLOR_ERROR), 0);
        lv_obj_set_style_radius(disconnect_btn, 8, 0);
        lv_obj_add_event_cb(disconnect_btn, on_disconnect_click, LV_EVENT_CLICKED, nullptr);
        
        lv_obj_t* disconnect_label = lv_label_create(disconnect_btn);
        lv_label_set_text(disconnect_label, "Disconnect");
        lv_obj_set_style_text_font(disconnect_label, &lv_font_montserrat_12, 0);
        lv_obj_center(disconnect_label);
        
        lv_obj_clear_flag(g_state.connected_panel, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(g_state.connected_panel, LV_OBJ_FLAG_HIDDEN);
    }
    
    // CRITICAL: Force reset scroll to prevent push-down bug after layout changes
    if (g_state.screen) {
        lv_obj_scroll_to(g_state.screen, 0, 0, LV_ANIM_OFF);
    }
}

lv_obj_t* create_network_item(lv_obj_t* parent, const wifi_network_info_t* network) {
    lv_obj_t* item = lv_obj_create(parent);
    lv_obj_remove_style_all(item);
    lv_obj_set_size(item, SCREEN_WIDTH - 32, ITEM_HEIGHT);
    lv_obj_set_style_bg_color(item, lv_color_hex(STYLE_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(item, 12, 0);
    lv_obj_set_style_pad_all(item, ITEM_PADDING, 0);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
    
    // Click effect
    lv_obj_set_style_bg_color(item, lv_color_hex(0x3A3A3A), LV_STATE_PRESSED);
    lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
    
    // Store SSID in user data
    char* ssid_copy = (char*)lv_malloc(33);
    if (!ssid_copy) {
        lv_obj_delete(item);
        return nullptr;
    }
    strncpy(ssid_copy, network->ssid, 32);
    ssid_copy[32] = '\0';
    lv_obj_set_user_data(item, ssid_copy);
    lv_obj_add_event_cb(item, on_network_click, LV_EVENT_CLICKED, (void*)(intptr_t)network->authmode);
    lv_obj_add_event_cb(item, on_network_item_delete, LV_EVENT_DELETE, nullptr);
    
    // Signal strength icon
    lv_obj_t* signal = lv_label_create(item);
    lv_label_set_text(signal, get_signal_icon(network->rssi));
    lv_obj_set_style_text_font(signal, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(signal, lv_color_hex(get_signal_color(network->rssi)), 0);
    lv_obj_align(signal, LV_ALIGN_LEFT_MID, 0, 0);
    
    // Network name
    lv_obj_t* name = lv_label_create(item);
    lv_label_set_text(name, network->ssid);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(name, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_align(name, LV_ALIGN_LEFT_MID, 36, 0);
    lv_obj_set_width(name, SCREEN_WIDTH - 140);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    
    // Lock icon for secured networks
    if (network->authmode != WIFI_AUTH_OPEN) {
        lv_obj_t* lock = lv_label_create(item);
        lv_label_set_text(lock, LV_SYMBOL_EYE_CLOSE);
        lv_obj_set_style_text_font(lock, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lock, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
        lv_obj_align(lock, LV_ALIGN_RIGHT_MID, -8, 0);
    }
    
    // Connected checkmark
    if (network->connected) {
        lv_obj_t* check = lv_label_create(item);
        lv_label_set_text(check, LV_SYMBOL_OK);
        lv_obj_set_style_text_font(check, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(check, lv_color_hex(STYLE_COLOR_SUCCESS), 0);
        lv_obj_align(check, LV_ALIGN_RIGHT_MID, -36, 0);
    }
    
    return item;
}

void update_network_list() {
    if (!g_state.network_list) return;
    
    // Clear existing items
    lv_obj_clean(g_state.network_list);
    
    // Get scan results
    wifi_network_info_t networks[MAX_NETWORKS];
    uint16_t count = wifi_service_get_scan_results(networks, MAX_NETWORKS);
    
    ESP_LOGI(TAG, "Displaying %d networks", count);
    
    if (count == 0) {
        lv_obj_t* empty = lv_label_create(g_state.network_list);
        lv_label_set_text(empty, "No networks found\nTap 'Scan' to search");
        lv_obj_set_style_text_font(empty, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(empty, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(empty, SCREEN_WIDTH - 64);
        lv_obj_align(empty, LV_ALIGN_TOP_MID, 0, 40);
        // Reset scroll after layout update
        if (g_state.screen) lv_obj_scroll_to(g_state.screen, 0, 0, LV_ANIM_OFF);
        return;
    }
    
    for (uint16_t i = 0; i < count; i++) {
        // Skip currently connected network (shown in connected panel)
        if (networks[i].connected) continue;
        
        create_network_item(g_state.network_list, &networks[i]);
    }
    
    // Update connected panel
    update_connected_panel();
    
    // CRITICAL: Force reset scroll to prevent push-down bug after layout changes
    if (g_state.screen) {
        lv_obj_scroll_to(g_state.screen, 0, 0, LV_ANIM_OFF);
    }
}

void show_password_modal(const char* ssid) {
    strncpy(g_state.selected_ssid, ssid, sizeof(g_state.selected_ssid) - 1);
    g_state.selected_ssid[sizeof(g_state.selected_ssid) - 1] = '\0';
    
    // Use an opaque full-screen password view.  A translucent, nested dialog
    // forces LVGL to retain and blend many more draw tasks at once, which is
    // unnecessary on this embedded portrait display.
    g_state.password_modal = lv_obj_create(g_state.screen);
    lv_obj_set_size(g_state.password_modal, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(g_state.password_modal, 0, 0);
    lv_obj_set_style_bg_color(g_state.password_modal, lv_color_hex(STYLE_COLOR_BACKGROUND), 0);
    lv_obj_set_style_bg_opa(g_state.password_modal, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_state.password_modal, 0, 0);
    lv_obj_set_style_pad_all(g_state.password_modal, 0, 0);
    lv_obj_clear_flag(g_state.password_modal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_state.password_modal, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(g_state.password_modal);
    
    // Title
    lv_obj_t* title = lv_label_create(g_state.password_modal);
    lv_label_set_text(title, "Enter Password");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 28);
    
    // Network name
    lv_obj_t* ssid_label = lv_label_create(g_state.password_modal);
    lv_label_set_text(ssid_label, ssid);
    lv_obj_set_style_text_font(ssid_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(ssid_label, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_set_width(ssid_label, SCREEN_WIDTH - 48);
    lv_obj_set_style_text_align(ssid_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(ssid_label, LV_LABEL_LONG_DOT);
    lv_obj_align(ssid_label, LV_ALIGN_TOP_MID, 0, 62);
    
    // Password text area
    g_state.password_ta = lv_textarea_create(g_state.password_modal);
    lv_obj_set_size(g_state.password_ta, SCREEN_WIDTH - 48, 56);
    lv_obj_align(g_state.password_ta, LV_ALIGN_TOP_MID, 0, 100);
    lv_textarea_set_placeholder_text(g_state.password_ta, "Password");
    lv_textarea_set_password_mode(g_state.password_ta, true);
    lv_textarea_set_one_line(g_state.password_ta, true);
    lv_textarea_set_max_length(g_state.password_ta, 64);
    lv_obj_set_style_bg_color(g_state.password_ta, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_border_color(g_state.password_ta, lv_color_hex(STYLE_COLOR_PRIMARY), LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(g_state.password_ta, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(g_state.password_ta, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    
    // Buttons row
    lv_obj_t* btn_row = lv_obj_create(g_state.password_modal);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_size(btn_row, SCREEN_WIDTH - 48, 52);
    lv_obj_align(btn_row, LV_ALIGN_TOP_MID, 0, 178);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Cancel button
    lv_obj_t* cancel_btn = lv_btn_create(btn_row);
    lv_obj_set_size(cancel_btn, 140, 44);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0x3A3A3A), 0);
    lv_obj_set_style_radius(cancel_btn, 22, 0);
    lv_obj_add_event_cb(cancel_btn, on_password_cancel, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t* cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_set_style_text_font(cancel_label, &lv_font_montserrat_16, 0);
    lv_obj_center(cancel_label);
    
    // Connect button
    lv_obj_t* connect_btn = lv_btn_create(btn_row);
    lv_obj_set_size(connect_btn, 140, 44);
    lv_obj_set_style_bg_color(connect_btn, lv_color_hex(STYLE_COLOR_PRIMARY), 0);
    lv_obj_set_style_radius(connect_btn, 22, 0);
    lv_obj_add_event_cb(connect_btn, on_password_ok, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t* connect_label = lv_label_create(connect_btn);
    lv_label_set_text(connect_label, "Connect");
    lv_obj_set_style_text_font(connect_label, &lv_font_montserrat_16, 0);
    lv_obj_center(connect_label);

    // Keep the keyboard completely below the controls and explicitly handle
    // its checkmark/cancel keys as connect/cancel actions.
    lv_obj_t* kb = lv_keyboard_create(g_state.password_modal);
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_LOWER,
                        PASSWORD_KB_LOWER_MAP, PASSWORD_KB_LOWER_CTRL);
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_UPPER,
                        PASSWORD_KB_UPPER_MAP, PASSWORD_KB_UPPER_CTRL);
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_SPECIAL,
                        PASSWORD_KB_SYMBOL_MAP, PASSWORD_KB_SYMBOL_CTRL);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(kb, g_state.password_ta);
    lv_keyboard_set_popovers(kb, true);
    lv_obj_set_size(kb, SCREEN_WIDTH, 540);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x171A1F), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(kb, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_row(kb, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_column(kb, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x343942), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_radius(kb, 10, LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_color(kb, lv_color_hex(0x555B66), LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), LV_PART_ITEMS);
    lv_obj_set_style_text_font(kb, &lv_font_montserrat_20, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(kb, lv_color_hex(STYLE_COLOR_PRIMARY),
                              static_cast<lv_style_selector_t>(
                                  static_cast<uint32_t>(LV_PART_ITEMS) |
                                  static_cast<uint32_t>(LV_STATE_PRESSED)));
    lv_obj_add_event_cb(kb, on_keyboard_event, LV_EVENT_ALL, nullptr);

    lv_obj_add_state(g_state.password_ta, LV_STATE_FOCUSED);
}

void hide_password_modal() {
    if (g_state.password_modal) {
        lv_obj_del(g_state.password_modal);
        g_state.password_modal = nullptr;
        g_state.password_ta = nullptr;
    }
}

// Event handlers
void on_network_click(lv_event_t* e) {
    lv_obj_t* target = (lv_obj_t*)lv_event_get_current_target(e);
    char* ssid = (char*)lv_obj_get_user_data(target);
    uint8_t authmode = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    
    if (!ssid) return;
    
    ESP_LOGI(TAG, "Selected network: %s (auth: %d)", ssid, authmode);
    
    if (authmode == WIFI_AUTH_OPEN) {
        // Open network - connect directly
        if (wifi_service_connect(ssid, nullptr)) {
            lv_label_set_text(g_state.status_label, "Connecting...");
            start_connection_watch();
        } else {
            lv_label_set_text(g_state.status_label, "Could not start connection");
        }
    } else {
        // Secured network - show password modal
        show_password_modal(ssid);
    }
}

void on_network_item_delete(lv_event_t* e) {
    lv_obj_t* target = (lv_obj_t*)lv_event_get_current_target(e);
    char* ssid = (char*)lv_obj_get_user_data(target);
    if (ssid) {
        lv_free(ssid);
        lv_obj_set_user_data(target, nullptr);
    }
}

void on_scan_click(lv_event_t* e) {
    start_scan();
}

void on_disconnect_click(lv_event_t* e) {
    ESP_LOGI(TAG, "Disconnecting");
    wifi_service_disconnect();
    update_connected_panel();
}

void on_password_ok(lv_event_t* e) {
    const char* password = lv_textarea_get_text(g_state.password_ta);
    
    ESP_LOGI(TAG, "Connecting to %s", g_state.selected_ssid);
    const bool started = wifi_service_connect(g_state.selected_ssid, password);

    hide_password_modal();
    if (started) {
        lv_label_set_text(g_state.status_label, "Connecting...");
        start_connection_watch();
    } else {
        lv_label_set_text(g_state.status_label, "Could not start connection");
    }
}

void on_password_cancel(lv_event_t* e) {
    hide_password_modal();
}

void on_keyboard_event(lv_event_t* e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        on_password_ok(e);
    } else if (code == LV_EVENT_CANCEL) {
        on_password_cancel(e);
    }
}

// Back button callback for navbar
void wifi_back_cb(lv_event_t* e) {
    screen_manager_show(SCREEN_SETTINGS);
}

}  // namespace

lv_obj_t* wifi_screen_create(void) {
    ESP_LOGI(TAG, "Creating WiFi screen");
    
    // Create screen
    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen, lv_color_hex(STYLE_COLOR_BACKGROUND), 0);
    
    // CRITICAL: Disable all scrolling on the screen itself to prevent overflow/wrap bug
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLL_CHAIN);
    
    g_state.screen = screen;
    
    // Register screen loaded event for auto-scan when entering
    lv_obj_add_event_cb(screen, on_screen_loaded, LV_EVENT_SCREEN_LOADED, nullptr);
    
    // Navbar with back button
    navbar_create_with_back(screen, "WiFi", wifi_back_cb);
    
    // Content container
    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, SCREEN_WIDTH, CONTENT_HEIGHT);
    lv_obj_set_pos(content, 0, CONTENT_START_Y);
    lv_obj_set_style_pad_all(content, 16, 0);
    // Disable all scrolling on content container
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLL_CHAIN);
    
    // Status label (Scanning..., Connecting..., etc.)
    g_state.status_label = lv_label_create(content);
    lv_label_set_text(g_state.status_label, "");
    lv_obj_set_style_text_font(g_state.status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_state.status_label, lv_color_hex(STYLE_COLOR_PRIMARY), 0);
    lv_obj_align(g_state.status_label, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // Scan button - large, easy to tap
    g_state.scan_btn = lv_btn_create(content);
    lv_obj_set_size(g_state.scan_btn, 130, 50);
    lv_obj_align(g_state.scan_btn, LV_ALIGN_TOP_RIGHT, 0, -8);
    lv_obj_set_style_bg_color(g_state.scan_btn, lv_color_hex(STYLE_COLOR_PRIMARY), 0);
    lv_obj_set_style_bg_color(g_state.scan_btn, lv_color_hex(STYLE_COLOR_PRIMARY_DARK), LV_STATE_PRESSED);
    lv_obj_set_style_radius(g_state.scan_btn, 25, 0);
    lv_obj_set_style_shadow_width(g_state.scan_btn, 8, 0);
    lv_obj_set_style_shadow_opa(g_state.scan_btn, LV_OPA_30, 0);
    lv_obj_add_event_cb(g_state.scan_btn, on_scan_click, LV_EVENT_CLICKED, nullptr);
    
    // Scan button icon + text
    lv_obj_t* scan_content = lv_obj_create(g_state.scan_btn);
    lv_obj_remove_style_all(scan_content);
    lv_obj_set_size(scan_content, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_center(scan_content);
    lv_obj_set_flex_flow(scan_content, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(scan_content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(scan_content, 8, 0);
    lv_obj_clear_flag(scan_content, LV_OBJ_FLAG_CLICKABLE);
    
    lv_obj_t* scan_icon = lv_label_create(scan_content);
    lv_label_set_text(scan_icon, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_font(scan_icon, &lv_font_montserrat_18, 0);
    
    lv_obj_t* scan_label = lv_label_create(scan_content);
    lv_label_set_text(scan_label, "Scan");
    lv_obj_set_style_text_font(scan_label, &lv_font_montserrat_18, 0);
    
    // Connected network panel
    g_state.connected_panel = lv_obj_create(content);
    lv_obj_set_size(g_state.connected_panel, SCREEN_WIDTH - 32, 90);
    lv_obj_align(g_state.connected_panel, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_bg_color(g_state.connected_panel, lv_color_hex(STYLE_COLOR_SURFACE), 0);
    lv_obj_set_style_radius(g_state.connected_panel, 12, 0);
    lv_obj_set_style_border_width(g_state.connected_panel, 1, 0);
    lv_obj_set_style_border_color(g_state.connected_panel, lv_color_hex(STYLE_COLOR_SUCCESS), 0);
    lv_obj_clear_flag(g_state.connected_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_state.connected_panel, LV_OBJ_FLAG_HIDDEN);
    
    // Available networks label
    lv_obj_t* avail_label = lv_label_create(content);
    lv_label_set_text(avail_label, "Available Networks");
    lv_obj_set_style_text_font(avail_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(avail_label, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_align(avail_label, LV_ALIGN_TOP_LEFT, 0, 140);
    
    // Network list (scrollable)
    g_state.network_list = lv_obj_create(content);
    lv_obj_remove_style_all(g_state.network_list);
    lv_obj_set_size(g_state.network_list, SCREEN_WIDTH - 32, CONTENT_HEIGHT - 200);
    lv_obj_align(g_state.network_list, LV_ALIGN_TOP_MID, 0, 170);
    lv_obj_set_flex_flow(g_state.network_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_state.network_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(g_state.network_list, 8, 0);
    lv_obj_add_flag(g_state.network_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(g_state.network_list, LV_DIR_VER);
    // Prevent elastic/bounce scroll and stop scroll from propagating to parent
    lv_obj_clear_flag(g_state.network_list, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(g_state.network_list, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_clear_flag(g_state.network_list, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_flag(g_state.network_list, LV_OBJ_FLAG_SCROLL_ONE);
    lv_obj_set_scroll_snap_y(g_state.network_list, LV_SCROLL_SNAP_START);
    
    // Initial scan and connected panel update will be triggered by LV_EVENT_SCREEN_LOADED
    
    return screen;
}

void wifi_screen_refresh(void) {
    if (g_state.screen) {
        update_connected_panel();
        start_scan();
    }
}
