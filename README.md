# Ægir

ESP32-P4/LVGL firmware for Ægir, a prototype trimix gas analyzer, on the native-portrait Guition JC4880P443C_I_W (JC-ESP32P4-M3). It includes Wi-Fi, revision-specific HTTPS OTA, persistent settings/calibration, hardware sensor and power drivers, and a separate deterministic simulator. Physical firmware reports unavailable hardware instead of substituting simulated gas or battery readings.

**Prototype order status: HOLD.** Follow the [whole-system review](hardware/system-review/README.md) for current evidence and unresolved interfaces. No physical charging, thermal, sealing or reference-gas qualification has been completed. The accuracy targets of ±0.2 percentage points O2 and ±0.5 points He remain unproven. Charging is deliberately inhibited and J104 stays open until the cell, protection and temperature-sensing requirements are qualified.

## Project layout

The repository root is the main software workspace: firmware lives in `main/`,
with host tests in `tests/`, simulator support in `simulator/` and development
tools in `scripts/`. Mechanical and electronics files are grouped under the
[hardware overview](hardware/README.md): Fusion CAD is in `hardware/cad/`,
KiCad PCB projects are in `hardware/pcb/`, and Bambu Studio projects are in
`hardware/cad/rev04/3d-print/printing/bambu-studio/`.

## Quick Start with ESP-IDF

