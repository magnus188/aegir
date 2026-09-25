#pragma once
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create the settings screen with menu options
 * @return The screen object
 */
lv_obj_t* settings_screen_create(void);

#ifdef __cplusplus
}
#endif
