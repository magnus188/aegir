# Ziphius hardware

The ESP32 software project is in [`../software/firmware/`](../software/firmware/).
Hardware work is grouped by discipline here so the editable files, previews
and print packages are easy to find.

## Start here

| Area | Main entry point | Contents |
| --- | --- | --- |
| CAD | [cad/](cad/) | Fusion enclosure models, STEP exports, revision notes and image previews |
| 3D printing | [cad/rev04/3d-print/](cad/rev04/3d-print/) | A3 print release, STL files, checks and Bambu Studio projects |
| Bambu Studio | [cad/rev04/3d-print/printing/bambu-studio/](cad/rev04/3d-print/printing/bambu-studio/) | PLA and PETG `.3mf` projects, grouped by part |
| PCB | [pcb/](pcb/) | Active KiCad projects, previews and manufacturing review |
| Main PCB | [pcb/analyzer/Trimix_Analyzer.kicad_pro](pcb/analyzer/Trimix_Analyzer.kicad_pro) | Current integrated analyzer schematic and board |
| USB PCB | [pcb/usb-input/Trimix_USB_Input.kicad_pro](pcb/usb-input/Trimix_USB_Input.kicad_pro) | USB input daughterboard |
| System review | [system-review/](system-review/) | Cross-discipline electrical, mechanical and firmware review evidence |

Earlier enclosure, power-board and EasyEDA migration checkpoints are available
through Git history. The files above are the active design entry points.

## Supporting material

- [Analyzer design notes](ANALYZER_DESIGN.md)
- [USB charging notes](USB_CHARGING.md)
- [Software calibration notes](SOFTWARE_CALIBRATION.md)
- [Blank gas-characterization log](characterization-log-template.csv)
- [KiCad MCP setup](MCP_SETUP.md)

Current verification records remain beside the work they describe. Preview
folders contain human-viewable images; editable source models stay in their
project folders.
