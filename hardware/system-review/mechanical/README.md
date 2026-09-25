# SystemReview mechanical checkpoints

**Latest integration checkpoint: [ConnectorReview v3](connector-review/README.md), saved and reopened using the owner's rear photograph and 13.4 mm glass-to-bare-header-tip measurement.** The [current native model](connector-review/interface-checkpoint/Trimix_Enclosure_A3_ConnectorReview_Interfaces.f3d) places the Guition expansion header, USB ports and onboard SD socket at their observed locations. It preserves the approved exterior and the routed PCB. Complete mated cables expose new conflicts; [the interface checks](connector-review/interface-checks.json) are explicitly failures requiring correction. The smaller carrier, holder and wire-route alternatives remain unadopted studies. The [independent parent check](connector-review/interface-checkpoint/root-review.json) verifies the saved inputs, native reopen receipt, dimensions and inspected views; it does not close the fit failures.

The v11 evidence below remains a preserved baseline. Its local plug allocations and sampled service paths do **not** prove fit or card access for the newly located Guition interfaces. No current complete-harness fit or manufacturing release is claimed.

## Preserved v11 baseline

**SystemReview v11** contains the matched routed **0962** main PCB, the frozen USB board, exact supplier M2/M3 insert models and the corrected carrier. The native archive has been saved and reopened with exact geometry, poses, parameters, grounding, source metadata and visibility agreement. The approved enclosure remains **85 × 180 × 43 mm**. Earlier native versions and every unrelated FlowGrid document are preserved.

This is a completed **bounded digital mechanical review**, not a PCB order, production-print, charging or gas-analysis release. The remaining physical interfaces are collected in [MEASUREMENTS.md](MEASUREMENTS.md). Canonical electrical adoption and firmware evidence are tracked separately by the project-level review.

## Current files and evidence

- [Editable native Fusion archive](final-local-checkpoint/Trimix_Enclosure_A3_SystemReview_FinalLocal.f3d), [saved/reopened summary](final-local-checkpoint/summary.json) and [native reopen receipt](final-local-checkpoint/native-reopen.json).
- [STEP assembly](final-local-checkpoint/Trimix_Enclosure_A3_SystemReview_FinalLocal.step). All 2,543 physical solids and overall bounds match after unit-aware reimport. The unchanged strict 1 ppm summed-volume check fails at **1.434 ppm / 0.4408211 mm³**, and one manifold analytic-plane lookup remains unmatched. Five-definition checks produced 53,998 bidirectional samples (maximum 0.0039418 mm) and 27 selected sections (maximum bound difference 0.0006552 mm), below the existing 0.02 mm diagnostic. This is limited geometric evidence, **not strict per-body equivalence**. The [raw checks and final disposition](final-local-checkpoint/step-disposition.json) retain both exceptions; the native archive remains authoritative.
- [Immutable input map](final-local-checkpoint/bound-inputs-map.json): 353 source/evidence bindings, including frozen PCB/USB contracts, adopted insert sources, native checks, actual inspection images and diagnostic slices. Mutable inputs are snapshotted; declared large frozen dependencies retain their exact hashes.
- [W85 complete checks](verification/final-local-checks/width-tests/W85/trial-summary.json), [W87 actual regeneration/restoration](verification/final-local-checks/width-tests/W87/trial-summary.json), [JJ alternative](verification/final-local-checks/oxygen-variants/summary.json) and [PCB-thickness driver probes](verification/final-local-checks/thickness-drivers/board-thickness-drivers.json).
- Actual Fusion views: [assembled](verification/final-local-checks/views/assembled.png), [rear access](verification/final-local-checks/views/rear-access.png), [carrier apertures](verification/final-local-checks/views/carrier-apertures.png). No parts were moved for these views.

The full native design contains **2,546 placed solids**, including three hidden JJ configuration references, with **1,211 timeline entries**. The default AO2 physical assembly contains **2,543 solids**; the main PCB accounts for 2,347. STEP is intended to contain the default physical assembly; the editable F3D retains both oxygen configuration references.

## Board, height and registration contract

