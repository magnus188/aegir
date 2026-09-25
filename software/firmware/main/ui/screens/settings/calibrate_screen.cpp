#include "calibrate_screen.h"
#include "sensors/sensor_interface.h"
#include "services/gas_calibration_service.h"
#include "services/oxygen_selection_service.h"
#include "services/sd_log_service.h"
#include "../screen_manager.h"
#include "../../components/navbar.h"
#include "../../styles/styles.h"
#include <esp_log.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
constexpr int WIDTH = 480;
constexpr int NAV_HEIGHT = 70;
constexpr int CONTENT_HEIGHT = 730;
constexpr int KEYBOARD_HEIGHT = 260;
struct CalibrateState {
    lv_obj_t *screen{}, *content{}, *keyboard{}, *mode_label{}, *record_label{};
    lv_obj_t *channel_matrix{}, *step_matrix{}, *sample_label{}, *guide_label{};
    lv_obj_t *oxygen_input{}, *helium_input{}, *uncertainty_input{}, *reference_input{};
    lv_obj_t *points_label{}, *status_label{}, *capture_btn{}, *save_btn{};
    lv_obj_t *sim_profile{};
    lv_obj_t *selection_matrix{}, *selection_status{}, *replacement_check{}, *air_check{}, *air_advice{};
    lv_timer_t *timer{};
    gas_cal_channel_t channel = GAS_CAL_AO2;
    unsigned point = 0;
};
CalibrateState g;
const char *CHANNELS[] = {"AO2", "JJ-CCR", "Helium", ""};
const char *STEPS[] = {"1. Baseline", "2. Span", ""};
const char *SELECTIONS[] = {"Not set", "AO2", "JJ-CCR", ""};
void defaults_for_point();

lv_obj_t *label(lv_obj_t *parent, const char *text, const lv_font_t *font = &lv_font_montserrat_16,
                uint32_t color = STYLE_COLOR_TEXT_LIGHT) {
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, text);
    lv_obj_set_width(obj, lv_pct(100));
    lv_label_set_long_mode(obj, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    return obj;
}

