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
constexpr int PAD = 16;
constexpr int ROW_HEIGHT = 252;

struct HistoryState {
    lv_obj_t* list = nullptr;
    lv_obj_t* count_label = nullptr;
};

HistoryState g_state;

uint32_t severity_color(analysis_severity_t severity) {
    switch (severity) {
        case ANALYSIS_SEVERITY_NORMAL: return STYLE_COLOR_SUCCESS;
        case ANALYSIS_SEVERITY_ADVISORY: return STYLE_COLOR_WARNING;
        case ANALYSIS_SEVERITY_ALARM:
        case ANALYSIS_SEVERITY_FAULT: return STYLE_COLOR_ERROR;
        default: return STYLE_COLOR_TEXT_DIM;
    }
}

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

void refresh_history();
void back_cb(lv_event_t*) { screen_manager_show(SCREEN_HOME); }
void clear_cb(lv_event_t*) { analysis_history_clear(); refresh_history(); }
void screen_loaded_cb(lv_event_t*) { refresh_history(); }

lv_obj_t* create_row(lv_obj_t* parent, const analysis_history_record_t& record, uint8_t index) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, SCREEN_WIDTH - PAD * 2, ROW_HEIGHT);
    lv_obj_set_style_bg_color(row, lv_color_hex(STYLE_COLOR_TILE), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, 8, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(STYLE_COLOR_TILE_BORDER), 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    char buf[128];
    std::snprintf(buf, sizeof(buf), "CAPTURE %02u", static_cast<unsigned>(index + 1));
    add_label(row, buf, 14, 12, &lv_font_montserrat_12, STYLE_COLOR_TEXT_DIM);
    const char* source = !record.source_known ? "Legacy" :
        (record.source == SENSOR_SOURCE_SIMULATED ? "Simulation" : "Hardware");
    add_label(row, source, 112, 12, &lv_font_montserrat_12, STYLE_COLOR_TEXT_DIM);
    lv_obj_t* severity = add_label(row, analysis_severity_label(record.severity), 330, 11,
                                    &lv_font_montserrat_14, severity_color(record.severity), 104);
    lv_obj_set_style_text_align(severity, LV_TEXT_ALIGN_RIGHT, 0);
    add_label(row, record.mix_label, 14, 33, &lv_font_montserrat_22,
              STYLE_COLOR_TEXT_LIGHT, 414);

    add_label(row, "OXYGEN", 14, 76, &lv_font_montserrat_12, STYLE_COLOR_TEXT_DIM);
    add_label(row, "HELIUM", 164, 76, &lv_font_montserrat_12, STYLE_COLOR_TEXT_DIM);
    add_label(row, "NITROGEN", 316, 76, &lv_font_montserrat_12, STYLE_COLOR_TEXT_DIM);
    std::snprintf(buf, sizeof(buf), "%.1f%%", record.oxygen_percent);
    add_label(row, buf, 14, 94, &lv_font_montserrat_28, STYLE_COLOR_CYAN);
    std::snprintf(buf, sizeof(buf), "%.0f%%", record.helium_percent);
    add_label(row, buf, 164, 94, &lv_font_montserrat_28, STYLE_COLOR_CYAN);
    std::snprintf(buf, sizeof(buf), "%.1f%%", record.nitrogen_percent);
    add_label(row, buf, 316, 99, &lv_font_montserrat_20, STYLE_COLOR_TEXT_LIGHT);

    lv_obj_t* rule = lv_obj_create(row);
    lv_obj_remove_style_all(rule);
    lv_obj_set_pos(rule, 14, 136);
    lv_obj_set_size(rule, 420, 1);
    lv_obj_set_style_bg_color(rule, lv_color_hex(STYLE_COLOR_TILE_BORDER), 0);
    lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);

    char co[24], co2[24];
    if (record.co_valid && std::isfinite(record.co_ppm))
        std::snprintf(co, sizeof(co), "%.1f ppm", record.co_ppm);
    else std::snprintf(co, sizeof(co), "not measured");
    if (std::isfinite(record.co2_ppm) && record.co2_ppm >= 0)
        std::snprintf(co2, sizeof(co2), "%.0f ppm", record.co2_ppm);
    else std::snprintf(co2, sizeof(co2), "not measured");
    std::snprintf(buf, sizeof(buf), "CO %s   |   CO2 %s", co, co2);
    add_label(row, buf, 14, 148, &lv_font_montserrat_14, STYLE_COLOR_TEXT_LIGHT, 420);
    std::snprintf(buf, sizeof(buf), "%s   Depth %.0fm   MOD %.0f/%.0fm   PPO2 %.2f",
                  analysis_gas_mode_label(record.gas_mode), record.planned_depth_m,
                  record.mod_working_m, record.mod_secondary_m, record.ppo2_at_depth);
    add_label(row, buf, 14, 174, &lv_font_montserrat_14, STYLE_COLOR_TEXT_DIM, 420);
    std::snprintf(buf, sizeof(buf), "EAD %.0fm   END %.0fm   Density %.1f g/L",
                  record.ead_m, record.end_m, record.gas_density_g_l);
    add_label(row, buf, 14, 198, &lv_font_montserrat_14, STYLE_COLOR_TEXT_DIM, 420);
    if (record.oxygen_selection == OXYGEN_UNCONFIGURED)
        std::snprintf(buf, sizeof(buf), "O2 sensor identity: not recorded");
    else std::snprintf(buf, sizeof(buf), "O2 sensor: %s | setup %u | calibration r%u",
        oxygen_selection_label(record.oxygen_selection),
        static_cast<unsigned>(record.oxygen_selection_generation),
        static_cast<unsigned>(record.oxygen_calibration_revision));
    add_label(row, buf, 14, 222, &lv_font_montserrat_12, STYLE_COLOR_TEXT_DIM, 420);
    return row;
}