The integrated board SHA-256 is `0962ad86f9834ce71b6439d0f95db603753801153490e78f387a88521e582788`; its input STEP is `81eef0182d0d3bbaaf1a433cf43dce3e70f975c9ccd6d8d94bd196aa00c99014`. The [frozen handoff](../electrical/routing-candidate/sensitive-layout-refinement/frozen-local-bundle/review/geometry-handoff-manifest.json) binds matching native PCB, netlist, DRC, STEP and height contracts. The [active import contract](verification/incoming-boards.json) also identifies the unchanged final USB source.

All **169 references** are accounted for. There are **145 purchasable populated references**, six DNPs and 18 board features. The 153-row height contract contains **147 fitted maximum/allocation envelopes** plus six DNP rows: the extra two fitted allocations are J101/J102 wire and solder space, not purchased components. The remaining 16 references are 14 bare testpads and two mounting holes. There are 16 fitted B.Cu envelopes. U110, L201 and L701 have no populated STEP visual model; their complete source-bound maximum envelopes are included in the native fit checks. The contract distinguishes manufacturer maxima, conservative footprint-derived bounds and provisional cable/solder allocations.

The imported main assembly and all its descendants remain rigid. At 85 mm enclosure width, KiCad coordinates map to **Fusion X = 50.4 + x, Y = 120 − y**. The native joint follows `PcbX = CaseWidth − PcbWidth − 4.6 mm`, while purchased board dimensions and hole spacing remain fixed. Width changes move the board, mounts, carrier cuts, fastener axes and review allocations together. The USB stays centred on its own datum.

The detailed main layer sum is **1.5642 mm**, including the 1.4942 mm dielectric substrate. Finished-board allocation remains **1.60 mm**, with a fixed back seat at Z20.5. Centre registration gives ordinary copper faces Z20.5179/Z22.0821 and the exporter component references Z20.4679/Z22.1321. These are explicit exporter datums, not measured mask or solder thickness. No purchased geometry is scaled. The tests include finished thickness **1.44, 1.60 and 1.76 mm**, front-side maxima raised 0.16 mm, and four additional nominal Ø5 × 50 mm driver probes at the thickness extremes.

**The historical R301/J301 mating-box failure is closed on this matched source.** R301 is now at PCB (11.95, 71.15), and the full approved J301 mating box, inward ribbon allowance and at least 6.1 mm axial unmate allocation clear in the final native checks. J301 is rotated 180° with its pin assignment preserved, so its standard ribbon exit points inward. The 35 × 5.5 × 12.5 mm mating allocation and cable/turn band remain engineering allowances; actual socket, ribbon, strain relief and bend radius require measurement.

## Carrier and exact inserts

The existing lower carrier opening remains at PCB x−0.4–20.525/y83.7–99.7. Two source-bound **through-openings** accommodate the final rear components:

| Opening | PCB x, mm | PCB y, mm |
|---|---:|---:|
| Shared C103/C107 | 11.955–15.795 | 77.805–81.595 |
| R504 | 16.425–20.075 | 46.925–49.075 |

[Exact transient/native comparison](verification/final-local-carrier/native-applied.json) shows one connected carrier, unchanged outer bounds, complete 2 mm material rings, unchanged mounting support and a minimum 2.105 mm web to the old lower opening. The two cuts remove 44.8022 mm³; the resulting native volume is 4048.146574862508 mm³. All other 2,542 physical solids were compared by bilateral native Boolean difference and remained unchanged. No alternate rear resistor opening was cut.

The adopted common M2 insert is **CNC Kitchen TC-M2x3.0**, EAN4262391010013, ten pieces: 3 mm length, Ø3.6 crest, supplier Ø3.2 pilot guidance and selected 2 mm material beyond the crest. Seven pilots retain blind floors; the upper PCB and lower display supports are through-open. The lower PCB pilot is deliberately classified as **partly open and relieved**: at least 4.299 mm full-diameter depth precedes a tiny peripheral remainder of the front-opening wall. It satisfies the 4 mm depth guidance without cutting the exterior. The failed stricter full-exit control is retained in [its disposition](verification/short-m2-implementation/lower-pilot-exit-disposition.json).

