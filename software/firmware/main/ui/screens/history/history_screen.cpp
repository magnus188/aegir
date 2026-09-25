#include "history_screen.h"
#include "services/analysis_history.h"
#include "../screen_manager.h"
#include "../../components/navbar.h"
#include "../../styles/styles.h"
#include <esp_log.h>
#include <cstdio>
#include <cmath>

static const char* TAG = "HISTORY_SCREEN";

namespace {

constexpr int SCREEN_WIDTH = 480;
constexpr int SCREEN_HEIGHT = 800;
constexpr int NAVBAR_HEIGHT = 70;
constexpr int PAD = 16;

struct HistoryState {
    lv_obj_t* screen = nullptr;
    lv_obj_t* list = nullptr;
    lv_obj_t* count_label = nullptr;
};

HistoryState g_state;

uint32_t severity_color(analysis_severity_t severity) {
    switch (severity) {
        case ANALYSIS_SEVERITY_NORMAL:
            return STYLE_COLOR_SUCCESS;
        case ANALYSIS_SEVERITY_ADVISORY:
            return STYLE_COLOR_WARNING;
        case ANALYSIS_SEVERITY_ALARM:
        case ANALYSIS_SEVERITY_FAULT:
            return STYLE_COLOR_ERROR;
        default:
            return STYLE_COLOR_TEXT_DIM;
    }
}

void refresh_history();

void back_cb(lv_event_t*) {
    screen_manager_show(SCREEN_HOME);
}

void clear_cb(lv_event_t*) {
    analysis_history_clear();
    refresh_history();
}

void screen_loaded_cb(lv_event_t*) {
    refresh_history();
}

lv_obj_t* create_row(lv_obj_t* parent, const analysis_history_record_t& record, uint8_t index) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, SCREEN_WIDTH - PAD * 2, 152);
    lv_obj_set_style_bg_color(row, lv_color_hex(STYLE_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, 8, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(STYLE_COLOR_BORDER), 0);
    lv_obj_set_style_pad_all(row, 10, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    char buf[96];
    std::snprintf(buf, sizeof(buf), "%02u  %s", static_cast<unsigned>(index + 1), record.mix_label);
    lv_obj_t* title = lv_label_create(row);
    lv_label_set_text(title, buf);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t* severity = lv_label_create(row);
    lv_label_set_text(severity, analysis_severity_label(record.severity));
    lv_obj_set_style_text_font(severity, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(severity, lv_color_hex(severity_color(record.severity)), 0);
    lv_obj_align(severity, LV_ALIGN_TOP_RIGHT, 0, 2);

    std::snprintf(buf, sizeof(buf), "O2 %.1f%%  He %.0f%%  N2 %.1f%%  | %s",
                  record.oxygen_percent, record.helium_percent,
                  record.nitrogen_percent, !record.source_known ? "Legacy" :
                  (record.source == SENSOR_SOURCE_SIMULATED ? "Simulation" : "Hardware"));
    lv_obj_t* gases = lv_label_create(row);
    lv_label_set_text(gases, buf);
    lv_obj_set_style_text_font(gases, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(gases, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_align(gases, LV_ALIGN_TOP_LEFT, 0, 30);

    char co[24], co2[24];
    if (record.co_valid && std::isfinite(record.co_ppm)) std::snprintf(co, sizeof(co), "%.1f ppm", record.co_ppm);
    else std::snprintf(co, sizeof(co), "not measured");
    if (std::isfinite(record.co2_ppm) && record.co2_ppm >= 0) std::snprintf(co2, sizeof(co2), "%.0f ppm", record.co2_ppm);
    else std::snprintf(co2, sizeof(co2), "not measured");
    std::snprintf(buf, sizeof(buf), "CO %s  |  CO2 %s", co, co2);
    lv_obj_t *trace = lv_label_create(row);
    lv_label_set_text(trace, buf);
    lv_obj_set_style_text_font(trace, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(trace, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_align(trace, LV_ALIGN_TOP_LEFT, 0, 52);

    std::snprintf(buf, sizeof(buf), "%s  Depth %.0fm  MOD %.0f/%.0fm  PPO2 %.2f",
                  analysis_gas_mode_label(record.gas_mode), record.planned_depth_m, record.mod_working_m,
                  record.mod_secondary_m, record.ppo2_at_depth);
    lv_obj_t* derived = lv_label_create(row);
    lv_label_set_text(derived, buf);
    lv_obj_set_style_text_font(derived, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(derived, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_align(derived, LV_ALIGN_TOP_LEFT, 0, 74);

    std::snprintf(buf, sizeof(buf), "EAD %.0fm  END %.0fm  Density %.1f g/L",
                  record.ead_m, record.end_m, record.gas_density_g_l);
    lv_obj_t* limits = lv_label_create(row);
    lv_label_set_text(limits, buf);
    lv_obj_set_style_text_font(limits, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(limits, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_align(limits, LV_ALIGN_TOP_LEFT, 0, 96);
    if (record.oxygen_selection == OXYGEN_UNCONFIGURED)
        std::snprintf(buf, sizeof(buf), "O2 sensor identity: not recorded");
    else std::snprintf(buf, sizeof(buf), "O2 sensor: %s | setup %u | calibration r%u",
        oxygen_selection_label(record.oxygen_selection), static_cast<unsigned>(record.oxygen_selection_generation),
        static_cast<unsigned>(record.oxygen_calibration_revision));
    lv_obj_t *identity = lv_label_create(row);
    lv_label_set_text(identity, buf);
    lv_obj_set_style_text_font(identity, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(identity, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_align(identity, LV_ALIGN_TOP_LEFT, 0, 118);

    return row;
}

void refresh_history() {
    if (!g_state.list) return;
    lv_obj_clean(g_state.list);

    uint8_t count = analysis_history_count();
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%u captured analyses", static_cast<unsigned>(count));
    lv_label_set_text(g_state.count_label, buf);

    if (count == 0) {
        lv_obj_t* empty = lv_label_create(g_state.list);
        lv_label_set_text(empty, "No captured analyses");
        lv_obj_set_style_text_font(empty, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(empty, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
        lv_obj_set_width(empty, SCREEN_WIDTH - PAD * 2);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(empty, LV_ALIGN_TOP_MID, 0, 60);
        return;
    }

    for (uint8_t i = 0; i < count; ++i) {
        analysis_history_record_t record = {};
        if (analysis_history_get(i, &record)) {
            create_row(g_state.list, record, i);
        }
    }
    ESP_LOGI(TAG, "History refreshed (%u records)", static_cast<unsigned>(count));
}

}  // namespace

lv_obj_t* history_screen_create(void) {
    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen, lv_color_hex(STYLE_COLOR_BG_DARK), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen, screen_loaded_cb, LV_EVENT_SCREEN_LOADED, nullptr);
    g_state.screen = screen;

    navbar_create_with_back(screen, "History", back_cb);

    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, SCREEN_WIDTH - PAD * 2, 52);
    lv_obj_set_pos(header, PAD, NAVBAR_HEIGHT + 12);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    g_state.count_label = lv_label_create(header);
    lv_label_set_text(g_state.count_label, "0 captured analyses");
    lv_obj_set_style_text_font(g_state.count_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(g_state.count_label, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_align(g_state.count_label, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* clear_btn = lv_btn_create(header);
    lv_obj_set_size(clear_btn, 110, 40);
    lv_obj_align(clear_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(clear_btn, lv_color_hex(STYLE_COLOR_BG_CARD), 0);
    lv_obj_set_style_radius(clear_btn, 8, 0);
    lv_obj_set_style_shadow_width(clear_btn, 0, 0);
    lv_obj_add_event_cb(clear_btn, clear_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* clear_label = lv_label_create(clear_btn);
    lv_label_set_text(clear_label, "Clear");
    lv_obj_set_style_text_font(clear_label, &lv_font_montserrat_16, 0);
    lv_obj_center(clear_label);

    g_state.list = lv_obj_create(screen);
    lv_obj_remove_style_all(g_state.list);
    lv_obj_set_size(g_state.list, SCREEN_WIDTH, SCREEN_HEIGHT - NAVBAR_HEIGHT - 76);
    lv_obj_set_pos(g_state.list, 0, NAVBAR_HEIGHT + 72);
    lv_obj_set_style_pad_hor(g_state.list, PAD, 0);
    lv_obj_set_style_pad_row(g_state.list, 10, 0);
    lv_obj_set_flex_flow(g_state.list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_state.list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(g_state.list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(g_state.list, LV_SCROLLBAR_MODE_AUTO);

    refresh_history();
    return screen;
}
