#include "navbar.h"
#include "status_icons.h"
#include "../styles/styles.h"
#include "../screens/screen_manager.h"

namespace {
constexpr lv_coord_t NAVBAR_HEIGHT = 70;  // Increased for better touch targets
constexpr lv_coord_t NAVBAR_PAD = 12;
constexpr lv_coord_t BACK_BTN_SIZE = 50;  // Large touch target
constexpr lv_coord_t STATUS_WIDTH = 100;  // Space for status icons

void default_back_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        screen_manager_show(SCREEN_HOME);
    }
}
}

lv_obj_t* navbar_create(lv_obj_t* parent, const char* title) {
    // Container
    lv_obj_t* navbar = lv_obj_create(parent);
    lv_obj_remove_style_all(navbar);
    lv_obj_set_size(navbar, LV_PCT(100), NAVBAR_HEIGHT);
    lv_obj_align(navbar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(navbar, lv_color_hex(STYLE_COLOR_PRIMARY), 0);
    lv_obj_set_style_bg_opa(navbar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_left(navbar, NAVBAR_PAD, 0);
    lv_obj_set_style_pad_right(navbar, NAVBAR_PAD, 0);
    lv_obj_clear_flag(navbar, LV_OBJ_FLAG_SCROLLABLE);

    // Title label - centered
    lv_obj_t* label = lv_label_create(navbar);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_font(label, styles_get_font_bold(), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    // Status icons (WiFi + Battery) - right side
    lv_obj_t* status = status_icons_create(navbar);
    lv_obj_align(status, LV_ALIGN_RIGHT_MID, -NAVBAR_PAD, 0);

    // Store label in user data for later updates
    lv_obj_set_user_data(navbar, label);

    return navbar;
}

lv_obj_t* navbar_create_with_back(lv_obj_t* parent, const char* title, lv_event_cb_t back_cb) {
    // Container
    lv_obj_t* navbar = lv_obj_create(parent);
    lv_obj_remove_style_all(navbar);
    lv_obj_set_size(navbar, LV_PCT(100), NAVBAR_HEIGHT);
    lv_obj_align(navbar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(navbar, lv_color_hex(STYLE_COLOR_PRIMARY), 0);
    lv_obj_set_style_bg_opa(navbar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_left(navbar, NAVBAR_PAD, 0);
    lv_obj_set_style_pad_right(navbar, NAVBAR_PAD, 0);
    lv_obj_clear_flag(navbar, LV_OBJ_FLAG_SCROLLABLE);

    // Back button - left side, large touch target
    lv_obj_t* back_btn = lv_obj_create(navbar);
    lv_obj_remove_style_all(back_btn);
    lv_obj_set_size(back_btn, BACK_BTN_SIZE, BACK_BTN_SIZE);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_flag(back_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(back_btn, LV_OBJ_FLAG_SCROLLABLE);
    
    // Back button styling - subtle rounded background on press
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(STYLE_COLOR_PRIMARY_DARK), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(back_btn, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_radius(back_btn, BACK_BTN_SIZE / 2, 0);
    
    // Back icon
    lv_obj_t* back_icon = lv_label_create(back_btn);
    lv_label_set_text(back_icon, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(back_icon, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(back_icon, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_center(back_icon);
    
    // Back button event
    lv_obj_add_event_cb(back_btn, back_cb ? back_cb : default_back_cb, LV_EVENT_CLICKED, nullptr);

    // Title label - centered (accounting for back button)
    lv_obj_t* label = lv_label_create(navbar);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_font(label, styles_get_font_bold(), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    
    // Status icons (WiFi + Battery) - right side
    lv_obj_t* status = status_icons_create(navbar);
    lv_obj_align(status, LV_ALIGN_RIGHT_MID, -NAVBAR_PAD, 0);

    // Store label in user data for later updates
    lv_obj_set_user_data(navbar, label);

    return navbar;
}

void navbar_set_title(lv_obj_t* navbar, const char* title) {
    lv_obj_t* label = static_cast<lv_obj_t*>(lv_obj_get_user_data(navbar));
    if (label) {
        lv_label_set_text(label, title);
    }
}

lv_coord_t navbar_get_height(void) {
    return NAVBAR_HEIGHT;
}
