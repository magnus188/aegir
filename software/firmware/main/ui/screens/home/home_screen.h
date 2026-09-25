#pragma once
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create the home screen with 2x2 menu grid
 * @return The screen object
 */
lv_obj_t* home_screen_create(void);

#ifdef __cplusplus
}
#endif
