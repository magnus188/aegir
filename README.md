![Ægir 1 logo](logo/aegir-lockup-dark.png)

Ægir is a work-in-progress trimix gas analyzer for technical diving. It is not
qualified as a breathing-gas safety instrument; use it at your own risk.

## The name

In Norse mythology, Ægir (/ˈæːɣir/) is the giant who rules the sea. He is
known for hosting the gods in his halls adorned with gold and, together with
Rán, is the father of nine daughters who personify the waves.

The runes ᛅᛁᚾ are a Viking Age-style representation of einn, Old Norse for
“one”, marking the first version of the project: Ægir 1.

## Features

- Oxygen (O₂) and helium (He) sensor inputs, plus experimental CO monitoring
- 4.3-inch touch screen for gas analysis, calibration and settings
- Mix calculations, dive planning and analysis history
- Desktop simulator with demo readings

## Screenshots

These screens show the simulator with demo data.

| Analysis | Calibration | Device settings |
| :---: | :---: | :---: |
| <img src="hardware/system-review/verification/ui-current/analysis-live.png" alt="Mix analysis screen" width="240"> | <img src="hardware/system-review/verification/ui-current/calibration-overview.png" alt="Calibration screen" width="240"> | <img src="hardware/system-review/verification/ui-current/device-status.png" alt="Device settings screen" width="240"> |

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
- [Main PCB](hardware/pcb/analyzer/Trimix_Analyzer.kicad_pro) and [USB input PCB](hardware/pcb/usb-input/Trimix_USB_Input.kicad_pro)
- [Enclosure CAD](hardware/cad/rev04/3d-print/Trimix_Enclosure_A3_PrintReview.f3d) and [3D-print files](hardware/cad/rev04/3d-print/printing/)
- [Logo](logo/README.md)

## License

Software: [GPLv3](LICENSES/GPL-3.0-only.txt). PCB and CAD files:
[CERN-OHL-S-2.0](LICENSES/CERN-OHL-S-2.0.txt). The Ægir name and logo have a
separate [brand policy](logo/LICENSE.md). See the [license map](LICENSE.md)
for details and third-party files.
