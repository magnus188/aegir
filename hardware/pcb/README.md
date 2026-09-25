# PCB designs

Open the active KiCad projects from their source folders:

| Board | Project | Purpose |
| --- | --- | --- |
| Main analyzer | [Trimix_Analyzer.kicad_pro](analyzer/Trimix_Analyzer.kicad_pro) | Current routed analyzer board, schematic sheets, and libraries |
| USB input | [Trimix_USB_Input.kicad_pro](usb-input/Trimix_USB_Input.kicad_pro) | Separate USB input daughterboard |

The [main-board manufacturing review](../system-review/electrical/main-final/README.md)
contains CAM files, assembly lists, stencil instructions, and a required
fill-and-cap hole map. **Prototype order remains on hold** pending the
[whole-system acceptance review](../system-review/README.md).

Earlier power-board and migration projects are available in Git history.
KiCad `.kicad_prl` files, locks, autosaves and backup folders are local editor
state and are ignored by Git.
