#include "cylinders_screen.h"
#include "services/cylinder_profiles.h"
#include "services/mix_label_service.h"
#include "../screen_manager.h"
#include "../../components/navbar.h"
#include "../../styles/styles.h"
#include <esp_log.h>
#include <cstdio>
#include <cstdint>

static const char* TAG = "CYLINDERS_SCREEN";

namespace {

constexpr int SCREEN_WIDTH = 480;
constexpr int SCREEN_HEIGHT = 800;
constexpr int PAD = 16;

struct CylindersState {
    lv_obj_t* selected_name = nullptr;
    lv_obj_t* selected_status = nullptr;
    lv_obj_t* selected_detail = nullptr;
    lv_obj_t* oxygen_value = nullptr;
    lv_obj_t* helium_value = nullptr;
    lv_obj_t* depth_value = nullptr;
    lv_obj_t* label_preview = nullptr;
    lv_obj_t* list = nullptr;
};

CylindersState g_state;

lv_obj_t* add_label(lv_obj_t* parent, const char* text, int x, int y,
                    const lv_font_t* font, uint32_t color, int width = 0) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_pos(label, x, y);
    if (width) {
        lv_obj_set_width(label, width);
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    }
    return label;
}

lv_obj_t* create_panel(lv_obj_t* parent, int y, int height, bool highlighted = false) {
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_set_pos(panel, PAD, y);
    lv_obj_set_size(panel, SCREEN_WIDTH - PAD * 2, height);
    lv_obj_set_style_bg_color(panel, lv_color_hex(STYLE_COLOR_TILE), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(
        highlighted ? STYLE_COLOR_CYAN : STYLE_COLOR_TILE_BORDER), 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

lv_obj_t* create_button(lv_obj_t* parent, const char* text, int x, int width,
                        bool primary, lv_event_cb_t callback) {
    lv_obj_t* button = lv_btn_create(parent);
    lv_obj_set_pos(button, x, 157);
    lv_obj_set_size(button, width, 36);
    lv_obj_set_style_bg_color(button, lv_color_hex(
        primary ? STYLE_COLOR_PRIMARY : STYLE_COLOR_BG_CARD), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x245D72), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(button, primary ? 0 : 1, 0);
    lv_obj_set_style_border_color(button, lv_color_hex(STYLE_COLOR_TILE_BORDER), 0);
    lv_obj_set_style_radius(button, 6, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_center(label);
    return button;
}

void refresh();
void back_cb(lv_event_t*) { screen_manager_show(SCREEN_HOME); }
void next_cb(lv_event_t*) { cylinder_profiles_select_next(); refresh(); }

void recheck_cb(lv_event_t*) {
    cylinder_profile_t profile = {};
    if (cylinder_profiles_get_selected(&profile))
        cylinder_profiles_mark_selected_recheck(!profile.needs_recheck);
    refresh();
}

void reset_cb(lv_event_t*) { cylinder_profiles_reset_defaults(); refresh(); }

void select_row_cb(lv_event_t* event) {
    const uint8_t index = static_cast<uint8_t>(
        reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
    cylinder_profiles_select(index);
    refresh();
}

void create_row(lv_obj_t* parent, uint8_t index, const cylinder_profile_t& profile) {
    const bool selected = index == cylinder_profiles_selected_index();
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, SCREEN_WIDTH - PAD * 2, 68);
    lv_obj_set_style_bg_color(row, lv_color_hex(STYLE_COLOR_TILE), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x183844), LV_STATE_PRESSED);
    lv_obj_set_style_radius(row, 8, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(
        selected ? STYLE_COLOR_CYAN : STYLE_COLOR_TILE_BORDER), 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, select_row_cb, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<intptr_t>(index)));

    char buf[128];
    std::snprintf(buf, sizeof(buf), "%02u", static_cast<unsigned>(index + 1));
    add_label(row, buf, 13, 16, &lv_font_montserrat_14, STYLE_COLOR_CYAN);
    add_label(row, profile.name, 48, 9, &lv_font_montserrat_18,
              STYLE_COLOR_TEXT_LIGHT, 270);
    lv_obj_t* status = add_label(row, profile.needs_recheck ? "CHECK" : "READY",
                                 329, 12, &lv_font_montserrat_12,
                                 profile.needs_recheck ? STYLE_COLOR_WARNING : STYLE_COLOR_SUCCESS, 104);
    lv_obj_set_style_text_align(status, LV_TEXT_ALIGN_RIGHT, 0);
    std::snprintf(buf, sizeof(buf), "%s  |  O2 %.1f%%  He %.0f%%  %.0fm",
                  analysis_gas_mode_label(profile.gas_mode), profile.oxygen_percent,
                  profile.helium_percent, profile.planned_depth_m);
    add_label(row, buf, 48, 40, &lv_font_montserrat_12, STYLE_COLOR_TEXT_DIM, 382);
}

void refresh() {
    cylinder_profile_t selected = {};
    if (!cylinder_profiles_get_selected(&selected)) return;

    lv_label_set_text(g_state.selected_name, selected.name);
    lv_label_set_text(g_state.selected_status,
                      selected.needs_recheck ? "NEEDS ANALYSIS" : "READY");
    lv_obj_set_style_text_color(g_state.selected_status, lv_color_hex(
        selected.needs_recheck ? STYLE_COLOR_WARNING : STYLE_COLOR_SUCCESS), 0);

    char buf[512];
    std::snprintf(buf, sizeof(buf), "%s  |  %s", selected.serial,
                  analysis_gas_mode_label(selected.gas_mode));
    lv_label_set_text(g_state.selected_detail, buf);
    std::snprintf(buf, sizeof(buf), "%.1f%%", selected.oxygen_percent);
    lv_label_set_text(g_state.oxygen_value, buf);
    std::snprintf(buf, sizeof(buf), "%.0f%%", selected.helium_percent);
    lv_label_set_text(g_state.helium_value, buf);
    std::snprintf(buf, sizeof(buf), "%.0fm", selected.planned_depth_m);
    lv_label_set_text(g_state.depth_value, buf);

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
    if (selected.needs_recheck && result.severity < ANALYSIS_SEVERITY_ADVISORY)
        result.severity = ANALYSIS_SEVERITY_ADVISORY;
    analysis_history_record_t pseudo = analysis_history_record_from_result(&readings, &result);
    mix_label_build_text(&pseudo, &selected, buf, sizeof(buf));
    lv_label_set_text(g_state.label_preview, buf);

    const int32_t scroll_y = lv_obj_get_scroll_y(g_state.list);
    lv_obj_clean(g_state.list);
    for (uint8_t i = 0; i < cylinder_profiles_count(); ++i) {
        cylinder_profile_t profile = {};
        if (cylinder_profiles_get(i, &profile)) create_row(g_state.list, i, profile);
    }
    lv_obj_scroll_to_y(g_state.list, scroll_y, LV_ANIM_OFF);
}

void loaded_cb(lv_event_t*) { refresh(); }

}  // namespace

