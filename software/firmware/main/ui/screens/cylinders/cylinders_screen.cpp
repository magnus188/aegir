#include "cylinders_screen.h"
#include "services/cylinder_profiles.h"
#include "services/mix_label_service.h"
#include "../screen_manager.h"
#include "../../components/navbar.h"
#include "../../styles/styles.h"
#include <esp_log.h>
#include <cstdio>

static const char* TAG = "CYLINDERS_SCREEN";

namespace {

constexpr int SCREEN_WIDTH = 480;
constexpr int SCREEN_HEIGHT = 800;
constexpr int NAVBAR_HEIGHT = 70;
constexpr int PAD = 16;

struct CylindersState {
    lv_obj_t* screen = nullptr;
    lv_obj_t* selected_label = nullptr;
    lv_obj_t* detail_label = nullptr;
    lv_obj_t* label_preview = nullptr;
    lv_obj_t* list = nullptr;
};

CylindersState g_state;

void refresh();

void back_cb(lv_event_t*) {
    screen_manager_show(SCREEN_HOME);
}

lv_obj_t* create_panel(lv_obj_t* parent, int y, int h) {
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_set_size(panel, SCREEN_WIDTH - PAD * 2, h);
    lv_obj_set_pos(panel, PAD, y);
    lv_obj_set_style_bg_color(panel, lv_color_hex(STYLE_COLOR_SURFACE), 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(STYLE_COLOR_BORDER), 0);
    lv_obj_set_style_pad_all(panel, 12, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

lv_obj_t* create_button(lv_obj_t* parent, const char* text, int x, int y, int w, lv_event_cb_t cb) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, 40);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, lv_color_hex(STYLE_COLOR_BG_CARD), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(STYLE_COLOR_PRIMARY), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_center(label);
    return btn;
}

void next_cb(lv_event_t*) {
    cylinder_profiles_select_next();
    refresh();
}

void recheck_cb(lv_event_t*) {
    cylinder_profile_t profile = {};
    if (cylinder_profiles_get_selected(&profile)) {
        cylinder_profiles_mark_selected_recheck(!profile.needs_recheck);
    }
    refresh();
}

void reset_cb(lv_event_t*) {
    cylinder_profiles_reset_defaults();
    refresh();
}

void select_row_cb(lv_event_t* e) {
    uint8_t index = static_cast<uint8_t>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    cylinder_profiles_select(index);
    refresh();
}

void create_row(lv_obj_t* parent, uint8_t index, const cylinder_profile_t& profile) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, SCREEN_WIDTH - PAD * 2, 72);
    lv_obj_set_style_bg_color(row, lv_color_hex(index == cylinder_profiles_selected_index() ?
                                                STYLE_COLOR_BG_CARD : STYLE_COLOR_SURFACE), 0);
    lv_obj_set_style_radius(row, 8, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(STYLE_COLOR_BORDER), 0);
    lv_obj_set_style_pad_all(row, 10, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, select_row_cb, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<intptr_t>(index)));

    char buf[96];
    std::snprintf(buf, sizeof(buf), "%u  %s  %s",
                  static_cast<unsigned>(index + 1), profile.name,
                  profile.needs_recheck ? "CHECK" : "OK");
    lv_obj_t* title = lv_label_create(row);
    lv_label_set_text(title, buf);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    std::snprintf(buf, sizeof(buf), "%s | O2 %.1f%%  He %.0f%%  %.0fm",
                  analysis_gas_mode_label(profile.gas_mode), profile.oxygen_percent,
                  profile.helium_percent, profile.planned_depth_m);
    lv_obj_t* detail = lv_label_create(row);
    lv_label_set_text(detail, buf);
    lv_obj_set_style_text_font(detail, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(detail, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_align(detail, LV_ALIGN_BOTTOM_LEFT, 0, 0);
}

void refresh() {
    cylinder_profile_t selected = {};
    cylinder_profiles_get_selected(&selected);

    char buf[192];
    std::snprintf(buf, sizeof(buf), "%s  %s", selected.name,
                  selected.needs_recheck ? "needs analysis" : "ready");
    lv_label_set_text(g_state.selected_label, buf);

    std::snprintf(buf, sizeof(buf), "Serial %s | %s | O2 %.1f%% He %.0f%% | Planned %.0fm",
                  selected.serial, analysis_gas_mode_label(selected.gas_mode),
                  selected.oxygen_percent, selected.helium_percent, selected.planned_depth_m);
    lv_label_set_text(g_state.detail_label, buf);

    sensor_readings_t readings = {};
    readings.oxygen_percent = selected.oxygen_percent;
    readings.helium_percent = selected.helium_percent;
    readings.co2_ppm = 420.0f;
    readings.status = SENSOR_STATUS_STABLE;
    readings.source = SENSOR_SOURCE_SIMULATED;
    readings.timestamp_ms = selected.last_analyzed_ms;
    analysis_input_t input = {};
    input.readings = readings;
    input.manual_he_percent = -1.0f;
    input.planned_depth_m = selected.planned_depth_m;
    input.gas_mode = selected.gas_mode;
    input.limits = analysis_default_limits();
    analysis_result_t result = analysis_calculate(&input);
    if (selected.needs_recheck && result.severity < ANALYSIS_SEVERITY_ADVISORY) {
        result.severity = ANALYSIS_SEVERITY_ADVISORY;
    }
    analysis_history_record_t pseudo = analysis_history_record_from_result(&readings, &result);
    mix_label_build_text(&pseudo, &selected, buf, sizeof(buf));
    lv_label_set_text(g_state.label_preview, buf);

    lv_obj_clean(g_state.list);
    for (uint8_t i = 0; i < cylinder_profiles_count(); ++i) {
        cylinder_profile_t profile = {};
        if (cylinder_profiles_get(i, &profile)) {
            create_row(g_state.list, i, profile);
        }
    }
}

void loaded_cb(lv_event_t*) {
    refresh();
}

}  // namespace

