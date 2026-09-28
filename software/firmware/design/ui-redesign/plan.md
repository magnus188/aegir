# UI redesign plan

Target: the existing 480 × 800 portrait LVGL interface. Preserve the analyzer's measurement, calculation, calibration, history, cylinder, settings, and save behavior. Apply the selected dark instrument visual direction without changing gas analysis rules.

## Selected analysis direction

- Live readings and graph: large O2 and He readings, CO, and humidity in place of the O2 setup metric. Give the 60-sample O2/He graph substantially more space. Keep the mix summary and both save actions visible.
- Profile and planning: keep the simulator gas selector in the top status banner. Put selected cylinder, gas-use mode, He override, planned depth, and calculated depth/density results on the second page. Swipe horizontally between the two pages, with a visible page indicator.
- Use a plain `Analyse Mix` header. Omit the Ægir wordmark and the standalone advisory card. Preserve warnings and fault information in the measurement status area; preserve the explicit distinction between simulated and hardware readings. Calibration remains accessible through Settings.
- Show humidity as `--` when the environmental sensor is unavailable. Simulator readings come from the selected gas profile rather than fixed display values.

## Main menu

- Use a prominent Analyse tile and four smaller destination tiles. Keep Analyse, Dive Planner, History, Cylinders, and Settings, along with the real Wi-Fi/battery indicators.
- Use one [Tabler outline icon family](../../assets/icons/tabler/README.md): flask, line chart, history, scuba tank, and horizontal adjustments.

## History, cylinders, and settings

- Use the dark 50 px header, outlined instrument tiles, cyan data accents, and restrained labels from the menu and analysis views across these three screens.
- History keeps the capture count, clear action, source and severity, gas readings, derived values, and oxygen sensor identity. Cylinder profiles keep selection, next/recheck/default controls, and the complete export label. Settings retains all seven destinations, with the existing inactive factory reset marked unavailable.

## Secondary screens

- Dive Planner, Wi-Fi, Software Update, Calibration, Safety Settings, and Device Settings use the dark shared header and the instrument tile, border, and cyan accent palette. Dive Planner keeps its controls and layout.
- Device Settings presents read-only storage, battery, and SD status in one card. It omits SD eject and retry actions because the card is inaccessible. Brightness and sleep use full-width segmented choices; brightness shows the saved value when it does not match either preset.

## Implementation

- Implemented the menu and two analysis pages in the shared LVGL firmware UI. The two analysis pages use horizontal swipe navigation and visible page indicators; both save buttons remain fixed at the bottom.
- The live page shows O2, He, humidity, CO, a larger 60-sample chart, and the mix summary. The chart uses a 0–100% axis so high oxygen readings remain visible. The simulator profile selector sits in the top status banner on both pages. The second page holds cylinder, gas mode, He override, planned depth, and calculated values, matching the device layout. Temperature remains on that page; ambient pressure is omitted from the UI.
- The status banner retains simulation provenance, sensor and calibration state, advisory text, and SD status. Missing environmental data displays `--`. Calibration remains available through Settings.
- History, Cylinders, and Settings use the shared instrument header and tile palette. The cylinder preview buffer is large enough to show the complete generated label.

## Verification

- The host simulator build and UI smoke test cover all five menu routes, a simulated touch swipe in both directions, touch input for profile/mode/He/depth controls, fault and unstable save blocking, the Settings calibration path, averaged history save, and selected cylinder save.
- The ESP32-P4 v3 and pre-v3 application builds both link successfully and pass their partition-size checks.
- The analysis, history, cylinder, and UI host checks run without hardware. Physical touchscreen and sensor validation remain for device testing.
- Touch-driven smoke checks cover Settings to Cylinders, profile selection including a scrolled row, Next, Recheck, Defaults, the complete label preview, History Clear, and back navigation.
- The UI smoke test checks the shared header palette on secondary screens and touch selection for brightness and sleep. Both ESP32-P4 build variants still pass partition checks.
