# Device UI font

The device UI uses [Oxanium](https://github.com/sevmeyer/oxanium), a square,
technical sans-serif with Light, Regular and Bold weights. The font files and
the generated LVGL glyph bitmaps in `../../main/ui/fonts/custom_font_*.c` are
covered by the included [SIL Open Font License 1.1](OFL.txt). The conversion
script is GPL-3.0-only.

To regenerate the eight checked-in LVGL fonts, install `lv_font_conv@1.5.3`
and run `python3 main/ui/fonts/convert_fonts.py` from the firmware directory.
Only printable ASCII (`0x20–0x7E`) is embedded in these assets.
