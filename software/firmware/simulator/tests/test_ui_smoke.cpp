#include <lvgl.h>

#include "services/battery_service.h"
#include "services/backlight_service.h"
#include "services/ota_service.h"
#include "services/settings_service.h"
#include "services/wifi_service.h"
#include "ui/screens/screen_manager.h"
#include "services/gas_calibration_service.h"
#include "services/analysis_history.h"
#include "services/cylinder_profiles.h"
#include "sensors/sensor_interface.h"
#include "services/storage_service.h"
#include "services/sd_log_service.h"
#include "ui/images/menu_icons.h"
#include "ui/styles/styles.h"
extern "C" void battery_mock_set_available(bool available);

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

uint8_t g_draw_buffer[480 * 40 * 2];
uint16_t g_pixels[480 * 800]{};
lv_point_t g_touch_point{};
bool g_touch_down = false;

void pointer_read_cb(lv_indev_t*, lv_indev_data_t* data) {
    data->point = g_touch_point;
    data->state = g_touch_down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void flush_cb(lv_display_t* display, const lv_area_t* area, uint8_t* pixels) {
    const auto *source = reinterpret_cast<const uint16_t *>(pixels);
    for (int y = area->y1; y <= area->y2; ++y)
        for (int x = area->x1; x <= area->x2; ++x)
            g_pixels[y * 480 + x] = *source++;
    lv_display_flush_ready(display);
}

void pump_lvgl(int frames = 3) {
    for (int i = 0; i < frames; ++i) {
        lv_tick_inc(16);
        lv_timer_handler();
    }
}

void tap(int x, int y) {
    g_touch_point = {x, y};
    g_touch_down = true;
    pump_lvgl(4);
    g_touch_down = false;
    pump_lvgl(5);
}

void swipe(int start_x, int end_x, int y) {
    g_touch_point = {start_x, y};
    g_touch_down = true;
    pump_lvgl(3);
    for (int step = 1; step <= 12; ++step) {
        g_touch_point = {start_x + (end_x - start_x) * step / 12, y};
        pump_lvgl(2);
    }
    g_touch_down = false;
    pump_lvgl(24);
}

bool show_and_check(screen_id_t screen) {
    screen_manager_show(screen);
    pump_lvgl();
    if (screen_manager_current() != screen) {
        std::fprintf(stderr, "Expected current screen %d, got %d\n", screen, screen_manager_current());
        return false;
    }
    if (screen == SCREEN_ANALYSE) {
        pump_lvgl(8);
    }
    const auto expected = screen == SCREEN_ANALYSE ? sd_log::Mode::Analysis :
        screen == SCREEN_CALIBRATE ? sd_log::Mode::Calibration : sd_log::Mode::Idle;
    if (sd_log_status().activity != expected) {
        std::fprintf(stderr, "Wrong SD activity after loading screen %d\n", screen);
        return false;
    }
    return true;
}

lv_obj_t *find_label(lv_obj_t *parent, const char *text) {
    if (lv_obj_check_type(parent, &lv_label_class) && std::strstr(lv_label_get_text(parent), text)) return parent;
    for (unsigned i = 0; i < lv_obj_get_child_count(parent); ++i)
        if (auto *found = find_label(lv_obj_get_child(parent, i), text)) return found;
    return nullptr;
}

bool header_matches_palette(const char* title) {
    auto* label = find_label(lv_screen_active(), title);
    auto* header = label ? lv_obj_get_parent(label) : nullptr;
    const bool matches = header && lv_color_eq(
        lv_obj_get_style_bg_color(header, LV_PART_MAIN), lv_color_hex(STYLE_COLOR_BG_DARK));
    std::printf("%s %s header uses the instrument palette\n", matches ? "PASS" : "FAIL", title);
    return matches;
}

int count_visible_labels(lv_obj_t *parent, const char *text) {
    int count = lv_obj_check_type(parent, &lv_label_class) &&
                !lv_obj_has_flag(parent, LV_OBJ_FLAG_HIDDEN) &&
                std::strcmp(lv_label_get_text(parent), text) == 0 ? 1 : 0;
    for (unsigned i = 0; i < lv_obj_get_child_count(parent); ++i)
        count += count_visible_labels(lv_obj_get_child(parent, i), text);
    return count;
}

lv_obj_t *find_class(lv_obj_t *parent, const lv_obj_class_t *type) {
    if (lv_obj_check_type(parent, type)) return parent;
    for (unsigned i = 0; i < lv_obj_get_child_count(parent); ++i)
        if (auto *found = find_class(lv_obj_get_child(parent, i), type)) return found;
    return nullptr;
}
lv_obj_t *find_checkbox(lv_obj_t *parent, const char *text) {
    if (lv_obj_check_type(parent, &lv_checkbox_class) && std::strstr(lv_checkbox_get_text(parent), text)) return parent;
    for (unsigned i = 0; i < lv_obj_get_child_count(parent); ++i)
        if (auto *found = find_checkbox(lv_obj_get_child(parent, i), text)) return found;
    return nullptr;
}

lv_obj_t *find_matrix(lv_obj_t *parent, const char *first) {
    if (lv_obj_check_type(parent, &lv_buttonmatrix_class) &&
        std::strcmp(lv_buttonmatrix_get_button_text(parent, 0), first) == 0) return parent;
    for (unsigned i = 0; i < lv_obj_get_child_count(parent); ++i)
        if (auto *found = find_matrix(lv_obj_get_child(parent, i), first)) return found;
    return nullptr;
}

void select_matrix(const char *first, unsigned id) {
    auto *obj = find_matrix(lv_screen_active(), first);
    if (!obj) return;
    lv_buttonmatrix_set_selected_button(obj, id);
    lv_buttonmatrix_set_button_ctrl(obj, id, LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_obj_send_event(obj, LV_EVENT_VALUE_CHANGED, nullptr);
}

void select_analyse_profile(unsigned id) {
    auto *dropdown = find_class(lv_screen_active(), &lv_dropdown_class);
    if (!dropdown) return;
    lv_dropdown_set_selected(dropdown, id);
    lv_obj_send_event(dropdown, LV_EVENT_VALUE_CHANGED, nullptr);
    pump_lvgl();
}

lv_obj_t *reference_field(const char *field) {
    auto *title = find_label(lv_screen_active(), field);
    if (!title) return nullptr;
    auto *obj = lv_obj_get_child(lv_obj_get_parent(title), lv_obj_get_index(title) + 1);
    return obj && lv_obj_check_type(obj, &lv_textarea_class) ? obj : nullptr;
}

void set_reference(const char *field, const char *value) {
    if (auto *obj = reference_field(field)) lv_textarea_set_text(obj, value);
}

lv_obj_t *action(const char *text) {
    auto *obj = find_label(lv_screen_active(), text);
    return obj ? lv_obj_get_parent(obj) : nullptr;
}

void click(const char *text) {
    if (auto *obj = action(text)) lv_obj_send_event(obj, LV_EVENT_CLICKED, nullptr);
    pump_lvgl();
}

void snapshot(const char *name, const char *visible);

bool logging_navigation_checks() {
    bool ok = true;
    auto expect = [&](bool value, const char *name) {
        std::printf("%s %s\n", value ? "PASS" : "FAIL", name); ok &= value;
    };
    auto activity = [&](screen_id_t screen, sd_log::Mode mode) {
        return screen_manager_current() == screen && sd_log_status().activity == mode;
    };
    screen_manager_show(SCREEN_HOME); pump_lvgl();
    expect(sd_log_status().state == sd_log::State::Unavailable,
           "SD navigation checks use the real no-card simulator backend");
    screen_manager_show(SCREEN_ANALYSE); pump_lvgl();
    expect(activity(SCREEN_ANALYSE, sd_log::Mode::Analysis), "Analyse starts its SD activity");
    expect(!find_label(lv_screen_active(), "O2 setup") && find_label(lv_screen_active(), "HUMIDITY"),
           "Analysis replaces the setup tile with humidity");
    screen_manager_show(SCREEN_SETTINGS); pump_lvgl();
    click("Calibrate Sensors");
    // LVGL sends the destination's LOADED before the old screen's UNLOADED.
    // Exercise the Settings calibration click and both event handlers.
    expect(activity(SCREEN_CALIBRATE, sd_log::Mode::Calibration),
           "Settings calibration action starts Calibration activity");
    pump_lvgl(50);
    expect(activity(SCREEN_CALIBRATE, sd_log::Mode::Calibration),
           "Calibration activity persists through timer updates without media");
    expect(action(LV_SYMBOL_LEFT) != nullptr, "Calibration back action exists");
    click(LV_SYMBOL_LEFT);
    expect(activity(SCREEN_SETTINGS, sd_log::Mode::Idle), "Calibration back stops SD activity");
    expect(action("Calibrate Sensors") != nullptr, "Settings calibration action exists");
    click("Calibrate Sensors");
    expect(activity(SCREEN_CALIBRATE, sd_log::Mode::Calibration),
           "Settings can start a new Calibration activity");
    screen_manager_show(SCREEN_ANALYSE); pump_lvgl();
    expect(activity(SCREEN_ANALYSE, sd_log::Mode::Analysis),
           "Calibration unload cannot stop the newly loaded Analyse activity");
    expect(action(LV_SYMBOL_LEFT) != nullptr, "Analyse back action exists");
    click(LV_SYMBOL_LEFT);
    expect(activity(SCREEN_HOME, sd_log::Mode::Idle), "Analyse back stops SD activity");
    return ok;
}

bool menu_and_paging_checks() {
    bool ok = true;
    auto expect = [&](bool value, const char *name) {
        std::printf("%s %s\n", value ? "PASS" : "FAIL", name); ok &= value;
    };
    screen_manager_show(SCREEN_HOME); pump_lvgl();
    snapshot("home-redesign", nullptr);
    expect(!find_label(lv_screen_active(), "Trimix Analysator"), "Home omits the old title");
    const struct { const char *label; screen_id_t destination; } menu[] = {
        {"Analyse", SCREEN_ANALYSE}, {"Dive", SCREEN_DIVE_PLANNER},
        {"History", SCREEN_HISTORY}, {"Cylinders", SCREEN_CYLINDERS},
        {"Settings", SCREEN_SETTINGS},
    };
    for (const auto &item : menu) {
        if (item.destination == SCREEN_ANALYSE) tap(75, 143);
        else click(item.label);
        expect(screen_manager_current() == item.destination, item.label);
        screen_manager_show(SCREEN_HOME); pump_lvgl();
    }
    screen_manager_show(SCREEN_ANALYSE); pump_lvgl();
    auto *trend = find_label(lv_screen_active(), "SAMPLE TREND");
    auto *pages = trend ? lv_obj_get_parent(lv_obj_get_parent(trend)) : nullptr;
    expect(pages && lv_obj_get_scroll_dir(pages) == LV_DIR_HOR,
           "Analysis pages support horizontal navigation");
    expect(find_label(lv_screen_active(), "HUMIDITY") && !find_label(lv_screen_active(), "O2 setup") &&
           !find_label(lv_screen_active(), "Advisory"), "Live page shows humidity without removed cards");
    auto *oxygen_heading = find_label(lv_screen_active(), "OXYGEN");
    auto *live_page = oxygen_heading ? lv_obj_get_parent(oxygen_heading) : nullptr;
    auto *oxygen_value = live_page ? lv_obj_get_child(live_page, 2) : nullptr;
    auto *helium_value = live_page ? lv_obj_get_child(live_page, 3) : nullptr;
    expect(oxygen_value && helium_value &&
           std::strchr(lv_label_get_text(oxygen_value), '%') &&
           std::strchr(lv_label_get_text(helium_value), '%') &&
           count_visible_labels(lv_screen_active(), "DEMO INPUT") == 2,
           "Both simulated sensor inputs are visible before calibration");
    expect(!find_label(lv_screen_active(), "Set up oxygen to enable saving") &&
           lv_obj_has_state(action("Save Avg"), LV_STATE_DISABLED),
           "Duplicate setup footer is absent while analysis saving stays gated");
    auto *chart = live_page ? find_class(live_page, &lv_chart_class) : nullptr;
    auto *oxygen_series = chart ? lv_chart_get_series_next(chart, nullptr) : nullptr;
    auto *helium_series = oxygen_series ? lv_chart_get_series_next(chart, oxygen_series) : nullptr;
    bool oxygen_plotted = false, helium_plotted = false;
    if (chart && oxygen_series && helium_series) {
        const auto *oxygen_points = lv_chart_get_series_y_array(chart, oxygen_series);
        const auto *helium_points = lv_chart_get_series_y_array(chart, helium_series);
        for (uint32_t i = 0; i < lv_chart_get_point_count(chart); ++i) {
            oxygen_plotted |= oxygen_points[i] != LV_CHART_POINT_NONE;
            helium_plotted |= helium_points[i] != LV_CHART_POINT_NONE;
        }
    }
    expect(oxygen_plotted && helium_plotted, "Both demo inputs reach the trend chart");
    expect(!find_label(lv_screen_active(), "Swipe for profile") &&
           !find_label(lv_screen_active(), "Swipe for live"),
           "Analysis navigation has no swipe instruction text");
    snapshot("analysis-demo-input", nullptr);
    auto *demo_profiles = find_class(lv_screen_active(), &lv_dropdown_class);
    lv_area_t demo_area{};
    if (demo_profiles) lv_obj_get_coords(demo_profiles, &demo_area);
    expect(demo_profiles && demo_area.y1 < 106 &&
           std::strstr(lv_dropdown_get_options(demo_profiles), "EAN32") &&
           std::strstr(lv_dropdown_get_options(demo_profiles), "Trimix 18/45") &&
           std::strstr(lv_dropdown_get_options(demo_profiles), "High CO") &&
           std::strstr(lv_dropdown_get_options(demo_profiles), "Unstable"),
           "Requested demo profiles are in the top status banner");
    if (demo_profiles) {
        tap(225, 65);
        expect(lv_dropdown_is_open(demo_profiles), "Top banner profile selector opens with touch");
        snapshot("analysis-profile-menu", nullptr);
        tap(200, 136);
        expect(!lv_dropdown_is_open(demo_profiles) &&
               sensor_get_mock_profile() == SENSOR_MOCK_PROFILE_EAN32,
               "Top banner selects EAN32 demo gas with touch");
        select_analyse_profile(3);
        auto *co_display = find_label(lv_screen_active(), "ppm");
        expect(sensor_get_mock_profile() == SENSOR_MOCK_PROFILE_HIGH_CO && co_display &&
               std::atof(lv_label_get_text(co_display)) > 14.0f &&
               lv_color_eq(lv_obj_get_style_text_color(co_display, LV_PART_MAIN),
                           lv_color_hex(STYLE_COLOR_ERROR)) &&
               find_label(lv_screen_active(), "ALARM") &&
               find_label(lv_screen_active(), "CO above configured alarm"),
               "High CO is red in the reading and top alert");
        snapshot("analysis-high-co", nullptr);
        settings_set(SETTING_CO_ALARM_PPM, 25);
        settings_set(SETTING_CO_ADVISORY_PPM, 10);
        select_analyse_profile(3);
        co_display = find_label(lv_screen_active(), "ppm");
        expect(co_display && lv_color_eq(lv_obj_get_style_text_color(co_display, LV_PART_MAIN),
               lv_color_hex(STYLE_COLOR_WARNING)),
               "Changing Safety Settings moves the same CO reading to amber");
        settings_reset(SETTING_CO_ADVISORY_PPM);
        settings_reset(SETTING_CO_ALARM_PPM);

        settings_set(SETTING_HUMIDITY_ADVISORY_PCT, 40);
        settings_set(SETTING_HUMIDITY_ALARM_PCT, 55);
        select_analyse_profile(0);
        auto *humidity_display = find_label(lv_screen_active(), "% RH");
        expect(humidity_display && lv_color_eq(lv_obj_get_style_text_color(humidity_display, LV_PART_MAIN),
               lv_color_hex(STYLE_COLOR_WARNING)),
               "Chamber humidity turns amber at its configured advisory");
        settings_set(SETTING_HUMIDITY_ALARM_PCT, 45);
        select_analyse_profile(0);
        humidity_display = find_label(lv_screen_active(), "% RH");
        expect(humidity_display && lv_color_eq(lv_obj_get_style_text_color(humidity_display, LV_PART_MAIN),
               lv_color_hex(STYLE_COLOR_ERROR)),
               "Chamber humidity turns red at its configured alarm");
        snapshot("analysis-high-humidity", nullptr);
        settings_reset_category(SETTINGS_CAT_SAFETY);
        select_analyse_profile(4);
        expect(sensor_get_mock_profile() == SENSOR_MOCK_PROFILE_UNSTABLE,
               "Top banner selects unstable demo gas");
        select_analyse_profile(2);
        expect(sensor_get_mock_profile() == SENSOR_MOCK_PROFILE_TRIMIX_18_45,
               "Top banner restores Trimix 18/45 demo gas");
    }
    auto *average_button = action("Save Avg");
    auto *average_label = average_button ? find_label(average_button, "Save Avg") : nullptr;
    lv_area_t average_area{}, average_label_area{};
    if (average_button && average_label) {
        lv_obj_get_coords(average_button, &average_area);
        lv_obj_get_coords(average_label, &average_label_area);
    }
    expect(average_button && average_label && std::strstr(lv_label_get_text(average_label), LV_SYMBOL_SAVE) &&
           std::abs((average_area.x1 + average_area.x2) - (average_label_area.x1 + average_label_area.x2)) <= 2 &&
           std::abs((average_area.y1 + average_area.y2) - (average_label_area.y1 + average_label_area.y2)) <= 2,
           "Save Avg disk icon and caption are centered");
    auto *cylinder_button = action("Save Cyl");
    auto *cylinder_icon = cylinder_button ? find_class(cylinder_button, &lv_image_class) : nullptr;
    auto *cylinder_label = cylinder_button ? find_label(cylinder_button, "Save Cyl") : nullptr;
    lv_area_t cylinder_area{}, cylinder_icon_area{}, cylinder_label_area{};
    if (cylinder_button && cylinder_icon && cylinder_label) {
        lv_obj_get_coords(cylinder_button, &cylinder_area);
        lv_obj_get_coords(cylinder_icon, &cylinder_icon_area);
        lv_obj_get_coords(cylinder_label, &cylinder_label_area);
    }
    expect(cylinder_button && cylinder_icon && cylinder_label &&
           lv_image_get_src(cylinder_icon) == &button_icon_cylinder &&
           std::abs((cylinder_area.x1 + cylinder_area.x2) - (cylinder_icon_area.x1 + cylinder_label_area.x2)) <= 2 &&
           std::abs((cylinder_area.y1 + cylinder_area.y2) - (cylinder_icon_area.y1 + cylinder_icon_area.y2)) <= 2,
           "Save Cyl uses the centered cylinder artwork");
    if (pages) {
        swipe(420, 55, 440);
        expect(lv_obj_get_scroll_x(pages) == 480, "Swipe reaches analysis details page");
        snapshot("analysis-planning", nullptr);
        expect(!find_label(lv_screen_active(), "DEMO GAS PROFILE") &&
               !find_matrix(lv_screen_active(), "Air") &&
               find_label(lv_screen_active(), "SELECTED CYLINDER") &&
               find_label(lv_screen_active(), "PLANNED DEPTH") && action("Save Avg"),
               "Analysis details retain device controls without a demo section");
        expect(find_label(lv_screen_active(), "Temperature") &&
               !find_label(lv_screen_active(), "Pressure"),
               "Analysis details show temperature without an ambient pressure reading");
        tap(180, 230);
        expect(lv_buttonmatrix_get_selected_button(find_matrix(lv_screen_active(), "Back")) == 1,
               "Gas-use mode remains selectable");
        tap(178, 315);
        expect(!find_label(lv_screen_active(), "Auto"), "Helium override control responds to touch");
        tap(416, 315);
        expect(find_label(lv_screen_active(), "35 m"), "Planned depth control responds to touch");
        tap(282, 315);
        tap(68, 230);
        select_analyse_profile(5);
        expect(sensor_get_mock_profile() == SENSOR_MOCK_PROFILE_SENSOR_FAULT &&
               find_label(lv_screen_active(), "Fault") &&
               lv_obj_has_state(action("Save Avg"), LV_STATE_DISABLED) &&
               lv_obj_has_state(action("Save Cyl"), LV_STATE_DISABLED),
               "Faulted readings disable both save actions");
        swipe(55, 420, 440);
        snapshot("analysis-fault", nullptr);
        swipe(420, 55, 440);
        select_analyse_profile(4);
        expect(sensor_get_mock_profile() == SENSOR_MOCK_PROFILE_UNSTABLE &&
               lv_obj_has_state(action("Save Avg"), LV_STATE_DISABLED),
               "Unstable readings cannot be saved");
        select_analyse_profile(2);
        swipe(55, 420, 440);
        expect(lv_obj_get_scroll_x(pages) == 0, "Swipe returns to live graph page");
    }
    return ok;
}

void settle(unsigned profile) {
    auto *obj = find_class(lv_screen_active(), &lv_dropdown_class);
    if (obj) {
        lv_dropdown_set_selected(obj, profile);
        lv_obj_send_event(obj, LV_EVENT_VALUE_CHANGED, nullptr);
    }
    for (int i = 0; i < 40; ++i) { lv_tick_inc(500); lv_timer_handler(); }
}

void snapshot(const char *name, const char *visible = nullptr) {
    const char *directory = std::getenv("TRIMIX_UI_CAPTURE_DIR");
    if (!directory) return;
    if (visible) {
        if (auto *obj = find_label(lv_screen_active(), visible)) {
            if (std::strcmp(visible, "Review and save") == 0) obj = lv_obj_get_parent(obj);
            lv_obj_scroll_to_view_recursive(obj, LV_ANIM_OFF);
        }
    } else if (auto *obj = find_label(lv_screen_active(), "SIMULATION -")) lv_obj_scroll_to_view_recursive(obj, LV_ANIM_OFF);
    pump_lvgl(20);
    lv_refr_now(nullptr);
    char path[1024]; std::snprintf(path, sizeof(path), "%s/%s.ppm", directory, name);
    FILE *file = std::fopen(path, "wb");
    if (!file) return;
    std::fprintf(file, "P6\n480 800\n255\n");
    for (uint16_t pixel : g_pixels) {
        const unsigned char rgb[] = {static_cast<unsigned char>(((pixel >> 11) & 31) * 255 / 31),
            static_cast<unsigned char>(((pixel >> 5) & 63) * 255 / 63), static_cast<unsigned char>((pixel & 31) * 255 / 31)};
        std::fwrite(rgb, 1, sizeof(rgb), file);
    }
    std::fclose(file);
}

bool calibration_checks() {
    bool ok = true;
    auto expect = [&](bool value, const char *name) {
        std::printf("%s %s\n", value ? "PASS" : "FAIL", name); ok &= value;
    };
    sensor_set_mock_profile(SENSOR_MOCK_PROFILE_AIR);
    screen_manager_show(SCREEN_CALIBRATE); pump_lvgl();
    expect(find_label(lv_screen_active(), "SIMULATION - practice data"), "Calibration explicitly identifies simulated data");
    expect(find_matrix(lv_screen_active(), "AO2"), "All three independently selected channels exist");
    expect(!find_label(lv_screen_active(), "CO2 400"), "Legacy CO2 commands are absent from the calibration wizard");
    oxygen_selection_status_t setup{}; oxygen_selection_get_status(&setup);
    expect(!setup.configured && find_label(lv_screen_active(), "confirmation required"), "First setup has no default physical oxygen sensor");
    select_matrix("Not set", 1);
    oxygen_selection_get_status(&setup);
    expect(!setup.configured, "Touching AO2 stages a choice without confirming it");
    click("Confirm installed sensor");
    oxygen_selection_get_status(&setup);
    expect(setup.choice == OXYGEN_AO2 && setup.calibration_required, "Explicit confirmation selects AO2 and requires calibration");
    snapshot("oxygen-setup");
    auto *known_air = find_checkbox(lv_screen_active(), "Known fresh air");
    lv_obj_add_state(known_air, LV_STATE_CHECKED);
    lv_obj_send_event(known_air, LV_EVENT_VALUE_CHANGED, nullptr);
    expect(oxygen_selection_probe_active(), "Known-air check explicitly requests both raw input measurements");
    settle(0);
    oxygen_selection_get_status(&setup);
    expect(setup.choice == OXYGEN_AO2 && find_label(lv_screen_active(), "Input check is inconclusive"),
           "Unknown cell-response bounds leave selection manual and unchanged");
    snapshot("oxygen-air-check", "Input check is inconclusive");
    settle(4);
    expect(find_label(lv_screen_active(), "AO2 input: unavailable | JJ input: unavailable"),
           "Faulted raw voltages are unavailable in the known-air panel");
    snapshot("oxygen-air-fault", "Input check unavailable");
    settle(0);
    lv_obj_clear_state(known_air, LV_STATE_CHECKED);
    lv_obj_send_event(known_air, LV_EVENT_VALUE_CHANGED, nullptr);
    auto *reference_id = reference_field("Reference ID /");
    lv_obj_send_event(reference_id, LV_EVENT_FOCUSED, nullptr); pump_lvgl();
    lv_area_t input_area{}; lv_obj_get_coords(reference_id, &input_area);
    expect(input_area.y1 >= 70 && input_area.y2 < 540, "Focused reference field remains above the touchscreen keyboard");
    auto *keyboard = find_class(lv_screen_active(), &lv_keyboard_class);
    lv_obj_send_event(keyboard, LV_EVENT_READY, nullptr);
    settle(0);
    auto *capture = action("Capture reference point");
    expect(capture && !lv_obj_has_state(capture, LV_STATE_DISABLED), "Stable sample enables capture");
    snapshot("calibration-overview");
    click("Capture reference point");
    expect(find_label(lv_screen_active(), "Enter a reference ID"), "Capture requires a reference identity");
    set_reference("Reference ID /", "TEST-AIR");
    click("Capture reference point");
    gas_cal_point_t point{};
    expect(gas_calibration_get_point(GAS_CAL_AO2, 0, &point), "UI captures AO2 baseline");
    expect(!point.reference.uncertainty_known, "Missing uncertainty remains unknown");
    select_matrix("1. Baseline", 1); settle(1);
    set_reference("Reference ID /", "TEST-EAN32");
    click("Capture reference point");
    expect(action("Save channel calibration") && !lv_obj_has_state(action("Save channel calibration"), LV_STATE_DISABLED),
           "Two captured references enable the save button");
    lv_obj_scroll_to_view_recursive(lv_obj_get_parent(find_label(lv_screen_active(), "Review and save")), LV_ANIM_OFF);
    pump_lvgl();
    lv_area_t navbar_title_area{};
    lv_obj_get_coords(find_label(lv_screen_active(), "Calibration (demo)"), &navbar_title_area);
    expect(navbar_title_area.y1 >= 0 && navbar_title_area.y2 < 70,
           "Simulation identity and back navigation remain visible while reviewing references");
    snapshot("calibration-review", "Review and save");
    click("Save channel calibration");
    gas_cal_record_t saved{};
    expect(gas_calibration_get_record(GAS_CAL_AO2, &saved) && saved.valid && saved.simulated &&
           !saved.characterization_validated, "UI saves a source-separated bench record");
    const uint32_t crc = saved.crc32;
    settle(4);
    expect(lv_obj_has_state(capture, LV_STATE_DISABLED), "Faulted sample disables capture");
    click("Capture reference point");
    expect(gas_calibration_get_record(GAS_CAL_AO2, &saved) && saved.crc32 == crc,
           "Failed capture preserves the saved record");
    snapshot("calibration-fault", "Spread");
    select_matrix("AO2", 1);
    expect(!gas_calibration_get_record(GAS_CAL_JJCCR, &saved), "JJ-CCR does not inherit AO2 calibration");
    expect(lv_obj_has_state(capture, LV_STATE_DISABLED), "Unselected oxygen input cannot capture references");
    select_matrix("Not set", 2); click("Confirm installed sensor");
    oxygen_selection_get_status(&setup);
    expect(setup.choice == OXYGEN_JJCCR && setup.calibration_required, "Changing installed type requires independent JJ-CCR calibration");
    settle(0); set_reference("Reference ID /", "JJ-AIR"); click("Capture reference point");
    click("Discard draft points");
    expect(!gas_calibration_get_point(GAS_CAL_JJCCR, 0, &point), "Discard removes draft only");
    click("Capture reference point");
    select_matrix("1. Baseline", 1); settle(1);
    set_reference("Reference ID /", "JJ-EAN32"); click("Capture reference point"); click("Save channel calibration");
    oxygen_selection_get_status(&setup);
    expect(!setup.calibration_required && gas_calibration_get_record(GAS_CAL_JJCCR, &saved), "JJ-CCR gets its own confirmed calibration");
    select_matrix("AO2", 2); settle(0);
    set_reference("Reference ID /", "HE-ZERO-AIR"); click("Capture reference point");
    select_matrix("1. Baseline", 1); settle(2);
    set_reference("Reference ID /", "HE-18-45"); click("Capture reference point"); click("Save channel calibration");
    expect(gas_calibration_get_record(GAS_CAL_HELIUM, &saved) && !saved.characterization_validated,
           "Helium UI fit cannot assert characterized performance");
    snapshot("calibration-helium-saved", "Review and save");
    screen_manager_show(SCREEN_ANALYSE);
    for (int i = 0; i < 5; ++i) { lv_tick_inc(1000); lv_timer_handler(); }
    snapshot("analysis-live");
    if (auto *trend = find_label(lv_screen_active(), "SAMPLE TREND")) {
        auto *pages = lv_obj_get_parent(lv_obj_get_parent(trend));
        lv_obj_scroll_to_x(pages, 480, LV_ANIM_OFF);
        pump_lvgl();
        snapshot("analysis-planning-stable");
    }
    click("Save Avg");
    expect(analysis_history_count() > 0, "Calibrated simulated measurements retain normal history capture");
    click("Save Cyl");
    cylinder_profile_t saved_cylinder{};
    expect(cylinder_profiles_get_selected(&saved_cylinder) && saved_cylinder.configured &&
           !saved_cylinder.needs_recheck && saved_cylinder.oxygen_percent > 17.0f &&
           saved_cylinder.helium_percent > 44.0f,
           "Calibrated simulated measurements still update the selected cylinder");
    screen_manager_show(SCREEN_HISTORY); pump_lvgl();
    expect(find_label(lv_screen_active(), "CO 0.3 ppm") &&
           find_label(lv_screen_active(), "CO2 not measured"),
           "Current history records CO and marks CO2 unmeasured");
    expect(find_label(lv_screen_active(), "Simulation"), "History identifies simulated records");
    expect(find_label(lv_screen_active(), "O2 sensor: JJ-CCR"), "History preserves the selected oxygen identity and calibration revision");
    snapshot("history-gas-provenance");
    screen_manager_show(SCREEN_CALIBRATE); pump_lvgl();
    auto *replacement = find_class(lv_screen_active(), &lv_checkbox_class);
    lv_obj_add_state(replacement, LV_STATE_CHECKED);
    gas_cal_record_t old_jj{}; gas_calibration_get_record(GAS_CAL_JJCCR, &old_jj);
    click("Confirm installed sensor");
    oxygen_selection_get_status(&setup);
    gas_cal_record_t retained_jj{}; gas_calibration_get_record(GAS_CAL_JJCCR, &retained_jj);
    expect(setup.calibration_required && old_jj.crc32 == retained_jj.crc32, "Replacement requires new calibration and preserves the previous saved record");
    known_air = find_checkbox(lv_screen_active(), "Known fresh air");
    lv_obj_add_state(known_air, LV_STATE_CHECKED);
    lv_obj_send_event(known_air, LV_EVENT_VALUE_CHANGED, nullptr);
    screen_manager_show(SCREEN_ANALYSE); pump_lvgl(50);
    expect(!oxygen_selection_probe_active() && !lv_obj_has_state(known_air, LV_STATE_CHECKED),
           "Leaving setup stops probing and requires a new known-air confirmation next time");
    expect(find_label(lv_screen_active(), "Calibrate") && lv_obj_has_state(action("Save Avg"), LV_STATE_DISABLED),
           "Analysis cannot save a result using an old replacement-cell calibration");
    screen_manager_show(SCREEN_DEVICE); pump_lvgl();
    expect(find_label(lv_screen_active(), "Storage:") && find_label(lv_screen_active(), "Battery: simulated"),
           "Device status exposes storage state and simulated battery provenance");
    snapshot("device-status");
    storage_pause_writes(true); battery_mock_set_available(false);
    lv_tick_inc(1100); lv_timer_handler();
    expect(find_label(lv_screen_active(), "Storage: read only"), "Read-only storage is visible on the device screen");
    expect(find_label(lv_screen_active(), "Battery: unavailable") && !find_label(lv_screen_active(), "Battery: simulated"),
           "Unavailable fuel gauge does not display a fabricated charge percentage");
    snapshot("device-unavailable");
    storage_pause_writes(false); battery_mock_set_available(true);
    return ok;
}

bool secondary_screen_checks() {
    bool ok = true;
    auto expect = [&](bool value, const char *name) {
        std::printf("%s %s\n", value ? "PASS" : "FAIL", name); ok &= value;
    };

    screen_manager_show(SCREEN_SETTINGS); pump_lvgl();
    expect(!find_label(lv_screen_active(), "Cylinder Profiles") &&
           !find_label(lv_screen_active(), "MEASUREMENT") &&
           !find_label(lv_screen_active(), "DEVICE & SYSTEM"),
           "Settings omits the cylinder shortcut and section headings");
    bool has_numbered_items = false;
    for (unsigned i = 1; i <= 7; ++i) {
        char number[3];
        std::snprintf(number, sizeof(number), "%02u", i);
        has_numbered_items |= count_visible_labels(lv_screen_active(), number) != 0;
    }
    expect(!has_numbered_items, "Settings items have no numbers");
    click("Factory Reset");
    expect(screen_manager_current() == SCREEN_SETTINGS,
           "Unavailable factory reset remains inactive");
    screen_manager_show(SCREEN_HOME); pump_lvgl();
    tap(115, 420);
    expect(screen_manager_current() == SCREEN_CYLINDERS,
           "Home menu Cylinders tile opens with touch");
    expect(find_label(lv_screen_active(), "Share payload: trimix-label-v1"),
           "Cylinder preview includes the complete export label");

    const uint8_t first = cylinder_profiles_selected_index();
    tap(95, 238);
    expect(cylinder_profiles_selected_index() == (first + 1) % cylinder_profiles_count(),
           "Next selects the following cylinder");
    cylinder_profile_t profile{};
    cylinder_profiles_get_selected(&profile);
    const bool recheck_before = profile.needs_recheck;
    tap(225, 238);
    cylinder_profiles_get_selected(&profile);
    expect(profile.needs_recheck != recheck_before, "Recheck toggles the selected profile");
    tap(375, 238);
    expect(cylinder_profiles_selected_index() == 0, "Defaults restores the first profile");
    tap(130, 650);
    expect(cylinder_profiles_selected_index() == 1, "Profile row selects a cylinder with touch");
    if (auto *spare = find_label(lv_screen_active(), "Spare")) {
        lv_obj_scroll_to_view_recursive(spare, LV_ANIM_OFF);
        pump_lvgl();
        lv_area_t row{};
        lv_obj_get_coords(lv_obj_get_parent(spare), &row);
        tap(150, (row.y1 + row.y2) / 2);
    }
    expect(cylinder_profiles_selected_index() == 5,
           "Scrolled cylinder profiles remain selectable");
    tap(375, 238);

    screen_manager_show(SCREEN_HISTORY); pump_lvgl();
    expect(analysis_history_count() > 0, "Saved history is available before clearing");
    tap(409, 87);
    expect(analysis_history_count() == 0 && find_label(lv_screen_active(), "NO CAPTURES YET"),
           "History Clear removes records and shows the empty state");
    snapshot("history-empty");
    tap(49, 25);
    expect(screen_manager_current() == SCREEN_HOME, "Instrument back action returns home");
    return ok;
}

}  // namespace

int main() {
    lv_init();
    lv_display_t* display = lv_display_create(480, 800);
    lv_display_set_flush_cb(display, flush_cb);
    lv_display_set_buffers(display, g_draw_buffer, nullptr, sizeof(g_draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_indev_t* pointer = lv_indev_create();
    lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(pointer, pointer_read_cb);
    lv_indev_set_display(pointer, display);

    settings_init();
    wifi_service_init();
    battery_service_init();
    ota_service_init();

    sd_log_start();
    screen_manager_init();
    battery_start_monitoring();
    pump_lvgl();

    bool ok = true;
    const bool splash_visible = screen_manager_current() == SCREEN_SPLASH &&
                                find_class(lv_screen_active(), &lv_image_class) != nullptr;
    std::printf("%s startup shows the selected Ægir logo\n", splash_visible ? "PASS" : "FAIL");
    ok = splash_visible && ok;
    snapshot("splash");
    lv_tick_inc(2000);
    pump_lvgl();
    const bool splash_finished = screen_manager_current() == SCREEN_HOME;
    std::printf("%s splash opens the home screen automatically\n", splash_finished ? "PASS" : "FAIL");
    ok = splash_finished && ok;

    ok = show_and_check(SCREEN_HOME) && ok;
    ok = show_and_check(SCREEN_ANALYSE) && ok;
    ok = menu_and_paging_checks() && ok;
    ok = show_and_check(SCREEN_DIVE_PLANNER) && ok;
    snapshot("dive-planner", nullptr);
    ok = header_matches_palette("Dive Planner") && ok;
    ok = show_and_check(SCREEN_HISTORY) && ok;
    ok = show_and_check(SCREEN_CYLINDERS) && ok;
    snapshot("cylinders");
    ok = show_and_check(SCREEN_SETTINGS) && ok;
    snapshot("settings");
    ok = show_and_check(SCREEN_WIFI) && ok;
    snapshot("wifi", nullptr);
    ok = header_matches_palette("WiFi") && ok;
    ok = show_and_check(SCREEN_UPDATE) && ok;
    snapshot("software-update", nullptr);
    ok = header_matches_palette("Software Update") && ok;
    ok = show_and_check(SCREEN_CALIBRATE) && ok;
    snapshot("calibration", nullptr);
    ok = header_matches_palette("Calibration (demo)") && ok;
    ok = show_and_check(SCREEN_SAFETY) && ok;
    snapshot("safety-settings", nullptr);
    ok = header_matches_palette("Safety Settings") && ok;
    const bool sensor_limits_visible = find_label(lv_screen_active(), "CO advisory") &&
        find_label(lv_screen_active(), "CO alarm") &&
        find_label(lv_screen_active(), "Chamber RH advisory") &&
        find_label(lv_screen_active(), "Chamber RH alarm") &&
        !find_label(lv_screen_active(), "CO2 advisory");
    std::printf("%s Safety Settings expose CO and chamber RH thresholds instead of CO2\n",
                sensor_limits_visible ? "PASS" : "FAIL");
    ok = sensor_limits_visible && ok;
    if (sensor_limits_visible) {
        auto* name = find_label(lv_screen_active(), "CO advisory");
        auto* row = lv_obj_get_parent(name);
        auto* plus = lv_obj_get_child(row, 3);
        lv_obj_send_event(plus, LV_EVENT_CLICKED, nullptr);
        const bool adjusted = settings_get(SETTING_CO_ADVISORY_PPM) == 4;
        lv_obj_send_event(plus, LV_EVENT_CLICKED, nullptr);
        const bool ordered = settings_get(SETTING_CO_ADVISORY_PPM) == 4;
        auto* rh_name = find_label(lv_screen_active(), "Chamber RH alarm");
        auto* content = lv_obj_get_parent(lv_obj_get_parent(rh_name));
        lv_obj_scroll_to_y(content, 400, LV_ANIM_OFF);
        pump_lvgl();
        lv_area_t rh_area{};
        lv_obj_get_coords(rh_name, &rh_area);
        const bool scrolled = rh_area.y1 >= 70 && rh_area.y2 < 730;
        snapshot("safety-settings-sensor-alerts", nullptr);
        click("Reset Safety Limits");
        const bool reset = settings_get(SETTING_CO_ADVISORY_PPM) ==
                           settings_get_default(SETTING_CO_ADVISORY_PPM);
        const bool safety_controls = adjusted && ordered && scrolled && reset;
        std::printf("%s Safety limits adjust, stay ordered, scroll, and reset\n",
                    safety_controls ? "PASS" : "FAIL");
        ok = safety_controls && ok;
    }
    ok = show_and_check(SCREEN_DEVICE) && ok;
    snapshot("device-settings", nullptr);
    ok = header_matches_palette("Device Settings") && ok;
    snapshot("sd-device-status", "SD");
    const bool sd_actions_absent = !find_label(lv_screen_active(), "Safely eject SD card") &&
                                   !find_label(lv_screen_active(), "Retry SD card (preserve files)") &&
                                   find_label(lv_screen_active(), "SD") != nullptr;
    std::printf("%s SD status remains without inaccessible card actions\n", sd_actions_absent ? "PASS" : "FAIL");
    ok = sd_actions_absent && ok;
    auto* brightness_slider = find_class(lv_screen_active(), &lv_slider_class);
    auto* sleep_choices = find_matrix(lv_screen_active(), "Never");
    const bool selectors_present = brightness_slider && sleep_choices &&
        lv_slider_get_min_value(brightness_slider) == 10 &&
        lv_slider_get_max_value(brightness_slider) == 100 &&
        !find_matrix(lv_screen_active(), "100%");
    std::printf("%s Device settings show a 10-100 brightness slider and sleep choices\n",
                selectors_present ? "PASS" : "FAIL");
    ok = selectors_present && ok;
    if (selectors_present) {
        lv_area_t slider_area{};
        lv_obj_get_coords(brightness_slider, &slider_area);
        const int slider_y = (slider_area.y1 + slider_area.y2) / 2;
        tap(slider_area.x1 + 8, slider_y);
        const bool brightness_low = settings_get(SETTING_BRIGHTNESS) <= 15 &&
                                    backlight_get() == settings_get(SETTING_BRIGHTNESS);
        tap(slider_area.x2 - 8, slider_y);
        const bool brightness_high = settings_get(SETTING_BRIGHTNESS) >= 95 &&
                                     backlight_get() == settings_get(SETTING_BRIGHTNESS);
        tap((slider_area.x1 + slider_area.x2) / 2, slider_y);
        const bool brightness_middle = settings_get(SETTING_BRIGHTNESS) >= 45 &&
                                       settings_get(SETTING_BRIGHTNESS) <= 65 &&
                                       backlight_get() == settings_get(SETTING_BRIGHTNESS);
        swipe((slider_area.x1 + slider_area.x2) / 2, slider_area.x1 + 8, slider_y);
        const bool drag_low = settings_get(SETTING_BRIGHTNESS) <= 15 &&
                              backlight_get() == settings_get(SETTING_BRIGHTNESS);
        swipe(slider_area.x1 + 8, slider_area.x2 - 8, slider_y);
        const bool drag_high = settings_get(SETTING_BRIGHTNESS) >= 95 &&
                               backlight_get() == settings_get(SETTING_BRIGHTNESS);
        tap(290, 497);
        const bool sleep_three = settings_get(SETTING_SCREEN_TIMEOUT) == 2;
        tap(190, 497);
        const bool sleep_one = settings_get(SETTING_SCREEN_TIMEOUT) == 1;
        const bool selector_touch = brightness_low && brightness_high && brightness_middle &&
                                    drag_low && drag_high &&
                                    sleep_three && sleep_one;
        std::printf("%s Brightness slider and sleep choices respond to touch and persist their values\n",
                    selector_touch ? "PASS" : "FAIL");
        ok = selector_touch && ok;
        settings_set(SETTING_BRIGHTNESS, 95);
        backlight_set(95);
        screen_manager_show(SCREEN_SETTINGS); pump_lvgl();
        screen_manager_show(SCREEN_DEVICE); pump_lvgl();
        brightness_slider = find_class(lv_screen_active(), &lv_slider_class);
        const bool custom_brightness = find_label(lv_screen_active(), "Current 95%") &&
            brightness_slider && lv_slider_get_value(brightness_slider) == 95;
        std::printf("%s Existing custom brightness values reappear on the slider\n",
                    custom_brightness ? "PASS" : "FAIL");
        ok = custom_brightness && ok;
        click("Reset to Defaults");
        const bool reset_brightness = settings_get(SETTING_BRIGHTNESS) == 80 &&
            backlight_get() == 80 && brightness_slider &&
            lv_slider_get_value(brightness_slider) == 80;
        std::printf("%s Reset restores slider and physical brightness to 80%%\n",
                    reset_brightness ? "PASS" : "FAIL");
        ok = reset_brightness && ok;
    }
    ok = show_and_check(SCREEN_ANALYSE) && ok;
    snapshot("sd-analysis-status");
    ok = find_label(lv_screen_active(),"SD") && ok;
    ok = logging_navigation_checks() && ok;
    ok = calibration_checks() && ok;
    ok = secondary_screen_checks() && ok;

    return ok ? 0 : 1;
}
