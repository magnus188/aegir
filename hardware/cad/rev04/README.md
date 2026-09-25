# Trimix enclosure A3 - Revision 04

The separate [PrintReview build package](3d-print/) contains the later PLA
fit/PETG enclosure refinements, print projects and community assembly
documentation. This page and the files beside it describe the preserved A3 v3
baseline. Use the PrintReview package's own verification record for its revised
geometry and exports.

**Status: editable engineering concept completed and reviewed; physical manufacture remains unqualified.** A3 has a **180 H x 85 W x 43 D mm body**, with measured display and occupied battery-holder envelopes retained. The complete modeled assembly is **180 H x 97 W x 43 D mm**, including the opposed gas-fitting references. Native Fusion and STEP are saved, and the STEP geometry-import verification passed. Revision 03/A2 remains available through Git history.

This trades depth for a taller, wider body. Against A2's 125 H x 75 W x 56 D mm body, A3 is **23.2% thinner** but has **25.3% more body bounding-box volume** (657.9 versus 525.0 cm³). These are exterior rectangular-envelope comparisons, not material or internal free-space volumes. The measured factory display casing occupies **52.9% of the 180 x 85 mm front rectangle**; that percentage is not the illuminated screen area.

The front contains the portrait 4.3-inch screen in its existing factory casing with the factory back removed. The owner measured 116.8 H x 69.3 W x 13.7 D mm. The protected FMA 1S2P holder with two 3400 mAh cells measures 80.4 H x 42 W x 20.35 D mm. Battery and future PCB sit side by side behind the display. The sample cartridge occupies the taller upper region; the power button is on the viewer-left side, USB-C is on the bottom, and opposed gas fittings connect at the upper sides. Coordinates remain +X viewer-left, +Y up, +Z rearward.

The PCB strip at the current supplied datums is **30 x 99 mm**, X 50.4..80.4, Y 21..120, Z 20.5..22.1, before an upper button notch and lower USB-service notch. It is a mechanical allocation, not a revised KiCad layout.

[USB provenance](components/GCT_USB4720_A3_PROVENANCE.md) and [sensor provenance](components/SENSOR_PROVENANCE.md) identify the manufacturer drawing references and unmeasured allowances. The GCT connector and sensor models are editable reconstructions, not authenticated manufacturer CAD.

## Delivered files and actual checks

- [Editable native Fusion archive](Trimix_Enclosure_A3.f3d), also saved as `Trimix_Enclosure_A3` in the Trimix analyzer cloud folder.
- [STEP assembly](Trimix_Enclosure_A3.step); geometry-import verification is discussed below.
- [Three-sheet A3 review PDF](Trimix_Enclosure_A3_Review.pdf), with front/rear/side views, rear-open view, two actual sections and an exploded assembly.
- [Parts list](PARTS.csv) and [assembly selection sets](verification/parts-and-assemblies.json). Generic hardware references are not supplier order codes.

