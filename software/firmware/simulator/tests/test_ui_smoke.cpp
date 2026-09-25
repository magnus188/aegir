#include <lvgl.h>

#include "services/battery_service.h"
#include "services/ota_service.h"
#include "services/settings_service.h"
#include "services/wifi_service.h"
#include "ui/screens/screen_manager.h"
#include "services/gas_calibration_service.h"
#include "services/analysis_history.h"
#include "sensors/sensor_interface.h"
#include "services/storage_service.h"
#include "services/sd_log_service.h"
extern "C" void battery_mock_set_available(bool available);

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

uint8_t g_draw_buffer[480 * 40 * 2];
uint16_t g_pixels[480 * 800]{};

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
    expect(action("O2 setup") != nullptr, "Analyse exposes the direct sensor-setup action");
    click("O2 setup");
    // LVGL sends the destination's LOADED before the old screen's UNLOADED.
    // Exercise the real click and both event handlers, not a modeled ordering.
    expect(activity(SCREEN_CALIBRATE, sd_log::Mode::Calibration),
           "Analyse unload cannot stop the newly loaded Calibration activity");
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
    click("Save Avg");
    expect(analysis_history_count() > 0, "Calibrated simulated measurements retain normal history capture");
    screen_manager_show(SCREEN_HISTORY); pump_lvgl();
    expect(find_label(lv_screen_active(), "CO 0.3 ppm") && find_label(lv_screen_active(), "CO2"),
           "History distinguishes measured CO from legacy simulated CO2");
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

}  // namespace

int main() {
    lv_init();
    lv_display_t* display = lv_display_create(480, 800);
    lv_display_set_flush_cb(display, flush_cb);
    lv_display_set_buffers(display, g_draw_buffer, nullptr, sizeof(g_draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);

    settings_init();
    wifi_service_init();
    battery_service_init();
    ota_service_init();

    sd_log_start();
    screen_manager_init();
    battery_start_monitoring();
    pump_lvgl();

    bool ok = true;
    ok = show_and_check(SCREEN_HOME) && ok;
    ok = show_and_check(SCREEN_ANALYSE) && ok;
    ok = show_and_check(SCREEN_DIVE_PLANNER) && ok;
    ok = show_and_check(SCREEN_HISTORY) && ok;
    ok = show_and_check(SCREEN_CYLINDERS) && ok;
    ok = show_and_check(SCREEN_SETTINGS) && ok;
    ok = show_and_check(SCREEN_WIFI) && ok;
    ok = show_and_check(SCREEN_UPDATE) && ok;
    ok = show_and_check(SCREEN_CALIBRATE) && ok;
    ok = show_and_check(SCREEN_SAFETY) && ok;
    ok = show_and_check(SCREEN_DEVICE) && ok;
    snapshot("sd-device-status", "SD");
    ok = find_label(lv_screen_active(),"Safely eject SD card") && ok;
    ok = find_label(lv_screen_active(),"Retry SD card (preserve files)") && ok;
    ok = show_and_check(SCREEN_ANALYSE) && ok;
    snapshot("sd-analysis-status");
    ok = find_label(lv_screen_active(),"SD") && ok;
    ok = logging_navigation_checks() && ok;
    ok = calibration_checks() && ok;

    return ok ? 0 : 1;
}
