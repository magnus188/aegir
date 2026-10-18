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

<img src="media/renders/aegir-portrait.png" alt="CAD-based render of the current Ægir prototype: black printed enclosure, flush white Ægir symbol and runes, side gas fitting and illuminated power button, with the live-analysis simulator on its portrait touchscreen" width="520">

*CAD-based render of the current prototype.*


<img src="media/renders/aegir-exploded.png" alt="Exploded CAD-based render showing the enclosure, display assembly, two-cell battery holder, populated KiCad PCB with bottom USB-C, sensor manifold, reconstructed sensors, rear cover and fasteners" width="960">

## Screenshots

These screens show the simulator with demo data.

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

## Contributing and future ideas

Design reviews, measurements, documentation and software fixes are welcome.
Start with the [contribution guide](CONTRIBUTING.md) and
[open issues](https://github.com/magnus188/aegir/issues). The
[v2 investigation backlog](https://github.com/magnus188/aegir/milestone/1)
collects community suggestions without promising a release date.

Maintainers can use the [repository guide](.github/MAINTAINING.md). For
exploitable vulnerabilities, use the [private reporting route](SECURITY.md).

## Acknowledgements

A big thank you to [@tobiasruber](https://github.com/tobiasruber) for reviewing and
improving the PCB design. His help is a big reason this project is still alive
and moving forward.

## License

Software: [GPLv3](LICENSES/GPL-3.0-only.txt). PCB and CAD files:
[CERN-OHL-S-2.0](LICENSES/CERN-OHL-S-2.0.txt). The Ægir name and logo have a
separate [brand policy](logo/LICENSE.md). See the [license map](LICENSE.md)
for details and third-party files.
