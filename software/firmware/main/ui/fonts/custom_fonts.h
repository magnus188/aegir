#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Custom font declarations
// These fonts will be generated from TTF files using LVGL font converter
LV_FONT_DECLARE(custom_font_normal_16);
LV_FONT_DECLARE(custom_font_bold_20);
LV_FONT_DECLARE(custom_font_bold_24);

// Font aliases for easier use
#define FONT_SMALL &custom_font_normal_16
#define FONT_NORMAL &custom_font_normal_16
#define FONT_MEDIUM &custom_font_bold_20
#define FONT_LARGE &custom_font_bold_24
#define FONT_TITLE &custom_font_bold_24
#define FONT_HEADER &custom_font_bold_24
#define FONT_BUTTON &custom_font_bold_24

#ifdef __cplusplus
}
#endif
