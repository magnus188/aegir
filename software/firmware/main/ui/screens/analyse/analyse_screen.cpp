#include "services/sd_log_service.h"
#include "analyse_screen.h"
#include "analysis/analysis_calculator.h"
#include "sensors/sensor_interface.h"
#include "services/analysis_history.h"
#include "services/cylinder_profiles.h"
#include "services/mix_label_service.h"
#include "services/settings_service.h"
#include "services/gas_calibration_service.h"
#include "../screen_manager.h"
#include "../../components/navbar.h"
#include "../../styles/styles.h"
#include <esp_log.h>
#include <cstdio>
#include <cmath>
#include <initializer_list>

static const char* TAG = "ANALYSE_SCREEN";

namespace {

constexpr int SCREEN_WIDTH = 480;
constexpr int SCREEN_HEIGHT = 800;
constexpr int NAVBAR_HEIGHT = 70;
constexpr int PAD = 12;
constexpr uint8_t CAPTURE_AVERAGE_WINDOW = 5;
constexpr uint8_t CAPTURE_MIN_STABLE_SAMPLES = 3;

struct SampleSnapshot {
    sensor_readings_t readings = {};
    analysis_result_t result = {};
};

struct AnalyseState {
    lv_obj_t* screen = nullptr;
    lv_obj_t* status_label = nullptr;
    lv_obj_t* source_label = nullptr;
    lv_obj_t* o2_value = nullptr;
    lv_obj_t* jj_value = nullptr;
    lv_obj_t* he_value = nullptr;
    lv_obj_t* co_value = nullptr;
    lv_obj_t* env_value = nullptr;
    lv_obj_t* mix_value = nullptr;
    lv_obj_t* fractions_value = nullptr;
    lv_obj_t* mod_value = nullptr;
    lv_obj_t* equivalent_depth_value = nullptr;
    lv_obj_t* density_value = nullptr;
    lv_obj_t* ppo2_value = nullptr;
    lv_obj_t* advisory_label = nullptr;
    lv_obj_t* helium_value = nullptr;
    lv_obj_t* depth_value = nullptr;
    lv_obj_t* capture_status = nullptr;
    lv_obj_t* capture_btn = nullptr;
    lv_obj_t* cylinder_btn = nullptr;
    lv_obj_t* cylinder_label = nullptr;
    lv_obj_t* mode_matrix = nullptr;
    lv_obj_t* profile_matrix = nullptr;
    lv_obj_t* chart = nullptr;
    lv_chart_series_t* o2_series = nullptr;
    lv_chart_series_t* he_series = nullptr;
    lv_timer_t* sample_timer = nullptr;
    sensor_readings_t last_readings = {};
    analysis_result_t last_result = {};
    SampleSnapshot capture_samples[CAPTURE_AVERAGE_WINDOW] = {};
    uint8_t capture_sample_count = 0;
    uint8_t capture_sample_next = 0;
    analysis_gas_mode_t gas_mode = ANALYSIS_GAS_MODE_OC_BACK_GAS;
    float manual_helium = -1.0f;
    float planned_depth = 30.0f;
};

AnalyseState g_state;

static const char* profile_map[] = {
    "Air", "EAN32", "18/45", "\n",
    "CO2", "Unstable", "Fault", ""
};

static const char* mode_map[] = {
    "Back", "Deco", "CCR", "Bailout", ""
};

sensor_mock_profile_t profile_from_button(uint32_t id) {
    switch (id) {
        case 0: return SENSOR_MOCK_PROFILE_AIR;
        case 1: return SENSOR_MOCK_PROFILE_EAN32;
        case 2: return SENSOR_MOCK_PROFILE_TRIMIX_18_45;
        case 3: return SENSOR_MOCK_PROFILE_HIGH_CO2;
        case 4: return SENSOR_MOCK_PROFILE_UNSTABLE;
        case 5: return SENSOR_MOCK_PROFILE_SENSOR_FAULT;
        default: return SENSOR_MOCK_PROFILE_AIR;
    }
}

analysis_gas_mode_t mode_from_button(uint32_t id) {
    switch (id) {
        case 0: return ANALYSIS_GAS_MODE_OC_BACK_GAS;
        case 1: return ANALYSIS_GAS_MODE_DECO_GAS;
        case 2: return ANALYSIS_GAS_MODE_CCR_DILUENT;
        case 3: return ANALYSIS_GAS_MODE_BAILOUT;
        default: return ANALYSIS_GAS_MODE_OC_BACK_GAS;
    }
}

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

analysis_limits_t limits_from_settings() {
    return {
        .ppo2_working_x100 = settings_get(SETTING_PPO2_WORKING_X100),
        .ppo2_secondary_x100 = settings_get(SETTING_PPO2_SECONDARY_X100),
        .density_advisory_x10 = settings_get(SETTING_DENSITY_ADVISORY_X10),
        .density_alarm_x10 = settings_get(SETTING_DENSITY_ALARM_X10),
        .co2_advisory_ppm = settings_get(SETTING_CO2_ADVISORY_PPM),
    };
}

lv_obj_t* create_panel(lv_obj_t* parent, int w, int h) {
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_set_size(panel, w, h);
    lv_obj_set_style_bg_color(panel, lv_color_hex(STYLE_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(STYLE_COLOR_BORDER), 0);
    lv_obj_set_style_pad_all(panel, 10, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

lv_obj_t* create_metric(lv_obj_t* parent, const char* title, int x, int y, int w, int h,
                        lv_obj_t** value_out) {
    lv_obj_t* panel = create_panel(parent, w, h);
    lv_obj_set_pos(panel, x, y);

    lv_obj_t* title_label = lv_label_create(panel);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t* value = lv_label_create(panel);
    lv_label_set_text(value, "--");
    lv_obj_set_width(value, w - 20);
    lv_label_set_long_mode(value, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(value, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(value, lv_color_hex(STYLE_COLOR_DATA), 0);
    lv_obj_align(value, LV_ALIGN_BOTTOM_LEFT, 0, 2);
    *value_out = value;
    return panel;
}

lv_obj_t* create_small_button(lv_obj_t* parent, const char* text, int w, lv_event_cb_t cb, void* data) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, 34);
    lv_obj_set_style_bg_color(btn, lv_color_hex(STYLE_COLOR_BG_CARD), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(STYLE_COLOR_PRIMARY), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, data);
    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_center(label);
    return btn;
}

bool sample_usable_for_capture(const SampleSnapshot& sample) {
    return sample.result.valid && sample.readings.status == SENSOR_STATUS_STABLE;
}

void reset_capture_samples() {
    for (auto& sample : g_state.capture_samples) {
        sample = {};
    }
    g_state.capture_sample_count = 0;
    g_state.capture_sample_next = 0;
}

void remember_capture_sample(const sensor_readings_t& readings, const analysis_result_t& result) {
    g_state.capture_samples[g_state.capture_sample_next] = {readings, result};
    g_state.capture_sample_next = (g_state.capture_sample_next + 1U) % CAPTURE_AVERAGE_WINDOW;
    if (g_state.capture_sample_count < CAPTURE_AVERAGE_WINDOW) {
        ++g_state.capture_sample_count;
    }
}

uint8_t stable_capture_sample_count() {
    uint8_t count = 0;
    for (uint8_t i = 0; i < g_state.capture_sample_count; ++i) {
        if (sample_usable_for_capture(g_state.capture_samples[i])) {
            ++count;
        }
    }
    return count;
}

uint8_t averaged_capture_readings(sensor_readings_t* out) {
    if (!out) {
        return 0;
    }

    sensor_readings_t averaged = {};
    uint8_t count = 0;
    uint8_t co_count = 0, jj_count = 0;
    uint32_t latest_sequence = 0;

    for (uint8_t i = 0; i < g_state.capture_sample_count; ++i) {
        const SampleSnapshot& sample = g_state.capture_samples[i];
        if (!sample_usable_for_capture(sample)) {
            continue;
        }

        const sensor_readings_t& r = sample.readings;
        averaged.oxygen_percent += r.oxygen_percent;
        averaged.helium_percent += r.helium_percent;
        averaged.co2_ppm += r.co2_ppm;
        if (r.co_valid && std::isfinite(r.co_ppm)) { averaged.co_ppm += r.co_ppm; ++co_count; }
        if (r.oxygen_jj_valid && std::isfinite(r.oxygen_jj_percent)) { averaged.oxygen_jj_percent += r.oxygen_jj_percent; ++jj_count; }
        averaged.temperature_c += r.temperature_c;
        averaged.pressure_bar += r.pressure_bar;
        averaged.humidity_pct += r.humidity_pct;
        if (r.sequence >= latest_sequence) {
            latest_sequence = r.sequence;
            averaged.timestamp_ms = r.timestamp_ms;
            averaged.sequence = r.sequence;
            averaged.source = r.source;
            averaged.oxygen_calibrated = r.oxygen_calibrated;
            averaged.helium_calibrated = r.helium_calibrated;
            averaged.calibration_unvalidated = r.calibration_unvalidated;
            averaged.environment_valid = r.environment_valid;
            averaged.oxygen_selection = r.oxygen_selection;
            averaged.oxygen_selection_generation = r.oxygen_selection_generation;
            averaged.oxygen_calibration_revision = r.oxygen_calibration_revision;
            averaged.oxygen_configuration_required = r.oxygen_configuration_required;
            averaged.oxygen_calibration_required = r.oxygen_calibration_required;
        }
        ++count;
    }

    if (count < CAPTURE_MIN_STABLE_SAMPLES) {
        return count;
    }

    averaged.oxygen_percent /= count;
    averaged.helium_percent /= count;
    averaged.co2_ppm /= count;
    averaged.co_valid = co_count == count;
    averaged.co_ppm = averaged.co_valid ? averaged.co_ppm / count : NAN;
    averaged.oxygen_jj_valid = jj_count == count;
    averaged.oxygen_jj_percent = averaged.oxygen_jj_valid ? averaged.oxygen_jj_percent / count : NAN;
    averaged.temperature_c /= count;
    averaged.pressure_bar /= count;
    averaged.humidity_pct /= count;
    averaged.status = SENSOR_STATUS_STABLE;
    *out = averaged;
    return count;
}

void update_value_labels() {
    char buf[128];
    const sensor_readings_t& r = g_state.last_readings;
    const analysis_result_t& a = g_state.last_result;

    if (std::isfinite(r.oxygen_percent) && r.oxygen_percent >= 0 && r.oxygen_percent <= 100) {
        std::snprintf(buf, sizeof(buf), "%.1f%%", r.oxygen_percent);
        lv_label_set_text(g_state.o2_value, buf);
    } else lv_label_set_text(g_state.o2_value, std::isfinite(r.oxygen_percent) && r.oxygen_calibrated ? "Range" : "--");
    lv_label_set_text(g_state.jj_value, r.oxygen_configuration_required ? "Set up" : oxygen_selection_label(r.oxygen_selection));
    if (a.valid) {
        std::snprintf(buf, sizeof(buf), "%.1f%%", r.helium_percent);
        lv_label_set_text(g_state.he_value, buf);
    } else lv_label_set_text(g_state.he_value, r.source == SENSOR_SOURCE_HARDWARE &&
                            r.calibration_unvalidated && std::isfinite(r.helium_percent) ? "Bench" : "--");
    if (r.co_valid && std::isfinite(r.co_ppm)) {
        std::snprintf(buf, sizeof(buf), "%.1f", r.co_ppm);
        lv_label_set_text(g_state.co_value, buf);
    } else lv_label_set_text(g_state.co_value, "--");
    if (r.environment_valid || r.source == SENSOR_SOURCE_SIMULATED) {
        std::snprintf(buf, sizeof(buf), "%.1f C  %.2f bar  %.0f%% RH",
                      r.temperature_c, r.pressure_bar, r.humidity_pct);
        lv_label_set_text(g_state.env_value, buf);
    } else lv_label_set_text(g_state.env_value, "Environmental measurement unavailable");

    const char *display_status = sensor_status_label(r.status);
    if (r.oxygen_configuration_required) display_status = "Set up O2";
    else if (r.oxygen_calibration_required) display_status = "Calibrate";
    if (r.source == SENSOR_SOURCE_HARDWARE && r.status != SENSOR_STATUS_FAULT && r.status == SENSOR_STATUS_STABLE) {
        if (!r.oxygen_calibrated || !r.helium_calibrated) display_status = "Calibrate";
        else if (r.calibration_unvalidated) display_status = "Bench";
    }
    lv_label_set_text(g_state.status_label, display_status);
    lv_obj_set_style_text_color(g_state.status_label, lv_color_hex(severity_color(a.severity)), 0);

    const uint8_t stable_count = stable_capture_sample_count();
    std::snprintf(buf, sizeof(buf), "%s | %s | avg %u/%u | %s",
                  sensor_source_label(r.source),
                  r.source == SENSOR_SOURCE_SIMULATED ? sensor_mock_profile_name(sensor_get_mock_profile()) :
                  (r.calibration_unvalidated ? "bench only" : "acquisition"),
                  static_cast<unsigned>(stable_count),
                  static_cast<unsigned>(CAPTURE_AVERAGE_WINDOW),sd_log::state_label(sd_log_status().state));
    lv_label_set_text(g_state.source_label, buf);

    lv_label_set_text(g_state.mix_value, a.mix_label);
    std::snprintf(buf, sizeof(buf), "O2 %.1f%%  He %.0f%%  N2 %.1f%%",
                  a.oxygen_percent, a.helium_percent, a.nitrogen_percent);
    lv_label_set_text(g_state.fractions_value, buf);
    std::snprintf(buf, sizeof(buf), "MOD %.0f / %.0fm",
                  a.mod_working_m, a.mod_secondary_m);
    lv_label_set_text(g_state.mod_value, buf);
    std::snprintf(buf, sizeof(buf), "EAD %.0fm  END %.0fm", a.ead_m, a.end_m);
    lv_label_set_text(g_state.equivalent_depth_value, buf);
    std::snprintf(buf, sizeof(buf), "%.1f g/L", a.gas_density_g_l);
    lv_label_set_text(g_state.density_value, buf);
    lv_obj_set_style_text_color(g_state.density_value, lv_color_hex(severity_color(a.severity)), 0);
    std::snprintf(buf, sizeof(buf), "PPO2 %.2f bar at %.0fm", a.ppo2_at_depth, g_state.planned_depth);
    lv_label_set_text(g_state.ppo2_value, buf);
    lv_label_set_text(g_state.advisory_label, a.advisory);
    if (!a.valid) {
        lv_label_set_text(g_state.fractions_value, "Gas composition unavailable");
        lv_label_set_text(g_state.mod_value, "MOD --");
        lv_label_set_text(g_state.equivalent_depth_value, "EAD --  END --");
        lv_label_set_text(g_state.density_value, "-- g/L");
        lv_label_set_text(g_state.ppo2_value, "PPO2 --");
    }
    lv_obj_set_style_text_color(g_state.advisory_label, lv_color_hex(severity_color(a.severity)), 0);

    if (g_state.manual_helium >= 0.0f) {
        std::snprintf(buf, sizeof(buf), "He %.0f%%", g_state.manual_helium);
    } else {
        std::snprintf(buf, sizeof(buf), "He auto");
    }
    lv_label_set_text(g_state.helium_value, buf);
    std::snprintf(buf, sizeof(buf), "%.0fm", g_state.planned_depth);
    lv_label_set_text(g_state.depth_value, buf);

    if (g_state.cylinder_label) {
        cylinder_profile_t profile = {};
        if (cylinder_profiles_get_selected(&profile)) {
            std::snprintf(buf, sizeof(buf), "%s | %s | %s",
                          profile.name,
                          analysis_gas_mode_label(g_state.gas_mode),
                          profile.needs_recheck ? "needs check" : "ready");
            lv_label_set_text(g_state.cylinder_label, buf);
        }
    }

    if (g_state.capture_btn) {
        const bool capture_ready = a.valid &&
                                   r.status == SENSOR_STATUS_STABLE &&
                                   stable_count >= CAPTURE_MIN_STABLE_SAMPLES;
        if (capture_ready) {
            lv_obj_clear_state(g_state.capture_btn, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(g_state.capture_btn, LV_STATE_DISABLED);
        }
    }

    if (g_state.capture_status) {
        if (!a.valid) {
            lv_label_set_text(g_state.capture_status, "Save unavailable while sample is faulted");
        } else if (r.status != SENSOR_STATUS_STABLE) {
            lv_label_set_text(g_state.capture_status, "Waiting for stable sample");
        } else if (stable_count < CAPTURE_MIN_STABLE_SAMPLES) {
            std::snprintf(buf, sizeof(buf), "Averaging stable samples %u/%u",
                          static_cast<unsigned>(stable_count),
                          static_cast<unsigned>(CAPTURE_MIN_STABLE_SAMPLES));
            lv_label_set_text(g_state.capture_status, buf);
        } else {
            std::snprintf(buf, sizeof(buf), "Ready: %u-sample average",
                          static_cast<unsigned>(stable_count));
            lv_label_set_text(g_state.capture_status, buf);
        }
    }
}

void sample_once() {
    sensor_readings_t readings = {};
    if (sensor_read_all(&readings) != ESP_OK) {
        readings.status = SENSOR_STATUS_FAULT;
        readings.source = gas_calibration_is_simulated() ? SENSOR_SOURCE_SIMULATED : SENSOR_SOURCE_HARDWARE;
        readings.oxygen_percent = readings.oxygen_jj_percent = readings.helium_percent = NAN;
        readings.co_ppm = NAN;
        readings.oxygen_jj_valid = readings.co_valid = false;
    }
    if (g_state.last_readings.oxygen_selection_generation != readings.oxygen_selection_generation ||
        g_state.last_readings.oxygen_calibration_revision != readings.oxygen_calibration_revision)
        reset_capture_samples();
    g_state.last_readings = readings;

    analysis_input_t input = {};
    input.readings = readings;
    input.manual_he_percent = g_state.manual_helium;
    input.planned_depth_m = g_state.planned_depth;
    input.gas_mode = g_state.gas_mode;
    input.limits = limits_from_settings();
    g_state.last_result = analysis_calculate(&input);
    sd_log_result(readings,&g_state.last_result);
    remember_capture_sample(readings, g_state.last_result);

    if (g_state.chart && g_state.o2_series && g_state.he_series) {
        int o2 = g_state.last_result.valid ? static_cast<int>(g_state.last_result.oxygen_percent + 0.5f) : LV_CHART_POINT_NONE;
        int he = g_state.last_result.valid ? static_cast<int>(g_state.last_result.helium_percent + 0.5f) : LV_CHART_POINT_NONE;
        lv_chart_set_next_value(g_state.chart, g_state.o2_series, o2);
        lv_chart_set_next_value(g_state.chart, g_state.he_series, he);
    }
    update_value_labels();
}

void sample_timer_cb(lv_timer_t*) {
    sample_once();
}

void screen_visibility_cb(lv_event_t* event) {
    if (!g_state.sample_timer) return;

    if (lv_event_get_code(event) == LV_EVENT_SCREEN_LOADED) {
        sd_log_mode(sd_log::Mode::Analysis);
        sample_once();
        lv_timer_reset(g_state.sample_timer);
        lv_timer_resume(g_state.sample_timer);
    } else {
        sd_log_end_mode(sd_log::Mode::Analysis);
        lv_timer_pause(g_state.sample_timer);
    }
}

void profile_event_cb(lv_event_t* e) {
    lv_obj_t* matrix = static_cast<lv_obj_t*>(lv_event_get_target(e));
    uint32_t id = lv_buttonmatrix_get_selected_button(matrix);
    sensor_mock_profile_t profile = profile_from_button(id);
    sensor_set_mock_profile(profile);
    g_state.manual_helium = -1.0f;
    reset_capture_samples();
    sample_once();
}

void mode_event_cb(lv_event_t* e) {
    lv_obj_t* matrix = static_cast<lv_obj_t*>(lv_event_get_target(e));
    uint32_t id = lv_buttonmatrix_get_selected_button(matrix);
    g_state.gas_mode = mode_from_button(id);
    sample_once();
}

void adjust_helium_cb(lv_event_t* e) {
    intptr_t delta = reinterpret_cast<intptr_t>(lv_event_get_user_data(e));
    if (g_state.manual_helium < 0.0f) {
        g_state.manual_helium = g_state.last_readings.helium_percent > 0.0f ?
                                    g_state.last_readings.helium_percent :
                                    0.0f;
    }
    g_state.manual_helium += static_cast<float>(delta);
    if (g_state.manual_helium < 0.0f) g_state.manual_helium = 0.0f;
    if (g_state.manual_helium > 95.0f) g_state.manual_helium = 95.0f;
    sample_once();
}

void adjust_depth_cb(lv_event_t* e) {
    intptr_t delta = reinterpret_cast<intptr_t>(lv_event_get_user_data(e));
    g_state.planned_depth += static_cast<float>(delta);
    if (g_state.planned_depth < 0.0f) g_state.planned_depth = 0.0f;
    if (g_state.planned_depth > 150.0f) g_state.planned_depth = 150.0f;
    sample_once();
}

void capture_cb(lv_event_t*) {
    sensor_readings_t readings = {};
    uint8_t average_count = averaged_capture_readings(&readings);
    if (average_count < CAPTURE_MIN_STABLE_SAMPLES || g_state.last_readings.status != SENSOR_STATUS_STABLE) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Need %u stable samples before save",
                      static_cast<unsigned>(CAPTURE_MIN_STABLE_SAMPLES));
        lv_label_set_text(g_state.capture_status, buf);
        return;
    }

    analysis_input_t input = {};
    input.readings = readings;
    input.manual_he_percent = g_state.manual_helium;
    input.planned_depth_m = g_state.planned_depth;
    input.gas_mode = g_state.gas_mode;
    input.limits = limits_from_settings();
    analysis_result_t averaged_result = analysis_calculate(&input);
    if (!averaged_result.valid) {
        lv_label_set_text(g_state.capture_status, "Averaged sample unavailable");
        return;
    }

    analysis_history_record_t record =
        analysis_history_record_from_result(&readings, &averaged_result);
    if (analysis_history_add(&record) == ESP_OK) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Saved %u-sample average",
                      static_cast<unsigned>(average_count));
        lv_label_set_text(g_state.capture_status, buf);
        ESP_LOGI(TAG, "Saved averaged analysis: %s (%u samples)",
                 record.mix_label, static_cast<unsigned>(average_count));
    } else {
        lv_label_set_text(g_state.capture_status, "Save failed");
    }
}

void save_cylinder_cb(lv_event_t*) {
    sensor_readings_t readings = {};
    uint8_t average_count = averaged_capture_readings(&readings);
    if (average_count < CAPTURE_MIN_STABLE_SAMPLES || g_state.last_readings.status != SENSOR_STATUS_STABLE) {
        lv_label_set_text(g_state.capture_status, "Need stable average before cylinder save");
        return;
    }

    analysis_input_t input = {};
    input.readings = readings;
    input.manual_he_percent = g_state.manual_helium;
    input.planned_depth_m = g_state.planned_depth;
    input.gas_mode = g_state.gas_mode;
    input.limits = limits_from_settings();
    analysis_result_t averaged_result = analysis_calculate(&input);
    analysis_history_record_t record =
        analysis_history_record_from_result(&readings, &averaged_result);

    if (cylinder_profiles_update_selected_from_record(&record) == ESP_OK) {
        cylinder_profile_t profile = {};
        cylinder_profiles_get_selected(&profile);
        char label[384];
        mix_label_build_text(&record, &profile, label, sizeof(label));
        lv_label_set_text(g_state.capture_status, "Cylinder updated; label payload ready");
        ESP_LOGI(TAG, "Updated cylinder profile %s with %s", profile.name, record.mix_label);
    } else {
        lv_label_set_text(g_state.capture_status, "Cylinder update failed");
    }
    update_value_labels();
}

void add_value_adjuster(lv_obj_t* parent, const char* title, lv_obj_t** value_out,
                        lv_event_cb_t cb, int y) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_set_pos(label, 10, y);