| Check | Actual result and scope | Evidence |
| --- | --- | --- |
| Native model | 102 placed solid bodies, 66 occurrences, 44 component definitions and 1,006 timeline entries. All inspected entities healthy and all sketches fully constrained; zero positive-volume overlaps above 0.00001 mm³. | [Model audit](verification/model-audit.json) |
| Printed-part connectivity | Eight explicitly selected printed components each contain one solid. | [Model audit](verification/model-audit.json) |
| Individual hardware | 14 screw and 14 insert occurrences, with 28 parameter-driven mounting joints. | [Hardware and selected sections](verification/selected-wall-fastener-checks.json) |
| Selected wall sections | 20 deliberate material segments meet their selected nominal 2 mm gate, checked at intervals of at most 0.1 mm. This is not a global wall-thickness result. | [Selected checks](verification/selected-wall-fastener-checks.json) |
| Fastener engagement and approach | All 14 nominal screw/insert pairs pass the selected geometric checks; all 14 nominal driver shafts clear with the stated assembly-removal prerequisites. | [Fastener checks](verification/selected-wall-fastener-checks.json), [driver checks](verification/service-drivers.json) |
| Service motion | All eight specified service paths clear exact BRep tests at poses no more than 1 mm apart. Cables, hands and latch operation are unmodeled. | [Final eight-path report](verification/service-paths-cover-disconnect-battery-carrier-chamber-retainers-display-usb.json) |
| Gas passage | Continuous 5 mm cylinders and corner spheres clear all placed solids; the selected channel-dimension checks pass. This is geometric clearance, not a gas-flow or sensor-response result. | [Gas clearance](verification/gas-clearance-paths.json) |
| Parameter regeneration | Independent width 87 mm, height 182 mm and depth 45 mm trials pass health/static-clearance and fixed-part dimension checks. Original geometry restored. | [Regeneration](verification/parameter-regeneration.json) |
| Views and PDF | Nine actual Fusion view images approved; all three A3 PDF pages rendered and visually inspected without layout defects. | [View QA](verification/viewQA.json), [PDF QA](verification/PDFQA.json) |
| Saved exports | Native archive and STEP saved; native display units are mm. STEP import passes with 102 solids, 66 occurrences and exact overall bounds. Matching High-accuracy mass-property calculations differ by 0.007434 mm³ (about 0.024 ppm); the original 1 ppm acceptance tolerance is unchanged. Temporary import closed and native geometry, poses and timeline preserved. | [Export record](verification/final-delivery-export.json), [STEP import](verification/step-roundtrip.json) |

The independent reduction trials explain the retained size; they do not prove a globally smallest enclosure. At width 84 mm, the current 5 mm gas route fails despite no static part overlap. At depth 42 mm, the manifold intersects its lid and the rear gas return is truncated. At height 179 mm, the manifold intersects the housing and upper display retainer. All trial expressions and occurrence poses were restored. See [size-reduction diagnostics](verification/size-reduction-trials.json).

## Verified service sequence

The single rear cover gives access to the assembly. Remove its four screws, then withdraw it rearward (+Z) and unplug the battery mate rearward. The following paths were checked separately:

| Assembly | Prerequisites and tested motion |
| --- | --- |
| Protected battery pack | Cover off and battery unplugged; withdraw holder, cells and attached plug rearward. PCB and chamber stay installed. |
| PCB and carrier | Cover off, battery unplugged and wiring freed; remove the two shared screws and withdraw rearward. Battery stays installed. |
| Closed sample cartridge | Cover off, battery unplugged, chamber harness freed and both external gas fittings removed; withdraw rearward. Keep the lid, four lid screws and sensors attached. Housing rails and rear-cover capture replace separate chamber-mount screws. |
| Display retainers and display | Cover off and battery unplugged; remove two retainer screws and withdraw retainers rearward. Disconnect the display and remove the complete factory-cased module forward (-Z). Actual frame capture/contact remains unqualified. |
| USB cartridge | Cover/key off, battery removed and USB loom disconnected; translate inward +Y by 2.5 mm, then rearward +Z. Its two screws and inserts remain attached, while PCB and carrier stay installed. |

The four chamber-lid screws and two USB-cartridge screws have nominal driver access with their cartridges removed to the bench. Flexible wiring, latch release, hand clearance, real driver handles and safe grip still require physical confirmation. Exploded poses illustrate component ownership and are not removal trajectories.

## Offline PCB evidence

[pcb-strip-assessment.json](verification/pcb-strip-assessment.json) and [per-footprint clearances](verification/pcb-footprint-clearances.csv) read the existing KiCad preview without changing it. All 119 footprints have front courtyards; 115 are FIT. FIT courtyard area totals 1,469.64 mm², or 49.5% of the 30 x 99 mm gross strip. Conservative bounding rectangles total 1,573.98 mm².