lv_obj_t* cylinders_screen_create(void) {
    ESP_LOGI(TAG, "Creating cylinders screen");
    g_state = CylindersState{};

    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen, lv_color_hex(STYLE_COLOR_BG_DARK), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen, loaded_cb, LV_EVENT_SCREEN_LOADED, nullptr);
    g_state.screen = screen;

    navbar_create_with_back(screen, "Cylinder Profiles", back_cb);

    lv_obj_t* selected = create_panel(screen, NAVBAR_HEIGHT + 12, 128);
    g_state.selected_label = lv_label_create(selected);
    lv_label_set_text(g_state.selected_label, "--");
    lv_obj_set_style_text_font(g_state.selected_label, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(g_state.selected_label, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_align(g_state.selected_label, LV_ALIGN_TOP_LEFT, 0, 0);

    g_state.detail_label = lv_label_create(selected);
    lv_label_set_text(g_state.detail_label, "--");
    lv_obj_set_style_text_font(g_state.detail_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_state.detail_label, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_set_width(g_state.detail_label, SCREEN_WIDTH - PAD * 4);
    lv_label_set_long_mode(g_state.detail_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(g_state.detail_label, LV_ALIGN_TOP_LEFT, 0, 36);

    create_button(selected, "Next", 0, 74, 120, next_cb);
    create_button(selected, "Recheck", 136, 74, 130, recheck_cb);
    create_button(selected, "Defaults", 282, 74, 130, reset_cb);

    lv_obj_t* label = create_panel(screen, NAVBAR_HEIGHT + 156, 190);
    lv_obj_t* label_title = lv_label_create(label);
    lv_label_set_text(label_title, "Export-ready label");
    lv_obj_set_style_text_font(label_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_title, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_align(label_title, LV_ALIGN_TOP_LEFT, 0, 0);

    g_state.label_preview = lv_label_create(label);
    lv_label_set_text(g_state.label_preview, "--");
    lv_obj_set_style_text_font(g_state.label_preview, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_state.label_preview, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_set_width(g_state.label_preview, SCREEN_WIDTH - PAD * 4);
    lv_label_set_long_mode(g_state.label_preview, LV_LABEL_LONG_WRAP);
    lv_obj_align(g_state.label_preview, LV_ALIGN_TOP_LEFT, 0, 28);

    g_state.list = lv_obj_create(screen);
    lv_obj_remove_style_all(g_state.list);
    lv_obj_set_size(g_state.list, SCREEN_WIDTH, SCREEN_HEIGHT - NAVBAR_HEIGHT - 370);
    lv_obj_set_pos(g_state.list, 0, NAVBAR_HEIGHT + 362);
    lv_obj_set_style_pad_hor(g_state.list, PAD, 0);
    lv_obj_set_style_pad_row(g_state.list, 10, 0);
    lv_obj_set_flex_flow(g_state.list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_state.list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(g_state.list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(g_state.list, LV_SCROLLBAR_MODE_AUTO);

    refresh();
    return screen;
}
