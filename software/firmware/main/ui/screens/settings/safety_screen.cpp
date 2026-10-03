#include "safety_screen.h"
#include "services/settings_service.h"
#include "../screen_manager.h"
#include "../../components/navbar.h"
#include "../../styles/styles.h"
#include <esp_log.h>
#include <cstdio>

static const char* TAG = "SAFETY_SCREEN";

namespace {

constexpr int SCREEN_WIDTH = 480;
constexpr int SCREEN_HEIGHT = 800;
constexpr int NAVBAR_HEIGHT = 70;
constexpr int PAD = 16;
constexpr int ROW_H = 82;

struct LimitRow {
    const char* label;
    const char* unit;
    setting_key_t key;
    int step;
    bool decimal_x100;
    bool decimal_x10;
    lv_obj_t* value = nullptr;
};

LimitRow g_rows[] = {
    {"PPO2 working limit", "bar", SETTING_PPO2_WORKING_X100, 5, true, false, nullptr},
    {"PPO2 secondary limit", "bar", SETTING_PPO2_SECONDARY_X100, 5, true, false, nullptr},
    {"Density advisory", "g/L", SETTING_DENSITY_ADVISORY_X10, 1, false, true, nullptr},
    {"Density alarm", "g/L", SETTING_DENSITY_ALARM_X10, 1, false, true, nullptr},
    {"CO advisory", "ppm", SETTING_CO_ADVISORY_PPM, 1, false, false, nullptr},
    {"CO alarm", "ppm", SETTING_CO_ALARM_PPM, 1, false, false, nullptr},
    {"Chamber RH advisory", "%", SETTING_HUMIDITY_ADVISORY_PCT, 5, false, false, nullptr},
    {"Chamber RH alarm", "%", SETTING_HUMIDITY_ALARM_PCT, 5, false, false, nullptr},
};

uint32_t limit_color(setting_key_t key) {
    switch (key) {
        case SETTING_PPO2_WORKING_X100:
        case SETTING_DENSITY_ADVISORY_X10:
        case SETTING_CO_ADVISORY_PPM:
        case SETTING_HUMIDITY_ADVISORY_PCT:
            return STYLE_COLOR_WARNING;
        case SETTING_PPO2_SECONDARY_X100:
        case SETTING_DENSITY_ALARM_X10:
        case SETTING_CO_ALARM_PPM:
        case SETTING_HUMIDITY_ALARM_PCT:
            return STYLE_COLOR_ERROR;
        default:
            return STYLE_COLOR_DATA;
    }
}

void update_row(LimitRow& row) {
    char buf[32];
    int32_t value = settings_get(row.key);
    if (row.decimal_x100) {
        std::snprintf(buf, sizeof(buf), "%.2f %s", value / 100.0f, row.unit);
    } else if (row.decimal_x10) {
        std::snprintf(buf, sizeof(buf), "%.1f %s", value / 10.0f, row.unit);
    } else {
        std::snprintf(buf, sizeof(buf), "%ld %s", static_cast<long>(value), row.unit);
    }
    lv_label_set_text(row.value, buf);
}

void update_all_rows() {
    for (auto& row : g_rows) {
        update_row(row);
    }
}

void back_cb(lv_event_t*) {
    screen_manager_show(SCREEN_SETTINGS);
}

void adjust_cb(lv_event_t* e) {
    auto* row = static_cast<LimitRow*>(lv_event_get_user_data(e));
    if (!row) return;
    bool plus = lv_obj_has_state(static_cast<lv_obj_t*>(lv_event_get_target(e)), LV_STATE_USER_1);
    int32_t value = settings_get(row->key);
    value += plus ? row->step : -row->step;
    settings_set(row->key, value);
    update_row(*row);
}

void reset_cb(lv_event_t*) {
    settings_reset_category(SETTINGS_CAT_SAFETY);
    update_all_rows();
}

lv_obj_t* make_adjust_button(lv_obj_t* parent, const char* text, bool plus, LimitRow* row) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 40, 40);
    lv_obj_set_style_bg_color(btn, lv_color_hex(STYLE_COLOR_BG_CARD), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(STYLE_COLOR_TILE_BORDER), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    if (plus) {
        lv_obj_add_state(btn, LV_STATE_USER_1);
    }
    lv_obj_add_event_cb(btn, adjust_cb, LV_EVENT_CLICKED, row);
    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_center(label);
    return btn;
}

