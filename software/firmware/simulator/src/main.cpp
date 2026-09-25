#include "services/sd_log_service.h"
#include <lvgl.h>
#include "src/drivers/sdl/lv_sdl_keyboard.h"
#include "src/drivers/sdl/lv_sdl_mouse.h"
#include "src/drivers/sdl/lv_sdl_window.h"

#include "services/battery_service.h"
#include "services/analysis_history.h"
#include "services/ota_service.h"
#include "services/settings_service.h"
#include "services/wifi_service.h"
#include "ui/screens/screen_manager.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

namespace {

constexpr int kScreenWidth = 480;
constexpr int kScreenHeight = 800;

float parse_zoom(int argc, char** argv) {
    float zoom = 1.0f;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--zoom") == 0 && i + 1 < argc) {
            zoom = std::strtof(argv[++i], nullptr);
        } else if (std::strncmp(argv[i], "--zoom=", 7) == 0) {
            zoom = std::strtof(argv[i] + 7, nullptr);
        } else if (std::strcmp(argv[i], "--help") == 0) {
            std::printf("Usage: trimix_simulator [--zoom VALUE]\n");
            std::exit(0);
        }
    }

    if (zoom <= 0.0f) {
        std::fprintf(stderr, "Invalid --zoom value, using 1.0\n");
        return 1.0f;
    }
    return zoom;
}

}  // namespace

int main(int argc, char** argv) {
    const float zoom = parse_zoom(argc, argv);

    lv_init();

    lv_display_t* display = lv_sdl_window_create(kScreenWidth, kScreenHeight);
    lv_sdl_window_set_title(display, "Trimix Analysator Simulator");
    lv_sdl_window_set_zoom(display, zoom);
    lv_sdl_mouse_create();
    lv_sdl_keyboard_create();

    settings_init();
    wifi_service_init();
    battery_service_init();
    analysis_history_init();
    ota_service_init();

    sd_log_start();
    screens_init();
    battery_start_monitoring();
    wifi_service_auto_connect();

    for (;;) {
        lv_timer_handler();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