lv_obj_t* cylinders_screen_create(void) {
    ESP_LOGI(TAG, "Creating cylinders screen");
    g_state = CylindersState{};
    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen, lv_color_hex(STYLE_COLOR_BG_DARK), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen, loaded_cb, LV_EVENT_SCREEN_LOADED, nullptr);
    navbar_create_instrument(screen, "Cylinders", back_cb);

    lv_obj_t* selected = create_panel(screen, 62, 204, true);
    add_label(selected, "SELECTED CYLINDER", 14, 11,
              &lv_font_montserrat_12, STYLE_COLOR_TEXT_DIM);
    g_state.selected_status = add_label(selected, "--", 279, 11,
                                         &lv_font_montserrat_12, STYLE_COLOR_WARNING, 154);
    lv_obj_set_style_text_align(g_state.selected_status, LV_TEXT_ALIGN_RIGHT, 0);
    g_state.selected_name = add_label(selected, "--", 14, 31,
                                      &lv_font_montserrat_24, STYLE_COLOR_TEXT_LIGHT, 420);
    g_state.selected_detail = add_label(selected, "--", 14, 65,
                                        &lv_font_montserrat_14, STYLE_COLOR_TEXT_DIM, 420);
    add_label(selected, "OXYGEN", 14, 99, &lv_font_montserrat_12, STYLE_COLOR_TEXT_DIM);
    add_label(selected, "HELIUM", 160, 99, &lv_font_montserrat_12, STYLE_COLOR_TEXT_DIM);
    add_label(selected, "PLANNED DEPTH", 304, 99,
              &lv_font_montserrat_12, STYLE_COLOR_TEXT_DIM);
    g_state.oxygen_value = add_label(selected, "--", 14, 115,
                                     &lv_font_montserrat_24, STYLE_COLOR_CYAN);
    g_state.helium_value = add_label(selected, "--", 160, 115,
                                     &lv_font_montserrat_24, STYLE_COLOR_CYAN);
    g_state.depth_value = add_label(selected, "--", 304, 118,
                                    &lv_font_montserrat_20, STYLE_COLOR_TEXT_LIGHT);
    create_button(selected, "Next", 14, 128, true, next_cb);
    create_button(selected, "Recheck", 158, 128, false, recheck_cb);
    create_button(selected, "Defaults", 302, 132, false, reset_cb);

    lv_obj_t* label = create_panel(screen, 278, 220);
    add_label(label, "EXPORT LABEL", 14, 11,
              &lv_font_montserrat_12, STYLE_COLOR_TEXT_DIM);
    g_state.label_preview = add_label(label, "--", 14, 33,
                                       &lv_font_montserrat_12, STYLE_COLOR_TEXT_LIGHT, 420);
    lv_obj_set_style_text_line_space(g_state.label_preview, 0, 0);

    add_label(screen, "CYLINDER PROFILES", PAD + 2, 514,
              &lv_font_montserrat_12, STYLE_COLOR_TEXT_DIM);
    add_label(screen, "06 SAVED", 391, 514,
              &lv_font_montserrat_12, STYLE_COLOR_TEXT_DIM);
    g_state.list = lv_obj_create(screen);
    lv_obj_remove_style_all(g_state.list);
    lv_obj_set_size(g_state.list, SCREEN_WIDTH, SCREEN_HEIGHT - 538);
    lv_obj_set_pos(g_state.list, 0, 538);
    lv_obj_set_style_pad_hor(g_state.list, PAD, 0);
    lv_obj_set_style_pad_bottom(g_state.list, PAD, 0);
    lv_obj_set_style_pad_row(g_state.list, 9, 0);
    lv_obj_set_flex_flow(g_state.list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_state.list, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(g_state.list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(g_state.list, LV_SCROLLBAR_MODE_AUTO);

    refresh();
    return screen;
}
