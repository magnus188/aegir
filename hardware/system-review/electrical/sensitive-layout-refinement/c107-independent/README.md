# C107 rear-side same-package scout — no accepted relocation

**No native-clean, grounded relocation was established in this bounded pass. Do not merge `candidate/Trimix_Analyzer.kicad_pcb`.** The existing main board and completed CAM checkpoint were not modified.

The immutable source is `source.kicad_pcb`, copied once from final main SHA256 `9f274fdf198f1bcb871a4fc360e76693f8a5b048553e6a02998141737956514f`. C107 remains the exact TDK C3216X5R1E226M160AB 22µF/25V/X5R1206; no purchased package was scaled. The owner's C103 area at native X12–15/Y80–82 and existing REGN source via(12.79,81.15) were reserved. No shared/canonical candidate was edited.

## Bounded findings

1. The sampled local B-side region X9–17/Y75–81 had no entirely clear same1206 pose while respecting pads, courtyards, vias, source keepouts and the reserved C103 area. The closest usable-looking poses all intersect the original0.40mm PACK_P diagonal `dcf3adfc-98b6-4b63-9e3c-1e074a51d2df`, from(10.64,79.9091) to(16.1271,74.422). The prior failed B(12.8,78.6) pose was not repeated.
2. B(13.25,78.75),0° provides a direct REGN leg from pad1(11.775,78.75) to the existing REGN via. Its pad placement avoids the SCL clash found farther right. However, trial PACK detours either intersect the REGN land/leg or the upper USB_5V branch. That USB branch was **not** narrowed or moved. Ground-to-existing-via straight routes encounter SCL/SDA/raw-USB/BTST or PACK copper.
3. B(13.75,79.0),0° permits an explicit0.40mm PACK detour: (10.64,79.9091)→(11.1,79.95)→(11.1,77.69)→(14.15,77.69)→(16.1271,74.422). Its REGN leg is short0.25mm B copper from(12.275,79) to(12.79,81.15). A0.50/0.25mm GND via at(16.05,78.05) passed the isolated all-layer point/keepout/SMT-margin screen, with a short0.25mm B connection from ground pad(15.225,79). **This did not qualify the footprint:** the GND land itself overlaps SCL's via(15.75,79.55) and fanout. Native DRC correctly rejects it.
4. A0.60/0.30mm GND landing was not found among the5022 locally reachable0.05mm-grid positions for that pocket. Several0.50/0.25mm point candidates exist; this is a grid-bounded observation, not proof that every possible geometry is impossible. The new ground via does not resolve the component-land clash.

The complete isolated trial produced **0unconnected items,0schematic parity issues,8error-level violations and4warnings**. Errors are3short reports,1copper clearance,1hole clearance and3mask-bridge reports. The C107 ground land intersects SCL; the old B bootstrap branch also has only0.175mm clearance. Warnings include the deliberately retained old capacitor trace tails and relocated silkscreen. See [candidate/drc.json](candidate/drc.json). Zero opens in a shorted candidate is not connectivity acceptance.

## Mechanical information, for a future electrically valid candidate only

The [exact TDK characterization sheet](../../sources/c3216x5r1e226m160ab.pdf), also linked from the [TDK part page](https://product.tdk.com/en/search/capacitor/ceramic/mlcc/info?part_no=C3216X5R1E226M160AB), specifies L3.20±0.20, W1.60±0.20 and T1.60±0.20mm. Thus maximum body is **3.4×1.8×1.8mm**. Adding0.15mm on each XY side gives a conservative3.7×2.1mm envelope; adding0.15mm seating/assembly allowance gives1.95mm rearward height. These explicit manufacturer maxima are used instead of assuming the generic STEP body is exact.

For the rejected B(13.75,79),0° example:

- Maximum body plus XY allowance: native X11.90–15.60/Y77.95–80.05mm.
- Actual native B courtyard including line extent: X11.405–16.095/Y77.805–80.195mm.
- A provisional access-opening envelope covering that courtyard plus0.30mm per side would be X11.105–16.395/Y77.505–80.495mm. This is a **space request**, not an approved cut or wall assessment.
- Current final PCB placement puts B.Cu at Fusion Z20.5mm; the proposed part/allowance reaches Z18.55mm. Native-to-Fusion mapping for this checkpoint is X+50.4 and Y→120−Y; the body envelope would be Fusion X62.30–66.00/Y39.95–42.05/Z18.55–20.50mm.
- This is above/outside the existing lower carrier opening (native Y83.7–99.7). A new opening and actual solid wall/screw/service checks would be required. No carrier or enclosure change was made or qualified.

## Disposition and next useful coordination

Keep C107 in its current source position until an actually grounded/native-clean replacement exists. A further trial would require coordinated SCL or upper USB sensing-branch routing, or a genuinely different, source-qualified smaller capacitor—not the failed same1206 pose presented as a success. The owner is separately shortening C103; removing its old BTST branch alone does **not** resolve C107's SCL/SMT conflict. Do not widen global clearance exceptions, use an open via in an SMT land, cut the carrier, or narrow a load trunk to force this candidate through.

This report leaves the inherited REGN dynamic-decoupling concern open, with the evidence limits in [the current power-layout review](../../routing-candidate/sensitive-layout-refinement/root-final-review/README.md). No bench test, current rating, stable-regulator claim or order release follows from this scout.
