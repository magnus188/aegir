# UI redesign plan

Target: the existing 480 × 800 portrait LVGL interface. Preserve the analyzer's measurement, calculation, calibration, history, cylinder, settings, and save behavior. Apply the selected dark instrument visual direction without changing gas analysis rules.

## Selected analysis direction

- Live readings and graph: large O2 and He readings, CO, and humidity in place of the O2 setup metric. Give the 60-sample O2/He graph substantially more space. Keep the mix summary and both save actions visible.
- Profile and planning: move the simulator gas presets, selected cylinder, gas-use mode, He override, planned depth, and calculated depth/density results to a second page. Swipe horizontally between the two pages, with a visible page indicator.
- Use a plain `Analyse Mix` header. Omit the Ægir wordmark and the standalone advisory card. Preserve warnings and fault information in the measurement status area; preserve the explicit distinction between simulated and hardware readings. Calibration remains accessible through Settings.
- Show humidity as `--` when the environmental sensor is unavailable. Simulator readings come from the selected gas profile rather than fixed display values.

## Main menu

- Use a prominent Analyse tile and four smaller destination tiles. Keep Analyse, Dive Planner, History, Cylinders, and Settings, along with the real Wi-Fi/battery indicators.
- Use one [Tabler outline icon family](../../assets/icons/tabler/README.md): flask, line chart, history, scuba tank, and horizontal adjustments.

## Implementation

- Implemented the menu and two analysis pages in the shared LVGL firmware UI. The two analysis pages use horizontal swipe navigation and visible page indicators; both save buttons remain fixed at the bottom.
- The live page shows O2, He, humidity, CO, a larger 60-sample chart, and the mix summary. The chart uses a 0–100% axis so high oxygen readings remain visible. The planning page holds the simulator profile selector, cylinder, gas mode, He override, planned depth, and calculated values. Temperature and pressure remain on that page.
- The status banner retains simulation provenance, sensor and calibration state, advisory text, and SD status. Missing environmental data displays `--`. Calibration remains available through Settings.

## Verification

- The host simulator build and UI smoke test cover all five menu routes, a simulated touch swipe in both directions, touch input for profile/mode/He/depth controls, fault and unstable save blocking, the Settings calibration path, averaged history save, and selected cylinder save.
- The ESP32-P4 v3 and pre-v3 application builds both link successfully and pass their partition-size checks.
- The analysis, history, cylinder, and UI host checks run without hardware. Physical touchscreen and sensor validation remain for device testing.