    lv_obj_t* minus = create_small_button(parent, "-", 38, cb, reinterpret_cast<void*>(static_cast<intptr_t>(-5)));
    lv_obj_set_height(minus, 32);
    lv_obj_set_pos(minus, 160, y - 7);

    lv_obj_t* value = lv_label_create(parent);
    lv_label_set_text(value, "--");
    lv_obj_set_style_text_font(value, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(value, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_set_width(value, 72);
    lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(value, 202, y - 2);
    *value_out = value;

    lv_obj_t* plus = create_small_button(parent, "+", 38, cb, reinterpret_cast<void*>(static_cast<intptr_t>(5)));
    lv_obj_set_height(plus, 32);
    lv_obj_set_pos(plus, 282, y - 7);
}

}  // namespace

lv_obj_t* analyse_screen_create(void) {
    ESP_LOGI(TAG, "Creating analyse screen");
    g_state = AnalyseState{};

    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen, lv_color_hex(STYLE_COLOR_BG_DARK), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    g_state.screen = screen;

    navbar_create_with_back(screen, "Analyse Mix", nullptr);

    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, SCREEN_WIDTH, SCREEN_HEIGHT - NAVBAR_HEIGHT);
    lv_obj_set_pos(content, 0, NAVBAR_HEIGHT);
    lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);

    lv_obj_t* state_panel = create_panel(content, SCREEN_WIDTH - PAD * 2, 50);
    lv_obj_set_pos(state_panel, PAD, 10);
    g_state.status_label = lv_label_create(state_panel);
    lv_label_set_text(g_state.status_label, "Starting");
    lv_obj_set_style_text_font(g_state.status_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(g_state.status_label, lv_color_hex(STYLE_COLOR_WARNING), 0);
    lv_obj_align(g_state.status_label, LV_ALIGN_LEFT_MID, 0, 0);
    g_state.source_label = lv_label_create(state_panel);
    lv_label_set_text(g_state.source_label, "Simulated source");
    lv_obj_set_style_text_font(g_state.source_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_state.source_label, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_set_width(g_state.source_label, 300);
    lv_obj_set_style_text_align(g_state.source_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(g_state.source_label, LV_LABEL_LONG_DOT);
    lv_obj_align(g_state.source_label, LV_ALIGN_RIGHT_MID, 0, 0);

    create_metric(content, "Oxygen", PAD, 70, 108, 106, &g_state.o2_value);
    lv_obj_t *sensor_setup = create_metric(content, "O2 setup", 128, 70, 108, 106, &g_state.jj_value);
    lv_obj_add_flag(sensor_setup, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(sensor_setup, [](lv_event_t *) { screen_manager_show(SCREEN_CALIBRATE); }, LV_EVENT_CLICKED, nullptr);
    create_metric(content, "Helium", 244, 70, 108, 106, &g_state.he_value);
    create_metric(content, "CO ppm", 360, 70, 108, 106, &g_state.co_value);
    for (lv_obj_t *value : {g_state.o2_value, g_state.jj_value, g_state.he_value, g_state.co_value})
        lv_obj_set_style_text_font(value, &lv_font_montserrat_22, 0);

    lv_obj_t* chart_panel = create_panel(content, SCREEN_WIDTH - PAD * 2, 104);
    lv_obj_set_pos(chart_panel, PAD, 188);
    lv_obj_t* trend_title = lv_label_create(chart_panel);
    lv_label_set_text(trend_title, "Sample trend: selected oxygen, helium (%)");
    lv_obj_set_style_text_font(trend_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(trend_title, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_align(trend_title, LV_ALIGN_TOP_LEFT, 0, 0);
    g_state.chart = lv_chart_create(chart_panel);
    lv_obj_set_size(g_state.chart, SCREEN_WIDTH - 52, 60);
    lv_obj_align(g_state.chart, LV_ALIGN_BOTTOM_MID, 0, 2);
    lv_chart_set_type(g_state.chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(g_state.chart, 60);
    lv_chart_set_range(g_state.chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_obj_set_style_bg_opa(g_state.chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_state.chart, 0, 0);
    g_state.o2_series = lv_chart_add_series(g_state.chart, lv_palette_main(LV_PALETTE_CYAN), LV_CHART_AXIS_PRIMARY_Y);
    g_state.he_series = lv_chart_add_series(g_state.chart, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);

    lv_obj_t* profile_panel = create_panel(content, SCREEN_WIDTH - PAD * 2, 98);
    lv_obj_set_pos(profile_panel, PAD, 304);
    g_state.profile_matrix = lv_buttonmatrix_create(profile_panel);
    lv_buttonmatrix_set_map(g_state.profile_matrix, profile_map);
    lv_obj_set_size(g_state.profile_matrix, SCREEN_WIDTH - 48, 72);
    lv_obj_align(g_state.profile_matrix, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(g_state.profile_matrix, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_state.profile_matrix, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_row(g_state.profile_matrix, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_column(g_state.profile_matrix, 4, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_state.profile_matrix, 0, LV_PART_MAIN);
    lv_obj_set_style_text_font(g_state.profile_matrix, &lv_font_montserrat_14, LV_PART_ITEMS);
    lv_obj_set_style_text_color(g_state.profile_matrix, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(g_state.profile_matrix, lv_color_hex(STYLE_COLOR_BG_CARD), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(g_state.profile_matrix, lv_color_hex(STYLE_COLOR_PRIMARY), static_cast<lv_style_selector_t>(LV_PART_ITEMS) | static_cast<lv_style_selector_t>(LV_STATE_CHECKED));
    lv_obj_set_style_radius(g_state.profile_matrix, 6, LV_PART_ITEMS);
    for (uint32_t i = 0; i < 6; ++i) {
        lv_buttonmatrix_set_button_ctrl(g_state.profile_matrix, i, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    }
    lv_buttonmatrix_set_one_checked(g_state.profile_matrix, true);
    lv_buttonmatrix_set_button_ctrl(g_state.profile_matrix, 2, LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_obj_add_event_cb(g_state.profile_matrix, profile_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
#ifndef TRIMIX_SIMULATOR
    lv_obj_add_flag(profile_panel, LV_OBJ_FLAG_HIDDEN);
#endif

    lv_obj_t* mode_panel = create_panel(content, SCREEN_WIDTH - PAD * 2, 82);
    lv_obj_set_pos(mode_panel, PAD, 414);
    g_state.cylinder_label = lv_label_create(mode_panel);
    lv_label_set_text(g_state.cylinder_label, "--");
    lv_obj_set_style_text_font(g_state.cylinder_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_state.cylinder_label, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_set_width(g_state.cylinder_label, SCREEN_WIDTH - 48);
    lv_label_set_long_mode(g_state.cylinder_label, LV_LABEL_LONG_DOT);
    lv_obj_align(g_state.cylinder_label, LV_ALIGN_TOP_LEFT, 0, 0);

    g_state.mode_matrix = lv_buttonmatrix_create(mode_panel);
    lv_buttonmatrix_set_map(g_state.mode_matrix, mode_map);
    lv_obj_set_size(g_state.mode_matrix, SCREEN_WIDTH - 48, 44);
    lv_obj_align(g_state.mode_matrix, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(g_state.mode_matrix, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_state.mode_matrix, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_column(g_state.mode_matrix, 4, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_state.mode_matrix, 0, LV_PART_MAIN);
    lv_obj_set_style_text_font(g_state.mode_matrix, &lv_font_montserrat_14, LV_PART_ITEMS);
    lv_obj_set_style_text_color(g_state.mode_matrix, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(g_state.mode_matrix, lv_color_hex(STYLE_COLOR_BG_CARD), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(g_state.mode_matrix, lv_color_hex(STYLE_COLOR_PRIMARY), static_cast<lv_style_selector_t>(LV_PART_ITEMS) | static_cast<lv_style_selector_t>(LV_STATE_CHECKED));
    lv_obj_set_style_radius(g_state.mode_matrix, 6, LV_PART_ITEMS);
    for (uint32_t i = 0; i < 4; ++i) {
        lv_buttonmatrix_set_button_ctrl(g_state.mode_matrix, i, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    }
    lv_buttonmatrix_set_one_checked(g_state.mode_matrix, true);
    lv_buttonmatrix_set_button_ctrl(g_state.mode_matrix, 0, LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_obj_add_event_cb(g_state.mode_matrix, mode_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t* controls = create_panel(content, SCREEN_WIDTH - PAD * 2, 94);
    lv_obj_set_pos(controls, PAD, 508);
    add_value_adjuster(controls, "He override", &g_state.helium_value, adjust_helium_cb, 14);
    add_value_adjuster(controls, "Planned depth", &g_state.depth_value, adjust_depth_cb, 48);

    lv_obj_t* result_panel = create_panel(content, SCREEN_WIDTH - PAD * 2, 164);
    lv_obj_set_pos(result_panel, PAD, 608);
    g_state.mix_value = lv_label_create(result_panel);
    lv_label_set_text(g_state.mix_value, "--");
    lv_obj_set_style_text_font(g_state.mix_value, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(g_state.mix_value, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_align(g_state.mix_value, LV_ALIGN_TOP_LEFT, 0, 0);
    g_state.fractions_value = lv_label_create(result_panel);
    lv_label_set_text(g_state.fractions_value, "--");
    lv_obj_set_style_text_font(g_state.fractions_value, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_state.fractions_value, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_align(g_state.fractions_value, LV_ALIGN_TOP_LEFT, 0, 34);
    g_state.mod_value = lv_label_create(result_panel);
    lv_label_set_text(g_state.mod_value, "--");
    lv_obj_set_style_text_font(g_state.mod_value, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(g_state.mod_value, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_align(g_state.mod_value, LV_ALIGN_TOP_LEFT, 0, 64);
    g_state.ppo2_value = lv_label_create(result_panel);
    lv_label_set_text(g_state.ppo2_value, "--");
    lv_obj_set_style_text_font(g_state.ppo2_value, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_state.ppo2_value, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_align(g_state.ppo2_value, LV_ALIGN_TOP_LEFT, 0, 92);
    g_state.equivalent_depth_value = lv_label_create(result_panel);
    lv_label_set_text(g_state.equivalent_depth_value, "--");
    lv_obj_set_style_text_font(g_state.equivalent_depth_value, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_state.equivalent_depth_value, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_align(g_state.equivalent_depth_value, LV_ALIGN_TOP_LEFT, 0, 120);
    g_state.density_value = lv_label_create(result_panel);
    lv_label_set_text(g_state.density_value, "--");
    lv_obj_set_style_text_font(g_state.density_value, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(g_state.density_value, lv_color_hex(STYLE_COLOR_SUCCESS), 0);
    lv_obj_align(g_state.density_value, LV_ALIGN_TOP_RIGHT, 0, 0);

    g_state.env_value = lv_label_create(content);
    lv_label_set_text(g_state.env_value, "--");
    lv_obj_set_style_text_font(g_state.env_value, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_state.env_value, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_set_pos(g_state.env_value, PAD + 4, 784);

    g_state.advisory_label = lv_label_create(content);
    lv_label_set_text(g_state.advisory_label, "--");
    lv_obj_set_style_text_font(g_state.advisory_label, &lv_font_montserrat_14, 0);
    lv_obj_set_width(g_state.advisory_label, SCREEN_WIDTH - PAD * 2);
    lv_label_set_long_mode(g_state.advisory_label, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(g_state.advisory_label, PAD + 4, 810);

    g_state.capture_btn = lv_btn_create(content);
    lv_obj_set_size(g_state.capture_btn, 140, 46);
    lv_obj_set_pos(g_state.capture_btn, PAD, 844);
    lv_obj_set_style_bg_color(g_state.capture_btn, lv_color_hex(STYLE_COLOR_PRIMARY), 0);
    lv_obj_set_style_radius(g_state.capture_btn, 8, 0);
    lv_obj_set_style_shadow_width(g_state.capture_btn, 0, 0);
    lv_obj_add_event_cb(g_state.capture_btn, capture_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* capture_label = lv_label_create(g_state.capture_btn);
    lv_label_set_text(capture_label, "Save Avg");
    lv_obj_set_style_text_font(capture_label, &lv_font_montserrat_18, 0);
    lv_obj_center(capture_label);

    g_state.cylinder_btn = lv_btn_create(content);
    lv_obj_set_size(g_state.cylinder_btn, 140, 46);
    lv_obj_set_pos(g_state.cylinder_btn, 164, 844);
    lv_obj_set_style_bg_color(g_state.cylinder_btn, lv_color_hex(STYLE_COLOR_BG_CARD), 0);
    lv_obj_set_style_bg_color(g_state.cylinder_btn, lv_color_hex(STYLE_COLOR_PRIMARY), LV_STATE_PRESSED);
    lv_obj_set_style_radius(g_state.cylinder_btn, 8, 0);
    lv_obj_set_style_shadow_width(g_state.cylinder_btn, 0, 0);
    lv_obj_add_event_cb(g_state.cylinder_btn, save_cylinder_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* cylinder_btn_label = lv_label_create(g_state.cylinder_btn);
    lv_label_set_text(cylinder_btn_label, "Save Cyl");
    lv_obj_set_style_text_font(cylinder_btn_label, &lv_font_montserrat_18, 0);
    lv_obj_center(cylinder_btn_label);

    g_state.capture_status = lv_label_create(content);
    lv_label_set_text(g_state.capture_status, "Not saved");
    lv_obj_set_style_text_font(g_state.capture_status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_state.capture_status, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_set_width(g_state.capture_status, SCREEN_WIDTH - PAD * 2);
    lv_label_set_long_mode(g_state.capture_status, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(g_state.capture_status, PAD + 4, 900);

    sensor_set_mock_profile(SENSOR_MOCK_PROFILE_TRIMIX_18_45);
    sample_once();
    g_state.sample_timer = lv_timer_create(sample_timer_cb, 1000, nullptr);
    lv_timer_pause(g_state.sample_timer);
    lv_obj_add_event_cb(screen, screen_visibility_cb, LV_EVENT_SCREEN_LOADED, nullptr);
    lv_obj_add_event_cb(screen, screen_visibility_cb, LV_EVENT_SCREEN_UNLOADED, nullptr);
    return screen;
}
