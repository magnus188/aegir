# A3 community enclosure guide

**Build status:** source, BOM and print inputs are prepared. Final PDF/PPTX export
awaits the reviewed actual Fusion exploded view. No final guide files have been
created yet; the source release gate remains closed.

The English guide contains exactly 22 pages, including the cover. Its scope is
an **enclosure fit prototype**. The source separates digital geometry evidence
from physical fit, sealing, electrical commissioning and measurement validation.

## Source and bill of materials

- [Guide content](source/guide.json): editable text, tables, notes and references.
- [CAD-to-BOM map](source/part-map.json): original component names, stable CAD part
  IDs and purchasing groups. The map preserves the native names for existing checks.
- [Procurement BOM](source/procurement-bom.csv): one row per purchasing/fabrication
  group. Visual pieces of a purchased assembly do not create extra purchases.
- [Assembly supplies](source/assembly-supplies.csv): tubing, seal and harness
  specifications that still need selection or measurement.
- [Printed parts](source/printed-parts.csv): the 11 intended printable parts.
  Mesh, orientation, support and project fields come from the actual print manifest.
  The release inputs record whether final print review has been accepted.
- [Release inputs](source/release.json): final facts, actual Fusion image paths
  and verification evidence. The builder refuses unreviewed or incomplete inputs.
- [Physical fit checklist](FIT_CHECKLIST.md): measurements and test results to
  return with a community contribution.

The three newly printed interfaces, P09/P10/P11, require the revised native
geometry and its checks. The original component names for P09/P10 mention metal
because the checks preserve the original names. Their current description and
release manifest determine their fabrication process.

The two PCB rows identify a separate electronics handoff. The main board still
needs the A3 outline, routing and DRC. The 0.60 mm USB daughterboard also needs
its fabrication release. Seal allowances do not specify purchased seal parts.

The PLA project supports dimensional fit. PETG is the target material for the
later enclosure trial, including P09/P10/P11. Neither a supplied profile nor a
successful print qualifies sealing or component loads.

The existing nominal M2 holes around 3.2/3.3 mm and M3 holes around 4.3 mm are
clearance references, not approved heat-set retention pilots. Select an actual
insert SKU, qualify its printed coupon, then update the enclosure holes before
heat installation in the full parts. Until that step succeeds, follow the
assembly order as a loose dimensional fit.

## Rebuilding the guide

Use the source scripts in ../scripts:

1. guide_data.py writes the initial procurement grouping from the baseline CAD
   inventory. Reconcile its output against the final native assembly before release.
2. guide_content.py writes the 22-page guide source. It preserves an existing
   release.json so final image/evidence assignments survive a content rebuild.
3. guide_reconcile.py checks every mapped ID, quantity and purchased parent
   against the actual native exports. Its receipt records the source hashes.
   Run it after a BOM refresh. A match establishes quantities, not physical fit.
4. guide_assets.py records the individually reviewed actual Fusion views and
   creates image montages from separate native slide objects. The original CAD
   images remain unchanged. The exploded view requires its own recorded review.
5. guide_print_inputs.py maps the actual print manifest to the 11 stable part
   IDs and verifies mesh hashes and both material projects. Use --approved only
   after the final slicer review has been accepted.
6. guide_build.mjs uses the JavaScript @oai/artifact-tool package to create native
   editable text and tables, embeds the actual Fusion views and runs the
   presentation finalizer.
7. guide_pdf.py converts the finalized PPTX into its matching PDF using the
   **bundled** LibreOffice. It checks 22 pages, title coverage, eight native PPTX
   tables and unresolved tokens, then renders every page for visual review.

The local runtime is resolved through Codex workspace dependencies. Node:
/Users/magnustrandokken/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node.
Python uses the adjacent python/bin/python3. The only LibreOffice used is the
bundled dependencies/bin/override/soffice. Never substitute the user's desktop
LibreOffice. GUIDE_RUNTIME can select another bundled dependency root for both
builders; RUNTIME_NODE_MODULES and SKILL_DIR select the JavaScript package and
presentation-skill locations for another contributor's runtime.

Each revision uses a new output filename. Keep private candidates, validation
receipts and page renders in ../scripts/guide-build, outside the final-output
directory. Exclude that directory from the community download. Inspect every
rendered page before distributing the guide.
The authoring markers for the guide PDF and PPTX ran once for this operation.
Do not rerun them as part of a content revision in this same operation.

## Reuse and attribution

Project-created CAD and 3D-print files are licensed under
[CERN-OHL-S-2.0](../../../../../LICENSES/CERN-OHL-S-2.0.txt); project-written
guide scripts are GPLv3. See the repository
[license map](../../../../../LICENSE.md) before redistributing the package.

The editable packaging models use manufacturer drawings as references.
Manufacturer documents and trademarks retain their owners' rights. Source
links appear on the guide pages, in the notes and in the existing component provenance records.
No authenticated supplier CAD or unrestricted supplier-document license is claimed.