### Prerequisites
- [ESP-IDF v5.5.4](https://github.com/espressif/esp-idf/releases/tag/v5.5.4); `make setup-idf` installs the pinned environment
- Python 3.13
- Guition JC4880P443C_I_W: ESP32-P4 rev v1.3, 16 MB flash, 32 MB PSRAM, and ESP32-C6 coprocessor
- `g++`, CMake, pkg-config, and SDL2 development headers for full host validation

### Build and Upload
```bash
# See all common development commands
make help

# Run host-side validation
make test

# Install/check the exact ESP-IDF toolchain
make setup-idf
make doctor

# Identify whether the attached P4 is pre-v3 or v3+
make board-info PORT=/dev/cu.usbserial-110

# Build and flash; the port is required so the revision can be verified safely
make push PORT=/dev/cu.usbserial-110

# Build, flash, and monitor serial output
make push-monitor PORT=/dev/cu.usbserial-110

# Build and run the LVGL/SDL desktop simulator
make sim ZOOM=0.75
```

The first ESP32-P4 installation must be flashed over USB. An ESP32-S3 cannot
install this firmware through OTA because the chips and images are incompatible.

Run `make devices` to find likely serial ports. Firmware commands always source the pinned ESP-IDF v5.5.4 installation under `~/esp/v5.5.4/esp-idf`; an unrelated `idf.py` in `PATH` is never used. Override the installation location with `ESP_IDF_DIR=/path/to/esp-idf`.

The equivalent raw ESP-IDF commands remain available:

```bash
idf.py -B build/esp32p4-pre3 -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32p4;sdkconfig.defaults.esp32p4.pre3" set-target esp32p4
idf.py -B build/esp32p4-pre3 build
idf.py -B build/esp32p4-pre3 -p /dev/cu.usbserial-110 flash monitor
```

## Browser Demo

The production LVGL interface can also run in a browser through WebAssembly. The browser build uses the same screens, calculator logic, and deterministic simulator services as the native SDL simulator; hardware sensors, WiFi, battery state, NVS, and OTA installation remain simulated.

### Build locally

Activate an [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html), then run:

```bash
bash scripts/build_web_demo.sh
python3 -m http.server 8080 --directory web/build
```

Open `http://localhost:8080`. The generated site must be served over HTTP rather than opened directly from disk so the browser can load its WebAssembly module.

### Automatic GitHub Pages deployment

`.github/workflows/pages.yml` builds and deploys the demo after every push to `main`, and can also be run manually. Before the first deployment, select **GitHub Actions** under **Repository settings → Pages → Build and deployment → Source**. The project site will then be available at `https://magnus188.github.io/aegir/`.

### ESP-IDF Project Structure
```
├── CMakeLists.txt           # Main CMake configuration
├── sdkconfig.defaults       # ESP-IDF configuration defaults
├── main/                    # Main component source files
│   ├── main.cpp            # Application entry point and service startup
│   ├── services/           # WiFi, OTA, settings, battery, backlight
│   ├── sensors/            # Sensor abstraction and mock readings
│   ├── ui/                 # LVGL port, screens, styles, components
│   ├── idf_component.yml  # Component dependencies
│   └── CMakeLists.txt     # Component CMake file
├── tests/                  # Host-side C++ tests
├── scripts/                # Test and firmware size scripts
└── README.md               # This file
```

## Hardware Requirements

Use the [current electrical design](hardware/ANALYZER_DESIGN.md), [part qualification ledger](hardware/system-review/electrical/part-qualification.csv) and [main-board delivery index](hardware/system-review/electrical/main-final/README.md). These are prototype review files, not purchasing approval. Earlier enclosure/PCB snapshots preserve historical component choices.

### Controller and Display
- **ESP32-P4NRW32** application processor with 32 MB in-package PSRAM. Original pre-v3 boards are configured for their detected 16 MB flash; the v3 profile retains the 32 MB layout used by newer hardware.
- Onboard **ESP32-C6** WiFi 6/BLE coprocessor connected over SDIO.
- Native **480x800 MIPI-DSI IPS display** with ST7701 controller and GT911 capacitive touch.

### Power, Charging, and Battery
- **GCT USB4720-03-A** on a separate thin USB board, with TUSB320LAI CC detection and PI3USB9201 BC1.2 detection on the main board. USB-A-to-C and C-to-C behaviour remains subject to the documented source-current and startup tests.
- **BQ25895RTWR** 1S power-path charger, with TPS259470 overvoltage protection and TPS22950-Q1 input limiting. No USB-PD or high-voltage negotiation is provided; charging remains inhibited during commissioning.
- **Protected FMA FPML1S2P050C holder**, using its protected output. Holder/protection ratings, cell identity and NTC mounting require qualification.
- **TPS63020** configured for 5 V, with LM66100 host reverse isolation. Guition JP1 power entry remains a separate unresolved interface; do not power it through an unqualified harness.
- **TPS7A2030** regulated 3.0 V supply for the MD62.
- **LTC2954** hardware push-button shutdown plus firmware-managed normal shutdown.
- **MAX17048** 1S I2C fuel gauge.
- **2x 18650 cells in 1S2P**, owner-stated 3400 mAh each; no validated cell charge limits are inferred from capacity.
- **Normally-open momentary power button**; its final mechanical part and sealing remain unqualified.

### Sensors and Analog Front End
- Two **ADS122C04IPWR** 24-bit ADCs in TSSOP-16: U401 oxygen at 0x40 and U502 helium at 0x41. Initial profiles use 20 samples/s and the internal 2.048 V reference.
- Actual **BME280** breakout for humidity, temperature and pressure. A BMP280 cannot provide humidity.
- Retained **MD62 thermal-conductivity sensor** on regulated 3.0 V, with fixed matched bridge reference. Its helium response and compensation must be characterized with reference mixtures.
- **AO2 or R17JJ-CCR**, one installed oxygen sensor at a time. J401 and J402 have separate differential inputs; touchscreen selection and calibration identities persist independently.
- **ZE07-CO experimental module**, measuring CO rather than CO2. It is not a breathing-gas safety certification device. Its manufacturer excludes applications involving human safety; see the [CO review and manual](hardware/system-review/electrical/co-module-use-review.md). No undocumented recalibration commands are enabled. Fresh, valid chamber conditions and module warm-up are required before its reading is displayed.

### Wiring Notes
```
Signal/Rail       | Current hardware expectation
------------------|------------------------------------------------
External I2C      | GPIO28 SDA / GPIO29 SCL; managed 100 kHz sensor/power bus
Analog gas inputs | AO2/JJ differential paths and MD62 bridge into ADS122C04
HOST_3V3          | Supplied by Guition; external load margin remains unqualified
3.0 V rail        | MD62 through TPS7A2030; measure voltage at actual sensor leads
USB-C VBUS        | Protected/limited 5 V path to BQ25895
Display I2C       | GPIO7 SDA / GPIO8 SCL; separate touch/audio board bus
Onboard SD        | Slot 0: CLK43 CMD44 D0-3=39-42; LDO4 card supply
C6 SDIO           | Slot 1: CLK18 CMD19 D0-3=14-17; shared-controller arbitration
```

The [electrical/firmware/harness contract](hardware/system-review/electrical/interface-contract.md) defines logical contact mapping. The [owner photo and mechanical registration](hardware/system-review/integration-photo/README.md) place the Guition header and both USB sockets at the top and SD access on the left. Mated connector heights, cable routes and service access need their own clearance evidence. Display pins, timings and the ST7701S initialization table live in `main/board/`.

## Software Architecture

### Key Components

#### 1. Main Application (`main/main.cpp`)
- Persistent service startup
- Native display and screen initialization under the LVGL adapter lock
- Background service startup after the UI is ready

#### 2. LVGL Port (`main/ui/lvgl/lvgl_port.cpp`)
- Guition MIPI-DSI/ST7701S manual startup sequence
- Native GT911 touch registration
- Triple-buffered tear avoidance and LVGL adapter locking

#### 3. Sensor Interface (`main/sensors/sensor_interface.cpp/h`)
- Abstracted sensor reading functions
- Separate physical acquisition and deterministic simulator backends
- Persistent sensor selection and versioned calibration; stale, clipped and faulty samples are rejected

#### 4. Screen Management (`main/ui/screens/screen_manager.cpp/h`)
- LVGL-based UI screens
- Navigation between screens
- Settings, WiFi, update, and dive planner screens

#### 5. Services (`main/services/`)
- WiFi scanning/connection and saved credentials
- OTA update checks and firmware installation
- Settings, battery status, and backlight control
- Analysis history, persistent cylinder profiles, and export-ready gas label payloads

#### 6. Board Support
- In-tree Guition JC4880P443 display, backlight, and touch layer
- Separate pre-v3 and v3+ ESP32-P4 build profiles
- Native backlight, display, touch, and onboard ESP32-C6 integration

## Display Configuration

### ESP32-P4 Display
- **Panel**: 480x800 native portrait
- **Framework**: LVGL
- **Touch**: Capacitive (GT911)

### Screens

1. **Home Screen**
   - Main menu with navigation buttons
   - System status display
   - Version information

2. **Analyse Screen**
   - Gas analysis panel with physical and separately identified simulator sources
   - O2, He, experimental CO, environmental readings, stability, trend chart, planned depth, MOD, density, gas-use mode and averaged capture controls
   - Captures require stable samples and save an averaged analysis result to history
   - Stable averaged readings can update the selected cylinder profile and prepare an export-ready label payload

3. **Dive Planner Screen**
   - Gas planning calculator views
   - Partial-pressure top-up calculator for O2, helium, and air additions
   - Production calculator logic covered by host tests

4. **History Screen**
   - Captured analysis records with gas-use mode, mix fractions, CO, planned depth, MOD, density and advisory state; legacy CO2 fields remain distinct and are not relabelled as CO

5. **Cylinder Profiles Screen**
   - Persistent cylinder slots with selected cylinder, recheck state, stored mix, gas-use mode, planned depth, and label preview
   - Label text and CSV payloads are generated in firmware so later WiFi, QR, BLE, or phone handoff export can reuse the same data model

6. **Settings Screen**
   - Device settings navigation
   - Cylinder profiles, WiFi, software update, safety settings, calibration, and device information entrypoints

7. **WiFi Screen**
   - Network scanning, connection, password modal, and disconnect controls

8. **Software Update Screen**
   - GitHub release check and OTA install flow

9. **Calibrate Sensors Screen**
   - Guided known-gas oxygen and helium calibration with stability/fault checks and separate AO2/JJ/He records; failed saves retain the previous calibration

10. **Safety Settings Screen**
   - User-configured PPO2, density, and CO2 advisory limits used by Analyse

11. **Device Screen**
   - Device information and configuration controls

## Building and Flashing

### Prerequisites
- **ESP-IDF v5.5.4** installed with `make setup-idf`
- **ESP32-P4** toolchain and Python 3.13
- **Host validation tools**: `g++`, CMake, pkg-config, SDL2 development headers
- USB-C cable for programming

### Build Steps
```bash
# Inspect the attached chip and select the safe image family
make board-info PORT=/dev/cu.usbserial-110

# Run host tests
./scripts/run_tests.sh

# Build both incompatible P4 revision profiles
make build-all

# Validate firmware size against partitions.csv
make size-check P4_REV=pre3
make size-check P4_REV=v3

# Flash to device
make push PORT=/dev/cu.usbserial-110

# Monitor output
make monitor PORT=/dev/cu.usbserial-110
```

### Dependencies
The project uses ESP-IDF component manager for dependencies (defined in `main/idf_component.yml`):
- `lvgl/lvgl` 9.4.0 - Graphics library
- `espressif/esp_lcd_touch_gt911` 1.2.x - capacitive touch controller
- `espressif/esp_lvgl_adapter` 0.1.4 - task, input, and tear-safe display integration
- `espressif/esp_wifi_remote` 1.6.0 and `espressif/esp_hosted` 2.12.9 - onboard C6 networking

Dependencies are automatically downloaded during the build process.

## Sensor Hardware and Calibration Status

Physical firmware implements ADS122C04 acquisition, environmental and power monitoring, sensor selection and calibration persistence. Host tests exercise faults and state transitions; they do not establish actual sensor performance, wiring or charging behaviour. The simulator remains explicitly identified and does not substitute readings in physical firmware.

AO2, JJ-CCR and helium have separate records. Replacing a sensor requires calibration. Helium corrections must come from measured reference mixtures and held-out validation, including environmental and oxygen cross-effects. Historical/simulated CO2 fields remain distinct; this build has no installed CO2 measurement module. See the [software review](hardware/system-review/software/README.md).

## Performance Characteristics

### Display Pipeline
- **Panel**: native 480x800 RGB565 over two-lane MIPI-DSI
- **Tear avoidance**: three full panel framebuffers with partial LVGL rendering
- **LVGL refresh period**: 15 ms
- **Rotation**: none; pixels and touch coordinates stay in native portrait orientation

### Power Consumption
Whole-device active, idle, charging and hardware-off consumption remains unmeasured. Qualify backlight, radio, SD writes, heaters, converters and cable losses together; a display-only typical value is not a system budget. See the [bench equipment and test plan](hardware/system-review/lab-equipment.md).

## Differences from Original

### Enhanced Features
1. **480x800 portrait touch display**
2. **Capacitive touch**
3. **Tear-safe MIPI-DSI output** with no full-frame software rotation
4. **Software-defined sensor selection and calibration**
5. **Versioned OTA and persistent storage**; boot-time and power comparisons remain unmeasured

### Current Limitations
1. Physical fit, source protection, cell charging, thermal behaviour and gas accuracy remain unqualified; PCB order readiness is on hold.
2. Simulator streams support repeatable UI/logic tests; they are not physical measurement evidence.
3. Settings, calibration and recent history remain in NVS. [Optional onboard SD logging](hardware/system-review/software/sd-card/README.md) automatically records analysis/calibration sessions, raw samples and events. Device Settings provides status, safe eject and retry. Digital tests pass; actual card operation, power loss and simultaneous Wi-Fi use remain untested. Absent or faulty media leaves local analysis available.

## Development Notes

### Code Organization
- Sensor interface abstraction allows host tests and later hardware drivers.
- Modular screen design keeps LVGL screens separated by workflow.
- Services isolate WiFi, OTA, settings, battery, and backlight behavior.
- Board-level display and touch details are implemented in `main/board/guition_jc4880p443.c`.

### Testing Strategy
- `./scripts/run_tests.sh` runs static safety checks, production gas calculator tests, version consistency tests, and the host LVGL simulator smoke test when SDL2 is available.
- `./scripts/check_firmware_size.sh` validates the built app binary against the configured factory/OTA app partition.
- Hardware acceptance covers touch corners and drag axes, 100 navigation operations, a one-hour active UI/WiFi soak, an eight-hour idle soak, and two consecutive OTA upgrades.

## Troubleshooting

### Common Issues

1. **Display not working**
   - Verify the qualified 5 V power entry and source-current budget; follow the staged first-power procedure
   - Verify display cable connections
   - Confirm the PCB/module markings read `Guition JC4880P443` / `JC-ESP32P4-M3`
   - Confirm `make board-info` and the selected build profile agree

2. **Touch not responding**
   - Check the GT911 I2C bus on GPIO7 SDA / GPIO8 SCL
   - Confirm the board is physically mounted in native portrait orientation
   - Ensure proper grounding

3. **Sensors not reading**
   - Check I2C bus connections
   - Check device identities and bus errors in firmware diagnostics; use the documented I2C pin/address map
   - Check power supply to sensors

4. **Build errors**
   - Run `make doctor` and confirm ESP-IDF v5.5.4 with Python 3.13
   - Run `make clean`, then rebuild the correct P4 revision profile
   - Check that Git dependencies and `dependencies.lock` are available

### Debug Commands
```bash
# Read firmware diagnostics; no generic I2C-scan console is assumed
idf.py monitor

# Memory usage
idf.py size

# Real-time monitoring
idf.py monitor --decode-crashes
```

## Contributing

When adding new features:
1. Follow ESP-IDF coding standards
2. Keep display/touch changes inside the board-port boundary; do not add manual rotation
3. Add screen navigation in `main/ui/screens/screen_manager.cpp`
4. Test with both real and mock sensors
5. Update documentation

## License

A repository-wide open-source license has not yet been selected.
