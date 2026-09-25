# Software calibration — existing sensors, upgraded electronics

This revision keeps AO₂, R17JJ-CCR and MD62. It replaces the two ADC chips and the helium trimmer, and adds real acquisition, individual calibration records and a touchscreen workflow. **The accuracy targets remain unproven: ±0.2 percentage points O₂ and ±0.5 points He over the conditions actually tested.** No physical gas measurements were performed during this implementation.

The board remains under active electrical, routing and fit review. Use the [current system-review package](system-review/README.md) for order status and source-bound checks. Showcase, production printing and purchasing remain paused. The previous version of this document is available through Git history.

## What changed on the board

| Marking | Part / purpose |
|---|---|
| U401 | ADS122C04IPWR, TSSOP-16; reads the selected oxygen input, with both inputs available during an explicit known-air check; address **0x40** |
| U502 | ADS122C04IPWR, TSSOP-16; reads MD62 and voltage diagnostics; address **0x41** |
| RN501 | Vishay **ACASN2001U2001P1AT**, two matched 2 kΩ resistors making a fixed half-excitation reference |
| RV501, R502, R503 | Removed; the matched array replaces this adjustable branch |
| J401 / J402 | Existing AO₂ / JJ-CCR connectors in the approved positions; no sensor replacement |
| J501 | MD62 connection; regulated **3.0 V** excitation retained through TPS7A2030PDBVR |
| R506 / R507 | 680 Ω helium input resistors; acquisition profile revision 2 |

RN501 is a selected manufacturer order code; distributor stock and lead time still need checking. Its custom lands follow Vishay's IEC recommendation. Its nominal 3D body is drawing-derived, not exact manufacturer CAD. Use the [current part ledger](system-review/electrical/part-qualification.csv), native board markings and [passive analogue analysis](system-review/electrical/analogue-simulation/README.md). Earlier placement maps are historical checkpoints, not the current purchasing list.

The ADC samples a voltage. Software subtracts the measured zero offset and applies a measured sensitivity. A fixed reference is sufficient because software can correct the offset **provided the original voltage is inside the ADC's usable range**. It cannot recover a clipped signal, identify an unknown gas, or correct an unmeasured environmental dependency.

## Acquisition settings

Both converters use the internal 2.048 V reference, normal **20 samples/s conversion rate**, single-shot operation, and no IDAC or burnout excitation currents. AO₂ and JJ-CCR use gain 8 with the PGA enabled (±256 mV). MD62 uses gain 1 with PGA bypass (±2.048 V). Voltage diagnostics use gain 1 with PGA bypass.

