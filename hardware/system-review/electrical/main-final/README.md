# Main PCB — routed prototype review package, HOLD

The canonical KiCad project is [Trimix_Analyzer.kicad_pro](../../../pcb/analyzer/Trimix_Analyzer.kicad_pro). The board SHA256 is `0962ad86f9834ce71b6439d0f95db603753801153490e78f387a88521e582788`. It passes native DRC, connectivity and schematic parity with zero findings. Earlier board checkpoints remain available through Git history.

## Files to review

- [All references, values, MPNs and positions](cam/assembly-reference-map.csv): 169 physical references.
- [Purchasing BOM](cam/purchasing-bom.csv), [factory BOM](cam/factory-bom.csv), [manual BOM](cam/manual-bom.csv), [DNP list](cam/dnp-list.csv), [PCB/harness features](cam/pcb-features.csv).
- [Factory placement](cam/placement-factory.csv) and [all exported placements](cam/placement-all.csv). Use the printed side/rotation fields; do not infer orientation from a generic 3D marking.
- [Printed-reference legend](cam/marking-legend.csv): 141 references are on silk. The 28 assembly-only references have locator sheets [1](assembly-drawings/assembly-only-locators-1.svg), [2](assembly-drawings/assembly-only-locators-2.svg), [3](assembly-drawings/assembly-only-locators-3.svg), [4](assembly-drawings/assembly-only-locators-4.svg). Orange shows the selected pads. Both sides use explicitly labelled top-view coordinates; rear probing requires PCB removal.
- [Factory-only paste source and derivation](factory-stencil/derivation-receipt.json), with `.gtp/.gbp` files in `factory-stencil/`. These omit all manual/DNP paste, including C706. Use these for assembler review; the full-board paste files in `cam/` are diagnostic exports, not the factory subset.
- [27-hole fill/cap process](fill-cap-process/README.md), [exact hole list](fill-cap-process/required-fill-cap-holes.csv), [coordinate map](fill-cap-process/fill-cap-map.svg): all 24 package thermal bores plus 3 signal VIPPO require resin fill and copper cap on both faces.
- [Source-matched STEP](../Trimix_Main_PCB_Reviewed_Placement.step) and [maximum-height envelopes](../component-height-contract.csv). The STEP has normal pad thickness; no purchased part was scaled. Generic/absent visual models must be checked against the independent envelopes.
- [Native netlist](../analyzer-netlist.xml), [connector/testpoint pin map](../connector-testpoint-pinmap.csv), [selection qualification](../part-qualification.csv), [raw ERC](../main-erc.json), [DRC](../main-drc.json).

The population is 27 factory parts, 118 manual parts, six DNPs and 18 PCB/harness features. See the [assembly orientation supplement](assembly-notes.md) for the Coilcraft marked terminal and rear-component sequence. [Local power-layout changes and tradeoffs](../routing-candidate/sensitive-layout-refinement/frozen-local-bundle/README.md) include the actual 47 µF REGN part, short bootstrap and CO feedback/output paths.

## Verification and limits

Independent exported-byte review passed 126 checks: 44 copper/drill/connectivity, 20 assembly-list, 12 refinement-preservation and 50 process checks. [Independent receipt](../main-final-independent/final-0962ad86/verification-receipt.json). One external factory-qualification hold remains: actual fill/cap, flatness, stencil thickness/paste volume, joint coverage/voids, inspection and double-sided reflow are not established by the digital files.

The source audit passes 217 electrical assertions plus 205 revised-part identity/conservation checks, and rejects all 12 representative wiring/value faults plus four revised-part metadata faults. The 13 routing checks include the exact short rear bootstrap branch and reject three out-of-scope mutations. Raw ERC retains one exact TI-permitted LM66100 ST-to-ground exception; the guarded review finds no unexpected ERC errors or warnings. Root review preserves all 29 critical conductor witnesses, continuous In1 and every reviewed ground anchor. [Root verification](../routing-candidate/sensitive-layout-refinement/root-final-review/root-contract-erc-review.json), [power/ground review](../routing-candidate/sensitive-layout-refinement/root-final-review/root-power-layout-review.json).

The [source-matched final CAD disposition](../routing-candidate/sensitive-layout-refinement/promotion-proof/CAD-native-approval-summary.json) is bound to this board, STEP and maximum-height contract. Digital interference checks do not qualify actual printed dimensions, part mates or cable bending.

J104 remains open. No charging, fabrication or assembly order is authorized by these files. Exact cell/NTC, host input/mating harness, sensor/mechanical interfaces, factory process and physical startup/current/transient/thermal/EMC/gas/calibration qualifications remain in the system acceptance ledger. Routing completion and source consistency do not resolve those holds.

The USB daughterboard remains a separate frozen package under `../usb-final/`, including its unresolved GCT edge/annular process requirements. Do not substitute the main-board process for the 0.60 mm daughterboard.
