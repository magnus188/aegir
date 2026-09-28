#include "device_screen.h"
#include "../screen_manager.h"
#include "../../components/navbar.h"
#include "../../styles/styles.h"
#include "../../../services/settings_service.h"
#include "../../../services/backlight_service.h"
#include "../../../services/storage_service.h"
#include "../../../services/battery_service.h"
#include "../../../services/sd_log_service.h"
#include <esp_log.h>
#include <cstdio>
#include <initializer_list>

static const char* TAG = "DEVICE_SCREEN";

namespace {

// Layout constants
constexpr int SCREEN_WIDTH = 480;
constexpr int SCREEN_HEIGHT = 800;
constexpr int ITEM_HEIGHT = 64;
constexpr int CHOICE_HEIGHT = 112;
constexpr int ITEM_PAD = 8;
constexpr int CONTENT_PAD = 16;

// UI state
struct DeviceScreenState {
    lv_obj_t* screen = nullptr;
    lv_obj_t* brightness_slider = nullptr;
    lv_obj_t* brightness_value = nullptr;
    lv_obj_t* timeout_btns = nullptr;
    lv_obj_t* sound_switch = nullptr;
    lv_obj_t* storage_status = nullptr;
    lv_obj_t* battery_status = nullptr;
    lv_obj_t* sd_status = nullptr;
    lv_timer_t* status_timer = nullptr;
};

DeviceScreenState g_state;

// Forward declarations
void on_brightness_change(lv_event_t* e);
void on_timeout_change(lv_event_t* e);
void on_sound_change(lv_event_t* e);
void on_reset_click(lv_event_t* e);
void back_cb(lv_event_t* e);
void show_brightness_value(int brightness) {
    char label[28];
    std::snprintf(label, sizeof(label), "Current %d%%", brightness);
    lv_label_set_text(g_state.brightness_value, label);
}

void sync_brightness_slider() {
    if (!g_state.brightness_slider || !g_state.brightness_value) return;
    const int brightness = settings_get(SETTING_BRIGHTNESS);
    lv_slider_set_value(g_state.brightness_slider, brightness, LV_ANIM_OFF);
    show_brightness_value(brightness);
}

void save_brightness_slider() {
    if (!g_state.brightness_slider) return;
    const int target = lv_slider_get_value(g_state.brightness_slider);
    const int previous = settings_get(SETTING_BRIGHTNESS);
    if (target != previous) settings_set(SETTING_BRIGHTNESS, target);
    const int saved = settings_get(SETTING_BRIGHTNESS);
    if (saved != target) {
        // Storage can be read only or a save can fail; restore the actual setting.
        backlight_set(saved);
        lv_slider_set_value(g_state.brightness_slider, saved, LV_ANIM_OFF);
    }
    show_brightness_value(saved);
    if (saved != previous) ESP_LOGI(TAG, "Brightness: %d%%", saved);
}

void refresh_status() {
    storage_init();
    char text[320];
    if (!storage_ready()) {
        std::snprintf(text, sizeof(text), "Storage recovery required\n%s\nExisting data is preserved. Settings and calibration saves are unavailable.", storage_status_message());
    } else if (!storage_writes_allowed()) {
        std::snprintf(text, sizeof(text), "Storage: read only\nChanges cannot be saved during update probation or maintenance. Existing data is preserved.");
    } else std::snprintf(text, sizeof(text), "Storage: %s", storage_status_message());
    lv_label_set_text(g_state.storage_status, text);
    lv_obj_set_style_text_color(g_state.storage_status, lv_color_hex(storage_ready() && storage_writes_allowed() ?
        STYLE_COLOR_TEXT_LIGHT : STYLE_COLOR_WARNING), 0);
    if (!battery_is_available() || battery_get_hw_type() == BATTERY_HW_UNAVAILABLE)
        std::snprintf(text, sizeof(text), "Battery: unavailable\nCheck the battery connection and fuel-gauge communication.");
    else std::snprintf(text, sizeof(text), "Battery: %s | %u%% | %.3f V",
        battery_get_hw_type() == BATTERY_HW_MOCK ? "simulated" :
        battery_get_hw_type() == BATTERY_HW_MAX17048 ? "MAX17048" : "voltage estimate",
        static_cast<unsigned>(battery_get_percentage()), battery_get_voltage_mv() / 1000.0);
    lv_label_set_text(g_state.battery_status, text);
    const auto sd=sd_log_status();
    char capacity[32];
    if(sd.capacity_bytes)std::snprintf(capacity,sizeof(capacity),"%.1f GB",sd.capacity_bytes/1000000000.0);
    else std::snprintf(capacity,sizeof(capacity),"capacity unknown");
    std::snprintf(text,sizeof(text),"%s | %s\nRows %llu | synced %llu | missing %llu\n%s\n%s",
        sd_log::state_label(sd.state),capacity,
        static_cast<unsigned long long>(sd.written),static_cast<unsigned long long>(sd.synced),static_cast<unsigned long long>(sd.dropped),sd.detail,sd.path);
    lv_label_set_text(g_state.sd_status,text);
}
void status_timer_cb(lv_timer_t*) { refresh_status(); }
void visibility_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_SCREEN_LOADED) {
        refresh_status(); sync_brightness_slider();
        lv_timer_reset(g_state.status_timer); lv_timer_resume(g_state.status_timer);
    } else lv_timer_pause(g_state.status_timer);
}

