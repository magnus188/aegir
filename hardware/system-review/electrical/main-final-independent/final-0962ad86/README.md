# Independent review of the refined main PCB exports

**The exported geometry, physical copper connectivity and assembly ledgers pass. This is not a fabrication or assembly release.** The supplier must still accept the specified filled and capped holes; electrical, transient, thermal and physical-fit checks remain separate.

This review is bound to board SHA256 **0962ad86f9834ce71b6439d0f95db603753801153490e78f387a88521e582788** and XML netlist **b04e4701898d62cb49b5adbc709e634b919f88fb2f1c1c36e5818ad94b2f4b08**. Inputs come from `routing-candidate/sensitive-layout-refinement/frozen-local-bundle/`. No native board, schematic, CAM or manufacturing input was changed. The earlier `final-9f274fdf/` review is available through Git history.

## Results

- **44 general checks passed:** four copper layers, 547 physical pads, 2,247 tracks, 300 routed vias, 387 drilled holes/slots, saved zone fills, mask, paste, outline and placement all match the frozen native source. All 501 distinct electrical pin/net mappings match the XML.
- The independent Gerber/Excellon graph has **zero open nets and zero mixed-net shorts**, with 117 physical components and no unattributed isolated copper. Its 5,644 nodes use 5,641 spatial candidate comparisons. Plated barrels join layers; labels, shared pin numbers and assumed internal IC connections do not. Removed bridges, injected shorts, unplated crossings and disjoint region islands are exercised as negative controls, including mutations of actual exported copper.
- **20 assembly-ledger checks passed:** 27 factory parts + 118 manual parts + 6 DNP parts + 18 PCB features = 169 references. The purchasing BOM contains 145 populated parts. Factory placement is the exact subset of the full native placement, including side and rotation.
- **12 refinement-preservation checks passed:** every old/new schematic net retains the same reference/pin membership; individual control-circuit pad shapes, layers, coordinates and nets remain unchanged; the revised exact parts and their actual lands match their manufacturing assignments.
- **50 process checks passed. One external supplier-approval hold remains**, explicitly reported by `process-audit.json`; there are no other process-audit failures. A nonzero process-script exit is intentional while this approval is absent.

The independent clearance screen is 0.15 mm and is not a replacement for the owner's native 0.20 mm routing-rule audit. The exact masked U115.3 cap extension retains its two reviewed 0.125 mm copper gaps, with coordinate-specific treatment rather than a whole-net waiver. Excellon emits a harmless-to-this-parser G90 ordering warning; the actual hole/slot positions and dimensions are nevertheless reconciled individually.

## Revised parts and stencil

| Reference | Exact part | Side and land dimensions | Assembly |
| --- | --- | --- | --- |
| C103 | TDK C1005X7R1H473K050BE, 47 nF | Bottom, actual manufacturer 0402 lands: 0.40 × 0.55 mm | Factory |
| R702 | Yageo RT0402BRD07100KL, 100 kΩ | Top, actual resistor 0402 lands: 0.54 × 0.64 mm | Factory |
| C107 | TDK C2012X5R1A476M125AC, 47 µF | Bottom, actual manufacturer 0805 lands: 0.80 × 1.10 mm | Factory |
| R504 | Yageo RT0603BRD0710KL, 10 kΩ | Bottom, original 0603 package; native centre 18.25, 48 mm | Manual |

The factory stencil exactly retains only the original apertures belonging to the 27 factory parts. C103, R702 and C107 are included. R504 and all DNP/manual/PCB-feature apertures are excluded. The full native paste files are retained as export-equivalence diagnostics and **must not be substituted for the factory stencil**. Negative controls reject that substitution on both faces. In particular, C706's DNP ordinary-bore/paste overlap is absent from the factory output; it has not been waived merely because the component is unpopulated.

Q110 retains the exact DMN2056U manufacturer rectangular 0.90 × 0.80 mm lands. The ordinary C708 GND via remains at 4.95, 63.875 mm with 0.60/0.30 mm land/hole dimensions. The factory stencil has no ordinary routed-bore paste encounter requiring an undisclosed process.

## Required hole process

The exact 27-row fill/cap table is byte-identical to the prior review and independently matches current native pad/via objects and actual plated drill hits: **15 U201 thermal holes + 9 U101 thermal holes + 3 routed signal holes**. The three signal sites remain:

| Centre, mm | Net | Land / hole, mm |
| --- | --- | --- |
| 13.6, 89.775 | USB_CC_INT_N | 0.40 / 0.20 |
| 13.6, 91.0 | USB_OVP_UVLO | 0.50 / 0.25 |
| 17.95, 87.925 | USB_OVP_UVLO | 0.50 / 0.25 |

Original purchased-pad mask/paste apertures are retained and no front opening is added at the signal vias. The required process is epoxy resin filling, planarization and copper capping on both faces. **Tenting is not a substitute.** Twelve package thermal bores overlap current factory paste windows: eight at U201 and four at U101. All 24 thermal bores are specified for fill/cap, including the remaining twelve.

Digital metadata and an Excellon drill file cannot prove filling, finished plating, cap flatness, voiding or reflow quality. Supplier quotation and explicit process/stencil acceptance remain pending for this exact packet. No order, upload, purchase or physical assembly test was performed.

## Visual review

All four copper layers, both mask layers, both full-paste layers, both silkscreen layers, both factory-paste layers, outline and both drill outputs were rendered directly from exported Gerber/Excellon bytes and inspected. Magnified views cover U115, U201, C103, C107, R702, R504, Q110 and R125. The revised capacitor/resistor apertures are distinct and match the selected lands; the manual R504 view correctly has no factory paste. The drill images show the intended holes, not proof of later filling.

The 141 visible native reference declarations and 28 assembly-map-only references match the marking ledger. All four supplementary locator pages and the 27-hole process map were also inspected; their separate 11-file manifest remains unchanged. The locators identify side and pin numbers, including the revised C103/C107/R702. Bottom copper and drawings intentionally use top coordinates, so bottom silkscreen is mirrored in these views; these are not mirrored physical assembly views.

Root's separately bound power-path, reference-plane and hardware-contract reviews remain responsible for their electrical conclusions. This audit makes no current-capacity, regulator-stability, measurement-accuracy, sensor-identification or thermal-performance claim. Existing EL10 load/thermal, EL11 transient, EL13 cold-start and physical module/cell/fit/analogue qualification holds remain open.

## Reproduction

Use `/tmp/trimix-gerbonara-venv/bin/python` with the scripts in the parent directory and the exact paths recorded in `verification-receipt.json`:

- `audit_exports.py --board … --netlist … --cam … --bom …/cam/assembly-reference-map.csv --out …`
- `audit_assembly_ledgers.py --board … --cam … --expected-sha256 0962ad86f9834ce71b6439d0f95db603753801153490e78f387a88521e582788 --out …`
- `audit_final_process.py --board … --cam … --factory-cam …/factory-stencil --fill-cap-table …/fill-cap-process/required-fill-cap-holes.csv --expected-sha256 0962ad86f9834ce71b6439d0f95db603753801153490e78f387a88521e582788 --out …`
- Run this directory's `audit_refinement_preservation.py` and `render_remaining_cam.py` from the repository root.

The receipt binds source hashes, all audit scripts, actual commands, logs, inspection images and result JSON. No KiCad API, native connectivity engine or KiCad renderer was used by these independent CAM checks.
