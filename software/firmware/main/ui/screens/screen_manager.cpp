#include "screen_manager.h"
#include "home/home_screen.h"
#include "analyse/analyse_screen.h"
#include "dive_planner/dive_planner_screen.h"
#include "history/history_screen.h"
#include "cylinders/cylinders_screen.h"
#include "settings/settings_screen.h"
#include "settings/wifi_screen.h"
#include "settings/update_screen.h"
#include "settings/device_screen.h"
#include "settings/calibrate_screen.h"
#include "settings/safety_screen.h"
#include "../styles/styles.h"
#include <array>
#include <esp_log.h>

namespace {
static const char *TAG = "SCREEN_MANAGER";

class ScreenManager {
public:
    static ScreenManager& instance() {
        static ScreenManager mgr;
        return mgr;
    }

    void init() {
        ESP_LOGI(TAG, "Initializing screen manager");
        
        // Initialize styles first
        styles_init();
        
        // Create screens
        screens_[SCREEN_HOME] = home_screen_create();
        screens_[SCREEN_ANALYSE] = analyse_screen_create();
        screens_[SCREEN_DIVE_PLANNER] = dive_planner_screen_create();
        screens_[SCREEN_HISTORY] = history_screen_create();
        screens_[SCREEN_CYLINDERS] = cylinders_screen_create();
        screens_[SCREEN_SETTINGS] = settings_screen_create();
        screens_[SCREEN_WIFI] = wifi_screen_create();
        screens_[SCREEN_UPDATE] = update_screen_create();
        screens_[SCREEN_CALIBRATE] = calibrate_screen_create();
        screens_[SCREEN_SAFETY] = safety_screen_create();
        screens_[SCREEN_DEVICE] = device_screen_create();
        
        show(SCREEN_HOME);
    }

    void show(screen_id_t id) {
        if (id >= SCREEN_COUNT) {
            ESP_LOGE(TAG, "Invalid screen %d", id);
            return;
        }

        lv_obj_t* scr = screens_[id];
        if (scr == lv_screen_active()) {
            current_screen_ = id;
            return;
        }

        current_screen_ = id;
        
        // Reset any accumulated scroll offset and disable screen-level scrolling
        // This prevents the bug where screen content shifts down and wraps
        lv_obj_scroll_to(scr, 0, 0, LV_ANIM_OFF);
        lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLL_ELASTIC);
        lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLL_MOMENTUM);
        lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLL_CHAIN);
        
        lv_scr_load(scr);
    }

    screen_id_t current() const {
        return current_screen_;
    }

private:
    std::array<lv_obj_t*, SCREEN_COUNT> screens_{};
    screen_id_t current_screen_ = SCREEN_HOME;
};

static ScreenManager& mgr() {
    return ScreenManager::instance();
}

}  // namespace

extern "C" {

void screen_manager_init(void) {
    mgr().init();
}

void screen_manager_show(screen_id_t screen) {
    mgr().show(screen);
}

screen_id_t screen_manager_current(void) {
    return mgr().current();
}

void screens_init(void) {
    screen_manager_init();
}

}
