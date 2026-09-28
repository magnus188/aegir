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
#include "../../components/status_icons.h"
#include "../../images/menu_icons.h"
#include "../../styles/styles.h"
#include <esp_log.h>
#include <cstdio>
#include <cmath>

static const char* TAG = "ANALYSE_SCREEN";

namespace {

constexpr int SCREEN_WIDTH = 480;
constexpr int HEADER_HEIGHT = 50;
constexpr int PAGE_TOP = 106;
constexpr int PAGE_HEIGHT = 590;
constexpr uint8_t CAPTURE_AVERAGE_WINDOW = 5;
constexpr uint8_t CAPTURE_MIN_STABLE_SAMPLES = 3;

struct SampleSnapshot {
    sensor_readings_t readings = {};
    analysis_result_t result = {};
};

struct AnalyseState {
    lv_obj_t* screen = nullptr;
    lv_obj_t* pages = nullptr;
    lv_obj_t* page_dots[2] = {};
    lv_obj_t* status_label = nullptr;
    lv_obj_t* source_label = nullptr;
    lv_obj_t* sd_label = nullptr;
    lv_obj_t* o2_value = nullptr;
    lv_obj_t* he_value = nullptr;
    lv_obj_t* o2_input_label = nullptr;
    lv_obj_t* he_input_label = nullptr;
    lv_obj_t* co_value = nullptr;
    lv_obj_t* humidity_value = nullptr;
    lv_obj_t* co_alert = nullptr;
    lv_obj_t* humidity_alert = nullptr;
    lv_obj_t* env_value = nullptr;
    lv_obj_t* live_mix_value = nullptr;
    lv_obj_t* live_fractions_value = nullptr;
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
    lv_obj_t* profile_dropdown = nullptr;
    lv_obj_t* mode_matrix = nullptr;
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
    int current_page = 0;
};

AnalyseState g_state;
void set_page(int index, bool animate);

static const char* mode_map[] = {
    "Back", "Deco", "CCR", "Bailout", ""
};

#ifdef TRIMIX_SIMULATOR
sensor_mock_profile_t profile_from_dropdown(uint32_t id) {
    switch (id) {
        case 0: return SENSOR_MOCK_PROFILE_AIR;
        case 1: return SENSOR_MOCK_PROFILE_EAN32;
        case 2: return SENSOR_MOCK_PROFILE_TRIMIX_18_45;
        case 3: return SENSOR_MOCK_PROFILE_HIGH_CO;
        case 4: return SENSOR_MOCK_PROFILE_UNSTABLE;
        case 5: return SENSOR_MOCK_PROFILE_SENSOR_FAULT;
        default: return SENSOR_MOCK_PROFILE_TRIMIX_18_45;
    }
}

uint32_t dropdown_from_profile(sensor_mock_profile_t profile) {
    switch (profile) {
        case SENSOR_MOCK_PROFILE_EAN32: return 1;
        case SENSOR_MOCK_PROFILE_TRIMIX_18_45: return 2;
        case SENSOR_MOCK_PROFILE_HIGH_CO: return 3;
        case SENSOR_MOCK_PROFILE_UNSTABLE: return 4;
        case SENSOR_MOCK_PROFILE_SENSOR_FAULT: return 5;
        default: return 0;
    }
}
#endif

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

uint32_t reading_color(bool valid, analysis_severity_t severity) {
    if (!valid) return STYLE_COLOR_TEXT_DIM;
    if (severity == ANALYSIS_SEVERITY_ALARM) return STYLE_COLOR_ERROR;
    if (severity == ANALYSIS_SEVERITY_ADVISORY) return STYLE_COLOR_WARNING;
    return STYLE_COLOR_DATA;
}

const char* reading_alert(analysis_severity_t severity) {
    if (severity == ANALYSIS_SEVERITY_ALARM) return "ALARM";
    if (severity == ANALYSIS_SEVERITY_ADVISORY) return "ADVISORY";
    return "";
}

analysis_limits_t limits_from_settings() {
    return {
        .ppo2_working_x100 = settings_get(SETTING_PPO2_WORKING_X100),
        .ppo2_secondary_x100 = settings_get(SETTING_PPO2_SECONDARY_X100),
        .density_advisory_x10 = settings_get(SETTING_DENSITY_ADVISORY_X10),
        .density_alarm_x10 = settings_get(SETTING_DENSITY_ALARM_X10),
        .co_advisory_ppm = settings_get(SETTING_CO_ADVISORY_PPM),
        .co_alarm_ppm = settings_get(SETTING_CO_ALARM_PPM),
        .humidity_advisory_pct = settings_get(SETTING_HUMIDITY_ADVISORY_PCT),
        .humidity_alarm_pct = settings_get(SETTING_HUMIDITY_ALARM_PCT),
    };
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

    const bool demo_oxygen = r.source == SENSOR_SOURCE_SIMULATED && !r.oxygen_calibrated &&
                             r.status != SENSOR_STATUS_FAULT &&
                             std::isfinite(r.simulated_oxygen_input_percent) &&
                             r.simulated_oxygen_input_percent >= 0 && r.simulated_oxygen_input_percent <= 100;
    const bool simulator_helium_available = r.source == SENSOR_SOURCE_SIMULATED &&
                                            r.status != SENSOR_STATUS_FAULT &&
                                            std::isfinite(r.helium_percent) &&
                                            r.helium_percent >= 0 && r.helium_percent <= 100;
    const bool demo_helium = simulator_helium_available && !r.helium_calibrated;
    const float displayed_oxygen = demo_oxygen ? r.simulated_oxygen_input_percent : r.oxygen_percent;
    if (std::isfinite(displayed_oxygen) && displayed_oxygen >= 0 && displayed_oxygen <= 100) {
        std::snprintf(buf, sizeof(buf), "%.1f%%", displayed_oxygen);
        lv_label_set_text(g_state.o2_value, buf);
    } else lv_label_set_text(g_state.o2_value, std::isfinite(r.oxygen_percent) && r.oxygen_calibrated ? "Range" : "--");
    if (demo_oxygen) lv_obj_remove_flag(g_state.o2_input_label, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(g_state.o2_input_label, LV_OBJ_FLAG_HIDDEN);
    if (a.valid || simulator_helium_available) {
        std::snprintf(buf, sizeof(buf), "%.1f%%", r.helium_percent);
        lv_label_set_text(g_state.he_value, buf);
    } else lv_label_set_text(g_state.he_value, r.source == SENSOR_SOURCE_HARDWARE &&
                            r.calibration_unvalidated && std::isfinite(r.helium_percent) ? "Bench" : "--");
    if (demo_helium) lv_obj_remove_flag(g_state.he_input_label, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(g_state.he_input_label, LV_OBJ_FLAG_HIDDEN);
    if (a.co_valid) {
        std::snprintf(buf, sizeof(buf), "%.1f ppm", r.co_ppm);
        lv_label_set_text(g_state.co_value, buf);
    } else lv_label_set_text(g_state.co_value, "--");
    lv_obj_set_style_text_color(g_state.co_value,
        lv_color_hex(reading_color(a.co_valid, a.co_severity)), 0);
    lv_label_set_text(g_state.co_alert, reading_alert(a.co_severity));
    lv_obj_set_style_text_color(g_state.co_alert,
        lv_color_hex(reading_color(a.co_valid, a.co_severity)), 0);
    if (a.humidity_valid) {
        std::snprintf(buf, sizeof(buf), "%.0f%% RH", r.humidity_pct);
        lv_label_set_text(g_state.humidity_value, buf);
    } else lv_label_set_text(g_state.humidity_value, "--");
    lv_obj_set_style_text_color(g_state.humidity_value,
        lv_color_hex(reading_color(a.humidity_valid, a.humidity_severity)), 0);
    lv_label_set_text(g_state.humidity_alert, reading_alert(a.humidity_severity));
    lv_obj_set_style_text_color(g_state.humidity_alert,
        lv_color_hex(reading_color(a.humidity_valid, a.humidity_severity)), 0);
    if (r.environment_valid && std::isfinite(r.temperature_c)) {
        std::snprintf(buf, sizeof(buf), "Temperature %.1f C", r.temperature_c);
        lv_label_set_text(g_state.env_value, buf);
    } else lv_label_set_text(g_state.env_value, "Temperature unavailable");

    const char *display_status = sensor_status_label(r.status);
    if (r.status != SENSOR_STATUS_FAULT) {
        if (r.oxygen_configuration_required) display_status = "Set up O2";
        else if (r.oxygen_calibration_required) display_status = "Calibrate";
    }
    if (r.source == SENSOR_SOURCE_HARDWARE && r.status != SENSOR_STATUS_FAULT && r.status == SENSOR_STATUS_STABLE) {
        if (!r.oxygen_calibrated || !r.helium_calibrated) display_status = "Calibrate";
        else if (r.calibration_unvalidated) display_status = "Bench";
    }
    lv_label_set_text(g_state.status_label, display_status);
    lv_obj_set_style_text_color(g_state.status_label, lv_color_hex(severity_color(a.severity)), 0);

    const uint8_t stable_count = stable_capture_sample_count();
    std::snprintf(buf, sizeof(buf), "%s",
                  r.source == SENSOR_SOURCE_SIMULATED ?
                      (g_state.profile_dropdown ? "SIMULATED" : "SIMULATED - practice data") :
                  (r.calibration_unvalidated ? "HARDWARE - bench only" : "HARDWARE - live reading"));
    lv_label_set_text(g_state.source_label, buf);
    lv_obj_set_style_text_color(g_state.source_label,
        lv_color_hex(r.source == SENSOR_SOURCE_SIMULATED ? STYLE_COLOR_WARNING : STYLE_COLOR_TEXT_LIGHT), 0);
    lv_label_set_text(g_state.sd_label, sd_log::state_label(sd_log_status().state));

    lv_label_set_text(g_state.mix_value, a.mix_label);
    lv_label_set_text(g_state.live_mix_value, a.mix_label);
    std::snprintf(buf, sizeof(buf), "O2 %.1f%%  He %.0f%%  N2 %.1f%%",
                  a.oxygen_percent, a.helium_percent, a.nitrogen_percent);
    lv_label_set_text(g_state.fractions_value, buf);
    lv_label_set_text(g_state.live_fractions_value, buf);
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
    const char* advisory_text = a.advisory;
    uint32_t advisory_color = severity_color(a.severity);
    if (!a.valid && a.co_severity >= ANALYSIS_SEVERITY_ADVISORY) {
        advisory_text = a.co_severity == ANALYSIS_SEVERITY_ALARM ?
            "CO above configured alarm" : "CO above configured advisory";
        advisory_color = reading_color(a.co_valid, a.co_severity);
    } else if (!a.valid && a.humidity_severity >= ANALYSIS_SEVERITY_ADVISORY) {
        advisory_text = a.humidity_severity == ANALYSIS_SEVERITY_ALARM ?
            "Chamber RH above configured alarm" : "Chamber RH above configured advisory";
        advisory_color = reading_color(a.humidity_valid, a.humidity_severity);
    }
    lv_label_set_text(g_state.advisory_label, advisory_text);
    if (!a.valid) {
        lv_label_set_text(g_state.fractions_value, "Gas composition unavailable");
        lv_label_set_text(g_state.live_fractions_value, "Gas composition unavailable");
        lv_label_set_text(g_state.mod_value, "MOD --");
        lv_label_set_text(g_state.equivalent_depth_value, "EAD --  END --");
        lv_label_set_text(g_state.density_value, "-- g/L");
        lv_label_set_text(g_state.ppo2_value, "PPO2 --");
    }
    lv_obj_set_style_text_color(g_state.advisory_label, lv_color_hex(advisory_color), 0);

    if (g_state.manual_helium >= 0.0f) {
        std::snprintf(buf, sizeof(buf), "%.0f%%", g_state.manual_helium);
    } else {
        std::snprintf(buf, sizeof(buf), "Auto");
    }
    lv_label_set_text(g_state.helium_value, buf);
    std::snprintf(buf, sizeof(buf), "%.0f m", g_state.planned_depth);
    lv_label_set_text(g_state.depth_value, buf);

#ifdef TRIMIX_SIMULATOR
    if (g_state.profile_dropdown) {
        const uint32_t selected = dropdown_from_profile(sensor_get_mock_profile());
        if (lv_dropdown_get_selected(g_state.profile_dropdown) != selected)
            lv_dropdown_set_selected(g_state.profile_dropdown, selected);
    }
#endif
    if (g_state.cylinder_label) {
        cylinder_profile_t profile = {};
        if (cylinder_profiles_get_selected(&profile)) {
            std::snprintf(buf, sizeof(buf), "%s - %s",
                          profile.name, profile.needs_recheck ? "needs check" : "ready");
            lv_label_set_text(g_state.cylinder_label, buf);
        } else {
            lv_label_set_text(g_state.cylinder_label, "No cylinder selected");
        }
    }

    if (g_state.capture_btn) {
        const bool capture_ready = a.valid &&
                                   r.status == SENSOR_STATUS_STABLE &&
                                   stable_count >= CAPTURE_MIN_STABLE_SAMPLES;
        if (capture_ready) {
            lv_obj_clear_state(g_state.capture_btn, LV_STATE_DISABLED);
            lv_obj_clear_state(g_state.cylinder_btn, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(g_state.capture_btn, LV_STATE_DISABLED);
            lv_obj_add_state(g_state.cylinder_btn, LV_STATE_DISABLED);
        }
    }

    if (g_state.capture_status) {
        if (r.oxygen_configuration_required) {
            lv_label_set_text(g_state.capture_status, "");
        } else if (r.oxygen_calibration_required) {
            lv_label_set_text(g_state.capture_status, "Calibrate oxygen to enable saving");
        } else if (!a.valid) {
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
        const float oxygen_input = readings.source == SENSOR_SOURCE_SIMULATED &&
                                   !readings.oxygen_calibrated ? readings.simulated_oxygen_input_percent : readings.oxygen_percent;
        const bool input_available = readings.source == SENSOR_SOURCE_SIMULATED ?
                                     readings.status != SENSOR_STATUS_FAULT : g_state.last_result.valid;
        int o2 = input_available && std::isfinite(oxygen_input) && oxygen_input >= 0 && oxygen_input <= 100 ?
                 static_cast<int>(oxygen_input + 0.5f) : LV_CHART_POINT_NONE;
        int he = input_available && std::isfinite(readings.helium_percent) &&
                 readings.helium_percent >= 0 && readings.helium_percent <= 100 ?
                 static_cast<int>(readings.helium_percent + 0.5f) : LV_CHART_POINT_NONE;
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
        set_page(0, false);
        sample_once();
        lv_timer_reset(g_state.sample_timer);
        lv_timer_resume(g_state.sample_timer);
    } else {
        sd_log_end_mode(sd_log::Mode::Analysis);
        lv_timer_pause(g_state.sample_timer);
    }
}

#ifdef TRIMIX_SIMULATOR
void profile_event_cb(lv_event_t* e) {
    lv_obj_t* dropdown = static_cast<lv_obj_t*>(lv_event_get_target(e));
    sensor_mock_profile_t profile = profile_from_dropdown(lv_dropdown_get_selected(dropdown));
    sensor_set_mock_profile(profile);
    g_state.manual_helium = -1.0f;
    reset_capture_samples();
    sample_once();
}
#endif

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
        update_value_labels();
        lv_label_set_text(g_state.capture_status, "Cylinder updated; label payload ready");
        ESP_LOGI(TAG, "Updated cylinder profile %s with %s", profile.name, record.mix_label);
    } else {
        lv_label_set_text(g_state.capture_status, "Cylinder update failed");
    }
}

lv_obj_t* add_label(lv_obj_t* parent, const char* text, int x, int y,
                    const lv_font_t* font, uint32_t color, int width = 0) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    if (width > 0) {
        lv_obj_set_width(label, width);
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    }
    lv_obj_set_pos(label, x, y);
    return label;
}

void add_rule(lv_obj_t* parent, int x, int y, int w, int h = 1, uint32_t color = STYLE_COLOR_BORDER) {
    lv_obj_t* rule = lv_obj_create(parent);
    lv_obj_remove_style_all(rule);
    lv_obj_set_pos(rule, x, y);
    lv_obj_set_size(rule, w, h);
    lv_obj_set_style_bg_color(rule, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);
    lv_obj_clear_flag(rule, LV_OBJ_FLAG_CLICKABLE);
}

void style_matrix(lv_obj_t* matrix, int x, int y, int w, int h, int count, int checked) {
    lv_obj_set_pos(matrix, x, y);
    lv_obj_set_size(matrix, w, h);
    lv_obj_set_style_bg_opa(matrix, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(matrix, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_row(matrix, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_column(matrix, 4, LV_PART_MAIN);
    lv_obj_set_style_border_width(matrix, 0, LV_PART_MAIN);
    lv_obj_set_style_text_font(matrix, &lv_font_montserrat_16, LV_PART_ITEMS);
    lv_obj_set_style_text_color(matrix, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(matrix, lv_color_hex(STYLE_COLOR_BG_CARD), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(matrix, lv_color_hex(STYLE_COLOR_PRIMARY),
        static_cast<lv_style_selector_t>(LV_PART_ITEMS) | static_cast<lv_style_selector_t>(LV_STATE_CHECKED));
    lv_obj_set_style_border_color(matrix, lv_color_hex(STYLE_COLOR_BORDER), LV_PART_ITEMS);
    lv_obj_set_style_border_width(matrix, 1, LV_PART_ITEMS);
    lv_obj_set_style_radius(matrix, 6, LV_PART_ITEMS);
    for (int i = 0; i < count; ++i) {
        lv_buttonmatrix_set_button_ctrl(matrix, i, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    }
    lv_buttonmatrix_set_one_checked(matrix, true);
    lv_buttonmatrix_set_button_ctrl(matrix, checked, LV_BUTTONMATRIX_CTRL_CHECKED);
}

void add_compact_adjuster(lv_obj_t* parent, const char* title, int x, int y,
                          lv_obj_t** value_out, lv_event_cb_t callback) {
    add_label(parent, title, x, y, &lv_font_montserrat_14, STYLE_COLOR_TEXT_DIM);
    lv_obj_t* minus = create_small_button(parent, "-", 36, callback,
        reinterpret_cast<void*>(static_cast<intptr_t>(-5)));
    lv_obj_set_pos(minus, x, y + 28);
    *value_out = add_label(parent, "--", x + 42, y + 33,
                           &lv_font_montserrat_20, STYLE_COLOR_TEXT_LIGHT, 88);
    lv_obj_set_style_text_align(*value_out, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_t* plus = create_small_button(parent, "+", 36, callback,
        reinterpret_cast<void*>(static_cast<intptr_t>(5)));
    lv_obj_set_pos(plus, x + 137, y + 28);
}

void set_page(int index, bool animate) {
    if (!g_state.pages) return;
    g_state.current_page = index;
    lv_obj_scroll_to_x(g_state.pages, index * SCREEN_WIDTH, animate ? LV_ANIM_ON : LV_ANIM_OFF);
    for (int i = 0; i < 2; ++i) {
        if (g_state.page_dots[i]) {
            lv_obj_set_style_bg_color(g_state.page_dots[i],
                lv_color_hex(i == index ? STYLE_COLOR_DATA : STYLE_COLOR_BORDER), 0);
        }
    }
}

void page_scroll_cb(lv_event_t*) {
    const int index = lv_obj_get_scroll_x(g_state.pages) >= SCREEN_WIDTH / 2 ? 1 : 0;
    set_page(index, false);
}

void page_dot_cb(lv_event_t* event) {
    const int index = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
    set_page(index, true);
}

void create_header(lv_obj_t* screen) {
    lv_obj_t* back = lv_obj_create(screen);
    lv_obj_remove_style_all(back);
    lv_obj_set_pos(back, 8, 3);
    lv_obj_set_size(back, 93, 44);
    lv_obj_set_style_bg_color(back, lv_color_hex(STYLE_COLOR_BG_CARD), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(back, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_radius(back, 7, 0);
    lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(back, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(back, [](lv_event_t*) { screen_manager_show(SCREEN_HOME); }, LV_EVENT_CLICKED, nullptr);
    add_label(back, LV_SYMBOL_LEFT, 8, 8, &lv_font_montserrat_24, STYLE_COLOR_TEXT_LIGHT);
    add_label(back, "Back", 34, 11, &lv_font_montserrat_16, STYLE_COLOR_TEXT_LIGHT);

    lv_obj_t* title = add_label(screen, "Analyse Mix", 148, 10,
                                &lv_font_montserrat_24, STYLE_COLOR_TEXT_LIGHT, 185);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_t* status = status_icons_create(screen);
    lv_obj_align(status, LV_ALIGN_TOP_RIGHT, -10, 13);

    lv_obj_t* banner = lv_obj_create(screen);
    lv_obj_set_pos(banner, 14, HEADER_HEIGHT + 1);
    lv_obj_set_size(banner, SCREEN_WIDTH - 28, 50);
    lv_obj_set_style_bg_color(banner, lv_color_hex(0x08231F), 0);
    lv_obj_set_style_bg_opa(banner, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(banner, 1, 0);
    lv_obj_set_style_border_color(banner, lv_color_hex(0x1E604B), 0);
    lv_obj_set_style_radius(banner, 6, 0);
    lv_obj_set_style_pad_all(banner, 0, 0);
    lv_obj_clear_flag(banner, LV_OBJ_FLAG_SCROLLABLE);
#ifdef TRIMIX_SIMULATOR
    constexpr int source_label_width = 112;
#else
    constexpr int source_label_width = 310;
#endif
    g_state.source_label = add_label(banner, "Starting", 10, 5,
                                     &lv_font_montserrat_16, STYLE_COLOR_WARNING, source_label_width);
#ifdef TRIMIX_SIMULATOR
    g_state.profile_dropdown = lv_dropdown_create(banner);
    lv_obj_set_pos(g_state.profile_dropdown, 126, 2);
    lv_obj_set_size(g_state.profile_dropdown, 196, 25);
    lv_dropdown_set_options(g_state.profile_dropdown, "Air\nEAN32\nTrimix 18/45\nHigh CO\nUnstable\nFault");
    lv_dropdown_set_selected(g_state.profile_dropdown, dropdown_from_profile(sensor_get_mock_profile()));
    lv_obj_set_style_bg_color(g_state.profile_dropdown, lv_color_hex(0x124036), 0);
    lv_obj_set_style_bg_opa(g_state.profile_dropdown, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(g_state.profile_dropdown, lv_color_hex(0x1E604B), 0);
    lv_obj_set_style_border_width(g_state.profile_dropdown, 1, 0);
    lv_obj_set_style_radius(g_state.profile_dropdown, 4, 0);
    lv_obj_set_style_shadow_width(g_state.profile_dropdown, 0, 0);
    lv_obj_set_style_text_font(g_state.profile_dropdown, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_state.profile_dropdown, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_set_style_pad_left(g_state.profile_dropdown, 8, 0);
    lv_obj_set_style_pad_top(g_state.profile_dropdown, 3, 0);
    lv_obj_t* profile_list = lv_dropdown_get_list(g_state.profile_dropdown);
    lv_obj_set_style_bg_color(profile_list, lv_color_hex(STYLE_COLOR_BG_CARD), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(profile_list, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(profile_list, lv_color_hex(STYLE_COLOR_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(profile_list, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(profile_list, 6, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(profile_list, 0, LV_PART_MAIN);
    lv_obj_set_style_text_font(profile_list, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(profile_list, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), LV_PART_MAIN);
    lv_obj_set_style_text_line_space(profile_list, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(profile_list, lv_color_hex(STYLE_COLOR_PRIMARY), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(profile_list, LV_OPA_COVER, LV_PART_SELECTED);
    lv_obj_set_style_text_color(profile_list, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), LV_PART_SELECTED);
    lv_obj_add_event_cb(g_state.profile_dropdown, profile_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
#endif
    g_state.status_label = add_label(banner, "Starting", 332, 5,
                                     &lv_font_montserrat_16, STYLE_COLOR_WARNING, 108);
    lv_obj_set_style_text_align(g_state.status_label, LV_TEXT_ALIGN_RIGHT, 0);
    g_state.advisory_label = add_label(banner, "Waiting for measurements", 10, 29,
                                       &lv_font_montserrat_12, STYLE_COLOR_TEXT_DIM, 320);
    g_state.sd_label = add_label(banner, "SD --", 337, 29,
                                 &lv_font_montserrat_12, STYLE_COLOR_TEXT_DIM, 103);
    lv_obj_set_style_text_align(g_state.sd_label, LV_TEXT_ALIGN_RIGHT, 0);
}

void create_live_page(lv_obj_t* page) {
    add_label(page, "OXYGEN", 22, 6, &lv_font_montserrat_16, STYLE_COLOR_TEXT_DIM);
    add_label(page, "HELIUM", 261, 6, &lv_font_montserrat_16, STYLE_COLOR_TEXT_DIM);
    g_state.o2_value = add_label(page, "--", 20, 32, &lv_font_montserrat_48, STYLE_COLOR_DATA, 215);
    g_state.he_value = add_label(page, "--", 260, 32, &lv_font_montserrat_48, STYLE_COLOR_DATA, 215);
    g_state.o2_input_label = add_label(page, "DEMO INPUT", 22, 87, &lv_font_montserrat_12, STYLE_COLOR_WARNING);
    g_state.he_input_label = add_label(page, "DEMO INPUT", 261, 87, &lv_font_montserrat_12, STYLE_COLOR_WARNING);
    lv_obj_add_flag(g_state.o2_input_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_state.he_input_label, LV_OBJ_FLAG_HIDDEN);
    add_rule(page, 240, 7, 1, 94);
    add_rule(page, 18, 109, 444);

    add_label(page, "CHAMBER HUMIDITY", 22, 121, &lv_font_montserrat_14, STYLE_COLOR_TEXT_DIM);
    add_label(page, "CO", 261, 121, &lv_font_montserrat_14, STYLE_COLOR_TEXT_DIM);
    g_state.humidity_value = add_label(page, "--", 22, 145, &lv_font_montserrat_30, STYLE_COLOR_DATA, 216);
    g_state.co_value = add_label(page, "--", 261, 145, &lv_font_montserrat_30, STYLE_COLOR_DATA, 214);
    g_state.humidity_alert = add_label(page, "", 22, 181, &lv_font_montserrat_12, STYLE_COLOR_WARNING, 216);
    g_state.co_alert = add_label(page, "", 261, 181, &lv_font_montserrat_12, STYLE_COLOR_WARNING, 214);
    add_rule(page, 240, 120, 1, 77);
    add_rule(page, 18, 204, 444);

    add_label(page, "SAMPLE TREND  O2, He (%)", 19, 214,
              &lv_font_montserrat_14, STYLE_COLOR_TEXT_DIM);
    add_rule(page, 315, 220, 9, 9, STYLE_COLOR_DATA);
    add_label(page, "O2", 329, 214, &lv_font_montserrat_14, STYLE_COLOR_TEXT_DIM);
    add_rule(page, 368, 220, 9, 9, STYLE_COLOR_SUCCESS);
    add_label(page, "He", 383, 214, &lv_font_montserrat_14, STYLE_COLOR_TEXT_DIM);

    g_state.chart = lv_chart_create(page);
    lv_obj_set_pos(g_state.chart, 48, 247);
    lv_obj_set_size(g_state.chart, 414, 217);
    lv_obj_set_style_bg_opa(g_state.chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_state.chart, 0, 0);
    lv_obj_set_style_pad_all(g_state.chart, 0, 0);
    lv_obj_set_style_line_color(g_state.chart, lv_color_hex(0x3B5568), LV_PART_MAIN);
    lv_obj_set_style_line_opa(g_state.chart, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_line_width(g_state.chart, 1, LV_PART_MAIN);
    lv_obj_set_style_line_width(g_state.chart, 2, LV_PART_ITEMS);
    lv_chart_set_type(g_state.chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(g_state.chart, 60);
    lv_chart_set_range(g_state.chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_div_line_count(g_state.chart, 5, 5);
    g_state.o2_series = lv_chart_add_series(g_state.chart, lv_color_hex(STYLE_COLOR_DATA), LV_CHART_AXIS_PRIMARY_Y);
    g_state.he_series = lv_chart_add_series(g_state.chart, lv_color_hex(STYLE_COLOR_SUCCESS), LV_CHART_AXIS_PRIMARY_Y);
    for (int i = 0; i < 5; ++i) {
        char value[5];
        std::snprintf(value, sizeof(value), "%d", 100 - i * 25);
        add_label(page, value, 8, 240 + i * 54, &lv_font_montserrat_12, STYLE_COLOR_TEXT_DIM, 34);
    }
    add_label(page, "-60s", 45, 470, &lv_font_montserrat_12, STYLE_COLOR_TEXT_DIM);
    add_label(page, "-45s", 145, 470, &lv_font_montserrat_12, STYLE_COLOR_TEXT_DIM);
    add_label(page, "-30s", 247, 470, &lv_font_montserrat_12, STYLE_COLOR_TEXT_DIM);
    add_label(page, "-15s", 349, 470, &lv_font_montserrat_12, STYLE_COLOR_TEXT_DIM);
    add_label(page, "Now", 435, 470, &lv_font_montserrat_12, STYLE_COLOR_TEXT_DIM);

    add_rule(page, 18, 499, 444);
    g_state.live_mix_value = add_label(page, "--", 22, 516,
                                       &lv_font_montserrat_30, STYLE_COLOR_TEXT_LIGHT, 430);
    g_state.live_fractions_value = add_label(page, "--", 22, 555,
                                             &lv_font_montserrat_16, STYLE_COLOR_TEXT_DIM, 430);
}

void create_plan_page(lv_obj_t* page) {
    constexpr int plan_offset = 5;
    add_rule(page, 18, plan_offset, 444);
    add_label(page, "SELECTED CYLINDER", 22, plan_offset + 8,
              &lv_font_montserrat_14, STYLE_COLOR_TEXT_DIM);
    g_state.cylinder_label = add_label(page, "--", 22, plan_offset + 29,
                                       &lv_font_montserrat_24, STYLE_COLOR_DATA, 438);
    add_rule(page, 18, plan_offset + 65, 444);

    add_label(page, "GAS USE MODE", 22, plan_offset + 73,
              &lv_font_montserrat_14, STYLE_COLOR_TEXT_DIM);
    g_state.mode_matrix = lv_buttonmatrix_create(page);
    lv_buttonmatrix_set_map(g_state.mode_matrix, mode_map);
    style_matrix(g_state.mode_matrix, 18, plan_offset + 98, 444, 45, 4, 0);
    lv_obj_add_event_cb(g_state.mode_matrix, mode_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    add_rule(page, 18, plan_offset + 151, 444);

    add_compact_adjuster(page, "HE OVERRIDE", 22, plan_offset + 162,
                         &g_state.helium_value, adjust_helium_cb);
    add_rule(page, 239, plan_offset + 162, 1, 67);
    add_compact_adjuster(page, "PLANNED DEPTH", 261, plan_offset + 162,
                         &g_state.depth_value, adjust_depth_cb);
    add_rule(page, 18, plan_offset + 240, 444);

    g_state.mix_value = add_label(page, "--", 22, plan_offset + 249,
                                  &lv_font_montserrat_30, STYLE_COLOR_TEXT_LIGHT, 275);
    g_state.fractions_value = add_label(page, "--", 22, plan_offset + 284,
                                        &lv_font_montserrat_14, STYLE_COLOR_TEXT_DIM, 305);
    add_rule(page, 339, plan_offset + 252, 1, 65);
    g_state.density_value = add_label(page, "--", 350, plan_offset + 249,
                                      &lv_font_montserrat_28, STYLE_COLOR_DATA, 113);
    add_label(page, "Gas density", 350, plan_offset + 283,
              &lv_font_montserrat_12, STYLE_COLOR_TEXT_DIM);
    add_rule(page, 18, plan_offset + 326, 444);

    g_state.mod_value = add_label(page, "--", 22, plan_offset + 336,
                                  &lv_font_montserrat_18, STYLE_COLOR_TEXT_LIGHT, 205);
    g_state.ppo2_value = add_label(page, "--", 22, plan_offset + 366,
                                   &lv_font_montserrat_14, STYLE_COLOR_TEXT_DIM, 300);
    g_state.equivalent_depth_value = add_label(page, "--", 265, plan_offset + 336,
                                                &lv_font_montserrat_16, STYLE_COLOR_TEXT_LIGHT, 195);
    add_rule(page, 18, plan_offset + 394, 444);
    g_state.env_value = add_label(page, "--", 22, plan_offset + 401,
                                  &lv_font_montserrat_12, STYLE_COLOR_TEXT_DIM, 438);
}

void create_footer(lv_obj_t* screen) {
    for (int i = 0; i < 2; ++i) {
        lv_obj_t* hit = lv_obj_create(screen);
        lv_obj_remove_style_all(hit);
        lv_obj_set_pos(hit, 212 + 28 * i, 674);
        lv_obj_set_size(hit, 28, 22);
        lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(hit, page_dot_cb, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<intptr_t>(i)));
        lv_obj_t* dot = lv_obj_create(hit);
        lv_obj_remove_style_all(dot);
        lv_obj_set_pos(dot, 9, 6);
        lv_obj_set_size(dot, 10, 10);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
        g_state.page_dots[i] = dot;
    }
    set_page(0, false);

    g_state.capture_btn = lv_btn_create(screen);
    lv_obj_set_pos(g_state.capture_btn, 16, 712);
    lv_obj_set_size(g_state.capture_btn, 216, 52);
    lv_obj_set_style_bg_color(g_state.capture_btn, lv_color_hex(STYLE_COLOR_BG_CARD), 0);
    lv_obj_set_style_border_width(g_state.capture_btn, 1, 0);
    lv_obj_set_style_border_color(g_state.capture_btn, lv_color_hex(STYLE_COLOR_BORDER), 0);
    lv_obj_set_style_radius(g_state.capture_btn, 8, 0);
    lv_obj_set_style_shadow_width(g_state.capture_btn, 0, 0);
    lv_obj_set_style_pad_all(g_state.capture_btn, 0, 0);
    lv_obj_add_event_cb(g_state.capture_btn, capture_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* capture_label = add_label(g_state.capture_btn, LV_SYMBOL_SAVE "  Save Avg", 0, 0,
                                        &lv_font_montserrat_20, STYLE_COLOR_TEXT_LIGHT);
    lv_obj_center(capture_label);

    g_state.cylinder_btn = lv_btn_create(screen);
    lv_obj_set_pos(g_state.cylinder_btn, 248, 712);
    lv_obj_set_size(g_state.cylinder_btn, 216, 52);
    lv_obj_set_style_bg_color(g_state.cylinder_btn, lv_color_hex(STYLE_COLOR_PRIMARY), 0);
    lv_obj_set_style_radius(g_state.cylinder_btn, 8, 0);
    lv_obj_set_style_shadow_width(g_state.cylinder_btn, 0, 0);
    lv_obj_set_style_pad_all(g_state.cylinder_btn, 0, 0);
    lv_obj_add_event_cb(g_state.cylinder_btn, save_cylinder_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* cylinder_label = add_label(g_state.cylinder_btn, "Save Cyl", 0, 0,
                                         &lv_font_montserrat_20, STYLE_COLOR_TEXT_LIGHT);
    lv_obj_align(cylinder_label, LV_ALIGN_CENTER, 16, 0);
    lv_obj_t* cylinder_icon = lv_image_create(g_state.cylinder_btn);
    lv_image_set_src(cylinder_icon, &button_icon_cylinder);
    lv_obj_set_style_image_recolor(cylinder_icon, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_set_style_image_recolor_opa(cylinder_icon, LV_OPA_COVER, 0);
    lv_obj_clear_flag(cylinder_icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align_to(cylinder_icon, cylinder_label, LV_ALIGN_OUT_LEFT_MID, -8, 0);

    g_state.capture_status = add_label(screen, "Waiting for measurements", 18, 768,
                                        &lv_font_montserrat_12, STYLE_COLOR_TEXT_DIM, 445);
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
    create_header(screen);

    g_state.pages = lv_obj_create(screen);
    lv_obj_remove_style_all(g_state.pages);
    lv_obj_set_pos(g_state.pages, 0, PAGE_TOP);
    lv_obj_set_size(g_state.pages, SCREEN_WIDTH, PAGE_HEIGHT);
    lv_obj_set_flex_flow(g_state.pages, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(g_state.pages, 0, 0);
    lv_obj_set_style_pad_column(g_state.pages, 0, 0);
    lv_obj_add_flag(g_state.pages, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(g_state.pages, LV_DIR_HOR);
    lv_obj_set_scroll_snap_x(g_state.pages, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scrollbar_mode(g_state.pages, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(g_state.pages, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(g_state.pages, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_add_event_cb(g_state.pages, page_scroll_cb, LV_EVENT_SCROLL_END, nullptr);

    lv_obj_t* live = lv_obj_create(g_state.pages);
    lv_obj_remove_style_all(live);
    lv_obj_set_size(live, SCREEN_WIDTH, PAGE_HEIGHT);
    lv_obj_clear_flag(live, LV_OBJ_FLAG_SCROLLABLE);
    create_live_page(live);

    lv_obj_t* planning = lv_obj_create(g_state.pages);
    lv_obj_remove_style_all(planning);
    lv_obj_set_size(planning, SCREEN_WIDTH, PAGE_HEIGHT);
    lv_obj_clear_flag(planning, LV_OBJ_FLAG_SCROLLABLE);
    create_plan_page(planning);

    create_footer(screen);
    sensor_set_mock_profile(SENSOR_MOCK_PROFILE_TRIMIX_18_45);
    sample_once();
    g_state.sample_timer = lv_timer_create(sample_timer_cb, 1000, nullptr);
    lv_timer_pause(g_state.sample_timer);
    lv_obj_add_event_cb(screen, screen_visibility_cb, LV_EVENT_SCREEN_LOADED, nullptr);
    lv_obj_add_event_cb(screen, screen_visibility_cb, LV_EVENT_SCREEN_UNLOADED, nullptr);
    return screen;
}