lv_obj_t *panel(lv_obj_t *parent) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_width(obj, lv_pct(100));
    lv_obj_set_height(obj, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(obj, lv_color_hex(STYLE_COLOR_SURFACE), 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(STYLE_COLOR_BORDER), 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_radius(obj, 8, 0);
    lv_obj_set_style_pad_all(obj, 12, 0);
    lv_obj_set_style_pad_row(obj, 10, 0);
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

void status(const char *text, bool error = false) {
    lv_label_set_text(g.status_label, text);
    lv_obj_set_style_text_color(g.status_label, lv_color_hex(error ? STYLE_COLOR_ERROR : STYLE_COLOR_TEXT_DIM), 0);
}

void close_keyboard() {
    lv_keyboard_set_textarea(g.keyboard, nullptr);
    lv_obj_add_flag(g.keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_height(g.content, CONTENT_HEIGHT);
}

void keyboard_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_READY || lv_event_get_code(e) == LV_EVENT_CANCEL) close_keyboard();
}

void input_focus_cb(lv_event_t *e) {
    lv_obj_t *input = static_cast<lv_obj_t *>(lv_event_get_target(e));
    lv_keyboard_set_mode(g.keyboard, input == g.reference_input ? LV_KEYBOARD_MODE_TEXT_LOWER : LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(g.keyboard, input);
    lv_obj_remove_flag(g.keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_height(g.content, CONTENT_HEIGHT - KEYBOARD_HEIGHT);
    lv_obj_update_layout(g.content);
    lv_obj_scroll_to_view_recursive(input, LV_ANIM_OFF);
}

lv_obj_t *input(lv_obj_t *parent, const char *title, const char *value, bool numeric = true) {
    label(parent, title, &lv_font_montserrat_14, STYLE_COLOR_TEXT_DIM);
    lv_obj_t *obj = lv_textarea_create(parent);
    lv_obj_set_width(obj, lv_pct(100));
    lv_obj_set_height(obj, 44);
    lv_textarea_set_one_line(obj, true);
    lv_textarea_set_max_length(obj, numeric ? 7 : 63);
    if (numeric) lv_textarea_set_accepted_chars(obj, "0123456789.");
    lv_textarea_set_text(obj, value);
    lv_obj_set_style_text_font(obj, &lv_font_montserrat_18, 0);
    lv_obj_add_event_cb(obj, input_focus_cb, LV_EVENT_FOCUSED, nullptr);
    return obj;
}

lv_obj_t *button(lv_obj_t *parent, const char *text, lv_event_cb_t cb) {
    lv_obj_t *obj = lv_button_create(parent);
    lv_obj_set_width(obj, lv_pct(100));
    lv_obj_set_height(obj, 48);
    lv_obj_set_style_bg_color(obj, lv_color_hex(STYLE_COLOR_PRIMARY), 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(STYLE_COLOR_BORDER), LV_STATE_DISABLED);
    lv_obj_set_style_radius(obj, 7, 0);
    lv_obj_add_event_cb(obj, cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *text_obj = label(obj, text);
    lv_obj_set_style_text_align(text_obj, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(text_obj);
    return obj;
}

lv_obj_t *matrix(lv_obj_t *parent, const char *const *map, lv_event_cb_t cb) {
    lv_obj_t *obj = lv_buttonmatrix_create(parent);
    lv_obj_set_width(obj, lv_pct(100));
    lv_obj_set_height(obj, 50);
    lv_obj_set_style_pad_all(obj, 4, 0);
    lv_obj_set_style_pad_column(obj, 6, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_buttonmatrix_set_map(obj, map);
    lv_buttonmatrix_set_button_ctrl_all(obj, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_one_checked(obj, true);
    lv_buttonmatrix_set_button_ctrl(obj, 0, LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_obj_set_style_text_font(obj, &lv_font_montserrat_16, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(obj, lv_color_hex(STYLE_COLOR_BG_CARD), LV_PART_ITEMS);
    lv_obj_set_style_text_color(obj, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), LV_PART_ITEMS);
    lv_obj_set_style_radius(obj, 6, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(obj, lv_color_hex(STYLE_COLOR_PRIMARY), static_cast<lv_style_selector_t>(LV_PART_ITEMS) | static_cast<lv_style_selector_t>(LV_STATE_CHECKED));
    lv_obj_add_event_cb(obj, cb, LV_EVENT_VALUE_CHANGED, nullptr);
    return obj;
}

void enabled(lv_obj_t *obj, bool yes) {
    if (yes) lv_obj_clear_state(obj, LV_STATE_DISABLED);
    else lv_obj_add_state(obj, LV_STATE_DISABLED);
}

const char *sample_gate(const gas_raw_sample_t &s) {
    if (s.faults & GAS_FAULT_CLIPPED) return "ADC clipped: capture blocked. Check signal range.";
    if (s.faults & GAS_FAULT_STALE) return "Stale sample: capture blocked. Check connection.";
    if (s.faults & GAS_FAULT_EXCITATION) return "Sensor supply fault: capture blocked.";
    if (s.faults & GAS_FAULT_BIAS) return "Input bias fault: capture blocked.";
    if (s.faults) return "Sensor/ADC fault: capture blocked. Check connection.";
    if (!s.warmed) return "Warming: wait for the sensor to settle.";
    if (!s.stable) return "Not stable: keep reference gas flowing and wait.";
    return "Stable raw signal. Confirm the reference gas, then capture.";
}

void refresh() {
    // On hardware this only retrieves the acquisition cache; it does not drive the ADC.
    sensor_readings_t readings{};
    sensor_read_all(&readings);
    const bool simulated = gas_calibration_is_simulated();
    oxygen_selection_status_t selection{};
    oxygen_selection_get_status(&selection);
    if (g.selection_status) {
        char config[220];
        std::snprintf(config, sizeof(config), "Installed O2: %s | %s\nOnly this input supplies the oxygen reading. Old records remain stored.",
            oxygen_selection_label(selection.choice), selection.storage_error ? "storage unavailable" :
            !selection.configured ? "confirmation required" : selection.calibration_required ? "fresh calibration required" : "calibration saved");
        lv_label_set_text(g.selection_status, config);
    }
    if (g.air_advice) {
        const bool known_air = lv_obj_has_state(g.air_check, LV_STATE_CHECKED);
        gas_raw_sample_t a{}, j{};
        const bool av = sensor_get_raw(GAS_CAL_AO2, &a), jv = sensor_get_raw(GAS_CAL_JJCCR, &j);
        char air[420], a_value[32], j_value[32];
        if (av && !a.faults && std::isfinite(a.voltage_v)) std::snprintf(a_value, sizeof(a_value), "%.3f mV", a.voltage_v * 1000);
        else std::snprintf(a_value, sizeof(a_value), "unavailable");
        if (jv && !j.faults && std::isfinite(j.voltage_v)) std::snprintf(j_value, sizeof(j_value), "%.3f mV", j.voltage_v * 1000);
        else std::snprintf(j_value, sizeof(j_value), "unavailable");
        std::snprintf(air, sizeof(air), "%s\nAO2 input: %s | JJ input: %s", oxygen_selection_air_advice_label(
            oxygen_selection_air_advice(known_air)), a_value, j_value);
        lv_label_set_text(g.air_advice, air);
    }
    char mode[128]; std::snprintf(mode,sizeof(mode),"%s\n%s",simulated ? "SIMULATION - practice data" : "HARDWARE - bench calibration",sd_log::state_label(sd_log_status().state));
    lv_label_set_text(g.mode_label,mode);
    if (g.sim_profile) {
        if (simulated) lv_obj_remove_flag(g.sim_profile, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(g.sim_profile, LV_OBJ_FLAG_HIDDEN);
        const auto profile = sensor_get_mock_profile();
        lv_dropdown_set_selected(g.sim_profile, profile == SENSOR_MOCK_PROFILE_EAN32 ? 1 :
            profile == SENSOR_MOCK_PROFILE_TRIMIX_18_45 ? 2 : profile == SENSOR_MOCK_PROFILE_UNSTABLE ? 3 :
            profile == SENSOR_MOCK_PROFILE_SENSOR_FAULT ? 4 : profile == SENSOR_MOCK_PROFILE_HIGH_CO2 ? 5 : 0);
    }
    gas_raw_sample_t sample{};
    char buf[256];
    if (sensor_get_raw(g.channel, &sample)) {
        char voltage[32];
        if (std::isfinite(sample.voltage_v)) std::snprintf(voltage, sizeof(voltage), "%.3f mV", sample.voltage_v * 1000.0);
        else std::snprintf(voltage, sizeof(voltage), "unavailable");
        std::snprintf(buf, sizeof(buf), "%s  |  %s\nSpread %.3f mV  |  %s",
                      gas_calibration_channel_label(g.channel), voltage,
                      sample.peak_to_peak_v * 1000.0, sample_gate(sample));
        lv_label_set_text(g.sample_label, buf);
        enabled(g.capture_btn, oxygen_selection_channel_enabled(g.channel) && sample.warmed && sample.stable && !sample.faults &&
                              std::isfinite(sample.voltage_v));
    } else {
        lv_label_set_text(g.sample_label, "No raw sample. Waiting for the acquisition connection.");
        enabled(g.capture_btn, false);
    }
    gas_cal_record_t saved{};
    if (gas_calibration_get_record(g.channel, &saved) && saved.valid) {
        std::snprintf(buf, sizeof(buf), "%s record r%u | %s\n%s\n%s",
                      saved.simulated ? "Simulation" : "Hardware", static_cast<unsigned>(saved.revision),
                      saved.characterization_validated ? "characterized" : "bench only; accuracy unverified",
                      gas_calibration_record_is_current(&saved) &&
                          (g.channel == GAS_CAL_HELIUM || oxygen_selection_calibration_usable(g.channel, saved.revision)) ?
                          "Current channel calibration." : "Stored for reference; not active for this installation.",
                      saved.reference_budget_met ? "Reference uncertainty recorded." : "Reference uncertainty unknown or above budget.");
        lv_label_set_text(g.record_label, buf);
    } else {
        lv_label_set_text(g.record_label, "No saved calibration for this channel and data source.");
    }
    gas_cal_point_t points[2]{};
    bool captured[2] = {gas_calibration_get_point(g.channel, 0, &points[0]) && points[0].captured,
                        gas_calibration_get_point(g.channel, 1, &points[1]) && points[1].captured};
    if (captured[0] || captured[1]) {
        char descriptions[2][100]{};
        for (unsigned i = 0; i < 2; ++i) {
            if (captured[i]) std::snprintf(descriptions[i], sizeof(descriptions[i]),
                "%s: O2 %.2f%% / He %.2f%%, %.3f mV", i ? "Span" : "Baseline",
                points[i].reference.oxygen_percent, points[i].reference.helium_percent,
                points[i].sample.mean_voltage_v * 1000.0);
            else std::snprintf(descriptions[i], sizeof(descriptions[i]), "%s: not captured", i ? "Span" : "Baseline");
        }
        std::snprintf(buf, sizeof(buf), "%s\n%s", descriptions[0], descriptions[1]);
        lv_label_set_text(g.points_label, buf);
    } else if (saved.valid) {
        std::snprintf(buf, sizeof(buf), "Saved r%u: O2 %.2f%% / He %.2f%%\nSecond gas: O2 %.2f%% / He %.2f%%\nNo replacement points captured.",
                      static_cast<unsigned>(saved.revision), saved.points[0].reference.oxygen_percent,
                      saved.points[0].reference.helium_percent, saved.points[1].reference.oxygen_percent,
                      saved.points[1].reference.helium_percent);
        lv_label_set_text(g.points_label, buf);
    } else lv_label_set_text(g.points_label, "Baseline: not captured\nSpan: not captured");
    enabled(g.save_btn, oxygen_selection_channel_enabled(g.channel) && captured[0] && captured[1]);
}

void selection_cb(lv_event_t *) {
    close_keyboard();
    status("Choice staged only. Check the installed sensor, then press Confirm installed sensor.");
}
void confirm_selection_cb(lv_event_t *) {
    const auto choice = static_cast<oxygen_selection_t>(lv_buttonmatrix_get_selected_button(g.selection_matrix));
    const bool replacement = lv_obj_has_state(g.replacement_check, LV_STATE_CHECKED);
    const auto result = oxygen_selection_confirm(choice, replacement);
    if (result == OXYGEN_SELECT_OK) {
        lv_obj_clear_state(g.replacement_check, LV_STATE_CHECKED);
        gas_cal_channel_t channel;
        if (oxygen_selection_selected_channel(&channel)) {
            g.channel = channel;
            lv_buttonmatrix_set_selected_button(g.channel_matrix, channel);
            lv_buttonmatrix_set_button_ctrl(g.channel_matrix, channel, LV_BUTTONMATRIX_CTRL_CHECKED);
        }
        g.point = 0;
        lv_buttonmatrix_set_selected_button(g.step_matrix, 0);
        lv_buttonmatrix_set_button_ctrl(g.step_matrix, 0, LV_BUTTONMATRIX_CTRL_CHECKED);
        defaults_for_point();
        status("Installed sensor confirmed. Complete a fresh calibration when requested. This choice is not automatic sensor identification.");
    } else status(result == OXYGEN_SELECT_BAD_CHOICE ? "Choose AO2 or JJ-CCR before confirming." :
        "Could not save sensor selection. Previous configuration retained; check storage status.", true);
    refresh();
}
void air_check_cb(lv_event_t *) {
    oxygen_selection_set_probe_active(lv_obj_has_state(g.air_check, LV_STATE_CHECKED));
    refresh();
}

void defaults_for_point() {
    close_keyboard();
    lv_textarea_set_text(g.oxygen_input, g.point == 0 ? "20.9" : (g.channel == GAS_CAL_HELIUM ? "18.0" : "32.0"));
    lv_textarea_set_text(g.helium_input, g.point == 0 || g.channel != GAS_CAL_HELIUM ? "0.0" : "45.0");
    lv_textarea_set_text(g.uncertainty_input, "");
    lv_textarea_set_text(g.reference_input, "");
    lv_label_set_text(g.guide_label, g.point == 0 ?
        "1. Baseline: apply a known reference gas. It can be air for a comparison calibration; a zero point is not assumed." :
        "2. Span: apply a second known gas with a different concentration for the selected sensor. Enter its stated composition.");
    status("Enter the gas label values. Preset numbers are examples, not a certificate.");
}

void select_channel_cb(lv_event_t *e) {
    unsigned selected = lv_buttonmatrix_get_selected_button(static_cast<lv_obj_t *>(lv_event_get_target(e)));
    if (selected >= GAS_CAL_CHANNEL_COUNT) return;
    g.channel = static_cast<gas_cal_channel_t>(selected);
    g.point = 0;
    lv_buttonmatrix_set_button_ctrl(g.step_matrix, 0, LV_BUTTONMATRIX_CTRL_CHECKED);
    defaults_for_point();
    refresh();
}

void select_point_cb(lv_event_t *e) {
    unsigned selected = lv_buttonmatrix_get_selected_button(static_cast<lv_obj_t *>(lv_event_get_target(e)));
    if (selected > 1) return;
    g.point = selected;
    defaults_for_point();
    refresh();
}

bool parse_number(lv_obj_t *obj, float *out) {
    const char *text = lv_textarea_get_text(obj);
    if (!text || !*text) return false;
    char *end{};
    float value = std::strtof(text, &end);
    if (end == text || *end || !std::isfinite(value)) return false;
    *out = value;
    return true;
}

void capture_cb(lv_event_t *) {
    close_keyboard();
    gas_cal_reference_t reference{};
    if (!parse_number(g.oxygen_input, &reference.oxygen_percent) ||
        !parse_number(g.helium_input, &reference.helium_percent) ||
        reference.oxygen_percent < 0 || reference.helium_percent < 0 ||
        reference.oxygen_percent + reference.helium_percent > 100.0f) {
        status("Enter valid O2 and He percentages; their sum must be at most 100%.", true);
        return;
    }
    const char *uncertainty = lv_textarea_get_text(g.uncertainty_input);
    if (uncertainty && *uncertainty) {
        if (!parse_number(g.uncertainty_input, &reference.uncertainty_pp) || reference.uncertainty_pp <= 0) {
            status("Reference uncertainty must be a positive number or blank.", true);
            return;
        }
        reference.uncertainty_known = true;
    }
    const char *id = lv_textarea_get_text(g.reference_input);
    if (!id || !*id) {
        status("Enter a reference ID, e.g. cylinder/certificate number or 'ambient air'.", true);
        return;
    }
    std::snprintf(reference.reference_id, sizeof(reference.reference_id), "%s", id);
    gas_cal_result_t result = gas_calibration_capture(g.channel, g.point, &reference);
    sd_log_calibration_event(g.point==0 ? "capture_baseline":"capture_span",g.channel,result,&reference);
    if (result == GAS_CAL_OK) {
        status(g.point == 0 ? "Baseline captured. Select Span and apply the second reference gas." :
                             "Span captured. Review both points, then save. Saved calibration is unchanged until you save.");
    } else {
        char buf[192];
        std::snprintf(buf, sizeof(buf), "Capture rejected: %s. Previous calibration retained.", gas_calibration_result_label(result));
        status(buf, true);
    }
    refresh();
}

void save_cb(lv_event_t *) {
    close_keyboard();
    gas_cal_result_t result = gas_calibration_save(g.channel);
    sd_log_calibration_event("save",g.channel,result);
    if (result == GAS_CAL_OK) status(gas_calibration_is_simulated() ?
        "Simulation calibration saved separately. This does not calibrate physical sensors." :
        "Bench calibration saved. Known-gas and environmental validation are still required.");
    else {
        char buf[192];
        std::snprintf(buf, sizeof(buf), "Save rejected: %s. Previous calibration retained.", gas_calibration_result_label(result));
        status(buf, true);
    }
    refresh();
}

void cancel_cb(lv_event_t *) {
    close_keyboard();
    gas_calibration_cancel(g.channel);
    status("Draft points discarded. The saved calibration is unchanged.");
    refresh();
}

void simulation_cb(lv_event_t *e) {
    if (!gas_calibration_is_simulated()) return;
    const sensor_mock_profile_t profiles[] = {SENSOR_MOCK_PROFILE_AIR, SENSOR_MOCK_PROFILE_EAN32,
        SENSOR_MOCK_PROFILE_TRIMIX_18_45, SENSOR_MOCK_PROFILE_UNSTABLE, SENSOR_MOCK_PROFILE_SENSOR_FAULT,
        SENSOR_MOCK_PROFILE_HIGH_CO2};
    const unsigned selected = lv_dropdown_get_selected(static_cast<lv_obj_t *>(lv_event_get_target(e)));
    if (selected < sizeof(profiles) / sizeof(profiles[0])) sensor_set_mock_profile(profiles[selected]);
    refresh();
}
void back_cb(lv_event_t *) { close_keyboard(); screen_manager_show(SCREEN_SETTINGS); }
void timer_cb(lv_timer_t *) {
    sensor_readings_t readings{};
    if(sensor_read_all(&readings)==ESP_OK)sd_log_result(readings);
    refresh();
}
void visibility_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_SCREEN_LOADED) {
        sd_log_mode(sd_log::Mode::Calibration);
        oxygen_selection_set_probe_active(lv_obj_has_state(g.air_check, LV_STATE_CHECKED));
        refresh(); lv_timer_reset(g.timer); lv_timer_resume(g.timer);
    } else {
        sd_log_end_mode(sd_log::Mode::Calibration);
        oxygen_selection_set_probe_active(false);
        lv_obj_clear_state(g.air_check, LV_STATE_CHECKED);
        lv_timer_pause(g.timer); close_keyboard();
    }
}
} // namespace

lv_obj_t *calibrate_screen_create(void) {
    g = CalibrateState{};
    g.screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(g.screen, lv_color_hex(STYLE_COLOR_BG_DARK), 0);
    lv_obj_clear_flag(g.screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(g.screen, LV_DIR_NONE);
    lv_obj_t *navbar = navbar_create_with_back(g.screen, gas_calibration_is_simulated() ? "Calibration (demo)" : "Gas calibration", back_cb);
    lv_obj_add_flag(navbar, LV_OBJ_FLAG_FLOATING);
    g.content = lv_obj_create(g.screen);
    lv_obj_remove_style_all(g.content);
    lv_obj_set_pos(g.content, 0, NAV_HEIGHT);
    lv_obj_set_size(g.content, WIDTH, CONTENT_HEIGHT);
    lv_obj_set_style_pad_all(g.content, 14, 0);
    lv_obj_set_style_pad_row(g.content, 12, 0);
    lv_obj_set_flex_flow(g.content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(g.content, LV_DIR_VER);

    lv_obj_t *intro = panel(g.content);
    g.mode_label = label(intro, "--", &lv_font_montserrat_18, STYLE_COLOR_WARNING);
    label(intro, "Calibrate each sensor separately using known gases. Software corrects zero and sensitivity; it does not identify an unknown gas.", &lv_font_montserrat_14, STYLE_COLOR_TEXT_DIM);
    lv_obj_t *setup = panel(g.content);
    label(setup, "1. Installed oxygen sensor", &lv_font_montserrat_18);
    g.selection_status = label(setup, "Unconfigured", &lv_font_montserrat_14, STYLE_COLOR_WARNING);
    g.selection_matrix = matrix(setup, SELECTIONS, selection_cb);
    oxygen_selection_status_t initial{}; oxygen_selection_get_status(&initial);
    lv_buttonmatrix_set_selected_button(g.selection_matrix, initial.choice);
    lv_buttonmatrix_set_button_ctrl(g.selection_matrix, initial.choice, LV_BUTTONMATRIX_CTRL_CHECKED);
    g.replacement_check = lv_checkbox_create(setup);
    lv_checkbox_set_text(g.replacement_check, "New / replacement cell: recalibrate");
    lv_obj_set_style_text_font(g.replacement_check, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g.replacement_check, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    button(setup, "Confirm installed sensor", confirm_selection_cb);
    label(setup, "Choose the type physically installed. Changing type or replacing a cell blocks its previous calibration until you save a new one.", &lv_font_montserrat_14, STYLE_COLOR_TEXT_DIM);
    g.air_check = lv_checkbox_create(setup);
    lv_checkbox_set_text(g.air_check, "Known fresh air applied: check inputs");
    lv_obj_set_style_text_font(g.air_check, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g.air_check, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_add_event_cb(g.air_check, air_check_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    g.air_advice = label(setup, "--", &lv_font_montserrat_14, STYLE_COLOR_TEXT_DIM);
    label(g.content, "2. Calibrate the selected sensor", &lv_font_montserrat_18);
    g.channel_matrix = matrix(g.content, CHANNELS, select_channel_cb);
    g.record_label = label(g.content, "--", &lv_font_montserrat_14, STYLE_COLOR_TEXT_DIM);
    g.sim_profile = lv_dropdown_create(g.content);
    lv_obj_set_width(g.sim_profile, lv_pct(100));
    lv_dropdown_set_options(g.sim_profile, "Demo gas: Air\nDemo gas: EAN32\nDemo gas: 18/45\nDemo gas: Unstable\nDemo gas: Fault\nDemo gas: High CO2");
    const sensor_mock_profile_t profile = sensor_get_mock_profile();
    lv_dropdown_set_selected(g.sim_profile, profile == SENSOR_MOCK_PROFILE_EAN32 ? 1 : profile == SENSOR_MOCK_PROFILE_TRIMIX_18_45 ? 2 : 0);
    lv_obj_add_event_cb(g.sim_profile, simulation_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *sample = panel(g.content);
    g.sample_label = label(sample, "--", &lv_font_montserrat_16, STYLE_COLOR_DATA);
    g.step_matrix = matrix(g.content, STEPS, select_point_cb);
    g.guide_label = label(g.content, "--", &lv_font_montserrat_16);

    lv_obj_t *references = panel(g.content);
    label(references, "Known reference gas", &lv_font_montserrat_18);
    lv_obj_t *row = lv_obj_create(references);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100)); lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(row, 12, 0);
    lv_obj_t *oxygen = lv_obj_create(row); lv_obj_remove_style_all(oxygen);
    lv_obj_set_flex_grow(oxygen, 1); lv_obj_set_height(oxygen, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(oxygen, LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_row(oxygen, 5, 0);
    g.oxygen_input = input(oxygen, "O2 volume %", "20.9");
    lv_obj_t *helium = lv_obj_create(row); lv_obj_remove_style_all(helium);
    lv_obj_set_flex_grow(helium, 1); lv_obj_set_height(helium, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(helium, LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_row(helium, 5, 0);
    g.helium_input = input(helium, "He volume %", "0.0");
    g.reference_input = input(references, "Reference ID / cylinder / certificate", "", false);
    g.uncertainty_input = input(references, "Uncertainty +/- percentage points (optional)", "");
    label(references, "Use the uncertainty stated for the selected sensor's gas. Blank means comparison/bench only.", &lv_font_montserrat_14, STYLE_COLOR_TEXT_DIM);
    g.capture_btn = button(references, "Capture reference point", capture_cb);

    lv_obj_t *review = panel(g.content);
    label(review, "Review and save", &lv_font_montserrat_18);
    g.points_label = label(review, "--", &lv_font_montserrat_14);
    g.save_btn = button(review, "Save channel calibration", save_cb);
    button(review, "Discard draft points", cancel_cb);
    g.status_label = label(review, "--", &lv_font_montserrat_16, STYLE_COLOR_TEXT_DIM);
    label(g.content, "He: a two-point fit does not validate the MD62 across trimix compositions. CO: follow the exact module's documented service procedure; no CO calibration command is enabled. CO2 history remains separate.", &lv_font_montserrat_14, STYLE_COLOR_TEXT_DIM);

    g.keyboard = lv_keyboard_create(g.screen);
    lv_obj_add_flag(g.keyboard, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(g.keyboard, WIDTH, KEYBOARD_HEIGHT);
    lv_obj_align(g.keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(g.keyboard, keyboard_cb, LV_EVENT_READY, nullptr);
    lv_obj_add_event_cb(g.keyboard, keyboard_cb, LV_EVENT_CANCEL, nullptr);
    close_keyboard();
    gas_cal_channel_t selected_channel;
    if (oxygen_selection_selected_channel(&selected_channel)) {
        g.channel = selected_channel;
        lv_buttonmatrix_set_selected_button(g.channel_matrix, selected_channel);
        lv_buttonmatrix_set_button_ctrl(g.channel_matrix, selected_channel, LV_BUTTONMATRIX_CTRL_CHECKED);
    }
    defaults_for_point(); refresh();
    g.timer = lv_timer_create(timer_cb, 500, nullptr);
    lv_timer_pause(g.timer);
    lv_obj_add_event_cb(g.screen, visibility_cb, LV_EVENT_SCREEN_LOADED, nullptr);
    lv_obj_add_event_cb(g.screen, visibility_cb, LV_EVENT_SCREEN_UNLOADED, nullptr);
    return g.screen;
}