void create_limit_row(lv_obj_t* parent, LimitRow& row, int y) {
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_set_size(panel, SCREEN_WIDTH - PAD * 2, ROW_H);
    lv_obj_set_pos(panel, PAD, y);
    lv_obj_set_style_bg_color(panel, lv_color_hex(STYLE_COLOR_TILE), 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(STYLE_COLOR_TILE_BORDER), 0);
    lv_obj_set_style_pad_all(panel, 12, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* name = lv_label_create(panel);
    lv_label_set_text(name, row.label);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(name, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, -12);

    row.value = lv_label_create(panel);
    lv_label_set_text(row.value, "--");
    lv_obj_set_style_text_font(row.value, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(row.value, lv_color_hex(limit_color(row.key)), 0);
    lv_obj_align(row.value, LV_ALIGN_LEFT_MID, 0, 16);

    lv_obj_t* minus = make_adjust_button(panel, "-", false, &row);
    lv_obj_align(minus, LV_ALIGN_RIGHT_MID, -52, 0);
    lv_obj_t* plus = make_adjust_button(panel, "+", true, &row);
    lv_obj_align(plus, LV_ALIGN_RIGHT_MID, 0, 0);
    update_row(row);
}

}  // namespace

lv_obj_t* safety_screen_create(void) {
    ESP_LOGI(TAG, "Creating safety settings screen");

    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen, lv_color_hex(STYLE_COLOR_BG_DARK), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    navbar_create_with_back(screen, "Safety Settings", back_cb);

    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_pos(content, 0, NAVBAR_HEIGHT);
    lv_obj_set_size(content, SCREEN_WIDTH, SCREEN_HEIGHT - NAVBAR_HEIGHT - 88);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);

    lv_obj_t* intro = lv_label_create(content);
    lv_label_set_text(intro, "Configure the yellow advisory and red alarm levels shown on Analyse.");
    lv_obj_set_style_text_font(intro, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(intro, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_set_width(intro, SCREEN_WIDTH - PAD * 2);
    lv_obj_set_pos(intro, PAD, 12);

    lv_obj_t* gas_heading = lv_label_create(content);
    lv_label_set_text(gas_heading, "DIVE PLANNING");
    lv_obj_set_style_text_font(gas_heading, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(gas_heading, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_set_pos(gas_heading, PAD, 60);

    int y = 84;
    for (int i = 0; i < 4; ++i) {
        create_limit_row(content, g_rows[i], y);
        y += ROW_H + 10;
    }

    lv_obj_t* sensor_heading = lv_label_create(content);
    lv_label_set_text(sensor_heading, "SENSOR ALERTS");
    lv_obj_set_style_text_font(sensor_heading, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sensor_heading, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_set_pos(sensor_heading, PAD, y + 4);
    y += 30;
    for (int i = 4; i < 8; ++i) {
        create_limit_row(content, g_rows[i], y);
        y += ROW_H + 10;
    }

    lv_obj_t* note = lv_label_create(content);
    lv_label_set_text(note, "Chamber RH describes conditions at the sensors, not cylinder water content. CO alerts do not certify breathing gas.");
    lv_obj_set_style_text_font(note, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(note, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_set_width(note, SCREEN_WIDTH - PAD * 2);
    lv_obj_set_pos(note, PAD, y + 4);

    lv_obj_t* reset_btn = lv_btn_create(screen);
    lv_obj_set_size(reset_btn, SCREEN_WIDTH - PAD * 2, 48);
    lv_obj_set_pos(reset_btn, PAD, SCREEN_HEIGHT - 70);
    lv_obj_set_style_bg_color(reset_btn, lv_color_hex(STYLE_COLOR_TILE), 0);
    lv_obj_set_style_border_color(reset_btn, lv_color_hex(STYLE_COLOR_TILE_BORDER), 0);
    lv_obj_set_style_border_width(reset_btn, 1, 0);
    lv_obj_set_style_radius(reset_btn, 8, 0);
    lv_obj_set_style_shadow_width(reset_btn, 0, 0);
    lv_obj_add_event_cb(reset_btn, reset_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* reset_label = lv_label_create(reset_btn);
    lv_label_set_text(reset_label, "Reset Safety Limits");
    lv_obj_set_style_text_font(reset_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(reset_label, lv_color_hex(STYLE_COLOR_WARNING), 0);
    lv_obj_center(reset_label);

    return screen;
}
