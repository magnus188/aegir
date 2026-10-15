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

*Concept render of the proposed enclosure and interface.*

## PCB and enclosure

The current prototype uses one PCB with a bottom-facing GCT USB4105-GF-A
USB-C connector. The matching enclosure retains the protected battery holder
and adds support around the connector.

<img src="hardware/cad/pcb-v2-bottom/views/usb-bottom-open.png" alt="Current enclosure with integrated PCB and bottom USB-C support" width="420">

Open the [KiCad project](hardware/pcb/analyzer-v2-bottom/Trimix_Analyzer.kicad_pro)
and [Fusion model](hardware/cad/pcb-v2-bottom/Trimix_Enclosure_PCB2_Bottom.f3d).
The [hardware guide](hardware/PCB2-BOTTOM.txt) describes the current files and
remaining prototype checks. Supplier and physical qualification are pending.

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
- [Current PCB 2.0 prototype](hardware/pcb/analyzer-v2-bottom/Trimix_Analyzer.kicad_pro), with integrated USB-C facing the bottom
- [Current enclosure CAD](hardware/cad/pcb-v2-bottom/Trimix_Enclosure_PCB2_Bottom.f3d), [STEP](hardware/cad/pcb-v2-bottom/Trimix_Enclosure_PCB2_Bottom.step) [H2D print bed](hardware/cad/pcb-v2-bottom/Trimix_PCB2_Bottom_H2D.3mf) and [STL package](hardware/cad/pcb-v2-bottom/Trimix_PCB2_Bottom_PrintParts.zip)
- [Hardware package guide and qualification status](hardware/PCB2-BOTTOM.txt); superseded deliverables remain in Git history
- [Logo](logo/README.md)

## Contributing and future ideas

Design reviews, measurements, documentation and software fixes are welcome.
Start with the [contribution guide](CONTRIBUTING.md) and
[open issues](https://github.com/magnus188/aegir/issues). The
[v2 investigation backlog](https://github.com/magnus188/aegir/milestone/1)
collects community suggestions without promising a release date.

Maintainers can use the [repository guide](.github/MAINTAINING.md). For
exploitable vulnerabilities, use the [private reporting route](SECURITY.md).

## License

Software: [GPLv3](LICENSES/GPL-3.0-only.txt). PCB and CAD files:
[CERN-OHL-S-2.0](LICENSES/CERN-OHL-S-2.0.txt). The Ægir name and logo have a
separate [brand policy](logo/LICENSE.md). See the [license map](LICENSE.md)
for details and third-party files.
