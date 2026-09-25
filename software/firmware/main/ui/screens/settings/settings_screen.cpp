#include "settings_screen.h"
#include "../screen_manager.h"
#include "../../components/navbar.h"
#include "../../styles/styles.h"
#include <esp_log.h>

static const char* TAG = "SETTINGS_SCREEN";

namespace {

// Settings menu item configuration
struct SettingsItem {
    const char* label;
    const char* icon;
    uint32_t color;
    screen_id_t target_screen;  // SCREEN_COUNT = no navigation
};

constexpr SettingsItem SETTINGS_ITEMS[] = {
    { "Cylinder Profiles",  LV_SYMBOL_EDIT,      STYLE_COLOR_PRIMARY,  SCREEN_CYLINDERS },
    { "Calibrate Sensors",  LV_SYMBOL_REFRESH,   STYLE_COLOR_ACCENT,   SCREEN_CALIBRATE },
    { "Software Update",    LV_SYMBOL_DOWNLOAD,  STYLE_COLOR_PRIMARY,  SCREEN_UPDATE },
    { "WiFi Settings",      LV_SYMBOL_WIFI,      STYLE_COLOR_PRIMARY,  SCREEN_WIFI },
    { "Safety Settings",    LV_SYMBOL_WARNING,   STYLE_COLOR_WARNING,  SCREEN_SAFETY },
    { "Device Settings",    LV_SYMBOL_SETTINGS,  STYLE_COLOR_PRIMARY,  SCREEN_DEVICE },
    { "Factory Reset",      LV_SYMBOL_TRASH,     STYLE_COLOR_ERROR,    SCREEN_COUNT },
};
constexpr size_t SETTINGS_ITEM_COUNT = sizeof(SETTINGS_ITEMS) / sizeof(SETTINGS_ITEMS[0]);

// Layout constants
constexpr lv_coord_t ITEM_HEIGHT = 70;
constexpr lv_coord_t ITEM_PAD = 12;
constexpr lv_coord_t ITEM_RADIUS = 12;
constexpr lv_coord_t ICON_SIZE = 32;
constexpr lv_coord_t CONTENT_PAD = 16;

void settings_item_event_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        intptr_t index = reinterpret_cast<intptr_t>(lv_event_get_user_data(e));
        const SettingsItem& item = SETTINGS_ITEMS[index];
        ESP_LOGI(TAG, "Settings item clicked: %s", item.label);
        
        // Navigate to sub-screen if configured
        if (item.target_screen != SCREEN_COUNT) {
            screen_manager_show(item.target_screen);
        }
    }
}

lv_obj_t* create_settings_item(lv_obj_t* parent, const SettingsItem& item, size_t index) {
    // Item container
    lv_obj_t* btn = lv_obj_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, LV_PCT(100), ITEM_HEIGHT);
    
    // Background styling
    lv_obj_set_style_bg_color(btn, lv_color_hex(STYLE_COLOR_BG_CARD), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, ITEM_RADIUS, 0);
    
    // Pressed state
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2A2A2A), LV_STATE_PRESSED);
    
    // Layout - horizontal with padding
    lv_obj_set_style_pad_left(btn, 16, 0);
    lv_obj_set_style_pad_right(btn, 16, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    
    // Icon container (colored circle)
    lv_obj_t* icon_cont = lv_obj_create(btn);
    lv_obj_remove_style_all(icon_cont);
    lv_obj_set_size(icon_cont, 44, 44);
    lv_obj_align(icon_cont, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(icon_cont, lv_color_hex(item.color), 0);
    lv_obj_set_style_bg_opa(icon_cont, LV_OPA_20, 0);
    lv_obj_set_style_radius(icon_cont, 22, 0);
    lv_obj_clear_flag(icon_cont, LV_OBJ_FLAG_SCROLLABLE);
    
    // Icon
    lv_obj_t* icon = lv_label_create(icon_cont);
    lv_label_set_text(icon, item.icon);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(item.color), 0);
    lv_obj_center(icon);
    
    // Label
    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, item.label);
    lv_obj_set_style_text_font(label, styles_get_font_normal(), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 60, 0);
    
    // Chevron (right arrow)
    lv_obj_t* chevron = lv_label_create(btn);
    lv_label_set_text(chevron, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(chevron, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(chevron, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_align(chevron, LV_ALIGN_RIGHT_MID, 0, 0);
    
    // Event handler
    lv_obj_add_event_cb(btn, settings_item_event_cb, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<intptr_t>(index)));
    
    return btn;
}

}  // namespace

lv_obj_t* settings_screen_create(void) {
    ESP_LOGI(TAG, "Creating settings screen");
    
    // Screen base
    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen, lv_color_hex(STYLE_COLOR_BG_DARK), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // Navbar with back button
    navbar_create_with_back(screen, "Settings", nullptr);
    
    // Scrollable content area
    lv_coord_t content_top = navbar_get_height();
    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, 480, 800 - content_top);
    lv_obj_set_pos(content, 0, content_top);
    lv_obj_set_style_pad_all(content, CONTENT_PAD, 0);
    lv_obj_set_style_pad_row(content, ITEM_PAD, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
    // Prevent elastic/bounce scroll and snap to edges
    lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLL_ONE);
    lv_obj_set_scroll_snap_y(content, LV_SCROLL_SNAP_START);
    
    // Create settings items
    for (size_t i = 0; i < SETTINGS_ITEM_COUNT; i++) {
        create_settings_item(content, SETTINGS_ITEMS[i], i);
    }
    
    ESP_LOGI(TAG, "Settings screen created with %zu items", SETTINGS_ITEM_COUNT);
    return screen;
}
