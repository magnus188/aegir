# Ægir hardware

The repository root is the ESP32 software project. Hardware work is grouped by
discipline here so the editable files, previews and print packages are easy to
find.

## Start here

| Area | Main entry point | Contents |
| --- | --- | --- |
| CAD | [cad/](cad/) | Fusion enclosure models, STEP exports, revision notes and image previews |
| 3D printing | [cad/rev04/3d-print/](cad/rev04/3d-print/) | A3 print release, STL files, checks and Bambu Studio projects |
| Bambu Studio | [cad/rev04/3d-print/printing/bambu-studio/](cad/rev04/3d-print/printing/bambu-studio/) | Ready-to-review PLA and PETG `.3mf` projects, grouped by part |
| PCB | [pcb/](pcb/) | Active KiCad projects, PCB integration assets, previews and verification |
| Main PCB | [pcb/analyzer/Trimix_Analyzer.kicad_pro](pcb/analyzer/Trimix_Analyzer.kicad_pro) | Current integrated analyzer schematic and board |
| USB PCB | [pcb/usb-input/Trimix_USB_Input.kicad_pro](pcb/usb-input/Trimix_USB_Input.kicad_pro) | USB input daughterboard |
| System review | [system-review/](system-review/) | Cross-discipline electrical, mechanical and firmware review evidence |

The preserved P1 power design is in
[`pcb/power/Trimix_Power.kicad_pro`](pcb/power/Trimix_Power.kicad_pro). The
obsolete EasyEDA-derived KiCad workspace is intentionally separated under
[`pcb/archive/deprecated-kicad-import/`](pcb/archive/deprecated-kicad-import/)
and should not be used as the current design.

## Supporting material

- [Analyzer design notes](ANALYZER_DESIGN.md)
- [Power design notes](POWER_DESIGN.md)
- [USB charging notes](USB_CHARGING.md)
- [Software calibration notes](SOFTWARE_CALIBRATION.md)
- [KiCad MCP setup](MCP_SETUP.md)

Generated verification records remain beside the work they describe. Preview
folders contain human-viewable images; source models and engineering evidence
stay in their project folders so they are not confused with presentation
exports.
