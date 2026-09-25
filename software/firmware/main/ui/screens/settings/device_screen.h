#pragma once
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create the device settings screen
 * @return The screen object
 */
lv_obj_t* device_screen_create(void);

#ifdef __cplusplus
}
#endif
