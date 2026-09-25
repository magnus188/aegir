/*******************************************************************************
 * Size: 14 px
 * Bpp: 1
 * Opts: --font ../../../assets/fonts/light.ttf --size 14 --format lvgl --bpp 1 --no-compress --output custom_font_light_14.c --range 0x20-0x7E
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef CUSTOM_FONT_LIGHT_14
#define CUSTOM_FONT_LIGHT_14 1
#endif

#if CUSTOM_FONT_LIGHT_14

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xfe, 0x40,

    /* U+0022 "\"" */
    0xb6, 0x80,

    /* U+0023 "#" */
    0x10, 0x28, 0x91, 0xe2, 0x85, 0x3f, 0xa4, 0x50,
    0x80,

    /* U+0024 "$" */
    0x10, 0xfa, 0x4c, 0x89, 0x12, 0x15, 0x9, 0x12,
    0x25, 0xf0, 0x81, 0x0,

    /* U+0025 "%" */
    0xe1, 0x24, 0x89, 0x22, 0x50, 0x95, 0x9a, 0x90,
    0xa4, 0x49, 0x22, 0x48, 0xe0,

    /* U+0026 "&" */
    0x79, 0xa, 0x4, 0x8, 0x4f, 0xe1, 0x42, 0x84,
    0xf0,

    /* U+0027 "'" */
    0xe0,

    /* U+0028 "(" */
    0x12, 0x48, 0x88, 0x88, 0x88, 0x88, 0x42, 0x10,

    /* U+0029 ")" */
    0x84, 0x21, 0x11, 0x11, 0x11, 0x11, 0x24, 0x80,

    /* U+002A "*" */
    0x10, 0x23, 0x59, 0xc2, 0x89, 0x0,

    /* U+002B "+" */
    0x10, 0x20, 0x47, 0xf1, 0x2, 0x4, 0x0,

    /* U+002C "," */
    0xe0,

    /* U+002D "-" */
    0xf0,

    /* U+002E "." */
    0x80,

    /* U+002F "/" */
    0x0, 0x8, 0x20, 0x41, 0x2, 0x4, 0x10, 0x20,
    0x81, 0x4, 0x8, 0x0,

    /* U+0030 "0" */
    0x7a, 0x18, 0x61, 0x86, 0x18, 0x61, 0x85, 0xe0,

    /* U+0031 "1" */
    0x23, 0x28, 0x42, 0x10, 0x84, 0x27, 0xc0,

    /* U+0032 "2" */
    0x7a, 0x10, 0x41, 0x8, 0x42, 0x10, 0x83, 0xf0,

    /* U+0033 "3" */
    0x78, 0x10, 0x41, 0x38, 0x10, 0x41, 0x85, 0xe0,

    /* U+0034 "4" */
    0x8, 0x62, 0x92, 0x4a, 0x2f, 0xc2, 0x8, 0x20,

    /* U+0035 "5" */
    0xfa, 0x8, 0x20, 0xb8, 0x10, 0x41, 0x7, 0xe0,

    /* U+0036 "6" */
    0x7a, 0x18, 0x20, 0xbb, 0x18, 0x61, 0x85, 0xe0,

    /* U+0037 "7" */
    0xf8, 0x44, 0x22, 0x11, 0x8, 0x44, 0x0,

    /* U+0038 "8" */
    0x7a, 0x18, 0x61, 0x7a, 0x18, 0x61, 0x85, 0xe0,

    /* U+0039 "9" */
    0x7a, 0x18, 0x61, 0x85, 0xf0, 0x41, 0x85, 0xe0,

    /* U+003A ":" */
    0x82,

    /* U+003B ";" */
    0x83, 0x0,

    /* U+003C "<" */
    0x24, 0x48, 0x42, 0x20,

    /* U+003D "=" */
    0xfe, 0x0, 0x7, 0xf0,

    /* U+003E ">" */
    0x84, 0x21, 0x24, 0x80,

    /* U+003F "?" */
    0xf4, 0x42, 0x11, 0x10, 0x84, 0x1, 0x0,

    /* U+0040 "@" */
    0x7f, 0xa0, 0x28, 0x6, 0x79, 0x81, 0x60, 0x59,
    0xf6, 0x85, 0xa1, 0xe7, 0xe8, 0x2, 0x0, 0x7f,
    0x80,

    /* U+0041 "A" */
    0x8, 0x18, 0x28, 0x24, 0x24, 0x44, 0x5e, 0x42,
    0x81, 0x81,

    /* U+0042 "B" */
    0xfd, 0x6, 0xc, 0x18, 0x5f, 0xa0, 0xc1, 0x83,
    0xf8,

    /* U+0043 "C" */
    0x7a, 0x18, 0x20, 0x82, 0x8, 0x20, 0x81, 0xf0,

    /* U+0044 "D" */
    0xf9, 0xa, 0xc, 0x18, 0x30, 0x60, 0xc1, 0x85,
    0xf0,

    /* U+0045 "E" */
    0xfe, 0x8, 0x20, 0x83, 0xf8, 0x20, 0x83, 0xf0,

    /* U+0046 "F" */
    0xfe, 0x8, 0x20, 0x83, 0xf8, 0x20, 0x82, 0x0,

    /* U+0047 "G" */
    0x7c, 0xa, 0x4, 0x8, 0x13, 0xe0, 0xc1, 0x82,
    0xf8,

    /* U+0048 "H" */
    0x83, 0x6, 0xc, 0x18, 0x3f, 0xe0, 0xc1, 0x83,
    0x4,

    /* U+0049 "I" */
    0xff, 0xc0,

    /* U+004A "J" */
    0x8, 0x42, 0x10, 0x84, 0x21, 0x17, 0x80,

    /* U+004B "K" */
    0x85, 0xa, 0x24, 0x8a, 0x14, 0x24, 0x44, 0x85,
    0x8,

    /* U+004C "L" */
    0x82, 0x8, 0x20, 0x82, 0x8, 0x20, 0x83, 0xf0,

    /* U+004D "M" */
    0x81, 0xc3, 0xc3, 0xc5, 0xa5, 0xa5, 0xa9, 0xa9,
    0x99, 0x91,

    /* U+004E "N" */
    0x83, 0x87, 0xd, 0x19, 0x32, 0x62, 0xc5, 0x85,
    0x8,

    /* U+004F "O" */
    0x7c, 0xa, 0xc, 0x18, 0x30, 0x60, 0xc1, 0x80,
    0xf8,

    /* U+0050 "P" */
    0xfd, 0x6, 0xc, 0x18, 0x3f, 0xa0, 0x40, 0x81,
    0x0,

    /* U+0051 "Q" */
    0x7c, 0x4, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
    0x80, 0x7c, 0x3,

    /* U+0052 "R" */
    0xfd, 0x6, 0xc, 0x18, 0x3f, 0xa2, 0x42, 0x85,
    0x4,

    /* U+0053 "S" */
    0x7c, 0x6, 0x4, 0x8, 0xf, 0x80, 0x81, 0x82,
    0xf8,

    /* U+0054 "T" */
    0xfe, 0x20, 0x40, 0x81, 0x2, 0x4, 0x8, 0x10,
    0x20,

    /* U+0055 "U" */
    0x83, 0x6, 0xc, 0x18, 0x30, 0x60, 0xc1, 0x84,
    0xf0,

    /* U+0056 "V" */
    0x83, 0x6, 0xa, 0x24, 0x48, 0x8a, 0x14, 0x28,
    0x20,

    /* U+0057 "W" */
    0x84, 0x63, 0x24, 0xc9, 0x32, 0x54, 0x94, 0xa5,
    0x30, 0xcc, 0x32, 0x8, 0x80,

    /* U+0058 "X" */
    0x82, 0x88, 0xa1, 0x41, 0x2, 0xa, 0x34, 0x45,
    0x4,

    /* U+0059 "Y" */
    0x82, 0x89, 0x11, 0x42, 0x2, 0x4, 0x8, 0x10,
    0x20,

    /* U+005A "Z" */
    0xfe, 0x8, 0x20, 0x41, 0x4, 0x8, 0x20, 0x81,
    0xfc,

    /* U+005B "[" */
    0xf2, 0x49, 0x24, 0x92, 0x4e,

    /* U+005C "\\" */
    0x2, 0x8, 0x10, 0x40, 0x82, 0x8, 0x10, 0x40,
    0x82, 0x4,

    /* U+005D "]" */
    0xe4, 0x92, 0x49, 0x24, 0x9e,

    /* U+005E "^" */
    0x10, 0x51, 0x10, 0x10,

    /* U+005F "_" */
    0xfe,

    /* U+0060 "`" */
    0x88,

    /* U+0061 "a" */
    0x78, 0x10, 0x5f, 0x86, 0x17, 0xc0,

    /* U+0062 "b" */
    0x82, 0x8, 0x3e, 0x86, 0x18, 0x61, 0x87, 0xe0,

    /* U+0063 "c" */
    0x7a, 0x8, 0x20, 0x82, 0x7, 0x80,

    /* U+0064 "d" */
    0x4, 0x10, 0x5f, 0x86, 0x18, 0x61, 0x85, 0xf0,

    /* U+0065 "e" */
    0x7a, 0x18, 0x7f, 0x82, 0x7, 0x80,

    /* U+0066 "f" */
    0x34, 0x4f, 0x44, 0x44, 0x44,

    /* U+0067 "g" */
    0x7e, 0x18, 0x61, 0x86, 0x17, 0xc1, 0x85, 0xe0,

    /* U+0068 "h" */
    0x82, 0x8, 0x2e, 0x86, 0x18, 0x61, 0x86, 0x10,

    /* U+0069 "i" */
    0x9f, 0xc0,

    /* U+006A "j" */
    0x20, 0x12, 0x49, 0x24, 0x9c,

    /* U+006B "k" */
    0x84, 0x21, 0x2a, 0x53, 0x14, 0x94, 0x40,

    /* U+006C "l" */
    0xff, 0xc0,

    /* U+006D "m" */
    0xf7, 0x44, 0x62, 0x31, 0x18, 0x8c, 0x46, 0x22,

    /* U+006E "n" */
    0xba, 0x18, 0x61, 0x86, 0x18, 0x40,

    /* U+006F "o" */
    0x7a, 0x18, 0x61, 0x86, 0x17, 0x80,

    /* U+0070 "p" */
    0xfa, 0x18, 0x61, 0x86, 0x1f, 0xa0, 0x82, 0x0,

    /* U+0071 "q" */
    0x7e, 0x18, 0x61, 0x86, 0x17, 0xc1, 0x4, 0x10,

    /* U+0072 "r" */
    0xf4, 0x63, 0x8, 0x42, 0x0,

    /* U+0073 "s" */
    0x7a, 0x18, 0x1e, 0x4, 0x17, 0x80,

    /* U+0074 "t" */
    0x44, 0x4f, 0x44, 0x44, 0x43,

    /* U+0075 "u" */
    0x86, 0x18, 0x61, 0x86, 0x17, 0x40,

    /* U+0076 "v" */
    0x86, 0x24, 0x92, 0x50, 0xc2, 0x0,

    /* U+0077 "w" */
    0x88, 0xc6, 0x55, 0x4a, 0xa5, 0x52, 0x18, 0x88,

    /* U+0078 "x" */
    0x85, 0x22, 0x8, 0x31, 0x28, 0x40,

    /* U+0079 "y" */
    0x86, 0x24, 0x94, 0x50, 0xc2, 0x8, 0x20, 0x0,

    /* U+007A "z" */
    0xf8, 0x84, 0x44, 0x43, 0xe0,

    /* U+007B "{" */
    0xd, 0x24, 0xa4, 0x89, 0x24, 0x40,

    /* U+007C "|" */
    0xff, 0xf8,

    /* U+007D "}" */
    0x11, 0x24, 0x91, 0x49, 0x25, 0x0,

    /* U+007E "~" */
    0x22, 0xaa, 0x20
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 50, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 45, .box_w = 1, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 3, .adv_w = 92, .box_w = 3, .box_h = 3, .ofs_x = 1, .ofs_y = 7},
    {.bitmap_index = 5, .adv_w = 120, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 14, .adv_w = 120, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 26, .adv_w = 179, .box_w = 10, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 39, .adv_w = 125, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 48, .adv_w = 57, .box_w = 1, .box_h = 3, .ofs_x = 1, .ofs_y = 7},
    {.bitmap_index = 49, .adv_w = 74, .box_w = 4, .box_h = 15, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 57, .adv_w = 74, .box_w = 4, .box_h = 15, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 65, .adv_w = 123, .box_w = 7, .box_h = 6, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 71, .adv_w = 120, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 78, .adv_w = 45, .box_w = 1, .box_h = 3, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 79, .adv_w = 96, .box_w = 4, .box_h = 1, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 80, .adv_w = 45, .box_w = 1, .box_h = 1, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 81, .adv_w = 105, .box_w = 7, .box_h = 13, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 93, .adv_w = 120, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 101, .adv_w = 120, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 108, .adv_w = 120, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 116, .adv_w = 120, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 124, .adv_w = 120, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 132, .adv_w = 120, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 140, .adv_w = 120, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 148, .adv_w = 120, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 155, .adv_w = 120, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 163, .adv_w = 120, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 171, .adv_w = 45, .box_w = 1, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 172, .adv_w = 46, .box_w = 1, .box_h = 9, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 174, .adv_w = 120, .box_w = 4, .box_h = 7, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 178, .adv_w = 120, .box_w = 7, .box_h = 4, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 182, .adv_w = 120, .box_w = 4, .box_h = 7, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 186, .adv_w = 103, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 193, .adv_w = 195, .box_w = 10, .box_h = 13, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 210, .adv_w = 133, .box_w = 8, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 220, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 229, .adv_w = 122, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 237, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 246, .adv_w = 128, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 254, .adv_w = 122, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 262, .adv_w = 138, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 271, .adv_w = 148, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 280, .adv_w = 55, .box_w = 1, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 282, .adv_w = 103, .box_w = 5, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 289, .adv_w = 123, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 298, .adv_w = 112, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 306, .adv_w = 173, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 316, .adv_w = 148, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 325, .adv_w = 144, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 334, .adv_w = 132, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 343, .adv_w = 158, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 354, .adv_w = 145, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 363, .adv_w = 133, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 372, .adv_w = 110, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 381, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 390, .adv_w = 120, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 399, .adv_w = 163, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 412, .adv_w = 118, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 421, .adv_w = 113, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 430, .adv_w = 129, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 439, .adv_w = 72, .box_w = 3, .box_h = 13, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 444, .adv_w = 120, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 454, .adv_w = 72, .box_w = 3, .box_h = 13, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 459, .adv_w = 120, .box_w = 7, .box_h = 4, .ofs_x = 0, .ofs_y = 6},
    {.bitmap_index = 463, .adv_w = 112, .box_w = 7, .box_h = 1, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 464, .adv_w = 105, .box_w = 3, .box_h = 2, .ofs_x = 2, .ofs_y = 8},
    {.bitmap_index = 465, .adv_w = 119, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 471, .adv_w = 123, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 479, .adv_w = 115, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 485, .adv_w = 123, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 493, .adv_w = 120, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 499, .adv_w = 69, .box_w = 4, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 504, .adv_w = 123, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 512, .adv_w = 119, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 520, .adv_w = 46, .box_w = 1, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 522, .adv_w = 46, .box_w = 3, .box_h = 13, .ofs_x = -1, .ofs_y = -3},
    {.bitmap_index = 527, .adv_w = 108, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 534, .adv_w = 45, .box_w = 1, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 536, .adv_w = 168, .box_w = 9, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 544, .adv_w = 119, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 550, .adv_w = 122, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 556, .adv_w = 123, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 564, .adv_w = 123, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 572, .adv_w = 103, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 577, .adv_w = 110, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 583, .adv_w = 75, .box_w = 4, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 588, .adv_w = 119, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 594, .adv_w = 96, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 600, .adv_w = 152, .box_w = 9, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 608, .adv_w = 101, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 614, .adv_w = 96, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 622, .adv_w = 103, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 627, .adv_w = 64, .box_w = 3, .box_h = 15, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 633, .adv_w = 45, .box_w = 1, .box_h = 13, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 635, .adv_w = 64, .box_w = 3, .box_h = 15, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 641, .adv_w = 120, .box_w = 7, .box_h = 3, .ofs_x = 0, .ofs_y = 2}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};

