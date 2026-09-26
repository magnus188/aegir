# Ægir licensing

Copyright © 2025–2026 Magnus Trandokken and contributors in original Ægir
material. Contributors retain copyright in their own contributions.

This repository has separate licenses for software, hardware designs, and
branding. The grants below apply to **original Ægir material**; they do not
relicense third-party material included for reference or under its own license.

| Material | License |
| --- | --- |
| Project-written software, build and CAD scripts, tests, and general repository documentation | [GNU GPL v3.0 only](LICENSES/GPL-3.0-only.txt) (`GPL-3.0-only`) |
| Project-created PCB and mechanical design sources, drawings, fabrication and 3D-print exports, and hardware design documentation in `hardware/` | [CERN Open Hardware Licence v2, Strongly Reciprocal](LICENSES/CERN-OHL-S-2.0.txt) (`CERN-OHL-S-2.0`) |
| Ægir name, face/ship logo, wordmark, splash artwork and copies of the mark in firmware or documentation | [Brand and trademark policy](logo/LICENSE.md) |

The software license includes project-written scripts wherever they live,
including `hardware/**/scripts/`. The hardware license covers the editable
KiCad and Fusion projects, hardware review records, and their project-created
STEP, STL, 3MF and other design exports. The current source location is
<https://github.com/magnus188/aegir>.

## Third-party material

Keep the upstream notices and licenses when redistributing third-party files:

- Bosch BME280 driver in `software/firmware/main/third_party/bme280/`:
  BSD-3-Clause, with its own `LICENSE`.
- cJSON in `software/firmware/tests/third_party/cjson/`: MIT, with its own
  `LICENSE`.
- Guition display initializer in
  `software/firmware/main/board/guition_st7701_init.h`:
  [MIT](LICENSES/MIT-ultramcu.txt), with its copyright notice in the file.
- The ESP-Hosted patch in `software/firmware/patches/esp_hosted/`:
  [Apache-2.0](LICENSES/Apache-2.0.txt), with its notice in the patch.
- Oxanium UI font files in `software/firmware/assets/fonts/` and generated
  LVGL glyph data in `software/firmware/main/ui/fonts/custom_font_*.c`:
  [SIL Open Font License 1.1](software/firmware/assets/fonts/OFL.txt).
- Iceland outlines used in the Ægir wordmark: the font's
  [SIL Open Font License 1.1](logo/OFL-Iceland.txt) remains applicable
  alongside the project's brand policy.
- The two KiCad STEP models in
  `hardware/pcb/usb-input/Trimix_USB.3dshapes/`: GPL-3.0-or-later with their
  embedded KiCad exception, as stated in each STEP header.
- Manufacturer drawings, specifications, reference images, and other
  third-party evidence in `hardware/pcb/`, `hardware/cad/`, and
  `hardware/system-review/`: their owners' terms. In particular,
  `hardware/pcb/usb-input/reference/GCT_USB4720_RevB_drawing.pdf` is a
  manufacturer document, not Ægir hardware source.

The project does not claim rights over third-party names, fonts, component
models, documents, or design tools. See the notices next to individual files
when one is present. If a file's rights are unclear, do not assume this
project's license replaces them.

## Redistributing products

Commercial use and sales are allowed by both project licenses. If you
distribute GPL-covered firmware binaries, provide recipients the corresponding
source; distributed modified versions remain under GPLv3. If you make and
distribute products from the covered hardware designs, provide the complete
design source or its source location as required by CERN-OHL-S-2.0. Private
modifications need not be published merely
because they were made. Neither license grants the right to present a fork or
third-party product as an official Ægir product; see the brand policy.
