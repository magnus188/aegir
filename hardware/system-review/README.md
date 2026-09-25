# Trimix whole-system review

**Prototype order status: HOLD.** The main PCB is fully routed and its frozen native DRC reports zero violations, unconnected items and schematic-parity issues. This package records the corrections and reproducible digital evidence; it is not a fabrication release. Factory processes, power-entry and protection evidence, and actual component interfaces must close before ordering. Physical charging, temperature, sealing, module-fit and reference-gas tests have not been performed.

The approved A3 exterior and current mechanical review copies are preserved. The earlier PCBFit workspace is available through Git history. Purchasing, showcase work and production printing remain paused.

**2026-09-08 integration update:** the owner supplied a rear photograph with device-up confirmed and measured **13.4 mm from front glass to bare header tips**. The photo locates the Guition header and both USB ports at the top and the onboard SD socket at the left. The [photo registration](integration-photo/README.md) and [saved/reopened ConnectorReview v3](mechanical/connector-review/README.md) supersede the old illustrative connector locations. Complete mate allocations expose interference with the housing, carrier and holder; several straight cable plugs also leave inadequate wire-bend space. Local correction candidates are unadopted, and card service remains unqualified. Earlier v11 clearances do not prove fit of these interfaces. [Optional SD logging](software/sd-card/README.md) is implemented and digitally verified, using the onboard socket. The [bench-equipment record](lab-equipment.md) includes the confirmed multimeter, scope, DC PSU, DL24/P, DLA and FNB58.

The current main board is the reviewed `0962ad86` revision. Its charger bootstrap/REGN and CO-converter feedback/output-capacitor paths were shortened, while all 29 previously checked power/monitor paths were preserved. The [scoped power-layout review](electrical/routing-candidate/sensitive-layout-refinement/root-final-review/README.md) and independent manufacturing checks pass. Earlier routing candidates are available through Git history. Regulator stability, noise, startup and thermal performance still require physical evidence.

The preserved **SystemReview v11 baseline** is saved and its native archive reopens with the same geometry, parameters and assembly associations. It retains the approved 85 × 180 × 43 mm exterior and exact CNC Kitchen insert models. Two carrier through-openings clear the revised rear components with complete local 2 mm material borders. Its selected component, connector, service, gas and fastener allocations pass at 85 and 87 mm widths, with the intended 85 mm design restored. Both oxygen reference configurations pass their bounded checks. See the [v11 checkpoint](mechanical/final-local-checkpoint/final-handoff.json). The later ConnectorReview findings above take precedence for actual Guition mating and cable-space decisions.

The September 8 checkpoint recorded two P4 builds, **53 normal and 53 sanitized host tests**, 98 repository checks and 22 static-analysis units. Its [software receipt](software/sd-card/evidence/receipt.json) is point-in-time evidence: software source paths changed during this workspace cleanup, so it must be refreshed before it can bind a new build. The current checkout passes 94 host checks with `make test`; simulator and Hosted patch integration need downloaded ESP-IDF managed components. CO requires fresh chamber temperature/humidity within its documented range; invalid conditions clear its displayed value while oxygen and helium stay independent. The [CO verification](software/co-environment/README.md) records scoped tests and [manufacturer use restrictions](electrical/co-module-use-review.md). The CO module remains experimental and cannot certify breathing-gas safety.

Analysis and calibration record results, raw samples and events automatically. The result log identifies both applied O2 and He calibration revisions; concurrent saves cannot mislabel those converted values. Card faults remain separate from NVS settings/calibration. SD safe eject, failed unmounts, session transitions and shutdown preparation have digital coverage. Real card operation, simultaneous radio traffic, memory headroom, write-current peaks and abrupt-power-loss behavior remain physical checks.

The preserved v11 STEP is a reference export with a documented limitation (CAD09): body count, bounds and selected surface/section checks pass, but strict volume comparison and one underlying-plane correspondence do not establish full per-body equivalence. It also predates the photo-correct Guition details. Native Fusion checkpoints carry their own revision and source scope; the old STEP is not the new connector model.

The earlier v11 PLA/PETG diagnostics passed 50 slice checks for four fit-study parts; their interim slice outputs are in Git history. Support removal, printed dimensions, retention, sealing and temperature tests remain pending. [The acceptance ledger](acceptance.csv) separates completed digital work from the **13 remaining order-blocking gates**; those gates are related and are not 13 demonstrated hardware failures.

## Read the package

