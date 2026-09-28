#include "settings_screen.h"
#include "../screen_manager.h"
#include "../../components/navbar.h"
#include "../../styles/styles.h"
#include <esp_log.h>

static const char* TAG = "SETTINGS_SCREEN";

namespace {

struct SettingsItem {
    const char* label;
    const char* description;
    screen_id_t target_screen;  // SCREEN_COUNT retains the unavailable reset action.
};

constexpr SettingsItem SETTINGS_ITEMS[] = {
    {"Calibrate Sensors", "Oxygen and helium reference setup", SCREEN_CALIBRATE},
    {"Software Update", "Firmware version and update controls", SCREEN_UPDATE},
    {"WiFi Settings", "Network connection", SCREEN_WIFI},
    {"Safety Settings", "Gas and sensor alert limits", SCREEN_SAFETY},
    {"Device Settings", "Hardware and storage", SCREEN_DEVICE},
    {"Factory Reset", "Unavailable", SCREEN_COUNT},
};
constexpr size_t SETTINGS_ITEM_COUNT = sizeof(SETTINGS_ITEMS) / sizeof(SETTINGS_ITEMS[0]);

void settings_item_event_cb(lv_event_t* e) {
    const auto index = static_cast<size_t>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    const SettingsItem& item = SETTINGS_ITEMS[index];
    ESP_LOGI(TAG, "Settings item clicked: %s", item.label);
    if (item.target_screen != SCREEN_COUNT) screen_manager_show(item.target_screen);
}

void create_settings_item(lv_obj_t* parent, const SettingsItem& item, size_t index) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), 74);
    lv_obj_set_style_bg_color(row, lv_color_hex(STYLE_COLOR_TILE), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x183844), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(STYLE_COLOR_TILE_BORDER), 0);
    lv_obj_set_style_radius(row, 8, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, settings_item_event_cb, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<intptr_t>(index)));

    lv_obj_t* title = lv_label_create(row);
    lv_label_set_text(title, item.label);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_set_pos(title, 18, 13);

    lv_obj_t* detail = lv_label_create(row);
    lv_label_set_text(detail, item.description);
    lv_obj_set_style_text_font(detail, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(detail, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_set_pos(detail, 18, 43);
    lv_obj_set_width(detail, 380);

    if (item.target_screen != SCREEN_COUNT) {
        lv_obj_t* arrow = lv_label_create(row);
        lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_font(arrow, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(arrow, lv_color_hex(STYLE_COLOR_CYAN), 0);
        lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -15, 0);
    }
}

}  // namespace

lv_obj_t* settings_screen_create(void) {
    ESP_LOGI(TAG, "Creating settings screen");
    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen, lv_color_hex(STYLE_COLOR_BG_DARK), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    navbar_create_instrument(screen, "Settings", nullptr);

    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, 480, 750);
    lv_obj_set_pos(content, 0, 50);
    lv_obj_set_style_pad_hor(content, 16, 0);
    lv_obj_set_style_pad_top(content, 10, 0);
    lv_obj_set_style_pad_bottom(content, 16, 0);
    lv_obj_set_style_pad_row(content, 9, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLL_ONE);
    lv_obj_set_scroll_snap_y(content, LV_SCROLL_SNAP_START);

    for (size_t i = 0; i < SETTINGS_ITEM_COUNT; ++i) {
        create_settings_item(content, SETTINGS_ITEMS[i], i);
    }
    ESP_LOGI(TAG, "Settings screen created with %zu items", SETTINGS_ITEM_COUNT);
    return screen;
}
