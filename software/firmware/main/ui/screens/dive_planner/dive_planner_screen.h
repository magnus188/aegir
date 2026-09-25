#pragma once
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create the dive planner screen with interactive sliders
 * Features:
 * - Depth slider (0-100m)
 * - PPO2 slider (0.5-1.6 bar)
 * - Trimix toggle to enable helium calculations
 * - EAD slider (0-40m) - only when trimix enabled
 * - Helium slider (0-70%) - only when trimix enabled
 * - Lock buttons to fix one parameter while adjusting others
 * 
 * @return The created screen object
 */
lv_obj_t* dive_planner_screen_create(void);

#ifdef __cplusplus
}
#endif
