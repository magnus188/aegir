# A3 Bambu Studio print projects

These are local **fit-prototype projects**, prepared with the installed Bambu Studio **2.8.2.61**, an **H2D with 0.4 mm nozzles**, and **0.20 mm model layers**. No print job has been sent. PLA is for dry mechanical fit; PETG is the intended next material candidate, pending the physical checks below.

The eleven revised Fusion parts have separate editable, sliced `.3mf` projects
in [`bambu-studio/pla/`](bambu-studio/pla/) and
[`bambu-studio/petg/`](bambu-studio/petg/). Each project contains one part on
one plate. Eight exact-feature coupons are in the same material folders with
names beginning `C01` through `C08`. Earlier diagnostic baselines are available
through Git history; they are not part of the 38 delivered part/coupon projects.

Standalone `plate_*.gcode` and slicer result JSON files are generated outputs
and are not kept in Git. Open and reslice the `.3mf` projects for the actual
printer, filament and plate before printing.

Start with the coupons and the material you will actually use. Open a `.3mf` in Bambu Studio, confirm the real spool and plate, inspect the model and supports, then decide whether to print. The prepared profiles use Generic PLA or Generic PETG and a Textured PEI Plate; they are not a record of the user's installed spool or plate.

| Part | ID | Bed orientation |
|---|---|---|
| Housing | P01 | Front down, rear opening up |
| Rear cover | P02 | Outside rear face down |
| Carrier | P03 | Broad front face down |
| Display retainers | P04–P05 | Broad rear plate faces down |
| USB frame | P06 | Floor down |
| Sampling manifold | P07 | Front floor down, lid opening up |
| Chamber lid | P08 | Outside lid face down |
| Printed USB bezel | P09 | USB axis vertical, hidden rear flange down |
| Printed USB bridge | P10 | Broad rear face down |
| AO2 threaded adapter | P11 | Thread axis vertical |

Use the complete `TMX-A3-` prefix when matching these IDs to drawings. The geometry was moved only by rigid rotations and translations; it was never scaled. `print-manifest.json` records native and oriented mesh hashes, dimensions and transforms.

Supports are enabled where they can be reached through the enclosure openings or around the separate part. The manifold contains three editable support-blocker volumes covering its gas passages. Each blocker extends 0.35 mm beyond its protected core to account for bead width. The actual G-code is checked against the original protected cores. The revised external inlet underside can still receive removable support. Remove supports and debris before fitting sensors; the open cartridge allows inspection of the chamber pockets.

The source model and slicing checks do not qualify an airtight manifold, functional gasket, heat-set process, sensor retention, USB insertion load or operating temperature. The printed USB bezel has a documented local 1.8 mm panel section; it is not a blanket ≥2 mm wall claim or an IP rating. The modeled AO2 thread is nominal M16×1-6H; the full adapter is provided as a hand-fit coupon and has no automatic shrink allowance.

## Evidence and its limits

- `archive-reopen-verification.json`: all 38 projects reopened in Bambu Studio; archive CRC, embedded G-code checksum, mesh and profile checks passed.
- `visual-review.json`: recorded selected-layer review, proven geometry matches and localized P07 fit-check items.
- `project-index.csv`: all project paths and slicer time/material estimates.
- `verification-summary.json`: all 22 part projects, support checks and feature-omission candidates.
- `coupons/verification-summary.json`: all 16 coupon projects.
- Each project’s `inspection/` contains actual native plate images, a selected model-layer contact sheet, all-layer CSV, and an explicit inspection report.
- `model-boundary-sections.json` compares each model midlayer mesh boundary with actual extrusion strokes at 0.25 mm raster spacing and a 0.5 mm path margin. It can flag omissions; it does not prove global wall thickness or resolve submillimetre details.
- Bambu interleaves independent support-height events between the 0.20 mm model layers. Event counts therefore exceed the number of model layers on supported parts.
- Physical results belong in the coupon records. None are prefilled as passed.

The current model-layer screen identified only two local manifold transitions: a nominal 2 mm CO seat prints with its full thickness shifted by approximately +0.1 mm, and a narrow outer teardrop apex is omitted at the final partial layer. Their detailed evidence is retained for fit review. These do not establish the real printed roof or sensor fit; inspect the gas coupon and actual support surfaces.

## Reproduce the projects

Run with Python containing NumPy and Pillow. The optional exact-coupon tools are pinned in `runtime-requirements.txt` and are installed in the private `.runtime_lib/` directory for this workspace. They are not installed into the user's Python environment.

From the repository root:

```sh
python3 hardware/cad/rev04/3d-print/scripts/print_release.py prepare
python3 hardware/cad/rev04/3d-print/scripts/print_release.py coupons
python3 hardware/cad/rev04/3d-print/scripts/print_release.py slice
python3 hardware/cad/rev04/3d-print/scripts/print_release.py slice-coupons
python3 hardware/cad/rev04/3d-print/scripts/print_audit.py
python3 hardware/cad/rev04/3d-print/scripts/print_finalize.py
```

On this Mac, the bundled Python used is recorded in `environment.json`. Bambu’s CLI needs access to macOS graphics services even when slicing files. The pipeline uses `--datadir` within this print folder, resolves installed **system** presets and preserves official machine G-code. It never reads or modifies saved user profiles and implements no printer connection or send action. See the [official Bambu CLI documentation](https://github.com/bambulab/BambuStudio/wiki/Command-Line-Usage).

`profiles/provenance.json` records source preset paths and hashes. The starting process uses four walls, five top/bottom layers, 20% gyroid infill and a 4 mm outer brim. These are reviewable starting choices, not strength or material qualification.
