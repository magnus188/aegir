#pragma once
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create the software update screen
 * @return The screen object
 */
lv_obj_t* update_screen_create(void);

/**
 * Refresh update screen (re-check for updates)
 */
void update_screen_refresh(void);

#ifdef __cplusplus
}
#endif
