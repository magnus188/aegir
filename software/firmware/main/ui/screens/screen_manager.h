#pragma once
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SCREEN_HOME = 0,
    SCREEN_ANALYSE,
    SCREEN_DIVE_PLANNER,
    SCREEN_HISTORY,
    SCREEN_CYLINDERS,
    SCREEN_SETTINGS,
    SCREEN_WIFI,
    SCREEN_UPDATE,
    SCREEN_CALIBRATE,
    SCREEN_SAFETY,
    SCREEN_DEVICE,
    SCREEN_COUNT
} screen_id_t;

void screen_manager_init(void);
void screen_manager_show(screen_id_t screen);
screen_id_t screen_manager_current(void);

// Legacy alias to keep main.cpp unchanged
void screens_init(void);

#ifdef __cplusplus
}
#endif