The four rear inserts are **CNC Kitchen VORON M3×5×4**, EAN4262391010051: exact unscaled supplier geometry, Ø4.4 × 5 mm pilots and Ø9 mm bosses, preserving a complete selected 2 mm annulus beyond the crest. The M3 × 8 reference screws and existing tip relief remain unchanged; actual selected tip clearance is at least 0.3476 mm.

The audit explicitly classifies **14 intentional heat-set pilot interfaces** and four additional generic smooth M3 shaft/modelled female-thread representation pairs. Each classification is tied to exact occurrence, source, axial and radial bounds; unrelated interference remains a failure. The supplier M2 internal bore is visual/reference geometry. Neither that bore nor the smooth M3 shafts prove usable thread engagement, class, torque or retention. Both PCB screws have 3 mm nominal axial insert overlap throughout the tested finished-thickness range; the minimum measured screw-tip clearance is 0.835794 mm at W85 and 0.738157 mm at W87. Install and qualify inserts on coupons before a full enclosure, with the display removed.

## Service, gas and USB

Final native actual-body and maximum-allocation checks show **zero unrelated intersections**. Both endpoint suites pass all **eight sampled service paths**, 14 nominal drivers, selected material sections, aligned PCB/mount axes and the continuous Ø5 gas-probe route. JJ installed and cartridge-removal reference checks pass at both widths. W87 restores every parameter, pose, physical solid and protected-document state exactly. These are finite geometry checks, not global minimum-wall, continuous swept-volume, hand-access, wire-bend or physical-strength proofs.

The display is released from the rear and removed through the front. After rear-cover removal and battery disconnection, release the upper retainer, move it **+Z6 mm, −X1.5 mm, then rearward**, remove the lower retainer and lift the display in −Z. This staged route clears at both widths; the old straight W87 route remains a recorded failure. The pack, carrier, USB and closed sampling cartridge use their documented rear-service prerequisites. Gas fittings and required electrical connections must first be disconnected.

GCT USB4720-03-A follows the [manufacturer Rev B drawing](https://gct.co/files/drawings/usb4720.pdf): port centre Z26, nominal 0.60 mm board at Z24.9–25.5, nose Y0.61 and shell stakes Y3.56/7.56. The support ledge and capture pad were corrected to those datums; purchased pads and the exterior port were not moved to accommodate a placeholder. The native model is a drawing reconstruction, not manufacturer STEP. Internal plastic, bends, gasket section and actual seating remain limitations. P09/P10 are correctly named printed bezel/retaining bridge. The six-conductor harness, its solder height, strain relief and the 5.7 mm board-to-bridge gap need real measurements. The daughterboard's GCT fabrication/tolerance hold remains open; an uncompressed gasket establishes no enclosure IP rating.

The AO2 body is unchanged and unscaled. Hidden JJ references use the owner's relative −2 mm diameter/+2 mm length measurement, with the same-thread shoulder/nose and mated cable still provisional. Gas fittings, sensor support, feedthroughs, lid/return seals, actual flow distribution, leak rate and sensing response require physical qualification. J402 remains the fixed Amphenol142138 drawing-derived reconstruction; its actual mated elbow is not yet measured.

## Diagnostic printing and historical records

The final P01 housing, P03 carrier, P06 USB frame and P07 chamber passed **eight H2D PLA/PETG diagnostic projects and 50 checks**, with four closed connected meshes, open carrier apertures and no generated support in the protected gas passages. The interim slice receipt and layer sheets remain in Git history. P03's native 5.50 mm height quantizes to a 5.40 mm deposited top at 0.20 mm layers. Actual printing, support removal, finish and fit are still pending. No print job or new production release was sent. PLA is dry-fit only; PETG remains an unqualified material/process target. Material-family metadata is not a mass or FEA qualification.

Earlier placement, M2 and insert checkpoints are in Git history. They include the old R301 allocation failure, unsynchronised width-datum clash, generic insert geometry and obsolete overlap/tip figures. Use the final v11 evidence above for current dimensions and geometry.