// Event handlers
void on_brightness_change(lv_event_t* e) {
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    const int brightness = lv_slider_get_value(slider);
    backlight_set(brightness);
    show_brightness_value(brightness);
    if (!lv_obj_has_state(slider, LV_STATE_PRESSED)) save_brightness_slider();
}

void on_brightness_release(lv_event_t*) {
    save_brightness_slider();
}

void on_timeout_change(lv_event_t* e) {
    lv_obj_t* btnm = (lv_obj_t*)lv_event_get_target(e);
    uint32_t id = lv_buttonmatrix_get_selected_button(btnm);
    if (id != LV_BUTTONMATRIX_BUTTON_NONE) {
        settings_set(SETTING_SCREEN_TIMEOUT, id);
        const char* labels[] = {"never", "1 min", "3 min", "5 min"};
        ESP_LOGI(TAG, "Screen timeout: %s", labels[id]);
    }
}

void on_sound_change(lv_event_t* e) {
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
    bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    settings_set(SETTING_SOUND_ENABLED, enabled ? 1 : 0);
    ESP_LOGI(TAG, "Sound %s", enabled ? "enabled" : "disabled");
}

void on_reset_click(lv_event_t* e) {
    ESP_LOGI(TAG, "Resetting device settings to defaults");
    settings_reset_category(SETTINGS_CAT_DEVICE);
    
    // Update slider and physical backlight from the saved setting.
    int brightness = settings_get(SETTING_BRIGHTNESS);
    backlight_set(brightness);
    sync_brightness_slider();
    
    // Timeout buttons
    for (uint32_t i = 0; i < 4; i++) {
        lv_buttonmatrix_clear_button_ctrl(g_state.timeout_btns, i, LV_BUTTONMATRIX_CTRL_CHECKED);
    }
    lv_buttonmatrix_set_button_ctrl(g_state.timeout_btns, settings_get(SETTING_SCREEN_TIMEOUT), LV_BUTTONMATRIX_CTRL_CHECKED);
    
    // Sound
    if (settings_get(SETTING_SOUND_ENABLED)) {
        lv_obj_add_state(g_state.sound_switch, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(g_state.sound_switch, LV_STATE_CHECKED);
    }
}

void back_cb(lv_event_t* e) {
    screen_manager_show(SCREEN_SETTINGS);
}

// Helper to create section header
lv_obj_t* create_section_header(lv_obj_t* parent, const char* title) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(STYLE_COLOR_CYAN), 0);
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_set_style_pad_top(label, 10, 0);
    return label;
}

