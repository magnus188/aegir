#pragma once
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create a reusable navigation bar with title (no back button)
 * @param parent Parent object to attach navbar to
 * @param title Title text to display
 * @return The navbar container object
 */
lv_obj_t* navbar_create(lv_obj_t* parent, const char* title);

/**
 * Create a navigation bar with title and back button
 * @param parent Parent object to attach navbar to
 * @param title Title text to display
 * @param back_cb Callback function when back button is pressed (can be NULL for default home navigation)
 * @return The navbar container object
 */
lv_obj_t* navbar_create_with_back(lv_obj_t* parent, const char* title, lv_event_cb_t back_cb);

/**
 * Update navbar title
 * @param navbar The navbar object returned by navbar_create
 * @param title New title text
 */
void navbar_set_title(lv_obj_t* navbar, const char* title);

/**
 * Get navbar height for layout calculations
 * @return Height in pixels
 */
lv_coord_t navbar_get_height(void);

#ifdef __cplusplus
}
#endif