/*-----------------
 *    KERNING
 *----------------*/


/*Pair left and right glyphs for kerning*/
static const uint8_t kern_pair_glyph_ids[] =
{
    1, 34,
    1, 53,
    1, 55,
    1, 56,
    1, 58,
    14, 34,
    14, 43,
    14, 53,
    14, 55,
    14, 57,
    14, 58,
    14, 59,
    15, 53,
    15, 55,
    15, 56,
    15, 58,
    24, 13,
    24, 14,
    24, 15,
    34, 14,
    34, 36,
    34, 40,
    34, 48,
    34, 50,
    34, 52,
    34, 53,
    34, 54,
    34, 55,
    34, 56,
    34, 58,
    34, 68,
    34, 70,
    34, 72,
    34, 80,
    34, 82,
    34, 84,
    34, 85,
    34, 86,
    34, 87,
    34, 88,
    34, 90,
    35, 13,
    35, 15,
    35, 34,
    35, 58,
    36, 58,
    37, 13,
    37, 15,
    37, 34,
    37, 53,
    37, 58,
    38, 58,
    39, 13,
    39, 14,
    39, 15,
    39, 34,
    39, 43,
    39, 58,
    40, 58,
    41, 58,
    42, 58,
    43, 13,
    43, 15,
    43, 34,
    43, 58,
    43, 86,
    44, 14,
    44, 36,
    44, 40,
    44, 48,
    44, 50,
    44, 58,
    44, 77,
    44, 86,
    44, 90,
    45, 14,
    45, 36,
    45, 40,
    45, 43,
    45, 48,
    45, 50,
    45, 53,
    45, 54,
    45, 55,
    45, 56,
    45, 58,
    45, 85,
    45, 90,
    46, 58,
    47, 58,
    48, 13,
    48, 15,
    48, 34,
    48, 53,
    48, 57,
    48, 58,
    49, 13,
    49, 14,
    49, 15,
    49, 34,
    49, 58,
    50, 13,
    50, 15,
    50, 58,
    51, 14,
    51, 48,
    51, 53,
    51, 54,
    51, 55,
    51, 56,
    51, 58,
    51, 70,
    51, 80,
    51, 86,
    51, 88,
    51, 90,
    52, 13,
    52, 15,
    52, 34,
    52, 52,
    52, 53,
    52, 58,
    52, 87,
    53, 13,
    53, 14,
    53, 15,
    53, 27,
    53, 28,
    53, 34,
    53, 43,
    53, 48,
    53, 52,
    53, 58,
    53, 66,
    53, 68,
    53, 70,
    53, 78,
    53, 80,
    53, 83,
    53, 84,
    53, 86,
    53, 88,
    53, 90,
    53, 91,
    54, 13,
    54, 15,
    54, 34,
    54, 58,
    55, 13,
    55, 14,
    55, 15,
    55, 27,
    55, 28,
    55, 34,
    55, 58,
    55, 66,
    55, 70,
    55, 77,
    55, 80,
    55, 86,
    56, 13,
    56, 15,
    56, 27,
    56, 28,
    56, 34,
    56, 58,
    56, 66,
    56, 70,
    56, 73,
    56, 80,
    56, 86,
    56, 90,
    57, 14,
    57, 36,
    57, 40,
    57, 58,
    57, 66,
    57, 90,
    58, 13,
    58, 14,
    58, 15,
    58, 27,
    58, 28,
    58, 34,
    58, 35,
    58, 36,
    58, 37,
    58, 38,
    58, 39,
    58, 40,
    58, 41,
    58, 42,
    58, 43,
    58, 44,
    58, 45,
    58, 46,
    58, 47,
    58, 48,
    58, 49,
    58, 50,
    58, 51,
    58, 52,
    58, 53,
    58, 54,
    58, 58,
    58, 59,
    58, 66,
    58, 70,
    58, 80,
    58, 81,
    58, 84,
    58, 86,
    59, 34,
    59, 58,
    59, 66,
    59, 88,
    66, 87,
    66, 88,
    66, 90,
    67, 13,
    67, 15,
    69, 13,
    69, 15,
    70, 13,
    70, 15,
    71, 13,
    71, 15,
    71, 66,
    71, 70,
    71, 74,
    71, 75,
    71, 77,
    71, 80,
    72, 13,
    72, 15,
    75, 15,
    76, 66,
    76, 68,
    76, 69,
    76, 70,
    76, 72,
    76, 74,
    76, 80,
    76, 82,
    76, 84,
    76, 86,
    76, 87,
    76, 88,
    76, 90,
    79, 13,
    79, 15,
    80, 13,
    80, 15,
    81, 13,
    81, 15,
    82, 15,
    83, 1,
    83, 13,
    83, 14,
    83, 15,
    83, 66,
    83, 67,
    83, 68,
    83, 69,
    83, 70,
    83, 72,
    83, 73,
    83, 76,
    83, 77,
    83, 78,
    83, 79,
    83, 80,
    83, 81,
    83, 82,
    83, 83,
    83, 84,
    83, 86,
    83, 87,
    83, 88,
    83, 89,
    83, 90,
    83, 91,
    84, 13,
    84, 15,
    84, 87,
    84, 88,
    85, 14,
    85, 74,
    85, 75,
    87, 13,
    87, 14,
    87, 15,
    87, 84,
    88, 13,
    88, 14,
    88, 15,
    88, 84,
    90, 13,
    90, 14,
    90, 15
};

