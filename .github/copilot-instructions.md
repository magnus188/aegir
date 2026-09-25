# Ægir - Repository Instructions

Ægir is an ESP32-P4 trimix analyzer firmware project built with ESP-IDF and LVGL. It targets the native 480x800 Guition JC4880P443C_I_W with GT911 touch and an onboard ESP32-C6 Wi-Fi coprocessor.

## Working Effectively

### Prerequisites
- The pinned ESP-IDF v5.5.4 environment installed by `make setup-idf`.
- ESP32-P4 toolchain and Python 3.13.
- `g++` for host-side unit tests.

### Core Commands
```bash
make test
make build-all
make size-check P4_REV=pre3
make size-check P4_REV=v3
```

Use Makefile targets for firmware commands. They source the pinned environment and isolate pre-v3 and v3+ builds.

## Validation Rules
- Always run `./scripts/run_tests.sh` after code changes.
- For firmware changes, run `idf.py build` and `./scripts/check_firmware_size.sh`.
- Do not use copied production logic in tests; host tests should compile production `.cpp` files directly where possible.
- Do not add UI changes or new user-facing features when the task is under-the-hood hardening.
- Keep public C APIs stable unless a task explicitly requests a breaking change.

## Project Structure
```text
main/
  main.cpp                         # startup ordering
  services/                        # WiFi, OTA, settings, battery, backlight
  sensors/                         # sensor abstraction and mock readings
  ui/
    lvgl/                          # display/touch port
    screens/                       # LVGL screens
    components/                    # navbar and status icons
    styles/                        # colors and font selectors
    fonts/                         # generated LVGL custom fonts
tests/                             # host-side C++ tests
scripts/run_tests.sh               # host validation
scripts/check_firmware_size.sh     # app partition budget check
.github/workflows/                 # CI and release automation
```

## CI/CD
- Pull requests and non-main pushes run host tests, ESP-IDF build, firmware size check, and artifact upload.
- Pushes to `main` keep the current automatic release behavior after version bumping, duplicate-tag checks, tests, ESP-IDF build, and size validation.
- The app partition budget is defined by `partitions.csv`; firmware must fit in the configured factory/OTA app partition.

## Hardware Smoke Test
After firmware-level changes, test on the Guition JC4880P443C_I_W device:
1. Boot to Home.
2. Navigate Home, Settings, WiFi, Software Update, and Dive Planner.
3. Run WiFi scan twice.
4. Open and close the WiFi password modal.
5. Verify WiFi and battery status icons update.
6. Exercise 100 navigations and verify no touch takes more than 150 ms to show its destination.
7. Run the one-hour active soak and eight-hour idle soak without flicker, tearing, resets, or heap loss.
8. Perform two consecutive OTA upgrades to exercise both OTA slots.
