/*******************************************************************************
 * Size: 20 px
 * Bpp: 1
 * Opts: --font ../../../assets/fonts/normal.ttf --size 20 --format lvgl --bpp 1 --no-compress --output custom_font_normal_20.c --range 0x20-0x7E
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef CUSTOM_FONT_NORMAL_20
#define CUSTOM_FONT_NORMAL_20 1
#endif

#if CUSTOM_FONT_NORMAL_20

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0xff, 0xf0, 0x30,

    /* U+0022 "\"" */
    0xde, 0xf7, 0xb0,

    /* U+0023 "#" */
    0x4, 0x41, 0x98, 0x32, 0x4, 0x40, 0x88, 0xff,
    0xc4, 0x40, 0x88, 0x11, 0x1f, 0xf8, 0x88, 0x11,
    0x2, 0x20, 0xcc, 0x19, 0x0,

    /* U+0024 "$" */
    0x8, 0x4, 0x1f, 0xd9, 0x3c, 0x86, 0x43, 0x21,
    0x90, 0x6b, 0x4, 0xc2, 0x61, 0x30, 0x9a, 0x4f,
    0x26, 0xfe, 0x8, 0x4, 0x0,

    /* U+0025 "%" */
    0x70, 0x22, 0x21, 0x88, 0x84, 0x22, 0x20, 0x88,
    0x82, 0x24, 0x8, 0x93, 0xbc, 0x91, 0x2, 0x44,
    0x11, 0x10, 0xc4, 0x42, 0x11, 0x18, 0x44, 0x40,
    0xe0,

    /* U+0026 "&" */
    0x3e, 0x18, 0x23, 0x0, 0x60, 0xc, 0x19, 0x83,
    0x1f, 0xfe, 0xc, 0xc1, 0x98, 0x33, 0x6, 0x60,
    0xcc, 0x18, 0xfc, 0x0,

    /* U+0027 "'" */
    0xff,

    /* U+0028 "(" */
    0x8, 0x63, 0x9c, 0xe3, 0xc, 0x30, 0xc3, 0xc,
    0x30, 0xc3, 0xc, 0x30, 0xe1, 0x83, 0x6, 0x8,

    /* U+0029 ")" */
    0x41, 0x87, 0xe, 0x1c, 0x30, 0xc3, 0xc, 0x30,
    0xc3, 0xc, 0x30, 0xc3, 0x1c, 0x63, 0x18, 0x40,

    /* U+002A "*" */
    0xc, 0x3, 0x0, 0xc3, 0xb7, 0x7f, 0x87, 0x81,
    0xe0, 0xcc, 0x61, 0x80,

    /* U+002B "+" */
    0xc, 0x3, 0x0, 0xc0, 0x30, 0xff, 0xc3, 0x0,
    0xc0, 0x30, 0xc, 0x3, 0x0,

    /* U+002C "," */
    0x6d, 0xf8,

    /* U+002D "-" */
    0xfc,

    /* U+002E "." */
    0xc0,

    /* U+002F "/" */
    0x0, 0xc0, 0x20, 0x18, 0x4, 0x1, 0x0, 0xc0,
    0x20, 0x18, 0x4, 0x3, 0x0, 0x80, 0x60, 0x10,
    0xc, 0x2, 0x0, 0x80, 0x60, 0x10, 0x0,

    /* U+0030 "0" */
    0x3f, 0x60, 0xf0, 0x78, 0x3c, 0x1e, 0xf, 0x7,
    0x83, 0xc1, 0xe0, 0xf0, 0x78, 0x3c, 0x1b, 0xf0,

    /* U+0031 "1" */
    0xc, 0x6, 0xb, 0xd, 0x8c, 0xc0, 0x60, 0x30,
    0x18, 0xc, 0x6, 0x3, 0x1, 0x80, 0xc3, 0xfc,

    /* U+0032 "2" */
    0x7e, 0xc3, 0x3, 0x3, 0x3, 0x3, 0x7, 0xe,
    0x1c, 0x38, 0x70, 0xe0, 0xc0, 0xff,

    /* U+0033 "3" */
    0x7f, 0x60, 0xc0, 0x60, 0x30, 0x18, 0xc, 0x7c,
    0x3, 0x1, 0x80, 0xc0, 0x60, 0x3c, 0x1b, 0xf0,

    /* U+0034 "4" */
    0x7, 0x3, 0x83, 0xc3, 0x61, 0x31, 0x98, 0x8c,
    0x86, 0xc3, 0x5f, 0xc0, 0xc0, 0x60, 0x30, 0x18,

    /* U+0035 "5" */
    0x7f, 0x30, 0x18, 0xc, 0x6, 0x3, 0x1, 0xbc,
    0x3, 0x1, 0x80, 0xc0, 0x68, 0x3c, 0x19, 0xf8,

    /* U+0036 "6" */
    0x7e, 0x60, 0xb0, 0x58, 0xc, 0x6, 0x3, 0xfd,
    0x83, 0xc1, 0xe0, 0xf0, 0x78, 0x3c, 0x1b, 0xf0,

    /* U+0037 "7" */
    0xfd, 0x3, 0x2, 0x6, 0x6, 0x4, 0xc, 0x8,
    0x18, 0x18, 0x10, 0x30, 0x20, 0x60,

    /* U+0038 "8" */
    0x3f, 0x60, 0xf0, 0x78, 0x3c, 0x1e, 0xd, 0xfd,
    0x83, 0xc1, 0xe0, 0xf0, 0x78, 0x3c, 0x1b, 0xf0,

    /* U+0039 "9" */
    0x3f, 0x60, 0xf0, 0x78, 0x3c, 0x1e, 0xf, 0x6,
    0xff, 0x1, 0x80, 0xc0, 0x68, 0x3c, 0x1b, 0xf8,

    /* U+003A ":" */
    0xc0, 0x0, 0x30,

    /* U+003B ";" */
    0x6c, 0x0, 0x1, 0x6d, 0xe0,

    /* U+003C "<" */
    0x8, 0x61, 0x8c, 0x61, 0x83, 0x6, 0x18, 0x20,

    /* U+003D "=" */
    0xff, 0xc0, 0x0, 0x0, 0x0, 0xff, 0xc0,

    /* U+003E ">" */
    0x41, 0x83, 0x4, 0x18, 0x63, 0xc, 0x61, 0x0,

    /* U+003F "?" */
    0x7e, 0xc3, 0x3, 0x3, 0x3, 0x7, 0x6, 0xe,
    0x1c, 0x18, 0x18, 0x0, 0x0, 0x18,

    /* U+0040 "@" */
    0x3f, 0xf8, 0xc0, 0x1b, 0x0, 0x1e, 0x0, 0x3c,
    0xfc, 0x79, 0xc, 0xf0, 0x19, 0xe0, 0x33, 0xcf,
    0xe7, 0xb0, 0xcf, 0x61, 0x9e, 0xc0, 0x3d, 0x80,
    0xd9, 0xff, 0x30, 0x0, 0x60, 0x0, 0x60, 0x0,
    0x7f, 0xf0,

    /* U+0041 "A" */
    0x7, 0x0, 0x38, 0x3, 0x60, 0x1b, 0x0, 0x98,
    0xc, 0x60, 0x63, 0x2, 0x18, 0x30, 0x61, 0xbf,
    0x8, 0x8, 0xc0, 0x66, 0x3, 0x20, 0xc,

    /* U+0042 "B" */
    0xff, 0xb0, 0x7c, 0xf, 0x3, 0xc0, 0xf0, 0x6d,
    0xf3, 0x6, 0xc0, 0xf0, 0x3c, 0xf, 0x3, 0xc1,
    0xbf, 0xc0,

    /* U+0043 "C" */
    0x3f, 0x18, 0x3c, 0xf, 0x0, 0xc0, 0x30, 0xc,
    0x3, 0x0, 0xc0, 0x30, 0xc, 0x3, 0x3, 0x60,
    0x8f, 0xc0,

    /* U+0044 "D" */
    0xff, 0x30, 0x6c, 0xf, 0x3, 0xc0, 0xf0, 0x3c,
    0xf, 0x3, 0xc0, 0xf0, 0x3c, 0xf, 0x3, 0xc1,
    0xb7, 0xc0,

    /* U+0045 "E" */
    0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xff, 0xc0,
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xff,

    /* U+0046 "F" */
    0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xff, 0xc0,
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,

    /* U+0047 "G" */
    0x3f, 0x10, 0x6c, 0xb, 0x0, 0xc0, 0x30, 0xc,
    0x7f, 0x3, 0xc0, 0xf0, 0x3c, 0xf, 0x3, 0x60,
    0x8f, 0xc0,

    /* U+0048 "H" */
    0xc0, 0xf0, 0x3c, 0xf, 0x3, 0xc0, 0xf0, 0x3d,
    0xff, 0x3, 0xc0, 0xf0, 0x3c, 0xf, 0x3, 0xc0,
    0xf0, 0x30,

    /* U+0049 "I" */
    0xff, 0xff, 0xff, 0xf0,

    /* U+004A "J" */
    0x1, 0x80, 0xc0, 0x60, 0x30, 0x18, 0xc, 0x6,
    0x3, 0x1, 0x80, 0xc0, 0x68, 0x3e, 0x31, 0xf0,

    /* U+004B "K" */
    0xc0, 0xf0, 0x6c, 0x33, 0x18, 0xcc, 0x36, 0xf,
    0x3, 0xc0, 0xd8, 0x33, 0xc, 0x63, 0xc, 0xc1,
    0xb0, 0x30,

    /* U+004C "L" */
    0xc0, 0x60, 0x30, 0x18, 0xc, 0x6, 0x3, 0x1,
    0x80, 0xc0, 0x60, 0x30, 0x18, 0xc, 0x7, 0xfc,

    /* U+004D "M" */
    0xe0, 0x3f, 0x83, 0xfc, 0x1f, 0xa0, 0xbd, 0x5,
    0xec, 0x6f, 0x62, 0x79, 0x13, 0xcd, 0x9e, 0x6c,
    0xf1, 0x47, 0x8a, 0x3c, 0x61, 0xe1, 0xc,

    /* U+004E "N" */
    0xc0, 0xf8, 0x3f, 0xf, 0xc3, 0xd8, 0xf6, 0x3c,
    0xcf, 0x33, 0xc6, 0xf1, 0xbc, 0x37, 0xe, 0xc1,
    0xb0, 0x30,

    /* U+004F "O" */
    0x3f, 0x88, 0x1b, 0x1, 0xe0, 0x3c, 0x7, 0x80,
    0xf0, 0x1e, 0x3, 0xc0, 0x78, 0xf, 0x1, 0xe0,
    0x36, 0x4, 0x7f, 0x0,

    /* U+0050 "P" */
    0xff, 0x30, 0x6c, 0xf, 0x3, 0xc0, 0xf0, 0x3c,
    0x1b, 0x7e, 0xc0, 0x30, 0xc, 0x3, 0x0, 0xc0,
    0x30, 0x0,

    /* U+0051 "Q" */
    0x3f, 0x82, 0x6, 0x30, 0x19, 0x80, 0xcc, 0x6,
    0x60, 0x33, 0x1, 0x98, 0xc, 0xc0, 0x66, 0x3,
    0x30, 0x19, 0x80, 0xc6, 0x4, 0x1f, 0xc0, 0x3,
    0x0, 0x7,

    /* U+0052 "R" */
    0xff, 0x18, 0x33, 0x3, 0x60, 0x6c, 0xd, 0x81,
    0xb0, 0x66, 0xf8, 0xc6, 0x18, 0x63, 0x6, 0x60,
    0xcc, 0xd, 0x81, 0xc0,

    /* U+0053 "S" */
    0x3f, 0x98, 0x1b, 0x1, 0x60, 0xc, 0x1, 0x80,
    0x1f, 0xf0, 0x6, 0x0, 0x60, 0xc, 0x1, 0xe0,
    0x36, 0x4, 0x7f, 0x0,

    /* U+0054 "T" */
    0xff, 0xc3, 0x0, 0xc0, 0x30, 0xc, 0x3, 0x0,
    0xc0, 0x30, 0xc, 0x3, 0x0, 0xc0, 0x30, 0xc,
    0x3, 0x0,

    /* U+0055 "U" */
    0xc0, 0xf0, 0x3c, 0xf, 0x3, 0xc0, 0xf0, 0x3c,
    0xf, 0x3, 0xc0, 0xf0, 0x3c, 0xf, 0x3, 0x61,
    0x8f, 0xc0,

    /* U+0056 "V" */
    0xc0, 0x68, 0xd, 0x81, 0x30, 0x26, 0xc, 0x61,
    0x8c, 0x21, 0x8c, 0x11, 0x83, 0x20, 0x6c, 0x5,
    0x80, 0xe0, 0x18, 0x0,

    /* U+0057 "W" */
    0x83, 0x87, 0x87, 0xb, 0xe, 0x16, 0x3c, 0x64,
    0x68, 0xc8, 0x99, 0x99, 0x32, 0x32, 0x64, 0x6c,
    0xd8, 0x58, 0xb0, 0xa1, 0xc1, 0xc3, 0x83, 0x6,
    0x6, 0xc, 0x0,

    /* U+0058 "X" */
    0xc0, 0x98, 0x66, 0x30, 0xcc, 0x16, 0x6, 0x80,
    0xc0, 0x70, 0x1e, 0xd, 0x82, 0x31, 0x86, 0xc1,
    0xb0, 0x30,

    /* U+0059 "Y" */
    0x60, 0x26, 0x6, 0x30, 0xc3, 0xc, 0x19, 0x80,
    0xd0, 0xd, 0x0, 0x60, 0x6, 0x0, 0x60, 0x6,
    0x0, 0x60, 0x6, 0x0, 0x60,

    /* U+005A "Z" */
    0xff, 0xc0, 0x70, 0x18, 0xc, 0x3, 0x1, 0x80,
    0xc0, 0x20, 0x18, 0xc, 0x2, 0x1, 0x80, 0xc0,
    0x27, 0xf0,

    /* U+005B "[" */
    0xfe, 0x31, 0x8c, 0x63, 0x18, 0xc6, 0x31, 0x8c,
    0x63, 0x18, 0xc7, 0xc0,

    /* U+005C "\\" */
    0x40, 0x18, 0x2, 0x0, 0x80, 0x30, 0x4, 0x1,
    0x80, 0x20, 0xc, 0x1, 0x0, 0x60, 0x8, 0x3,
    0x0, 0x40, 0x10, 0x6, 0x0, 0x80, 0x30,

    /* U+005D "]" */
    0xf8, 0xc6, 0x31, 0x8c, 0x63, 0x18, 0xc6, 0x31,
    0x8c, 0x63, 0x1f, 0xc0,

    /* U+005E "^" */
    0xc, 0x3, 0x81, 0xa0, 0xcc, 0x61, 0x90, 0x20,

    /* U+005F "_" */
    0xff, 0xc0,

    /* U+0060 "`" */
    0xc6, 0x30,

    /* U+0061 "a" */
    0x7e, 0x43, 0x3, 0x3, 0x7b, 0xc3, 0xc3, 0xc3,
    0xc7, 0x7b,

    /* U+0062 "b" */
    0xc0, 0x60, 0x30, 0x18, 0xd, 0xf6, 0xf, 0x7,
    0x83, 0xc1, 0xe0, 0xf0, 0x78, 0x3e, 0x1e, 0xf8,

    /* U+0063 "c" */
    0x7e, 0x60, 0xb0, 0x18, 0xc, 0x6, 0x3, 0x1,
    0x80, 0xc1, 0xbf, 0x0,

    /* U+0064 "d" */
    0x1, 0x80, 0xc0, 0x60, 0x37, 0xde, 0xf, 0x7,
    0x83, 0xc1, 0xe0, 0xf0, 0x78, 0x7c, 0x3b, 0xec,

    /* U+0065 "e" */
    0x7e, 0xc3, 0xc3, 0xc3, 0xdf, 0xc0, 0xc0, 0xc2,
    0xc3, 0x7e,

    /* U+0066 "f" */
    0x1e, 0x60, 0xc1, 0x8f, 0x66, 0xc, 0x18, 0x30,
    0x60, 0xc1, 0x83, 0x6, 0x0,

    /* U+0067 "g" */
    0x7d, 0xe0, 0xf0, 0x78, 0x3c, 0x1e, 0xf, 0x7,
    0x83, 0xc3, 0xbe, 0xc0, 0x60, 0x3c, 0x1b, 0xf8,

    /* U+0068 "h" */
    0xc0, 0xc0, 0xc0, 0xc0, 0xde, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,

    /* U+0069 "i" */
    0xc0, 0xff, 0xff, 0xf0,

    /* U+006A "j" */
    0x18, 0x0, 0x1, 0x8c, 0x63, 0x18, 0xc6, 0x31,
    0x8c, 0x63, 0x1f, 0x80,

    /* U+006B "k" */
    0xc0, 0x60, 0x30, 0x18, 0xc, 0x36, 0x33, 0x31,
    0xb0, 0xf0, 0x78, 0x36, 0x19, 0x8c, 0x66, 0x18,

    /* U+006C "l" */
    0xff, 0xff, 0xff, 0xf0,

    /* U+006D "m" */
    0xdf, 0xf6, 0x30, 0xf1, 0x87, 0x8c, 0x3c, 0x61,
    0xe3, 0xf, 0x18, 0x78, 0xc3, 0xc6, 0x1e, 0x30,
    0xc0,

    /* U+006E "n" */
    0xde, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3,

    /* U+006F "o" */
    0x3f, 0x60, 0xf0, 0x78, 0x3c, 0x1e, 0xf, 0x7,
    0x83, 0xc1, 0xbf, 0x0,

    /* U+0070 "p" */
    0xdf, 0x60, 0xf0, 0x78, 0x3c, 0x1e, 0xf, 0x7,
    0x83, 0xe1, 0xef, 0xb0, 0x18, 0xc, 0x6, 0x0,

    /* U+0071 "q" */
    0x7d, 0xe0, 0xf0, 0x78, 0x3c, 0x1e, 0xf, 0x7,
    0x83, 0xc3, 0xbe, 0xc0, 0x60, 0x30, 0x18, 0xc,

    /* U+0072 "r" */
    0xde, 0xe3, 0xe3, 0xc3, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0,

    /* U+0073 "s" */
    0x3e, 0x82, 0xc2, 0xc0, 0xc0, 0x7e, 0x3, 0x3,
    0xc3, 0x7c,

    /* U+0074 "t" */
    0x61, 0x86, 0x18, 0xfd, 0x86, 0x18, 0x61, 0x86,
    0x18, 0x60, 0xf0,

    /* U+0075 "u" */
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0x7b,

    /* U+0076 "v" */
    0x41, 0xa0, 0x98, 0xc4, 0x62, 0x21, 0x90, 0xd8,
    0x28, 0x1c, 0xc, 0x0,

    /* U+0077 "w" */
    0x87, 0xe, 0x38, 0xf1, 0x47, 0x9b, 0x34, 0xd9,
    0x24, 0x59, 0xa2, 0xcd, 0x1e, 0x28, 0xe1, 0x2,
    0x0,

    /* U+0078 "x" */
    0xc3, 0x66, 0x64, 0x34, 0x18, 0x18, 0x3c, 0x66,
    0x46, 0xc3,

    /* U+0079 "y" */
    0x41, 0xb0, 0x98, 0xc4, 0x63, 0x21, 0xb0, 0x58,
    0x38, 0x18, 0x4, 0x2, 0x3, 0x1, 0x80, 0x80,

    /* U+007A "z" */
    0xff, 0x2, 0x6, 0xc, 0x18, 0x10, 0x30, 0x60,
    0xc0, 0xbf,

    /* U+007B "{" */
    0x4, 0x73, 0x8c, 0x30, 0xc3, 0xc, 0x31, 0x84,
    0x18, 0x30, 0xc3, 0xc, 0x30, 0xc3, 0x87, 0xc,

    /* U+007C "|" */
    0xff, 0xff, 0xff, 0xff, 0xf0,

    /* U+007D "}" */
    0x87, 0x1c, 0x63, 0x18, 0xc6, 0x30, 0xc2, 0x33,
    0x18, 0xc6, 0x31, 0x9d, 0xcc, 0x0,

    /* U+007E "~" */
    0x0, 0xe, 0x36, 0xdb, 0x1c
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 72, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 84, .box_w = 2, .box_h = 14, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 5, .adv_w = 144, .box_w = 5, .box_h = 4, .ofs_x = 2, .ofs_y = 10},
    {.bitmap_index = 8, .adv_w = 193, .box_w = 11, .box_h = 15, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 29, .adv_w = 193, .box_w = 9, .box_h = 18, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 50, .adv_w = 266, .box_w = 14, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 75, .adv_w = 188, .box_w = 11, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 95, .adv_w = 84, .box_w = 2, .box_h = 4, .ofs_x = 2, .ofs_y = 10},
    {.bitmap_index = 96, .adv_w = 116, .box_w = 6, .box_h = 21, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 112, .adv_w = 116, .box_w = 6, .box_h = 21, .ofs_x = 0, .ofs_y = -4},
    {.bitmap_index = 128, .adv_w = 181, .box_w = 10, .box_h = 9, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 140, .adv_w = 193, .box_w = 10, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 153, .adv_w = 95, .box_w = 3, .box_h = 5, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 155, .adv_w = 150, .box_w = 6, .box_h = 1, .ofs_x = 2, .ofs_y = 5},
    {.bitmap_index = 156, .adv_w = 85, .box_w = 2, .box_h = 1, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 157, .adv_w = 167, .box_w = 10, .box_h = 18, .ofs_x = 0, .ofs_y = -4},
    {.bitmap_index = 180, .adv_w = 193, .box_w = 9, .box_h = 14, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 196, .adv_w = 193, .box_w = 9, .box_h = 14, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 212, .adv_w = 193, .box_w = 8, .box_h = 14, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 226, .adv_w = 193, .box_w = 9, .box_h = 14, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 242, .adv_w = 193, .box_w = 9, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 258, .adv_w = 193, .box_w = 9, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 274, .adv_w = 193, .box_w = 9, .box_h = 14, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 290, .adv_w = 193, .box_w = 8, .box_h = 14, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 304, .adv_w = 193, .box_w = 9, .box_h = 14, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 320, .adv_w = 193, .box_w = 9, .box_h = 14, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 336, .adv_w = 85, .box_w = 2, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 339, .adv_w = 91, .box_w = 3, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 344, .adv_w = 193, .box_w = 6, .box_h = 10, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 352, .adv_w = 193, .box_w = 10, .box_h = 5, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 359, .adv_w = 193, .box_w = 6, .box_h = 10, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 367, .adv_w = 157, .box_w = 8, .box_h = 14, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 381, .adv_w = 295, .box_w = 15, .box_h = 18, .ofs_x = 2, .ofs_y = -4},
    {.bitmap_index = 415, .adv_w = 209, .box_w = 13, .box_h = 14, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 438, .adv_w = 209, .box_w = 10, .box_h = 14, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 456, .adv_w = 191, .box_w = 10, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 474, .adv_w = 212, .box_w = 10, .box_h = 14, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 492, .adv_w = 193, .box_w = 8, .box_h = 14, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 506, .adv_w = 184, .box_w = 8, .box_h = 14, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 520, .adv_w = 202, .box_w = 10, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 538, .adv_w = 193, .box_w = 10, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 556, .adv_w = 92, .box_w = 2, .box_h = 14, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 560, .adv_w = 166, .box_w = 9, .box_h = 14, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 576, .adv_w = 198, .box_w = 10, .box_h = 14, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 594, .adv_w = 172, .box_w = 9, .box_h = 14, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 610, .adv_w = 270, .box_w = 13, .box_h = 14, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 633, .adv_w = 222, .box_w = 10, .box_h = 14, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 651, .adv_w = 223, .box_w = 11, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 671, .adv_w = 197, .box_w = 10, .box_h = 14, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 689, .adv_w = 223, .box_w = 13, .box_h = 16, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 715, .adv_w = 216, .box_w = 11, .box_h = 14, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 735, .adv_w = 208, .box_w = 11, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 755, .adv_w = 168, .box_w = 10, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 773, .adv_w = 212, .box_w = 10, .box_h = 14, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 791, .adv_w = 182, .box_w = 11, .box_h = 14, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 811, .adv_w = 262, .box_w = 15, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 838, .adv_w = 186, .box_w = 10, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 856, .adv_w = 180, .box_w = 12, .box_h = 14, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 877, .adv_w = 196, .box_w = 10, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 895, .adv_w = 116, .box_w = 5, .box_h = 18, .ofs_x = 2, .ofs_y = -4},
    {.bitmap_index = 907, .adv_w = 167, .box_w = 10, .box_h = 18, .ofs_x = 0, .ofs_y = -4},
    {.bitmap_index = 930, .adv_w = 116, .box_w = 5, .box_h = 18, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 942, .adv_w = 193, .box_w = 10, .box_h = 6, .ofs_x = 1, .ofs_y = 8},
    {.bitmap_index = 950, .adv_w = 160, .box_w = 10, .box_h = 1, .ofs_x = 0, .ofs_y = -4},
    {.bitmap_index = 952, .adv_w = 155, .box_w = 4, .box_h = 3, .ofs_x = 2, .ofs_y = 11},
    {.bitmap_index = 954, .adv_w = 168, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 964, .adv_w = 175, .box_w = 9, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 980, .adv_w = 158, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 992, .adv_w = 175, .box_w = 9, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1008, .adv_w = 167, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1018, .adv_w = 117, .box_w = 7, .box_h = 14, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1031, .adv_w = 175, .box_w = 9, .box_h = 14, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 1047, .adv_w = 170, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1061, .adv_w = 70, .box_w = 2, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1065, .adv_w = 68, .box_w = 5, .box_h = 18, .ofs_x = -2, .ofs_y = -4},
    {.bitmap_index = 1077, .adv_w = 170, .box_w = 9, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1093, .adv_w = 67, .box_w = 2, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1097, .adv_w = 242, .box_w = 13, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1114, .adv_w = 170, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1124, .adv_w = 173, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1136, .adv_w = 175, .box_w = 9, .box_h = 14, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 1152, .adv_w = 175, .box_w = 9, .box_h = 14, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 1168, .adv_w = 150, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1178, .adv_w = 156, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1188, .adv_w = 124, .box_w = 6, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1199, .adv_w = 170, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1209, .adv_w = 148, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1221, .adv_w = 239, .box_w = 13, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1238, .adv_w = 158, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1248, .adv_w = 148, .box_w = 9, .box_h = 14, .ofs_x = 0, .ofs_y = -4},
    {.bitmap_index = 1264, .adv_w = 153, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1274, .adv_w = 116, .box_w = 6, .box_h = 21, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 1290, .adv_w = 84, .box_w = 2, .box_h = 18, .ofs_x = 2, .ofs_y = -4},
    {.bitmap_index = 1295, .adv_w = 116, .box_w = 5, .box_h = 21, .ofs_x = 0, .ofs_y = -4},
    {.bitmap_index = 1309, .adv_w = 193, .box_w = 10, .box_h = 4, .ofs_x = 1, .ofs_y = 3}
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
    -9, -13, -9, -7, -13, -4, -9, -40,
    -16, -27, -43, -13, -45, -31, -22, -49,
    -67, -45, -67, -4, -4, -4, -4, -4,
    -4, -27, -2, -16, -13, -27, -9, -9,
    -4, -9, -9, -9, -9, -9, -13, -13,
    -11, -4, -4, -2, -13, -9, -4, -4,
    -4, -4, -13, -9, -36, -4, -36, -13,
    -13, -9, -13, -13, -13, -9, -9, -2,
    -13, -4, -22, -4, -4, -4, -4, 8,
    4, -4, -4, -31, -9, -9, 15, -9,
    -9, -45, -4, -27, -22, -45, -9, -9,
    -13, -13, -4, -4, -4, -4, -4, -13,
    -36, -22, -36, -27, -9, -4, -4, -13,
    -18, -4, -4, -4, -9, -4, -18, -9,
    -9, -9, -9, -4, -4, -4, -4, -4,
    -4, -9, -4, -45, -40, -45, -31, -31,
    -27, -22, -4, -4, 9, -40, -40, -40,
    -40, -40, -40, -40, -40, -43, -34, -31,
    -4, -4, -2, -4, -31, -16, -31, -4,
    -4, -16, -13, -18, -18, 4, -18, -13,
    -18, -18, -9, -9, -13, -13, -13, -13,
    -9, -13, -9, -4, -27, -4, -4, -13,
    -4, -9, -49, -43, -49, -22, -22, -27,
    -13, -9, -13, -13, -13, -13, -13, -13,
    -38, -13, -13, -13, -13, -13, -13, -13,
    -13, -13, 9, -4, -13, -9, -36, -36,
    -36, -18, -36, -22, -4, -9, -4, -13,
    -4, -2, -2, -9, -9, -4, -4, -9,
    -9, -31, -31, -4, -4, 9, 9, 9,
    -4, -4, -9, -4, -8, -8, -8, -8,
    -8, -6, -8, -8, -6, -8, -4, -4,
    -4, -4, -4, -9, -9, -9, -9, -4,
    -9, -31, -9, -31, -9, -9, -9, -9,
    -9, -9, -9, -9, -4, -9, -9, -9,
    -9, -9, -9, -9, -9, -4, -9, -9,
    -4, -4, -4, -4, -4, -4, -18, 7,
    7, -27, -4, -27, -4, -22, -4, -22,
    -4, -27, -4, -27
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
const lv_font_t custom_font_normal_20 = {
#else
lv_font_t custom_font_normal_20 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 21,          /*The maximum line height required by the font*/
    .base_line = 4,             /*Baseline measured from the bottom of the line*/
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



#endif /*#if CUSTOM_FONT_NORMAL_20*/

