#pragma once
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create the titleless home screen with Analyse hero and four destination tiles
 * @return The screen object
 */
lv_obj_t* home_screen_create(void);

#ifdef __cplusplus
}
#endif