void style_btnmatrix(lv_obj_t* btnm) {
    lv_obj_remove_style_all(btnm);
    lv_obj_set_style_bg_color(btnm, lv_color_hex(STYLE_COLOR_BG_DARK), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btnm, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(btnm, lv_color_hex(STYLE_COLOR_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(btnm, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btnm, 3, LV_PART_MAIN);
    lv_obj_set_style_pad_column(btnm, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(btnm, 8, LV_PART_MAIN);

    lv_obj_set_style_bg_color(btnm, lv_color_hex(STYLE_COLOR_BG_CARD), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(btnm, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(btnm, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), LV_PART_ITEMS);
    lv_obj_set_style_text_font(btnm, &lv_font_montserrat_16, LV_PART_ITEMS);
    lv_obj_set_style_border_width(btnm, 0, LV_PART_ITEMS);
    lv_obj_set_style_radius(btnm, 6, LV_PART_ITEMS);

    lv_style_selector_t checked = static_cast<lv_style_selector_t>(LV_PART_ITEMS) | 
                                   static_cast<lv_style_selector_t>(LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(btnm, lv_color_hex(STYLE_COLOR_CYAN), checked);
    lv_obj_set_style_bg_opa(btnm, LV_OPA_COVER, checked);
    lv_obj_set_style_text_color(btnm, lv_color_hex(STYLE_COLOR_BG_DARK), checked);
}

// Make all buttons in a button matrix checkable
void make_btnmatrix_checkable(lv_obj_t* btnm, uint32_t btn_count) {
    for (uint32_t i = 0; i < btn_count; i++) {
        lv_buttonmatrix_set_button_ctrl(btnm, i, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    }
}

lv_obj_t* create_panel(lv_obj_t* parent, int height) {
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, LV_PCT(100), height);
    lv_obj_set_style_bg_color(panel, lv_color_hex(STYLE_COLOR_TILE), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(STYLE_COLOR_TILE_BORDER), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

lv_obj_t* create_choice_row(lv_obj_t* parent, const char* title, const char* icon,
                            const char* const* options, uint32_t count) {
    lv_obj_t* row = create_panel(parent, CHOICE_HEIGHT);
    lv_obj_t* row_icon = lv_label_create(row);
    lv_label_set_text(row_icon, icon);
    lv_obj_set_style_text_font(row_icon, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(row_icon, lv_color_hex(STYLE_COLOR_CYAN), 0);
    lv_obj_set_pos(row_icon, 16, 14);

    lv_obj_t* name = lv_label_create(row);
    lv_label_set_text(name, title);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(name, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_set_pos(name, 48, 16);

    lv_obj_t* choices = lv_buttonmatrix_create(row);
    lv_buttonmatrix_set_map(choices, options);
    style_btnmatrix(choices);
    lv_obj_set_pos(choices, 14, 52);
    lv_obj_set_size(choices, SCREEN_WIDTH - CONTENT_PAD * 2 - 28, 46);
    make_btnmatrix_checkable(choices, count);
    lv_buttonmatrix_set_one_checked(choices, true);
    return choices;
}

lv_obj_t* create_brightness_row(lv_obj_t* parent) {
    lv_obj_t* row = create_panel(parent, CHOICE_HEIGHT);
    lv_obj_t* icon = lv_label_create(row);
    lv_label_set_text(icon, LV_SYMBOL_IMAGE);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(STYLE_COLOR_CYAN), 0);
    lv_obj_set_pos(icon, 16, 14);

    lv_obj_t* name = lv_label_create(row);
    lv_label_set_text(name, "Brightness");
    lv_obj_set_style_text_font(name, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(name, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_set_pos(name, 48, 16);

    g_state.brightness_value = lv_label_create(row);
    lv_obj_set_style_text_font(g_state.brightness_value, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_state.brightness_value, lv_color_hex(STYLE_COLOR_DATA), 0);
    lv_obj_align(g_state.brightness_value, LV_ALIGN_TOP_RIGHT, -16, 16);

    lv_obj_t* slider = lv_slider_create(row);
    lv_obj_set_pos(slider, 27, 58);
    lv_obj_set_size(slider, SCREEN_WIDTH - CONTENT_PAD * 2 - 54, 13);
    lv_slider_set_range(slider, 10, 100);
    lv_obj_set_ext_click_area(slider, 12);
    lv_obj_set_style_bg_color(slider, lv_color_hex(STYLE_COLOR_BG_CARD), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(slider, lv_color_hex(STYLE_COLOR_TILE_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(slider, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(STYLE_COLOR_CYAN), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, 8, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 9, LV_PART_KNOB);
    lv_obj_set_style_shadow_width(slider, 0, LV_PART_KNOB);

    lv_obj_t* minimum = lv_label_create(row);
    lv_label_set_text(minimum, "10%");
    lv_obj_set_style_text_font(minimum, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(minimum, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_set_pos(minimum, 16, 84);
    lv_obj_t* maximum = lv_label_create(row);
    lv_label_set_text(maximum, "100%");
    lv_obj_set_style_text_font(maximum, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(maximum, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_align(maximum, LV_ALIGN_TOP_RIGHT, -16, 84);
    return slider;
}

}  // namespace

// Static button maps
static const char* timeout_map[] = {"Never", "1 min", "3 min", "5 min", ""};

lv_obj_t* device_screen_create(void) {
    ESP_LOGI(TAG, "Creating device settings screen");
    
    // Screen base
    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen, lv_color_hex(STYLE_COLOR_BG_DARK), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    g_state.screen = screen;
    
    // Navbar
    navbar_create_with_back(screen, "Device Settings", back_cb);
    
    // Scroll when a recovery message needs more room than the ordinary settings.
    lv_coord_t navbar_h = navbar_get_height();
    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_pos(content, CONTENT_PAD, navbar_h + CONTENT_PAD);
    lv_obj_set_size(content, SCREEN_WIDTH - CONTENT_PAD * 2, SCREEN_HEIGHT - navbar_h - CONTENT_PAD * 2);
    lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_OVERFLOW_VISIBLE);  // Clip children that overflow
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(content, ITEM_PAD, 0);
    lv_obj_set_style_pad_all(content, 0, 0);  // No internal padding

    create_section_header(content, "DEVICE STATUS");
    lv_obj_t* status_panel = create_panel(content, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(status_panel, 14, 0);
    lv_obj_set_style_pad_row(status_panel, 7, 0);
    lv_obj_set_flex_flow(status_panel, LV_FLEX_FLOW_COLUMN);
    g_state.storage_status = lv_label_create(status_panel);
    g_state.battery_status = lv_label_create(status_panel);
    g_state.sd_status = lv_label_create(status_panel);
    for (auto *label : {g_state.storage_status, g_state.battery_status,g_state.sd_status}) {
        lv_obj_set_width(label, LV_PCT(100));
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    }
    refresh_status();
    
    create_section_header(content, "DISPLAY");
    g_state.brightness_slider = create_brightness_row(content);
    sync_brightness_slider();
    lv_obj_add_event_cb(g_state.brightness_slider, on_brightness_change, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(g_state.brightness_slider, on_brightness_release, LV_EVENT_RELEASED, nullptr);
    lv_obj_add_event_cb(g_state.brightness_slider, on_brightness_release, LV_EVENT_PRESS_LOST, nullptr);
    g_state.timeout_btns = create_choice_row(content, "Sleep timer", LV_SYMBOL_EYE_CLOSE,
                                             timeout_map, 4);
    lv_buttonmatrix_set_button_ctrl(g_state.timeout_btns, settings_get(SETTING_SCREEN_TIMEOUT), LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_obj_add_event_cb(g_state.timeout_btns, on_timeout_change, LV_EVENT_VALUE_CHANGED, nullptr);
    
    create_section_header(content, "FEEDBACK");
    lv_obj_t* sound_row = create_panel(content, ITEM_HEIGHT);
    
    lv_obj_t* sound_icon = lv_label_create(sound_row);
    lv_label_set_text(sound_icon, LV_SYMBOL_VOLUME_MAX);
    lv_obj_set_style_text_font(sound_icon, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(sound_icon, lv_color_hex(STYLE_COLOR_CYAN), 0);
    lv_obj_align(sound_icon, LV_ALIGN_LEFT_MID, 16, 0);
    
    lv_obj_t* sound_name = lv_label_create(sound_row);
    lv_label_set_text(sound_name, "Sound");
    lv_obj_set_style_text_font(sound_name, styles_get_font_normal(), 0);
    lv_obj_set_style_text_color(sound_name, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_align(sound_name, LV_ALIGN_LEFT_MID, 48, 0);
    
    g_state.sound_switch = lv_switch_create(sound_row);
    lv_obj_align(g_state.sound_switch, LV_ALIGN_RIGHT_MID, -16, 0);
    lv_obj_set_size(g_state.sound_switch, 56, 32);  // Larger touch target
    lv_obj_set_style_bg_color(g_state.sound_switch, lv_color_hex(STYLE_COLOR_BG_CARD), LV_PART_MAIN);
    lv_style_selector_t sw_checked = static_cast<lv_style_selector_t>(LV_PART_INDICATOR) | 
                                      static_cast<lv_style_selector_t>(LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(g_state.sound_switch, lv_color_hex(STYLE_COLOR_CYAN), sw_checked);
    lv_obj_set_style_bg_color(g_state.sound_switch, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), LV_PART_KNOB);
    if (settings_get(SETTING_SOUND_ENABLED)) {
        lv_obj_add_state(g_state.sound_switch, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(g_state.sound_switch, on_sound_change, LV_EVENT_VALUE_CHANGED, nullptr);
    
    lv_obj_t* reset_btn = lv_btn_create(content);
    lv_obj_set_size(reset_btn, LV_PCT(100), 50);
    lv_obj_set_style_bg_color(reset_btn, lv_color_hex(STYLE_COLOR_TILE), 0);
    lv_obj_set_style_bg_color(reset_btn, lv_color_hex(STYLE_COLOR_BG_CARD), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(reset_btn, lv_color_hex(STYLE_COLOR_TILE_BORDER), 0);
    lv_obj_set_style_border_width(reset_btn, 1, 0);
    lv_obj_set_style_radius(reset_btn, 8, 0);
    lv_obj_set_style_shadow_width(reset_btn, 0, 0);
    lv_obj_set_style_margin_top(reset_btn, 10, 0);
    lv_obj_add_event_cb(reset_btn, on_reset_click, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t* reset_label = lv_label_create(reset_btn);
    lv_label_set_text(reset_label, "Reset to Defaults");
    lv_obj_set_style_text_font(reset_label, styles_get_font_normal(), 0);
    lv_obj_set_style_text_color(reset_label, lv_color_hex(STYLE_COLOR_WARNING), 0);
    lv_obj_center(reset_label);
    
    ESP_LOGI(TAG, "Device settings screen created");
    g_state.status_timer = lv_timer_create(status_timer_cb, 1000, nullptr);
    lv_timer_pause(g_state.status_timer);
    lv_obj_add_event_cb(screen, visibility_cb, LV_EVENT_SCREEN_LOADED, nullptr);
    lv_obj_add_event_cb(screen, visibility_cb, LV_EVENT_SCREEN_UNLOADED, nullptr);
    return screen;
}
