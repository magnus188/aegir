#pragma once
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Color palette
#define STYLE_COLOR_PRIMARY     0x1E88E5  // Instrument blue
#define STYLE_COLOR_PRIMARY_DARK 0x0D47A1 // Darker blue for pressed
#define STYLE_COLOR_ACCENT      0x00B8D4  // Cyan accent
#define STYLE_COLOR_BACKGROUND  0x0B0F14  // Main background
#define STYLE_COLOR_BG_DARK     0x0B0F14  // Dark background (alias)
#define STYLE_COLOR_BG_CARD     0x18212B  // Card background
#define STYLE_COLOR_SURFACE     0x111820  // Surface/card (alias)
#define STYLE_COLOR_BORDER      0x263442  // Instrument border
#define STYLE_COLOR_DATA        0x56CCF2  // Live data cyan
#define STYLE_COLOR_TEXT_LIGHT  0xFFFFFF  // White text
#define STYLE_COLOR_TEXT_DIM    0x9BA7B4  // Dimmed text
#define STYLE_COLOR_SUCCESS     0x2ECC71  // Green
#define STYLE_COLOR_WARNING     0xF2C94C  // Amber
#define STYLE_COLOR_ERROR       0xEB5757  // Red

// Button colors for each menu item (gradient pairs: main + accent)
#define STYLE_COLOR_ANALYSE         0x56CCF2
#define STYLE_COLOR_ANALYSE_ACCENT  0x1E88E5
#define STYLE_COLOR_DIVE_PLAN       0x2ECC71
#define STYLE_COLOR_DIVE_PLAN_ACCENT 0x1F8F4D
#define STYLE_COLOR_HISTORY         0xF2C94C
#define STYLE_COLOR_HISTORY_ACCENT  0xB38716
#define STYLE_COLOR_SETTINGS        0x9BA7B4
#define STYLE_COLOR_SETTINGS_ACCENT 0x52606D

/**
 * Initialize global styles and load fonts
 * Must be called after lv_init()
 */
void styles_init(void);

/**
 * Get loaded fonts
 */
const lv_font_t* styles_get_font_normal(void);
const lv_font_t* styles_get_font_bold(void);
const lv_font_t* styles_get_font_light(void);

/**
 * Get large font for buttons
 */
const lv_font_t* styles_get_font_button(void);

#ifdef __cplusplus
}
#endif
