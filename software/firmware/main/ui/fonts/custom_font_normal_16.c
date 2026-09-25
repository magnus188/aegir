/*******************************************************************************
 * Size: 16 px
 * Bpp: 1
 * Opts: --font ../../../assets/fonts/normal.ttf --size 16 --format lvgl --bpp 1 --no-compress --output custom_font_normal_16.c --range 0x20-0x7E
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef CUSTOM_FONT_NORMAL_16
#define CUSTOM_FONT_NORMAL_16 1
#endif

#if CUSTOM_FONT_NORMAL_16

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0x20,

    /* U+0022 "\"" */
    0x99, 0x90,

    /* U+0023 "#" */
    0x9, 0x4, 0x84, 0x82, 0x47, 0xf8, 0x90, 0x91,
    0xfe, 0x24, 0x12, 0x12, 0x9, 0x0,

    /* U+0024 "$" */
    0x10, 0x21, 0xf4, 0x99, 0x12, 0x24, 0x2b, 0x12,
    0x24, 0x4c, 0x97, 0xc2, 0x4, 0x0,

    /* U+0025 "%" */
    0xf0, 0x92, 0x22, 0x44, 0x49, 0x9, 0x5f, 0xea,
    0x42, 0x48, 0x49, 0x11, 0x22, 0x24, 0x87, 0x80,

    /* U+0026 "&" */
    0x7c, 0x41, 0x20, 0x10, 0x48, 0x23, 0xfe, 0x9,
    0x4, 0x82, 0x41, 0x1f, 0x0,

    /* U+0027 "'" */
    0xe0,

    /* U+0028 "(" */
    0x3, 0x6c, 0x88, 0x88, 0x88, 0x88, 0x88, 0x42,
    0x10,

    /* U+0029 ")" */
    0xc, 0x63, 0x11, 0x11, 0x11, 0x11, 0x11, 0x24,
    0x80,

    /* U+002A "*" */
    0x10, 0x22, 0x4f, 0xf1, 0x85, 0x11, 0x0,

    /* U+002B "+" */
    0x10, 0x10, 0x10, 0xff, 0x10, 0x10, 0x10, 0x10,

    /* U+002C "," */
    0x56,

    /* U+002D "-" */
    0xf8,

    /* U+002E "." */
    0x80,

    /* U+002F "/" */
    0x1, 0x2, 0x2, 0x4, 0x4, 0x8, 0x8, 0x18,
    0x10, 0x30, 0x20, 0x60, 0x40, 0x40,

    /* U+0030 "0" */
    0x7d, 0x6, 0xc, 0x18, 0x30, 0x60, 0xc1, 0x83,
    0x5, 0xf0,

    /* U+0031 "1" */
    0x8, 0x51, 0xa2, 0x40, 0x81, 0x2, 0x4, 0x8,
    0x11, 0xf8,

    /* U+0032 "2" */
    0x7d, 0x4, 0x8, 0x10, 0x61, 0x86, 0x18, 0xc1,
    0x2, 0xf8,

    /* U+0033 "3" */
    0xfa, 0x10, 0x41, 0x4, 0xe0, 0x41, 0x6, 0x1f,
    0x80,

    /* U+0034 "4" */
    0xc, 0x18, 0x51, 0xa2, 0x48, 0xb1, 0x5f, 0x4,
    0x8, 0x10,

    /* U+0035 "5" */
    0x7e, 0x81, 0x2, 0x5, 0xc0, 0x40, 0x81, 0x2,
    0x85, 0xf0,

    /* U+0036 "6" */
    0x7d, 0x6, 0x4, 0xf, 0xd0, 0x60, 0xc1, 0x83,
    0x5, 0xf0,

    /* U+0037 "7" */
    0xfa, 0xc, 0x10, 0x60, 0x81, 0x4, 0x8, 0x30,
    0x41, 0x80,

    /* U+0038 "8" */
    0x7d, 0x6, 0xc, 0x18, 0x2f, 0xa0, 0xc1, 0x83,
    0x5, 0xf0,

    /* U+0039 "9" */
    0x7d, 0x6, 0xc, 0x18, 0x30, 0x5f, 0x81, 0x3,
    0x5, 0xf0,

    /* U+003A ":" */
    0x81,

    /* U+003B ";" */
    0x50, 0x5, 0x70,

    /* U+003C "<" */
    0x12, 0x6c, 0xc6, 0x21,

    /* U+003D "=" */
    0xff, 0x0, 0x0, 0xff,

    /* U+003E ">" */
    0x8c, 0x63, 0x36, 0xc8,

    /* U+003F "?" */
    0x7a, 0x10, 0x41, 0xc, 0x63, 0x8, 0x20, 0x2,
    0x0,

    /* U+0040 "@" */
    0x7f, 0xec, 0x2, 0x80, 0x18, 0xf1, 0x90, 0x98,
    0x9, 0x8f, 0x99, 0x9, 0x90, 0x99, 0x2, 0x8f,
    0xe8, 0x0, 0x40, 0x3, 0xfe,

    /* U+0041 "A" */
    0xc, 0x3, 0x81, 0xa0, 0x48, 0x13, 0xc, 0x42,
    0x18, 0xbe, 0x60, 0x90, 0x3c, 0x4,

    /* U+0042 "B" */
    0xfd, 0x6, 0xc, 0x18, 0x77, 0xa1, 0xc1, 0x83,
    0x7, 0xf0,

    /* U+0043 "C" */
    0x7c, 0x42, 0x82, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x82, 0xc2, 0x7c,

    /* U+0044 "D" */
    0xfc, 0x82, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
    0x81, 0x82, 0xbc,

    /* U+0045 "E" */
    0xfe, 0x8, 0x20, 0x82, 0xf8, 0x20, 0x82, 0xf,
    0xc0,

    /* U+0046 "F" */
    0xff, 0x2, 0x4, 0x8, 0x17, 0xe0, 0x40, 0x81,
    0x2, 0x0,

    /* U+0047 "G" */
    0x7e, 0x83, 0x81, 0x80, 0x80, 0x8f, 0x81, 0x81,
    0x81, 0x41, 0x3e,

    /* U+0048 "H" */
    0x81, 0x81, 0x81, 0x81, 0x81, 0xbf, 0x81, 0x81,
    0x81, 0x81, 0x81,

    /* U+0049 "I" */
    0xff, 0xe0,

    /* U+004A "J" */
    0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x81, 0x2,
    0x88, 0xf0,

    /* U+004B "K" */
    0x83, 0x86, 0x8c, 0x98, 0xb0, 0xa0, 0xb0, 0x98,
    0x8c, 0x86, 0x83,

    /* U+004C "L" */
    0x81, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x81,
    0x3, 0xf8,

    /* U+004D "M" */
    0xc0, 0xf0, 0x3e, 0x1e, 0x85, 0xa1, 0x6c, 0xd9,
    0x26, 0x49, 0x9a, 0x62, 0x18, 0x84,

    /* U+004E "N" */
    0x81, 0xc1, 0xe1, 0xa1, 0xb1, 0x99, 0x89, 0x8d,
    0x85, 0x82, 0x83,

    /* U+004F "O" */
    0x7e, 0x40, 0xa0, 0x30, 0x18, 0xc, 0x6, 0x3,
    0x1, 0x80, 0xa0, 0x4f, 0xc0,

    /* U+0050 "P" */
    0xfd, 0x6, 0xc, 0x18, 0x37, 0xa0, 0x40, 0x81,
    0x2, 0x0,

    /* U+0051 "Q" */
    0x7e, 0x20, 0x48, 0xa, 0x2, 0x80, 0xa0, 0x28,
    0xa, 0x2, 0x80, 0x90, 0x23, 0xf0, 0x6, 0x0,
    0xc0,

    /* U+0052 "R" */
    0xfc, 0x82, 0x82, 0x82, 0x82, 0xbc, 0x88, 0x8c,
    0x84, 0x86, 0x83,

    /* U+0053 "S" */
    0x7e, 0x83, 0x81, 0x80, 0x80, 0x7e, 0x1, 0x1,
    0x81, 0xc1, 0x7e,

    /* U+0054 "T" */
    0xff, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10,

    /* U+0055 "U" */
    0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
    0x81, 0x42, 0x3c,

    /* U+0056 "V" */
    0xc0, 0xa0, 0xd0, 0x4c, 0x22, 0x31, 0x10, 0xc8,
    0x6c, 0x14, 0xa, 0x7, 0x0,

    /* U+0057 "W" */
    0x86, 0x28, 0xe2, 0x8a, 0x2c, 0xa2, 0xca, 0x64,
    0xb4, 0x51, 0x45, 0x14, 0x71, 0x46, 0x14, 0x21,
    0x80,

    /* U+0058 "X" */
    0xc2, 0x46, 0x64, 0x2c, 0x30, 0x18, 0x38, 0x2c,
    0x44, 0xc6, 0x82,

    /* U+0059 "Y" */
    0x41, 0x31, 0x88, 0x86, 0xc1, 0x40, 0x40, 0x20,
    0x10, 0x8, 0x4, 0x2, 0x0,

    /* U+005A "Z" */
    0xff, 0x3, 0x6, 0x4, 0x8, 0x18, 0x30, 0x20,
    0x60, 0xc0, 0xbf,

    /* U+005B "[" */
    0xf2, 0x49, 0x24, 0x92, 0x49, 0xc0,

    /* U+005C "\\" */
    0x40, 0x40, 0x60, 0x20, 0x30, 0x10, 0x18, 0x8,
    0x8, 0x4, 0x4, 0x2, 0x2, 0x1,

    /* U+005D "]" */
    0xe4, 0x92, 0x49, 0x24, 0x93, 0xc0,

    /* U+005E "^" */
    0x18, 0x3c, 0x64, 0xc2, 0x0,

    /* U+005F "_" */
    0xff,

    /* U+0060 "`" */
    0x99, 0x80,

    /* U+0061 "a" */
    0x78, 0x10, 0x5d, 0x86, 0x18, 0xdd,

    /* U+0062 "b" */
    0x81, 0x2, 0x5, 0xe8, 0x30, 0x60, 0xc1, 0x83,
    0x86, 0xf0,

    /* U+0063 "c" */
    0x7a, 0x18, 0x20, 0x82, 0x8, 0x5e,

    /* U+0064 "d" */
    0x2, 0x4, 0xb, 0xd8, 0x30, 0x60, 0xc1, 0x83,
    0xd, 0xe8,

    /* U+0065 "e" */
    0x7a, 0x18, 0x6f, 0x82, 0x8, 0x5e,

    /* U+0066 "f" */
    0x3a, 0x11, 0xb4, 0x21, 0x8, 0x42, 0x10,

    /* U+0067 "g" */
    0x7b, 0x6, 0xc, 0x18, 0x30, 0x61, 0xbd, 0x3,
    0x5, 0xf0,

    /* U+0068 "h" */
    0x82, 0x8, 0x2e, 0x86, 0x18, 0x61, 0x86, 0x18,
    0x40,

    /* U+0069 "i" */
    0x9f, 0xe0,

    /* U+006A "j" */
    0x10, 0x1, 0x11, 0x11, 0x11, 0x11, 0x1e,

    /* U+006B "k" */
    0x81, 0x2, 0x4, 0x69, 0x92, 0x28, 0x58, 0x99,
    0x12, 0x10,

    /* U+006C "l" */
    0xff, 0xe0,

    /* U+006D "m" */
    0xbb, 0xa1, 0x18, 0x46, 0x11, 0x84, 0x61, 0x18,
    0x46, 0x11,

    /* U+006E "n" */
    0xba, 0x18, 0x61, 0x86, 0x18, 0x61,

    /* U+006F "o" */
    0x7d, 0x6, 0xc, 0x18, 0x30, 0x60, 0xbe,

    /* U+0070 "p" */
    0xbd, 0x6, 0xc, 0x18, 0x30, 0x70, 0xde, 0x81,
    0x2, 0x0,

    /* U+0071 "q" */
    0x7b, 0x6, 0xc, 0x18, 0x30, 0x61, 0xbd, 0x2,
    0x4, 0x8,

    /* U+0072 "r" */
    0xbb, 0x18, 0x60, 0x82, 0x8, 0x20,

    /* U+0073 "s" */
    0x7a, 0x18, 0x20, 0x78, 0x18, 0x5e,

    /* U+0074 "t" */
    0x42, 0x11, 0xb4, 0x21, 0x8, 0x42, 0xe,

    /* U+0075 "u" */
    0x86, 0x18, 0x61, 0x86, 0x18, 0x5d,

    /* U+0076 "v" */
    0xc2, 0x8d, 0x13, 0x22, 0xc5, 0xe, 0x8,

    /* U+0077 "w" */
    0x46, 0x24, 0x62, 0x4f, 0x26, 0x96, 0x29, 0x42,
    0x94, 0x29, 0xc3, 0xc,

    /* U+0078 "x" */
    0x85, 0x26, 0x88, 0x31, 0xe4, 0xa1,

    /* U+0079 "y" */
    0x42, 0x8d, 0x11, 0x22, 0xc7, 0x4, 0x8, 0x10,
    0x20, 0x80,

    /* U+007A "z" */
    0xfc, 0x31, 0x84, 0x21, 0x8c, 0x2f,

    /* U+007B "{" */
    0x2, 0x64, 0x44, 0x44, 0x88, 0x44, 0x44, 0x46,
    0x30,

    /* U+007C "|" */
    0xff, 0xfc,

    /* U+007D "}" */
    0x4, 0x62, 0x22, 0x22, 0x11, 0x22, 0x22, 0x26,
    0xc0,

    /* U+007E "~" */
    0x73, 0x52, 0x8c
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 58, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 68, .box_w = 1, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 3, .adv_w = 115, .box_w = 4, .box_h = 3, .ofs_x = 1, .ofs_y = 8},
    {.bitmap_index = 5, .adv_w = 154, .box_w = 9, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 19, .adv_w = 154, .box_w = 7, .box_h = 15, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 33, .adv_w = 213, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 49, .adv_w = 151, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 62, .adv_w = 67, .box_w = 1, .box_h = 3, .ofs_x = 1, .ofs_y = 8},
    {.bitmap_index = 63, .adv_w = 93, .box_w = 4, .box_h = 17, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 72, .adv_w = 93, .box_w = 4, .box_h = 17, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 81, .adv_w = 145, .box_w = 7, .box_h = 7, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 88, .adv_w = 154, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 96, .adv_w = 76, .box_w = 2, .box_h = 4, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 97, .adv_w = 120, .box_w = 5, .box_h = 1, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 98, .adv_w = 68, .box_w = 1, .box_h = 1, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 99, .adv_w = 133, .box_w = 8, .box_h = 14, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 113, .adv_w = 154, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 123, .adv_w = 154, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 133, .adv_w = 154, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 143, .adv_w = 154, .box_w = 6, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 152, .adv_w = 154, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 162, .adv_w = 154, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 172, .adv_w = 154, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 182, .adv_w = 154, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 192, .adv_w = 154, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 202, .adv_w = 154, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 212, .adv_w = 68, .box_w = 1, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 213, .adv_w = 73, .box_w = 2, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 216, .adv_w = 154, .box_w = 4, .box_h = 8, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 220, .adv_w = 154, .box_w = 8, .box_h = 4, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 224, .adv_w = 154, .box_w = 4, .box_h = 8, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 228, .adv_w = 125, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 237, .adv_w = 236, .box_w = 12, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 258, .adv_w = 167, .box_w = 10, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 272, .adv_w = 167, .box_w = 7, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 282, .adv_w = 153, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 293, .adv_w = 170, .box_w = 8, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 304, .adv_w = 154, .box_w = 6, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 313, .adv_w = 147, .box_w = 7, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 323, .adv_w = 161, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 334, .adv_w = 154, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 345, .adv_w = 74, .box_w = 1, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 347, .adv_w = 133, .box_w = 7, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 357, .adv_w = 158, .box_w = 8, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 368, .adv_w = 137, .box_w = 7, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 378, .adv_w = 216, .box_w = 10, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 392, .adv_w = 177, .box_w = 8, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 403, .adv_w = 179, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 416, .adv_w = 158, .box_w = 7, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 426, .adv_w = 179, .box_w = 10, .box_h = 13, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 443, .adv_w = 173, .box_w = 8, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 454, .adv_w = 166, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 465, .adv_w = 134, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 476, .adv_w = 170, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 487, .adv_w = 145, .box_w = 9, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 500, .adv_w = 209, .box_w = 12, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 517, .adv_w = 149, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 528, .adv_w = 144, .box_w = 9, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 541, .adv_w = 157, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 552, .adv_w = 93, .box_w = 3, .box_h = 14, .ofs_x = 2, .ofs_y = -3},
    {.bitmap_index = 558, .adv_w = 133, .box_w = 8, .box_h = 14, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 572, .adv_w = 93, .box_w = 3, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 578, .adv_w = 154, .box_w = 8, .box_h = 5, .ofs_x = 1, .ofs_y = 6},
    {.bitmap_index = 583, .adv_w = 128, .box_w = 8, .box_h = 1, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 584, .adv_w = 124, .box_w = 3, .box_h = 3, .ofs_x = 2, .ofs_y = 9},
    {.bitmap_index = 586, .adv_w = 134, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 592, .adv_w = 140, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 602, .adv_w = 126, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 608, .adv_w = 140, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 618, .adv_w = 134, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 624, .adv_w = 94, .box_w = 5, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 631, .adv_w = 140, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 641, .adv_w = 136, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 650, .adv_w = 56, .box_w = 1, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 652, .adv_w = 55, .box_w = 4, .box_h = 14, .ofs_x = -2, .ofs_y = -3},
    {.bitmap_index = 659, .adv_w = 136, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 669, .adv_w = 54, .box_w = 1, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 671, .adv_w = 194, .box_w = 10, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 681, .adv_w = 136, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 687, .adv_w = 138, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 694, .adv_w = 140, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 704, .adv_w = 140, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 714, .adv_w = 120, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 720, .adv_w = 125, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 726, .adv_w = 99, .box_w = 5, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 733, .adv_w = 136, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 739, .adv_w = 118, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 746, .adv_w = 191, .box_w = 12, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 758, .adv_w = 126, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 764, .adv_w = 119, .box_w = 7, .box_h = 11, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 774, .adv_w = 122, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 780, .adv_w = 93, .box_w = 4, .box_h = 17, .ofs_x = 2, .ofs_y = -3},
    {.bitmap_index = 789, .adv_w = 67, .box_w = 1, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 791, .adv_w = 93, .box_w = 4, .box_h = 17, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 800, .adv_w = 154, .box_w = 8, .box_h = 3, .ofs_x = 1, .ofs_y = 3}
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
    -7, -11, -7, -5, -11, -4, -7, -32,
    -13, -22, -34, -11, -36, -25, -18, -39,
    -54, -36, -54, -4, -4, -4, -4, -4,
    -4, -22, -2, -13, -11, -22, -7, -7,
    -4, -7, -7, -7, -7, -7, -11, -11,
    -9, -4, -4, -2, -11, -7, -4, -4,
    -4, -4, -11, -7, -29, -4, -29, -11,
    -11, -7, -11, -11, -11, -7, -7, -2,
    -11, -4, -18, -4, -4, -4, -4, 6,
    4, -4, -4, -25, -7, -7, 12, -7,
    -7, -36, -4, -22, -18, -36, -7, -7,
    -11, -11, -4, -4, -4, -4, -4, -11,
    -29, -18, -29, -22, -7, -4, -4, -11,
    -14, -4, -4, -4, -7, -4, -14, -7,
    -7, -7, -7, -4, -4, -4, -4, -4,
    -4, -7, -4, -36, -32, -36, -25, -25,
    -22, -18, -4, -4, 7, -32, -32, -32,
    -32, -32, -32, -32, -32, -34, -27, -25,
    -4, -4, -2, -4, -25, -13, -25, -4,
    -4, -13, -11, -14, -14, 4, -14, -11,
    -14, -14, -7, -7, -11, -11, -11, -11,
    -7, -11, -7, -4, -22, -4, -4, -11,
    -4, -7, -39, -34, -39, -18, -18, -22,
    -11, -7, -11, -11, -11, -11, -11, -11,
    -31, -11, -11, -11, -11, -11, -11, -11,
    -11, -11, 7, -4, -11, -7, -29, -29,
    -29, -14, -29, -18, -4, -7, -4, -11,
    -4, -2, -2, -7, -7, -4, -4, -7,
    -7, -25, -25, -4, -4, 7, 7, 7,
    -4, -4, -7, -4, -6, -6, -6, -6,
    -6, -5, -6, -6, -5, -6, -3, -3,
    -3, -4, -4, -7, -7, -7, -7, -4,
    -7, -25, -7, -25, -7, -7, -7, -7,
    -7, -7, -7, -7, -4, -7, -7, -7,
    -7, -7, -7, -7, -7, -4, -7, -7,
    -4, -4, -4, -4, -4, -4, -14, 5,
    5, -22, -4, -22, -4, -18, -4, -18,
    -4, -22, -4, -22
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
const lv_font_t custom_font_normal_16 = {
#else
lv_font_t custom_font_normal_16 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 17,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -2,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if CUSTOM_FONT_NORMAL_16*/

