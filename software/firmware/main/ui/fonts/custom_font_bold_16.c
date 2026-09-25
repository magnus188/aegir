/*******************************************************************************
 * Size: 16 px
 * Bpp: 1
 * Opts: --font ../../../assets/fonts/bold.ttf --size 16 --format lvgl --bpp 1 --no-compress --output custom_font_bold_16.c --range 0x20-0x7E
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef CUSTOM_FONT_BOLD_16
#define CUSTOM_FONT_BOLD_16 1
#endif

#if CUSTOM_FONT_BOLD_16

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0xfc, 0xc,

    /* U+0022 "\"" */
    0xde, 0xf6,

    /* U+0023 "#" */
    0xd, 0x82, 0x60, 0x98, 0x64, 0x7f, 0xc4, 0xc3,
    0x23, 0xfe, 0x26, 0x9, 0x86, 0x41, 0xb0,

    /* U+0024 "$" */
    0x10, 0x10, 0x7e, 0xd3, 0xd0, 0xd0, 0xd0, 0x76,
    0x13, 0x13, 0x13, 0x53, 0xfe, 0x10,

    /* U+0025 "%" */
    0xf0, 0x89, 0x8, 0x91, 0x9, 0x30, 0x92, 0xff,
    0x49, 0x4, 0x90, 0x89, 0x8, 0x91, 0x9, 0x10,
    0xf0,

    /* U+0026 "&" */
    0x3e, 0x61, 0x30, 0x18, 0x6c, 0x33, 0xff, 0xd,
    0x86, 0xc3, 0x61, 0x9f, 0x0,

    /* U+0027 "'" */
    0xfc,

    /* U+0028 "(" */
    0x0, 0xcc, 0xcc, 0x63, 0x18, 0xc6, 0x31, 0x8c,
    0x61, 0x86, 0x10,

    /* U+0029 ")" */
    0x6, 0x18, 0x61, 0x8c, 0x63, 0x18, 0xc6, 0x31,
    0x8c, 0xcc, 0x40,

    /* U+002A "*" */
    0x18, 0x18, 0xdb, 0xff, 0x3c, 0x3c, 0x66, 0x0,

    /* U+002B "+" */
    0x18, 0x18, 0x18, 0xff, 0x18, 0x18, 0x18, 0x18,

    /* U+002C "," */
    0x6d, 0xb4,

    /* U+002D "-" */
    0xf0,

    /* U+002E "." */
    0xc0,

    /* U+002F "/" */
    0x3, 0x2, 0x6, 0x4, 0xc, 0xc, 0x18, 0x18,
    0x10, 0x30, 0x20, 0x60, 0x60, 0xc0,

    /* U+0030 "0" */
    0x3d, 0x8f, 0x1e, 0x3c, 0x78, 0xf1, 0xe3, 0xc7,
    0x8d, 0xe0,

    /* U+0031 "1" */
    0x18, 0x58, 0x58, 0xd8, 0x18, 0x18, 0x18, 0x18,
    0x18, 0x18, 0x7f,

    /* U+0032 "2" */
    0x7d, 0x8c, 0x18, 0x30, 0xe3, 0x8e, 0x38, 0xe1,
    0x83, 0x78,

    /* U+0033 "3" */
    0xfd, 0x8c, 0x18, 0x30, 0x67, 0x81, 0x83, 0x7,
    0xf, 0xe0,

    /* U+0034 "4" */
    0x1c, 0x1c, 0x3c, 0x2c, 0x6c, 0x4c, 0xcc, 0xbf,
    0xc, 0xc, 0xc,

    /* U+0035 "5" */
    0xff, 0x83, 0x6, 0xd, 0xc0, 0xc1, 0x83, 0x7,
    0xf, 0xf0,

    /* U+0036 "6" */
    0x7d, 0x8f, 0x6, 0xf, 0xd8, 0xf1, 0xe3, 0xc7,
    0x85, 0xe0,

    /* U+0037 "7" */
    0xf6, 0xc, 0x10, 0x60, 0x83, 0x6, 0x18, 0x30,
    0x41, 0x80,

    /* U+0038 "8" */
    0x3d, 0xf, 0x1e, 0x3c, 0x6f, 0xb1, 0xe3, 0xc7,
    0x8d, 0xe0,

    /* U+0039 "9" */
    0x3d, 0x8f, 0x1e, 0x3c, 0x6f, 0xc1, 0x83, 0x7,
    0x8d, 0xf0,

    /* U+003A ":" */
    0xc0, 0x3,

    /* U+003B ";" */
    0xf0, 0x3f, 0xf0,

    /* U+003C "<" */
    0x10, 0xcc, 0xc6, 0x18, 0x62,

    /* U+003D "=" */
    0xff, 0x0, 0x0, 0x0, 0xff,

    /* U+003E ">" */
    0x47, 0x1c, 0x73, 0xbb, 0x88,

    /* U+003F "?" */
    0x7d, 0x8c, 0x18, 0x30, 0xe3, 0x8e, 0x18, 0x0,
    0x0, 0xc0,

    /* U+0040 "@" */
    0x7f, 0xec, 0x3, 0xc0, 0x1d, 0xf9, 0xc8, 0x9c,
    0x9, 0xcf, 0x9d, 0x89, 0xd8, 0x1d, 0x83, 0xcf,
    0xec, 0x0, 0x40, 0x3, 0xfe,

    /* U+0041 "A" */
    0xe, 0x3, 0x81, 0xe0, 0x6c, 0x13, 0xc, 0xc3,
    0x19, 0xbe, 0x60, 0x98, 0x3c, 0xc,

    /* U+0042 "B" */
    0xfe, 0xc3, 0xc3, 0xc3, 0xc7, 0xde, 0xc7, 0xc3,
    0xc3, 0xc3, 0xfe,

    /* U+0043 "C" */
    0x3f, 0x21, 0xb0, 0x18, 0xc, 0x6, 0x3, 0x1,
    0x80, 0xc0, 0x21, 0x8f, 0x80,

    /* U+0044 "D" */
    0xfe, 0xc2, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc2, 0xde,

    /* U+0045 "E" */
    0xff, 0x83, 0x6, 0xc, 0x1b, 0xf0, 0x60, 0xc1,
    0x83, 0xf8,

    /* U+0046 "F" */
    0xff, 0x83, 0x6, 0xc, 0x1b, 0xf0, 0x60, 0xc1,
    0x83, 0x0,

    /* U+0047 "G" */
    0x3e, 0x83, 0xc0, 0xc0, 0xc0, 0xcf, 0xc3, 0xc3,
    0xc3, 0x41, 0x7c,

    /* U+0048 "H" */
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xdf, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3,

    /* U+0049 "I" */
    0xff, 0xff, 0xfc,

    /* U+004A "J" */
    0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3,
    0x3, 0x42, 0x7c,

    /* U+004B "K" */
    0xc3, 0xe1, 0xb1, 0x99, 0x8d, 0x86, 0xc3, 0x61,
    0x98, 0xc6, 0x63, 0xb0, 0xe0,

    /* U+004C "L" */
    0xc1, 0x83, 0x6, 0xc, 0x18, 0x30, 0x60, 0xc1,
    0x83, 0xf8,

    /* U+004D "M" */
    0xe0, 0xfe, 0x3f, 0xc7, 0xf8, 0xfd, 0xb7, 0xb6,
    0xf6, 0xde, 0x73, 0xcc, 0x79, 0x8f, 0x39, 0x80,

    /* U+004E "N" */
    0xe3, 0xe3, 0xf3, 0xf3, 0xdb, 0xdb, 0xcd, 0xcd,
    0xc6, 0xc6, 0xc7,

    /* U+004F "O" */
    0x3f, 0x60, 0xb0, 0x78, 0x3c, 0x1e, 0xf, 0x7,
    0x83, 0xc1, 0xa0, 0xdf, 0x80,

    /* U+0050 "P" */
    0xfe, 0xc3, 0xc3, 0xc3, 0xc3, 0xde, 0xc0, 0xc0,
    0xc0, 0xc0, 0xc0,

    /* U+0051 "Q" */
    0x3f, 0x18, 0x23, 0x6, 0x60, 0xcc, 0x19, 0x83,
    0x30, 0x66, 0xc, 0xc1, 0x88, 0x31, 0xf8, 0x1,
    0x80, 0x1e,

    /* U+0052 "R" */
    0xfe, 0x61, 0xb0, 0xd8, 0x6c, 0x36, 0xf3, 0x71,
    0x98, 0xc6, 0x63, 0xb0, 0xc0,

    /* U+0053 "S" */
    0x3f, 0x60, 0xf0, 0x58, 0xc, 0x3, 0xf8, 0x6,
    0x3, 0x1, 0xe0, 0xdf, 0x80,

    /* U+0054 "T" */
    0xff, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
    0x18, 0x18, 0x18,

    /* U+0055 "U" */
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0x42, 0x7e,

    /* U+0056 "V" */
    0xc1, 0xe1, 0x98, 0xcc, 0x66, 0x21, 0xb0, 0xd8,
    0x68, 0x1c, 0xc, 0x6, 0x0,

    /* U+0057 "W" */
    0x63, 0xd, 0x8c, 0x66, 0x39, 0x99, 0xe6, 0x37,
    0x90, 0xde, 0xc3, 0x4d, 0xd, 0x34, 0x1c, 0xc0,
    0x63, 0x1, 0x86, 0x0,

    /* U+0058 "X" */
    0x63, 0xb1, 0x8d, 0x87, 0x41, 0x80, 0xe0, 0x70,
    0x6c, 0x36, 0x31, 0x98, 0xe0,

    /* U+0059 "Y" */
    0x61, 0x98, 0x63, 0x30, 0xec, 0x1a, 0x3, 0x0,
    0xc0, 0x30, 0xc, 0x3, 0x0, 0xc0,

    /* U+005A "Z" */
    0xff, 0x6, 0xe, 0xc, 0x1c, 0x18, 0x30, 0x30,
    0x60, 0xc0, 0xdf,

    /* U+005B "[" */
    0xfc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcf,

    /* U+005C "\\" */
    0x40, 0x60, 0x60, 0x30, 0x30, 0x10, 0x18, 0x8,
    0xc, 0xc, 0x6, 0x6, 0x2, 0x3,

    /* U+005D "]" */
    0xf8, 0xc6, 0x31, 0x8c, 0x63, 0x18, 0xc6, 0x31,
    0xfc,

    /* U+005E "^" */
    0x10, 0x71, 0xb6, 0x30, 0x20,

    /* U+005F "_" */
    0xff,

    /* U+0060 "`" */
    0xc6, 0x30,

    /* U+0061 "a" */
    0x7c, 0x8c, 0x1b, 0xbc, 0x78, 0xf1, 0xbf,

    /* U+0062 "b" */
    0xc1, 0x83, 0x6, 0xec, 0x78, 0xf1, 0xe3, 0xc7,
    0xcf, 0xf0,

    /* U+0063 "c" */
    0x7d, 0x8f, 0x6, 0xc, 0x18, 0x31, 0xbe,

    /* U+0064 "d" */
    0x6, 0xc, 0x1b, 0xbc, 0x78, 0xf1, 0xe3, 0xc7,
    0x9d, 0xf8,

    /* U+0065 "e" */
    0x7d, 0x8f, 0x1e, 0xfc, 0x18, 0x31, 0xbe,

    /* U+0066 "f" */
    0x1e, 0x60, 0xc7, 0xb3, 0x6, 0xc, 0x18, 0x30,
    0x60, 0xc0,

    /* U+0067 "g" */
    0x77, 0x8f, 0x1e, 0x3c, 0x78, 0xf3, 0xbf, 0x6,
    0x8d, 0xf0,

    /* U+0068 "h" */
    0xc1, 0x83, 0x6, 0xec, 0x78, 0xf1, 0xe3, 0xc7,
    0x8f, 0x18,

    /* U+0069 "i" */
    0xc3, 0xff, 0xfc,

    /* U+006A "j" */
    0x30, 0x3, 0x33, 0x33, 0x33, 0x33, 0x3e,

    /* U+006B "k" */
    0xc0, 0xc0, 0xc0, 0xc6, 0xcc, 0xd8, 0xf0, 0xd8,
    0xdc, 0xcc, 0xc6,

    /* U+006C "l" */
    0xff, 0xff, 0xfc,

    /* U+006D "m" */
    0xdf, 0xd9, 0x8f, 0x31, 0xe6, 0x3c, 0xc7, 0x98,
    0xf3, 0x1e, 0x63,

    /* U+006E "n" */
    0xdd, 0x8f, 0x1e, 0x3c, 0x78, 0xf1, 0xe3,

    /* U+006F "o" */
    0x3d, 0x8f, 0x1e, 0x3c, 0x78, 0xf1, 0xbc,

    /* U+0070 "p" */
    0xdd, 0x8f, 0x1e, 0x3c, 0x78, 0xf9, 0xfe, 0xc1,
    0x83, 0x0,

    /* U+0071 "q" */
    0x77, 0x8f, 0x1e, 0x3c, 0x78, 0xf3, 0xbf, 0x6,
    0xc, 0x18,

    /* U+0072 "r" */
    0xfb, 0xbc, 0xf0, 0xc3, 0xc, 0x30,

    /* U+0073 "s" */
    0x3d, 0xb, 0x3, 0xe0, 0x60, 0xf0, 0xfc,

    /* U+0074 "t" */
    0x30, 0x60, 0xc7, 0xb3, 0x6, 0xc, 0x18, 0x30,
    0x60, 0x78,

    /* U+0075 "u" */
    0xc7, 0x8f, 0x1e, 0x3c, 0x78, 0xf1, 0xbb,

    /* U+0076 "v" */
    0xc6, 0xcd, 0x9b, 0x23, 0xc6, 0x8c, 0x8,

    /* U+0077 "w" */
    0x47, 0x36, 0x73, 0x6f, 0x26, 0xd6, 0x2d, 0xa3,
    0x9a, 0x31, 0x83, 0x8,

    /* U+0078 "x" */
    0xcd, 0xd9, 0xa1, 0x83, 0xf, 0x3b, 0x66,

    /* U+0079 "y" */
    0xc6, 0xcd, 0x9b, 0x63, 0xc6, 0x8c, 0xc, 0x10,
    0x60, 0xc0,

    /* U+007A "z" */
    0xfe, 0x18, 0x70, 0xc3, 0xc, 0x18, 0x6f,

    /* U+007B "{" */
    0x1, 0x9c, 0xc6, 0x31, 0x8c, 0xc6, 0x18, 0xc6,
    0x31, 0x8e, 0x10,

    /* U+007C "|" */
    0xff, 0xff, 0xff, 0xf0,

    /* U+007D "}" */
    0x3, 0x1c, 0x63, 0x18, 0xc6, 0x18, 0xcc, 0x63,
    0x18, 0xce, 0x40,

    /* U+007E "~" */
    0x39, 0xb7, 0x91, 0x80
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 58, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 70, .box_w = 2, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 4, .adv_w = 124, .box_w = 5, .box_h = 3, .ofs_x = 1, .ofs_y = 8},
    {.bitmap_index = 6, .adv_w = 147, .box_w = 10, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 21, .adv_w = 147, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 35, .adv_w = 225, .box_w = 12, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 52, .adv_w = 161, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 65, .adv_w = 70, .box_w = 2, .box_h = 3, .ofs_x = 1, .ofs_y = 8},
    {.bitmap_index = 66, .adv_w = 95, .box_w = 5, .box_h = 17, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 77, .adv_w = 95, .box_w = 5, .box_h = 17, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 88, .adv_w = 146, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 96, .adv_w = 147, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 104, .adv_w = 70, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 106, .adv_w = 117, .box_w = 4, .box_h = 1, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 107, .adv_w = 70, .box_w = 2, .box_h = 1, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 108, .adv_w = 134, .box_w = 8, .box_h = 14, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 122, .adv_w = 147, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 132, .adv_w = 147, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 143, .adv_w = 147, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 153, .adv_w = 147, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 163, .adv_w = 147, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 174, .adv_w = 147, .box_w = 7, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 184, .adv_w = 147, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 194, .adv_w = 147, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 204, .adv_w = 147, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 214, .adv_w = 147, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 224, .adv_w = 70, .box_w = 2, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 226, .adv_w = 70, .box_w = 2, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 229, .adv_w = 147, .box_w = 5, .box_h = 8, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 234, .adv_w = 147, .box_w = 8, .box_h = 5, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 239, .adv_w = 147, .box_w = 5, .box_h = 8, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 244, .adv_w = 124, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 254, .adv_w = 236, .box_w = 12, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 275, .adv_w = 168, .box_w = 10, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 289, .adv_w = 172, .box_w = 8, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 300, .adv_w = 159, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 313, .adv_w = 172, .box_w = 8, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 324, .adv_w = 156, .box_w = 7, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 334, .adv_w = 149, .box_w = 7, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 344, .adv_w = 166, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 355, .adv_w = 179, .box_w = 8, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 366, .adv_w = 80, .box_w = 2, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 369, .adv_w = 138, .box_w = 8, .box_h = 11, .ofs_x = -1, .ofs_y = 0},
    {.bitmap_index = 380, .adv_w = 166, .box_w = 9, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 393, .adv_w = 140, .box_w = 7, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 403, .adv_w = 219, .box_w = 11, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 419, .adv_w = 179, .box_w = 8, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 430, .adv_w = 180, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 443, .adv_w = 163, .box_w = 8, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 454, .adv_w = 183, .box_w = 11, .box_h = 13, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 472, .adv_w = 177, .box_w = 9, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 485, .adv_w = 173, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 498, .adv_w = 136, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 509, .adv_w = 172, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 520, .adv_w = 142, .box_w = 9, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 533, .adv_w = 225, .box_w = 14, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 553, .adv_w = 148, .box_w = 9, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 566, .adv_w = 145, .box_w = 10, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 580, .adv_w = 157, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 591, .adv_w = 97, .box_w = 4, .box_h = 14, .ofs_x = 2, .ofs_y = -3},
    {.bitmap_index = 598, .adv_w = 133, .box_w = 8, .box_h = 14, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 612, .adv_w = 97, .box_w = 5, .box_h = 14, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 621, .adv_w = 147, .box_w = 7, .box_h = 5, .ofs_x = 1, .ofs_y = 6},
    {.bitmap_index = 626, .adv_w = 128, .box_w = 8, .box_h = 1, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 627, .adv_w = 111, .box_w = 4, .box_h = 3, .ofs_x = 1, .ofs_y = 9},
    {.bitmap_index = 629, .adv_w = 138, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 636, .adv_w = 140, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 646, .adv_w = 127, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 653, .adv_w = 140, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 663, .adv_w = 138, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 670, .adv_w = 107, .box_w = 7, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 680, .adv_w = 140, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 690, .adv_w = 135, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 700, .adv_w = 58, .box_w = 2, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 703, .adv_w = 57, .box_w = 4, .box_h = 14, .ofs_x = -1, .ofs_y = -3},
    {.bitmap_index = 710, .adv_w = 141, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 721, .adv_w = 58, .box_w = 2, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 724, .adv_w = 198, .box_w = 11, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 735, .adv_w = 135, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 742, .adv_w = 138, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 749, .adv_w = 140, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 759, .adv_w = 140, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 769, .adv_w = 121, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 775, .adv_w = 124, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 782, .adv_w = 112, .box_w = 7, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 792, .adv_w = 135, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 799, .adv_w = 124, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 806, .adv_w = 199, .box_w = 12, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 818, .adv_w = 131, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 825, .adv_w = 124, .box_w = 7, .box_h = 11, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 835, .adv_w = 127, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 842, .adv_w = 95, .box_w = 5, .box_h = 17, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 853, .adv_w = 70, .box_w = 2, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 857, .adv_w = 95, .box_w = 5, .box_h = 17, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 868, .adv_w = 147, .box_w = 9, .box_h = 3, .ofs_x = 0, .ofs_y = 3}
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
    34, 51,
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
    38, 37,
    38, 58,
    39, 13,
    39, 14,
    39, 15,
    39, 34,
    39, 43,
    39, 58,
    40, 42,
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
    44, 53,
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
    46, 38,
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
    51, 36,
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
    52, 55,
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
    87, 66,
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
    -3, -4, -22, -2, -13, -11, -22, -7,
    -7, -4, -7, -7, -7, -7, -7, -11,
    -11, -9, -4, -4, -2, -11, -7, -4,
    -4, -4, -4, -11, -6, -7, -29, -4,
    -29, -11, -11, -7, -2, -11, -11, -11,
    -7, -7, -2, -11, -4, -18, -4, -4,
    -4, -4, 9, 6, 4, -4, -4, -25,
    -7, -7, 12, -7, -7, -36, -4, -22,
    -18, -36, -7, -7, -6, -11, -11, -4,
    -4, -4, -4, -4, -11, -29, -18, -29,
    -22, -7, -4, -4, -11, -14, 3, -4,
    -4, -4, -7, -4, -14, -7, -7, -7,
    -7, -4, -4, -4, -4, -4, -4, 6,
    -7, -4, -36, -32, -36, -25, -25, -22,
    -18, -4, -4, 7, -32, -32, -32, -32,
    -32, -32, -32, -32, -34, -27, -25, -4,
    -4, -2, -4, -25, -13, -25, -4, -4,
    -13, -11, -14, -14, 4, -14, -11, -14,
    -14, -7, -7, -11, -11, -11, -11, -7,
    -11, -7, -4, -22, -4, -4, -11, -4,
    -7, -39, -34, -39, -18, -18, -22, -11,
    -7, -11, -11, -11, -11, -11, -11, -31,
    -11, -11, -11, -11, -11, -11, -11, -11,
    -11, 7, -4, -11, -7, -29, -29, -29,
    -14, -29, -18, -4, -7, -4, -11, -4,
    -2, -2, -7, -7, -4, -4, -7, -7,
    -25, -25, -4, -4, 7, 7, 7, -4,
    -4, -7, -4, -6, -6, -6, -6, -6,
    -5, -6, -6, -5, -6, -3, -3, -3,
    -4, -4, -7, -7, -7, -7, -4, -7,
    -25, -7, -25, -7, -7, -7, -7, -7,
    -7, -7, -7, -4, -7, -7, -7, -7,
    -7, -7, -7, -7, -4, -7, -7, -4,
    -4, -4, -4, -4, -4, -14, 5, 5,
    -22, -4, -22, -12, -4, -18, -4, -18,
    -4, -22, -4, -22
};

/*Collect the kern pair's data in one place*/
static const lv_font_fmt_txt_kern_pair_t kern_pairs =
{
    .glyph_ids = kern_pair_glyph_ids,
    .values = kern_pair_values,
    .pair_cnt = 308,
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
const lv_font_t custom_font_bold_16 = {
#else
lv_font_t custom_font_bold_16 = {
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



#endif /*#if CUSTOM_FONT_BOLD_16*/

