#pragma once

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the backlight PWM controller
 * Call this once at startup
 */
void backlight_init(void);

/**
 * Set backlight brightness
 * @param percent Saved brightness level 10-100; standby is a separate override
 */
void backlight_set(uint8_t percent);

/**
 * Get current backlight brightness
 * @return Current brightness level 0-100
 */
uint8_t backlight_get(void);

// Thread-safe temporary override. Does not change saved/user brightness.
bool backlight_set_standby(bool enabled);
bool backlight_is_standby(void);

#ifdef __cplusplus
}
#endif