| Area | Starting document / evidence |
|---|---|
| Check-by-check acceptance and unresolved gates | [Acceptance ledger](acceptance.csv) — scoped statuses and the remaining order gates |
| Electrical interface, pin audits and calculations | [Electrical review](electrical/README.md), [logical interface](electrical/interface-contract.md) |
| Exact purchasing and assembly populations | [Part qualification ledger](electrical/part-qualification.csv) — unresolved entries are not purchasing approval |
| USB source rules and legacy limitations | [Current USB-IF source review](electrical/usb-default-current-review/README.md), [limiter alternatives and rejected assumptions](electrical/usb-protection-research/transient-options/README.md) |
| Screen cable and power entry | [Matched harness review](electrical/host-harness-review/README.md), [Guition power and service isolation](electrical/host-power-review/README.md) |
| Fabrication and routing | [Manufacturing constraints](electrical/manufacturing-and-routing.md), [selected stackup](electrical/stackup-review.md), fresh ERC/DRC JSON files in `electrical/` |
| Current main-board files | [Routed main delivery index](electrical/main-final/README.md) — Gerbers, drills, factory/manual BOMs, factory-only stencil, marking guide and selected-hole process map; order HOLD |
| Independent manufacturing-file review | [Final CAM review](electrical/main-final-independent/final-0962ad86/README.md) — 44 geometry/connectivity, 20 assembly-ledger, 12 preservation and 50 process checks pass; supplier process approval remains pending |
| Frozen routed main PCB | [Independent final power and ground review](electrical/routing-candidate/sensitive-layout-refinement/root-final-review/README.md) — 29 prior power/monitor paths retained, 35 local routes connected, one connected In1 reference and no lost ground contacts; physical performance remains unqualified |
| Protection and process checks | [Independent mask/drill and rule controls](electrical/vippo-process-review/native-v3-independent/README.md) — factory process acceptance remains open |
| Copper-path diagnostic | [Method and limits](electrical/current-path-review/README.md) — geometry sensitivities, not equivalent resistance or current ratings |
| Supplier questions prepared for later use | [Unsent process-review request](electrical/supplier-review-request.md) — no supplier contacted or order placed |
| Mechanical model and measurements | [Current enclosure review](mechanical/README.md), [final saved assembly](mechanical/final-local-checkpoint/final-handoff.json), [measurement checklist](mechanical/MEASUREMENTS.md) — provisional models stay labelled |
| Oxygen setup and calibration | [Oxygen implementation and UI evidence](software/oxygen/README.md) |
| OTA, storage and maintenance | [OTA review](software/ota/README.md), [concurrent-save evidence](software/ota/storage-maintenance-race-verification.json) |
| Onboard SD recording and shared controller | [Use, implementation and limits](software/sd-card/README.md), [September 8 receipt](software/sd-card/evidence/receipt.json), [independent receipt binding check](integration-photo/sd-software-binding-check.json) |
| Production sensor, power and environmental acquisition | [Firmware integration](software/README.md), [input isolation and charging standby](software/power/input-isolation/README.md), [legacy attachment endurance](software/power/input-isolation/legacy-endurance-review/README.md) |
| Combined tests and firmware builds | [Dated verification records](verification/README.md); rerun against the new software paths before relying on their source hashes |
| Before wiring and first power | [Measurement and bring-up checklist](electrical/measurement-and-bringup.md), [available instruments](lab-equipment.md), [blank test record](test-record-template.csv) |
| Step-by-step bench sequence | [Draft first-power procedure](first-power.md) — all physical stages pending |
| Illustrated logical wiring and measurements | [Six-page interface packet](interface-packet.pdf) - reference only; actual mating views still need confirmation |
| Passive tolerance/noise and power-model limits | [Analogue study](electrical/analogue-simulation/README.md), [power-model probe](electrical/power-simulation/README.md) |

## Acceptance language

Every check uses one of these outcomes:

- **Passed digitally:** the identified calculation, source inspection, simulation, host test or native CAD/EDA check passed for its stated inputs and revision.
- **Correction required:** a demonstrated failure or unfinished implementation remains.
- **Missing evidence:** a needed dimension, rating, manufacturer datum or process qualification is not established.
- **Physical testing pending:** bench, fit, thermal, gas, electrical or sealing evidence is still required.

A host test can prove that a bus error is rejected; it cannot prove the physical bus signal integrity. An interference check can prove that the supplied solids do not overlap; it cannot prove that an unmeasured plug has the correct solid model. ERC and DRC alone do not establish charging safety or gas accuracy.

## Decisions retained

Only one oxygen sensor is installed. Both differential input paths remain, with the approved J401/J402 order. Touchscreen selection persists separately from calibration; GPIO52/J301.7 is unused. A new or replacement cell requires calibration. No analogue voltage is treated as a digital sensor identity and measurement never changes inputs automatically.

The MD62 remains on a regulated 3.0 V excitation supply. ADS122C04 measurement electronics replace the two older ADCs; the gas sensors are retained. The four-layer main board and separate thin GCT USB daughterboard remain the manufacturing targets. The humidity module must actually contain a BME280; a BMP280 cannot measure humidity. The installed ZE07 module measures CO, not CO₂.

Current firmware deliberately inhibits charging and leaves the hardware charge-arm connection J104 open. Exact cell charge limits, holder/plug ratings and the temperature-sensing arrangement must be qualified before a charging profile is enabled. This commissioning restriction is explicit; charging has not been certified by the software or schematic review.

The ±0.2 percentage-point oxygen and ±0.5-point helium values are **unproven targets**. Characterization must include independent reference mixtures, the reference instrument's uncertainty, oxygen cross-effects, environment, repeated measurements and battery versus USB operation. No correction coefficients or accuracy claims are copied from another analyzer.

## Attribution and scope

The project remains inspired by [captainigloo/Trimix-analyzer](https://github.com/captainigloo/Trimix-analyzer). Existing repository attribution and licensing are retained; this review makes no repository-wide licensing change. Bosch's BME280 driver is vendored with its BSD-3-Clause notice and pinned source manifest. Host parser tests use the same cJSON source as the local ESP-IDF dependency, retaining its licence; device firmware links ESP-IDF's JSON component.

Manufacturer documents, downloaded reference models and third-party CAD are evidence with their own terms, not newly relicensed project designs. Source links and hashes accompany the respective electrical, mechanical and software checks.
