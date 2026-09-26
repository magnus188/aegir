#!/usr/bin/env python3
"""Regenerate the LVGL glyph assets from the OFL-licensed Oxanium font."""

from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess


FONT_DIR = Path(__file__).resolve().parent
ASSET_DIR = Path(__file__).resolve().parents[3] / "assets" / "fonts"
GLYPH_RANGE = "0x20-0x7E"
FONT_CONFIGS = (
    ("Oxanium-Regular.ttf", "normal", (16, 20, 24)),
    ("Oxanium-Bold.ttf", "bold", (16, 20, 24)),
    ("Oxanium-Light.ttf", "light", (14, 16)),
)
NOTICE = (
    "/* Glyph artwork generated from Oxanium, copyright 2019 The Oxanium "
    "Project Authors.\n"
    " * Licensed under SIL Open Font License 1.1; see "
    "../../../assets/fonts/OFL.txt.\n"
    " */\n"
)


def main() -> None:
    converter = os.environ.get("LV_FONT_CONV") or shutil.which("lv_font_conv")
    if not converter:
        raise SystemExit("Install lv_font_conv@1.5.3 or set LV_FONT_CONV to its executable")

    for font_file, weight, sizes in FONT_CONFIGS:
        if not (ASSET_DIR / font_file).is_file():
            raise SystemExit(f"Missing font: {ASSET_DIR / font_file}")
        for size in sizes:
            font_name = f"custom_font_{weight}_{size}"
            output = FONT_DIR / f"{font_name}.c"
            subprocess.run(
                [
                    converter,
                    "--font", f"../../../assets/fonts/{font_file}",
                    "--size", str(size),
                    "--format", "lvgl",
                    "--bpp", "1",
                    "--no-compress",
                    "--range", GLYPH_RANGE,
                    "--lv-font-name", font_name,
                    "--lv-include", "lvgl.h",
                    "--output", output.name,
                ],
                cwd=FONT_DIR,
                check=True,
            )
            output.write_text(NOTICE + output.read_text())
            print(f"Generated {output.name}")


if __name__ == "__main__":
    main()
