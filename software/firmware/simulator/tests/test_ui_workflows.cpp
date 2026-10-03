#include <lvgl.h>
#include "services/settings_service.h"
#include "services/wifi_service.h"
#include "services/battery_service.h"
#include "services/ota_service.h"
#include "services/sd_log_service.h"
#include "ui/screens/screen_manager.h"
#include "ui/styles/styles.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
extern "C" void wifi_mock_set_ready(bool ready);
extern "C" void wifi_mock_set_scan_pending(bool pending);
extern "C" void wifi_mock_set_connect_pending(bool pending);

namespace {
int failed = 0;
uint8_t buffer[480 * 40 * 2];
uint16_t pixels[480 * 800];
lv_point_t point{};
bool pressed = false;

void expect(bool condition, const char* message) {
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", message);
    if (!condition) ++failed;
}
void pump(int frames = 4) {
    for (int i = 0; i < frames; ++i) { lv_tick_inc(16); lv_timer_handler(); }
}
lv_obj_t* label(lv_obj_t* root, const char* text, bool exact = true) {
    if (lv_obj_check_type(root, &lv_label_class)) {
        const char* value = lv_label_get_text(root);
        if (exact ? std::strcmp(value, text) == 0 : std::strstr(value, text) != nullptr) return root;
    }
    for (unsigned i = 0; i < lv_obj_get_child_count(root); ++i)
        if (auto* found = label(lv_obj_get_child(root, i), text, exact)) return found;
    return nullptr;
}
lv_obj_t* label(const char* text, bool exact = true) { return label(lv_screen_active(), text, exact); }
lv_obj_t* widget(lv_obj_t* root, const lv_obj_class_t* type) {
    if (lv_obj_check_type(root, type)) return root;
    for (unsigned i = 0; i < lv_obj_get_child_count(root); ++i)
        if (auto* found = widget(lv_obj_get_child(root, i), type)) return found;
    return nullptr;
}
void tap(lv_obj_t* obj) {
    expect(obj != nullptr, "Touch target exists");
    if (!obj) return;
    lv_obj_scroll_to_view_recursive(obj, LV_ANIM_OFF);
    pump();
    lv_area_t area{}; lv_obj_get_coords(obj, &area);
    point = {(area.x1 + area.x2) / 2, (area.y1 + area.y2) / 2};
    expect(point.x >= 0 && point.x < 480 && point.y >= 0 && point.y < 800, "Touch target is on screen");
    pressed = true; pump(4); pressed = false; pump(5);
}
void click(const char* text) { tap(label(text)); }
void show(screen_id_t screen) { screen_manager_show(screen); pump(24); }
void capture(const char* name) {
    const char* dir = std::getenv("TRIMIX_UI_CAPTURE_DIR");
    if (!dir) return;
    pump(); lv_refr_now(nullptr);
    char path[1024]; std::snprintf(path, sizeof(path), "%s/%s.ppm", dir, name);
    FILE* file = std::fopen(path, "wb");
    if (!file) return;
    std::fprintf(file, "P6\n480 800\n255\n");
    for (uint16_t pixel : pixels) {
        unsigned char rgb[] = {static_cast<unsigned char>(((pixel >> 11) & 31) * 255 / 31),
            static_cast<unsigned char>(((pixel >> 5) & 63) * 255 / 63), static_cast<unsigned char>((pixel & 31) * 255 / 31)};
        std::fwrite(rgb, 1, 3, file);
    }
    std::fclose(file);
}
lv_obj_t* planner_row(const char* title) {
    auto* obj = label(title);
    return obj ? lv_obj_get_parent(lv_obj_get_parent(obj)) : nullptr;
}
lv_obj_t* planner_value(const char* title) {
    auto* row = planner_row(title);
    return row ? lv_obj_get_child(lv_obj_get_child(row, 0), 1) : nullptr;
}
float value(const char* title) {
    auto* obj = planner_value(title);
    return obj ? std::strtof(lv_label_get_text(obj), nullptr) : NAN;
}
void set_slider(const char* title, int v) {
    auto* row = planner_row(title);
    auto* slider = row ? widget(row, &lv_slider_class) : nullptr;
    expect(slider != nullptr, "Planner slider exists");
    if (slider) { lv_slider_set_value(slider, v, LV_ANIM_OFF); lv_obj_send_event(slider, LV_EVENT_VALUE_CHANGED, nullptr); }
    pump();
}
void lock(const char* title) {
    auto* row = planner_row(title);
    tap(row ? lv_obj_get_child(lv_obj_get_child(row, 0), 2) : nullptr);
}
void key(const char* text) {
    auto* matrix = widget(lv_screen_active(), &lv_buttonmatrix_class);
    expect(matrix != nullptr, "Numeric keypad is open");
    if (!matrix) return;
    for (unsigned i = 0; ; ++i) {
        const char* button_text = lv_buttonmatrix_get_button_text(matrix, i);
        if (!button_text) break;
        if (std::strcmp(button_text, text) == 0) {
            lv_buttonmatrix_set_selected_button(matrix, i);
            lv_obj_send_event(matrix, LV_EVENT_VALUE_CHANGED, nullptr);
            pump(); return;
        }
    }
    expect(false, "Requested keypad key exists");
}
void planner_checks() {
    show(SCREEN_DIVE_PLANNER);
    auto* actual_ppo2 = label("PPO2:", false);
    auto* mod = label("MOD:", false);
    auto* result_card = actual_ppo2 ? lv_obj_get_parent(actual_ppo2) : nullptr;
    bool result_fits = result_card && mod;
    if (result_fits) {
        lv_area_t card{}, first{}, second{};
        lv_obj_get_coords(result_card, &card);
        lv_obj_get_coords(actual_ppo2, &first); lv_obj_get_coords(mod, &second);
        result_fits &= first.y1 == second.y1 && first.x2 < second.x1;
        for (unsigned i = 0; i < lv_obj_get_child_count(result_card); ++i) {
            lv_area_t area{}; lv_obj_get_coords(lv_obj_get_child(result_card, i), &area);
            result_fits &= area.x1 >= card.x1 && area.x2 <= card.x2 && area.y1 >= card.y1 && area.y2 <= card.y2;
        }
    }
    expect(result_fits, "All planner summary values fit the result card in two columns");
    tap(planner_value("Depth")); key("4"); key("5"); key("OK");
    expect(value("Depth") == 45, "Numeric keypad commits entered depth");
    tap(planner_value("Depth")); key("9"); click("Cancel");
    expect(value("Depth") == 45, "Cancel preserves the prior depth");
    lock("Depth"); tap(planner_value("Depth"));
    expect(!label("Depth (m)"), "Locked field cannot be edited through the numeric keypad");
    if (label("Cancel")) click("Cancel");
    set_slider("Depth", 80);
    auto* slider = widget(planner_row("Depth"), &lv_slider_class);
    expect(value("Depth") == 45 && lv_slider_get_value(slider) == 45,
           "Locked slider and displayed value stay synchronized");
    lock("Depth");
    auto* toggle = widget(lv_screen_active(), &lv_switch_class); tap(toggle);
    set_slider("Depth", 40);
    expect(value("EAD (Target)") == 40, "Air EAD equals actual depth");
    set_slider("O2", 32);
    expect(value("EAD (Target)") == 33, "EAD responds to oxygen fraction as well as helium");
    set_slider("O2", 100); set_slider("Helium", 100);
    expect(value("O2") + value("Helium") <= 100, "Helium edit cannot create more than 100 percent total gas");
    set_slider("O2", 21); set_slider("Helium", 45); set_slider("O2", 100);
    expect(value("O2") + value("Helium") <= 100, "Oxygen edit cannot create more than 100 percent total gas");
    set_slider("O2", 21); set_slider("Helium", 35); set_slider("Depth", 40);
    lock("Depth"); lock("EAD (Target)");
    const float depth = value("Depth");
    set_slider("Helium", 60);
    expect(value("Depth") == depth, "Trimix recalculation respects a locked depth");
    capture("planner-locked");
    lock("Depth"); lock("EAD (Target)");
    tap(toggle); set_slider("O2", 0);
    expect(!label("Air (21% O2)"), "Zero oxygen is never mislabeled as air");
    set_slider("O2", 21);
    tap(planner_value("PPO2")); key("."); key("OK");
    expect(std::fabs(value("PPO2") - 1.4f) < 0.01f, "A bare decimal point does not silently change PPO2");
    if (label("Cancel")) click("Cancel");
    click("Start +10"); expect(label("Top-up 60 -> 200", false), "Blend start pressure increases");
    click("Start -10"); expect(label("Top-up 50 -> 200", false), "Blend start pressure decreases");
    click("Final +10"); expect(label("Top-up 50 -> 210", false), "Blend final pressure increases");
    click("Source"); expect(label("Source EAN32", false), "Blend source cycles");
    capture("planner-blend");
    settings_set(SETTING_DENSITY_ADVISORY_X10, 30);
    settings_set(SETTING_DENSITY_ALARM_X10, 40);
    show(SCREEN_SETTINGS); show(SCREEN_DIVE_PLANNER);
    auto* density = label("g/L", false);
    expect(density && lv_color_eq(lv_obj_get_style_text_color(density, LV_PART_MAIN), lv_color_hex(STYLE_COLOR_ERROR)),
           "Planner reload uses the density thresholds configured in Safety Settings");
}
void planner_lock_matrix_checks() {
    show(SCREEN_DIVE_PLANNER);
    tap(widget(lv_screen_active(), &lv_switch_class));
    const char* tops[] = {nullptr, "Depth", "PPO2", "O2"};
    const char* bottoms[] = {nullptr, "EAD (Target)", "Helium"};
    const char* fields[] = {"Depth", "PPO2", "O2", "EAD (Target)", "Helium"};
    const int candidates[][5] = {{0, 10, 30, 80, 150}, {100, 125, 140, 160, 200},
        {0, 18, 32, 80, 100}, {0, 10, 30, 100, 200}, {0, 20, 45, 80, 100}};
    bool preserves_locks = true, compositions_valid = true;
    for (const char* top : tops) for (const char* bottom : bottoms) {
        set_slider("Helium", 0); set_slider("O2", 21); set_slider("Depth", 40); set_slider("Helium", 35);
        if (top) lock(top);
        if (bottom) lock(bottom);
        for (unsigned field = 0; field < 5; ++field) for (int candidate : candidates[field]) {
            const float top_before = top ? value(top) : 0, bottom_before = bottom ? value(bottom) : 0;
            set_slider(fields[field], candidate);
            preserves_locks &= (!top || value(top) == top_before) && (!bottom || value(bottom) == bottom_before);
            compositions_valid &= std::isfinite(value("O2")) && std::isfinite(value("Helium")) &&
                value("O2") >= 0 && value("Helium") >= 0 && value("O2") + value("Helium") <= 100 &&
                value("Depth") >= 0 && value("Depth") <= 150;
        }
        if (top) lock(top);
        if (bottom) lock(bottom);
    }
    expect(preserves_locks, "All 12 top/bottom lock combinations preserve locked values across 300 edits");
    expect(compositions_valid, "All 300 boundary edits keep displayed mixtures and depths valid");
}
void wifi_checks() {
    show(SCREEN_WIFI); pump(30);
    click("Trimix Lab");
    expect(label("Enter Password"), "Secured Wi-Fi opens the password view");
    auto* ta = widget(lv_screen_active(), &lv_textarea_class);
    expect(ta && lv_textarea_get_password_mode(ta), "Password is masked");
    capture("wifi-password");
    click("Cancel"); expect(!label("Enter Password") && !wifi_service_is_connected(), "Cancel closes password entry without connecting");
    click("Trimix Lab");
    ta = widget(lv_screen_active(), &lv_textarea_class);
    if (ta) lv_textarea_set_text(ta, "simulator-test");
    click("Connect"); pump(40);
    expect(wifi_service_is_connected() && label("Disconnect"), "Secured connection displays connected controls");
    click("Disconnect"); pump(40);
    auto* connected = label("Connected");
    expect(!wifi_service_is_connected() && (!connected || !lv_obj_is_visible(connected)),
           "Disconnect clears stale Connected status");
    click("Guest Net"); pump(40);
    expect(wifi_service_is_connected() && !label("Enter Password"), "Open network connects without a password modal");
    capture("wifi-connected");
    click("Disconnect");
    click("Scan"); pump(30);
    expect(label("Workshop") && label("Guest Net"), "Rescan repopulates network choices");
}
void ota_checks() {
    show(SCREEN_UPDATE); click("Check for Updates");
    expect(label("Please connect to WiFi first"), "Offline update check provides an actionable message");
    wifi_service_connect("Guest Net", nullptr);
    click("Check for Updates"); pump(30);
    expect(label("Update available!"), "Update check displays the simulated release");
    capture("update-available");
    click("Install Update"); pump(200);
    expect(ota_get_state() == OTA_STATE_SUCCESS, "Simulated installation completes");
    expect(lv_obj_get_child_count(lv_layer_top()) == 0,
           "Completed simulated update releases the full-screen input overlay");
    expect(label("Restart Now") && lv_obj_is_visible(lv_obj_get_parent(label("Restart Now"))),
           "Completed update provides a working return action in the simulator");
    capture("update-complete");
    // Don't tap through an overlay in the failing baseline.
    if (lv_obj_get_child_count(lv_layer_top()) == 0 && label("Restart Now")) {
        click("Restart Now"); pump(30);
        expect(ota_get_state() == OTA_STATE_IDLE && label("Check for Updates"), "Simulated reboot restores the update flow");
        click("Check for Updates"); pump(30); click("Install Update"); pump(40);
        expect(ota_get_state() == OTA_STATE_SUCCESS && lv_obj_get_child_count(lv_layer_top()) == 0,
               "A second simulated installation also completes without trapping input");
    }
}
unsigned timer_count() {
    unsigned count = 0;
    for (auto* timer = lv_timer_get_next(nullptr); timer; timer = lv_timer_get_next(timer)) ++count;
    return count;
}
void wifi_timeout_checks() {
    wifi_mock_set_ready(false);
    show(SCREEN_WIFI);
    const unsigned baseline = timer_count();
    for (int i = 0; i < 20; ++i) { show(SCREEN_SETTINGS); show(SCREEN_WIFI); }
    expect(timer_count() <= baseline, "Repeated navigation while Wi-Fi initializes does not accumulate retry timers");
    pump(1000);
    expect(label("WiFi unavailable", false), "Wi-Fi initialization has a bounded wait and actionable failure");
    wifi_mock_set_ready(true); click("Scan"); pump(30);
    expect(label("Guest Net"), "Scanning recovers when Wi-Fi becomes available");
    wifi_mock_set_scan_pending(true); click("Scan"); pump(1000);
    expect(label("Scan timed out", false), "A stuck scan times out instead of waiting forever");
    wifi_mock_set_scan_pending(false); click("Scan"); pump(30);
    expect(label("Workshop"), "Scanning can be retried after a timeout");
    wifi_mock_set_connect_pending(true); click("Guest Net"); pump(1950);
    expect(label("Connection failed", false) && !wifi_service_is_connected(), "An uncompleted connection times out with feedback");
    wifi_mock_set_connect_pending(false); click("Guest Net"); pump(40);
    expect(wifi_service_is_connected(), "A failed connection can be retried successfully");
}
void safety_checks() {
    show(SCREEN_SAFETY);
    const struct { const char* name; setting_key_t low, high; } limits[] = {
        {"PPO2 working limit", SETTING_PPO2_WORKING_X100, SETTING_PPO2_SECONDARY_X100},
        {"Density advisory", SETTING_DENSITY_ADVISORY_X10, SETTING_DENSITY_ALARM_X10},
        {"CO advisory", SETTING_CO_ADVISORY_PPM, SETTING_CO_ALARM_PPM},
        {"Chamber RH advisory", SETTING_HUMIDITY_ADVISORY_PCT, SETTING_HUMIDITY_ALARM_PCT},
    };
    for (const auto& item : limits) {
        auto* title = label(item.name);
        auto* plus = title ? lv_obj_get_child(lv_obj_get_parent(title), 3) : nullptr;
        expect(plus != nullptr, "Safety row has an increase control");
        if (plus) for (int i = 0; i < 25; ++i) lv_obj_send_event(plus, LV_EVENT_CLICKED, nullptr);
        expect(settings_get(item.low) < settings_get(item.high), item.name);
    }
    click("Reset Safety Limits");
    expect(settings_get(SETTING_PPO2_WORKING_X100) == 140 && settings_get(SETTING_DENSITY_ALARM_X10) == 63,
           "Reset restores all safety defaults");
}
void navigation_checks() {
    const screen_id_t screens[] = {SCREEN_HOME, SCREEN_ANALYSE, SCREEN_DIVE_PLANNER, SCREEN_HISTORY,
        SCREEN_CYLINDERS, SCREEN_SETTINGS, SCREEN_WIFI, SCREEN_UPDATE, SCREEN_CALIBRATE, SCREEN_SAFETY, SCREEN_DEVICE};
    for (int round = 0; round < 100; ++round) for (auto screen : screens) {
        show(screen);
        if (screen_manager_current() != screen) { expect(false, "Navigation reaches the requested screen"); return; }
        const auto mode = screen == SCREEN_ANALYSE ? sd_log::Mode::Analysis :
            screen == SCREEN_CALIBRATE ? sd_log::Mode::Calibration : sd_log::Mode::Idle;
        if (sd_log_status().activity != mode) { expect(false, "Navigation keeps logging activity consistent"); return; }
    }
    expect(true, "1,100 screen transitions preserve navigation and logging state");
}
}
int main(int argc, char** argv) {
    lv_init();
    auto* display = lv_display_create(480, 800);
    lv_display_set_flush_cb(display, [](lv_display_t* d, const lv_area_t* a, uint8_t* data) {
        auto* src = reinterpret_cast<uint16_t*>(data);
        for (int y = a->y1; y <= a->y2; ++y) for (int x = a->x1; x <= a->x2; ++x) pixels[y * 480 + x] = *src++;
        lv_display_flush_ready(d);
    });
    lv_display_set_buffers(display, buffer, nullptr, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    auto* pointer = lv_indev_create();
    lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(pointer, [](lv_indev_t*, lv_indev_data_t* data) {
        data->point = point; data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    });
    lv_indev_set_display(pointer, display);
    settings_init(); wifi_service_init(); battery_service_init(); ota_service_init(); sd_log_start();
    screens_init(); show(SCREEN_HOME);
    const char* scenario = argc > 1 ? argv[1] : "planner";
    if (!std::strcmp(scenario, "planner")) planner_checks();
    else if (!std::strcmp(scenario, "planner_locks")) planner_lock_matrix_checks();
    else if (!std::strcmp(scenario, "wifi")) wifi_checks();
    else if (!std::strcmp(scenario, "wifi_timeouts")) wifi_timeout_checks();
    else if (!std::strcmp(scenario, "ota")) ota_checks();
    else if (!std::strcmp(scenario, "safety")) safety_checks();
    else if (!std::strcmp(scenario, "navigation")) navigation_checks();
    else expect(false, "Unknown workflow scenario");
    return failed ? 1 : 0;
}