void refresh_history() {
    if (!g_state.list) return;
    lv_obj_clean(g_state.list);
    const uint8_t count = analysis_history_count();
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%02u", static_cast<unsigned>(count));
    lv_label_set_text(g_state.count_label, buf);

    if (count == 0) {
        lv_obj_t* empty = lv_obj_create(g_state.list);
        lv_obj_set_size(empty, SCREEN_WIDTH - PAD * 2, 180);
        lv_obj_set_style_bg_color(empty, lv_color_hex(STYLE_COLOR_TILE), 0);
        lv_obj_set_style_bg_opa(empty, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(empty, 1, 0);
        lv_obj_set_style_border_color(empty, lv_color_hex(STYLE_COLOR_TILE_BORDER), 0);
        lv_obj_set_style_radius(empty, 8, 0);
        lv_obj_set_style_pad_all(empty, 0, 0);
        lv_obj_clear_flag(empty, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* title = add_label(empty, "NO CAPTURES YET", 0, 56,
                                    &lv_font_montserrat_20, STYLE_COLOR_TEXT_LIGHT,
                                    SCREEN_WIDTH - PAD * 2);
        lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_t* detail = add_label(empty,
            "Save an averaged analysis to see it here.", 0, 96,
            &lv_font_montserrat_14, STYLE_COLOR_TEXT_DIM, SCREEN_WIDTH - PAD * 2);
        lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_CENTER, 0);
        return;
    }

    for (uint8_t i = 0; i < count; ++i) {
        analysis_history_record_t record = {};
        if (analysis_history_get(i, &record)) create_row(g_state.list, record, i);
    }
    ESP_LOGI(TAG, "History refreshed (%u records)", static_cast<unsigned>(count));
}

}  // namespace

lv_obj_t* history_screen_create(void) {
    g_state = HistoryState{};
    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen, lv_color_hex(STYLE_COLOR_BG_DARK), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen, screen_loaded_cb, LV_EVENT_SCREEN_LOADED, nullptr);

    navbar_create_instrument(screen, "History", back_cb);
    g_state.count_label = add_label(screen, "00", PAD, 68,
                                    &lv_font_montserrat_32, STYLE_COLOR_CYAN);
    add_label(screen, "CAPTURED ANALYSES", 82, 82,
              &lv_font_montserrat_14, STYLE_COLOR_TEXT_DIM);

    lv_obj_t* clear_btn = lv_btn_create(screen);
    lv_obj_set_pos(clear_btn, 354, 66);
    lv_obj_set_size(clear_btn, 110, 42);
    lv_obj_set_style_bg_color(clear_btn, lv_color_hex(STYLE_COLOR_TILE), 0);
    lv_obj_set_style_border_width(clear_btn, 1, 0);
    lv_obj_set_style_border_color(clear_btn, lv_color_hex(STYLE_COLOR_TILE_BORDER), 0);
    lv_obj_set_style_radius(clear_btn, 7, 0);
    lv_obj_set_style_shadow_width(clear_btn, 0, 0);
    lv_obj_add_event_cb(clear_btn, clear_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* clear_label = lv_label_create(clear_btn);
    lv_label_set_text(clear_label, "Clear");
    lv_obj_set_style_text_font(clear_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(clear_label, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_center(clear_label);

    lv_obj_t* rule = lv_obj_create(screen);
    lv_obj_remove_style_all(rule);
    lv_obj_set_pos(rule, PAD, 124);
    lv_obj_set_size(rule, SCREEN_WIDTH - PAD * 2, 1);
    lv_obj_set_style_bg_color(rule, lv_color_hex(STYLE_COLOR_TILE_BORDER), 0);
    lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);

    g_state.list = lv_obj_create(screen);
    lv_obj_remove_style_all(g_state.list);
    lv_obj_set_size(g_state.list, SCREEN_WIDTH, SCREEN_HEIGHT - 138);
    lv_obj_set_pos(g_state.list, 0, 138);
    lv_obj_set_style_pad_hor(g_state.list, PAD, 0);
    lv_obj_set_style_pad_bottom(g_state.list, PAD, 0);
    lv_obj_set_style_pad_row(g_state.list, 10, 0);
    lv_obj_set_flex_flow(g_state.list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_state.list, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(g_state.list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(g_state.list, LV_SCROLLBAR_MODE_AUTO);

    refresh_history();
    return screen;
}
