#include <emscripten/emscripten.h>
#include <lvgl.h>
#include "src/drivers/sdl/lv_sdl_keyboard.h"
#include "src/drivers/sdl/lv_sdl_mouse.h"
#include "src/drivers/sdl/lv_sdl_window.h"

#include "services/analysis_history.h"
#include "services/battery_service.h"
#include "services/ota_service.h"
#include "services/settings_service.h"
#include "services/wifi_service.h"
#include "ui/screens/screen_manager.h"

namespace {

constexpr int kScreenWidth = 480;
constexpr int kScreenHeight = 800;

void run_frame() {
    lv_timer_handler();
}

}  // namespace

int main() {
    lv_init();

    lv_display_t* display = lv_sdl_window_create(kScreenWidth, kScreenHeight);
    lv_sdl_window_set_title(display, "Trimix Analysator Web Demo");
    lv_sdl_mouse_create();
    lv_sdl_keyboard_create();

    settings_init();
    wifi_service_init();
    battery_service_init();
    analysis_history_init();
    ota_service_init();

    screens_init();
    battery_start_monitoring();
    wifi_service_auto_connect();

    // Emscripten schedules this callback with requestAnimationFrame. A native
    // infinite loop would block the browser's event loop and prevent input.
    emscripten_set_main_loop(run_frame, 0, true);
    return 0;
}
