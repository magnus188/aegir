![Ægir 1 logo](logo/aegir-lockup-dark.png)

Ægir is a work-in-progress trimix gas analyzer for technical diving. It is not
qualified as a breathing-gas safety instrument; use it at your own risk.

Made in Norway 🇳🇴.

## The name

In Norse mythology, Ægir (/ˈæːɣir/) is the giant who rules the sea. He is
known for hosting the gods in his halls adorned with gold and, together with
Rán, is the father of nine daughters who personify the waves.

The runes ᛅᛁᚾ are a Viking Age-style representation of einn, Old Norse for
“one”, marking the first version of the project: Ægir 1.

## Features

- Oxygen (O₂) and helium (He) sensor inputs, plus CO monitoring
- 4.3-inch touch screen for gas analysis, calibration and settings
- Mix calculations, dive planning and analysis history
- Desktop simulator with demo readings

**[Try the interactive browser demo](https://magnus188.github.io/aegir/).**

## Device design

<img src="media/aegir-device-concept-render.png" alt="Concept render of the Ægir handheld trimix analyzer with a black 3D-printed case, white logo, plastic gas fittings and portrait touchscreen" width="420">

*Concept render of the proposed enclosure and interface. Details may change
during prototyping; the screen readings are illustrative.*

## PCB design

| Main analyzer PCB | USB input PCB |
| :---: | :---: |
| <a href="hardware/pcb/review-images/README.md"><img src="hardware/pcb/review-images/01-main-3d-top.png" alt="KiCad 3D view of the main analyzer PCB" width="190"></a> | <a href="hardware/pcb/review-images/README.md"><img src="hardware/pcb/review-images/08-usb-front-routing.png" alt="Front copper routing of the USB input PCB" width="220"></a> |

[View the PCB schematics and routing images](hardware/pcb/review-images/README.md).
These are design-review snapshots; the PCBs are still on fabrication hold.

## Screenshots

These screens show the simulator with demo data. Swipe horizontally between
live analysis and analysis details; simulator profiles are in the top status banner.

| Main menu | Live analysis | Analysis details |
| :---: | :---: | :---: |
| <img src="software/firmware/docs/screenshots/main-menu.png" alt="Main menu with Tabler icons" width="240"> | <img src="software/firmware/docs/screenshots/analysis-live.png" alt="Live gas analysis and sample graph" width="240"> | <img src="software/firmware/docs/screenshots/analysis-planning.png" alt="Selected cylinder and gas analysis details" width="240"> |

| History | Cylinders | Settings |
| :---: | :---: | :---: |
| <img src="software/firmware/docs/screenshots/history.png" alt="Captured analysis history" width="240"> | <img src="software/firmware/docs/screenshots/cylinders.png" alt="Cylinder profiles and export label" width="240"> | <img src="software/firmware/docs/screenshots/settings.png" alt="Settings menu" width="240"> |

| Dive Planner | Device Settings |
| :---: | :---: |
| <img src="software/firmware/docs/screenshots/dive-planner.png" alt="Dive Planner with dark instrument header" width="240"> | <img src="software/firmware/docs/screenshots/device-settings.png" alt="Device status with brightness slider and sleep controls" width="240"> |

| Simulated high CO alert | Safety Settings |
| :---: | :---: |
| <img src="software/firmware/docs/screenshots/analysis-high-co.png" alt="Analyse shows a red CO alarm in the simulator" width="240"> | <img src="software/firmware/docs/screenshots/safety-settings.png" alt="Configurable advisory and alarm limits" width="240"> |

## Run it

For the desktop simulator, install a C++ compiler, CMake, `pkg-config` and SDL2,
then run from the repository root:

```sh
make sim-deps
make sim ZOOM=0.75
```

Run the host tests with `make test`.

To build and flash the firmware on the Guition JC4880P443C_I_W ESP32-P4 board,
install Python 3.13 and run:

```sh
make setup-idf
make devices
make board-info PORT=/dev/cu.usbserial-110
make push PORT=/dev/cu.usbserial-110
```

Replace the example `PORT` with the port reported by `make devices`. See the
[firmware guide](software/firmware/README.md) for other build options.

## Files

- [Firmware and simulator](software/firmware/)
- [PCB review images](hardware/pcb/review-images/README.md), [main PCB](hardware/pcb/analyzer/Trimix_Analyzer.kicad_pro) and [USB input PCB](hardware/pcb/usb-input/Trimix_USB_Input.kicad_pro)
- [Enclosure CAD](hardware/cad/rev04/3d-print/Trimix_Enclosure_A3_PrintReview.f3d) and [3D-print files](hardware/cad/rev04/3d-print/printing/)
- [Logo](logo/README.md)

## License

Software: [GPLv3](LICENSES/GPL-3.0-only.txt). PCB and CAD files:
[CERN-OHL-S-2.0](LICENSES/CERN-OHL-S-2.0.txt). The Ægir name and logo have a
separate [brand policy](logo/LICENSE.md). See the [license map](LICENSE.md)
for details and third-party files.