The driver resets and checks registers, writes and reads back each configuration, allows 100 ms for analog settling, clears old conversion data, starts a new conversion and polls for fresh readiness with a timeout. A nominal conversion takes about 50 ms. The 100 ms filter delay is conditional on source impedance: the digital tolerance sweep cannot qualify it for an unmeasured sensor. Validate settling at the actual sensor, cable and configured channel before relying on that delay. **20 samples/s is the ADC setting, not the delivered rate for each gas:** sequential channels, voltage diagnostics, settling and bus/task delays make the published rate lower. No helium constants from the inspiration repository are used. [TI ADS122C04 datasheet](https://www.ti.com/lit/ds/symlink/ads122c04.pdf)

The dedicated external I²C bus uses GPIO28/29 and addresses 0x40/0x41. The touchscreen bus on GPIO7/8 remains separate. The acquisition task enables the MD62 after ADC startup checks and disables its heater on acquisition/excitation faults. An excitation fault is latched until restart. It monitors the board's 3.0 V excitation and the O₂ bias, but cannot measure voltage drop in the remote sensor cable.

The initial 180-second MD62 warm-up lockout and 10-second electrical-stability window are **provisional settings**, not evidence that the sensor has reached thermal equilibrium. O₂ has a provisional 10-second startup delay. The stability gates start at 10 µV peak-to-peak for O₂ and 30 µV for MD62, with a tighter early/late-window drift check; characterize these with the assembled system.

## Touchscreen procedure

1. From **Analyse → O2 setup**, confirm the one installed oxygen type. Mark **New / replacement cell** when replacing a cell, even with the same type. This stops acquisition, resets accumulated samples, settles the selected input and requires fresh calibration. The explicit known-air check displays both raw inputs; connector advice remains manual/inconclusive until qualified response bounds exist. Then open **Settings → Calibration** for the installed oxygen channel or **Helium (MD62)**. Each identity has independent draft points and saved corrections.
2. Apply the first known reference gas at the intended controlled flow and near-atmospheric chamber pressure. Enter its known O₂ and He percentages, reference/cylinder ID and, if available, the reference uncertainty in percentage points. The first point is a baseline; it is not automatically assumed to be zero.
3. Wait for warm-up, fault-free readings and a stable window. Capture the baseline. A true zero correction requires a suitable known zero gas; a baseline above zero makes the zero an extrapolation.
4. Apply a second known mixture with enough concentration and voltage separation, enter its composition/ID/uncertainty, wait again and capture the span. The draft needs at least 5 percentage points of concentration separation and a signal span comfortably greater than measured noise.
5. Review both points and select **Save channel calibration**. A rejected capture, unstable/stale/faulty reading, insufficient span or storage failure leaves the previous valid calibration in use. **Discard** clears only the draft.
6. Calibrate helium separately and calibrate another oxygen cell only when that cell is actually installed and selected. Verify persistence by restarting and rechecking a known gas. Simulator practice records are separate from hardware records.

The current correction is `percent = (voltage − zero_voltage) × sensitivity`. Hardware records are stored in NVS with per-channel identity, ADC acquisition configuration/gain, version/revision and CRC. A changed measurement transfer profile makes incompatible corrections inactive while retaining intact records. Compatible OTA updates preserve the installed type and calibration; unknown future formats fail closed. Historical retention is finite, so export reference information for a longer archive. An inactive record is written and verified before switching the active record. Two-point calibration remains a **bench correction**, particularly for MD62. It does not qualify the device's accuracy. Unknown reference uncertainty is allowed for exploration and remains explicitly unqualified; the record notes whether the entered uncertainty meets a provisional quarter-target allocation (0.05 points O₂ / 0.125 points He). This is only one contribution to the full uncertainty budget.

The physical live screen labels results as calibration/bench status and withholds derived gas claims while helium characterization is unvalidated. Normal measurement uses only the explicitly selected oxygen input; the other input is probed only for the requested known-air aid. A failed AO₂ reading is never replaced by JJ-CCR. Selection generations prevent an old completed conversion or reset from overwriting the newly selected channel history. A disconnected passive oxygen cell can resemble a true zero signal with this circuit, so electrical fault flags cannot detect every disconnected or failed cell. Known-gas functional checks remain necessary.

## MD62 characterization and validation checklist

All boxes below are **pending physical work**. Do not mark a box complete from CAD or software tests.

- [ ] Record actual sensor identifiers, installation orientation, cell age, reference analyzer model/serial/calibration status, certificate uncertainty, gas composition and cylinder/reference IDs.
- [ ] Condition the MD62 according to the applicable Winsen manual and storage history. The reviewed guidance calls for at least 8 hours after short storage and longer recovery after extended storage; check the exact storage-duration table rather than using the daily startup timer. [Winsen MD62 manual](https://www.winsen-sensor.com/d/files/thermal/md62.pdf)
- [ ] Verify connector polarity, 3.0 ±0.1 V at the MD62 **under load**, ADC supplies, bias and all ADC input voltages during power-on, power-off and sensor disconnection. Check input leakage/back-powering and analog settling after MUX/gain changes.
- [ ] Measure shorted-input noise and sensor-connected noise. Record raw codes and volts, clipping, supply ripple, repeated cold starts, warm-up trajectories and restart persistence. Keep analog input filters and local supply-capacitor return paths short during routing.
- [ ] Determine thermal warm-up from repeated cold starts and actual drift, including the final enclosure. Adjust the provisional timer/stability thresholds only from the measurements.
- [ ] Obtain dive-shop reference mixtures spanning the intended He range and **several O₂ fractions**, including zero-He mixtures. Repeat each measurement and change the measurement order; avoid fitting a curve to one sequence of warming/drift.
- [ ] Reserve entire mixtures and separate measurement runs as validation data **before fitting**. Use them only to test the proposed correction, not to tune it. Include independent days/restarts where practical.
- [ ] Record chamber temperature, humidity, pressure and flow, ambient conditions, elapsed heater-on time and battery voltage. Repeat on battery, USB-A→USB-C charging and USB-C→USB-C charging. Charge-system commissioning requirements still apply.
- [ ] Compare a simple linear helium correction with composition/environment-dependent candidates using the measured data. Select additional terms only if they improve held-out results and remain physically plausible. The firmware contains no fitted nonlinear MD62 helium curve yet because no measured dataset exists.
- [ ] Report bias, repeatability, worst observed error, environmental/gas range, number of independent runs and reference-analyzer uncertainty. Include reference uncertainty, repeatability, resolution, drift and environmental sensitivity in an uncertainty budget. A low ADC noise figure does not establish gas accuracy.
- [ ] Check oxygen-cell open/short/reversed and low-output behavior; compare both channels against known gas. Check MD62 heater/cable faults, stale ADC/CO data and fault recovery. Confirm safe shutdown and charging behavior separately.
- [ ] Validate actual elbows, mating plugs, solder, cable bends and battery disconnection in the enclosure before final placement/routing lock.

The firmware now emits raw `CSV1` characterization rows with individual sample timestamps, sensor identity, configuration, calibration revision, environment and faults. Convert a captured serial log using `python3 software/firmware/scripts/extract_characterization.py serial.log measurements.csv`; the extractor opens no device and performs no flashing. The physical BME280 driver is implemented; absent or faulted environment remains unavailable. Use the [manual bench-log template](characterization-log-template.csv) for reference-gas and instrument information alongside raw captures. No measured dataset exists yet. Calendar-age reminders and automatic aging-based recalibration prompts are not implemented.

## CO remains CO

The ZE07-CO UART decoder receives documented 9600-baud frames, checks framing/checksum, observes warm-up and rejects stale data. It sends **no recalibration commands**. Current CO readings are separate from CO₂, and legacy CO₂ history stays identified as CO₂ when records are migrated. No CO₂ concentration is inferred from MD62 helium mode. The ZE07's humidity range and manufacturer restrictions still apply. [Winsen ZE07-CO manual](https://www.winsen-sensor.com/d/files/manual/ze07-co.pdf)

## Digital verification and remaining scope

The current [verification package](system-review/verification/README.md) binds
both firmware builds, host/sanitizer tests, logical pin checks and UI captures
to source hashes. Earlier software-calibration integration evidence in Git history
remains a historical checkpoint; its footprint counts and DRC totals do not
describe the current board.

Real gauge, charger, BME280 and hardware shutdown integration is implemented
and build-tested. Software fault injection covers selection, stale frames,
calibration isolation, storage failure, maintenance and OTA behaviour. No
physical firmware was flashed or exercised on the new PCB. Final routed PCB
and updated Fusion imports must pass their own checks before an order release.

Neither O₂ nor He target accuracy is demonstrated. Power sequencing, source
impedance, actual filter settling, off-state input leakage, thermal drift,
reference-gas response and enclosure sealing remain physical work. Keep the
MD62 unless measured results demonstrate a limitation that better electronics
and evidence-based correction cannot resolve.