/* Kerning between the respective left and right glyphs
 * 4.4 format which needs to scaled with `kern_scale`*/
static const int8_t kern_pair_values[] =
{
    -6, -9, -6, -5, -9, -3, -6, -28,
    -11, -19, -30, -9, -31, -22, -16, -34,
    -47, -31, -47, -3, -3, -3, -3, -3,
    -3, -19, -2, -11, -9, -19, -6, -6,
    -3, -6, -6, -6, -6, -6, -9, -9,
    -8, -3, -3, -2, -9, -6, -3, -3,
    -3, -3, -9, -6, -25, -3, -25, -9,
    -9, -6, -9, -9, -9, -6, -6, -2,
    -9, -3, -16, -3, -3, -3, -3, 5,
    3, -3, -3, -22, -6, -6, 11, -6,
    -6, -31, -3, -19, -16, -31, -6, -6,
    -9, -9, -3, -3, -3, -3, -3, -9,
    -25, -16, -25, -19, -6, -3, -3, -9,
    -13, -3, -3, -3, -6, -3, -13, -6,
    -6, -6, -6, -3, -3, -3, -3, -3,
    -3, -6, -3, -31, -28, -31, -22, -22,
    -19, -16, -3, -3, 6, -28, -28, -28,
    -28, -28, -28, -28, -28, -30, -24, -22,
    -3, -3, -2, -3, -22, -11, -22, -3,
    -3, -11, -9, -13, -13, 3, -13, -9,
    -13, -13, -6, -6, -9, -9, -9, -9,
    -6, -9, -6, -3, -19, -3, -3, -9,
    -3, -6, -34, -30, -34, -16, -16, -19,
    -9, -6, -9, -9, -9, -9, -9, -9,
    -27, -9, -9, -9, -9, -9, -9, -9,
    -9, -9, 6, -3, -9, -6, -25, -25,
    -25, -13, -25, -16, -3, -6, -3, -9,
    -3, -2, -2, -6, -6, -3, -3, -6,
    -6, -22, -22, -3, -3, 6, 6, 6,
    -3, -3, -6, -3, -5, -5, -5, -5,
    -5, -4, -5, -5, -4, -5, -3, -3,
    -3, -3, -3, -6, -6, -6, -6, -3,
    -6, -22, -6, -22, -6, -6, -6, -6,
    -6, -6, -6, -6, -3, -6, -6, -6,
    -6, -6, -6, -6, -6, -3, -6, -6,
    -3, -3, -3, -3, -3, -3, -13, 5,
    5, -19, -3, -19, -3, -16, -3, -16,
    -3, -19, -3, -19
};

/*Collect the kern pair's data in one place*/
static const lv_font_fmt_txt_kern_pair_t kern_pairs =
{
    .glyph_ids = kern_pair_glyph_ids,
    .values = kern_pair_values,
    .pair_cnt = 300,
    .glyph_ids_size = 0
};

/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = &kern_pairs,
    .kern_scale = 16,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t custom_font_light_14 = {
#else
lv_font_t custom_font_light_14 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 15,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if CUSTOM_FONT_LIGHT_14*/