An artificial same-side rectangle packing trial placed all 119 footprints, including DNP pads, with a 1 mm outer-edge allowance and an extra 0.5 mm gap between courtyard bounding rectangles. It also fit the current 21.4 x 17 mm upper notch, 5.8 x 4.2 mm lower USB notch, and two radius 3.6 mm mount reservations represented by conservative 7.2 mm squares. Each result includes explicit coordinates plus an independent overlap/boundary check. The 31 x 100 and 34 x 100 comparison trials are also recorded.

This is **conditional packaging evidence only**. The preview has no copper tracks or zones and no encoded mechanical keepouts. Ten footprints are placeholders: D101, J101, J102, J103, J301, J402, J801, J802, L101, SW101. J301's 34.12 x 6.18 mm courtyard rectangle must run along the strip rather than across its 30 mm width. Actual connector mating, wire bends, button/USB intrusion, switching-loop layout, thermal copper and routing/DRC remain open. DNP components still require their PCB pads unless the electrical design is deliberately changed.

## Native verification tools

Root-controlled Fusion execution uses these explicit entry points; importing them does not execute a check:

- [audit_a3.py](scripts/audit_a3.py): `audit()` defaults to 14 expected screws. It recomputes, checks feature/sketch health, records actual body instances/poses/bounds and hardware, and analyzes transient positive-volume interference. Hidden bodies and repeated definitions are included. No persistent geometry is created or moved.
- [verification_a3.py](scripts/verification_a3.py): `audit_paths()` checks eight candidate service routes using exact transformed BRep copies at intervals no greater than 1 mm; `audit_drivers()` checks actual fastener axes with nominal M2/M3 shafts. `regeneration_test()` explicitly varies dimensions independently by +2 mm and restores all original expressions in `finally`.
- [gas_checks_a3.py](scripts/gas_checks_a3.py): `audit()` checks continuous 5 mm cylinders and corner spheres against every placed solid, including fitting walls, alongside named source-expression channel checks. `size_reduction_trials()` independently diagnoses 84 mm width, 42 mm depth and 179 mm height, with native health, static intersections, gas clearance and fixed-part dimensions, then restores parameters and occurrence poses. These trials do not select a final size or qualify physical gas performance.
- [review_a3.py](scripts/review_a3.py): `style()`, `export_views()` and `exploded()` create actual Fusion view exports. AA cuts the battery/display stack; BB cuts the top manifold at GasY. Visibility, camera, temporary analyses and exploded poses are restored; pose recovery metadata is retained.
- [make_review_pdf.py](scripts/make_review_pdf.py): requires eight real view PNGs and reads only Revision 04 evidence. Creates three A3 landscape review sheets. Missing views are fatal. Final rendering and visual inspection are required before delivery.

The original candidate intent remains in [service-plan.json](verification/service-plan.json); the completed eight-path report linked above is authoritative. Earlier failed bridge-call files and intermediate checks are diagnostic history. A checkpoint or file emitted during a failed/rolled-back Fusion stage does not establish model completion.

## Boundaries before physical manufacture

The 14 independently positioned screws are four rear-cover, two carrier/PCB, two display-retainer, two USB board/adapter, and four independent chamber-lid screws. The closed chamber uses housing rails and rear-cover capture; it has no separate housing mounting screws. Its four lid screws remain with it during withdrawal. The USB cartridge retains its own board/adapter hardware during service.

The actual factory display capture lip/contact/preload, hardware choices, connector latches, flexible wiring, gas seals, representative sample renewal, pressure control, thermal response and final material remain unqualified. Selected wall-section checks must name their locations and assumptions; they are not a global minimum-thickness certificate. The AO2 M16 x 1 connection requires a measured seal that is gas-tight with hand tightening. Keep the atmospheric sampling requirement separate from the sensor's maximum pressure limits.

Native Fusion, STEP and review sheets are concept deliverables; print-ready release requires physical measurements and prototype testing. See [MEASUREMENTS.md](MEASUREMENTS.md).
