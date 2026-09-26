# A3 PrintReview build package

**Build status: documentation paused while the PCB is designed and fitted.** Geometry, STL exports and the 38
part/coupon print projects are saved and checked. The final community guide,
associative native drawings and persistent animation verification are still
being completed; the file map below describes the intended package.

This is the separate `Trimix_Enclosure_A3_PrintReview` design in the **Trimix
analyzer** Fusion folder. The approved exterior remains 180 H x 85 W x 43 D mm.
A3 v3 is preserved in the parent directory.

Use **PLA for an unpowered dimensional fit** and **PETG for the enclosure
material trial**. Physical fit, insert retention, support removal, leakage and
temperature testing remain pending. The current generic insert holes must be
qualified with the selected insert SKU and updated before heat installation in
full parts.

## Start here

1. Read the community guide and [physical fit checklist](docs/FIT_CHECKLIST.md).
2. Compare the [purchasing BOM](docs/source/procurement-bom.csv) with your actual
   modules. [Remaining supplies](docs/source/assembly-supplies.csv) identifies
   unresolved seals, tubing, connectors and wiring.
3. Open the coupon projects described in [Printing](printing/README.md). Record
   results with your printer, real filament, plate and selected hardware.
4. Review the native drawings and full-part orientations, then proceed through
   the numbered module installation steps. Keep the assembly unpowered for fit.
5. Return measured results with the release name and `TMX-A3-` part number.

## Files

| Location | Contents |
| --- | --- |
| `Trimix_Enclosure_A3_PrintReview.f3d` | Editable Fusion design archive |
| `Trimix_Enclosure_A3_PrintReview.step` | Assembly exchange geometry; numerical exception recorded below |
| `docs/` | Community PDF, editable presentation, BOM, checklist and source material |
| `drawings/` | Native Fusion drawing deliverables and engineering PDF sheets |
| `printing/source-stl/` | Eleven native-coordinate STL parts, in millimetres |
| `printing/oriented-stl/` | The same parts in their intended bed orientations |
| `printing/bambu-studio/pla/`, `printing/bambu-studio/petg/` | One part per H2D project |
| `printing/coupons/` | Eight fit coupons and their records |
| `previews/` | Actual Fusion assembly, part and section images |
| `verification/` | Geometry, export, service and metadata evidence |
| `scripts/` | Explicit CAD, slicing and documentation source scripts |

The H2D projects start with a 0.4 mm nozzle, 0.20 mm layers, four walls, five
top/bottom layers, 20% gyroid and a 4 mm brim. Confirm the actual material and
plate in Bambu Studio. Native machine presets were preserved; no printer job
was sent. The `pipeline_*` software-test specimens and raw inspection files are
diagnostic artifacts, not extra enclosure parts.

## What changed

- P09 USB bezel and P10 retaining bridge are printed parts. The bridge capture
  was thickened to 2 mm; the small visible USB opening and concealed fastening
  are retained.
- The nominal 5 mm gas path has 45-degree roofs where needed and a return
  channel exposed beneath the removable lid, with protected support-free cores.
- P11 now has a modeled M16 x 1 female thread, with a separate fit coupon.
- Four rear-cover locating tabs have 0.30 mm entry chamfers for easier alignment.
- Stable part numbers distinguish printables, purchased modules, hardware,
  supplied visual subcomponents and unresolved references.

[Verification](VERIFICATION.md) states the actual results and their limits.
The native archive reopened with matching solids, parameters and history. The
STEP matches solid count, bounds and scale but retains a 1.47 ppm numerical
volume discrepancy around the threads/manifold; the separate sampled surface
diagnostic was within 0.0013 mm. Do not treat every check as an unqualified pass.

The two PCBs, real wiring, gas seals, thermal behaviour, charging commissioning
and gas-analysis validation are later work. Nominal CAD hardware does not prove
clamping or printed-part strength.

## License and sources

The project-created enclosure, print files, and CAD exports in this package are
licensed under [CERN-OHL-S-2.0](../../../../LICENSES/CERN-OHL-S-2.0.txt).
Project-written generator scripts are GPLv3. Manufacturer drawings and
trademarks retain their owners' rights; the component models are
measured/drawing-derived references, not authenticated supplier CAD. See the
repository [license map](../../../../LICENSE.md).
