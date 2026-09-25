#pragma once
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create the WiFi settings screen
 * @return The screen object
 */
lv_obj_t* wifi_screen_create(void);

/**
 * Refresh the network list
 * Call this to start a new scan
 */
void wifi_screen_refresh(void);

#ifdef __cplusplus
}
#endif
