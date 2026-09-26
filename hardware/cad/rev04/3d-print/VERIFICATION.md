# PrintReview verification record

This package is a mechanical fit prototype. PLA is for dry fit; PETG is the
enclosure material target. No physical test in this record has been performed.
The preserved A3 v3 design remains separate from `Trimix_Enclosure_A3_PrintReview`.

## Native geometry

The revised model retains the approved 180 H x 85 W x 43 D mm housing envelope,
2.4 mm nominal walls and concealed USB fastening. Purchased parts were not
scaled. Three complete purchasing assemblies group the display, button and USB
connector; their visual children do not add purchasing quantities.

| Digital check | Recorded result | Evidence |
| --- | --- | --- |
| Solid interference | 102 placed solids; zero overlaps; healthy features | `verification/model-audit.json` |
| Fabricated parts | 11 printable single-solid parts | `verification/print-mesh-manifest.json` |
| Selected walls | 34 material sections across all 11 parts passed | `verification/selected-wall-fastener-checks.json` |
| Nominal fasteners | 14 screws, 14 inserts; engagement and tip gaps checked | Same selected-wall/fastener report |
| Driver access | All 14 nominal shafts clear | `verification/service-drivers.json` |
| Service sequence | Eight sampled removal paths clear with stated prerequisites | `verification/service-paths-cover-disconnect-battery-carrier-chamber-retainers-display-usb.json` |
| Gas route | Continuous nominal 5 mm geometric probe clear of modeled solids | `verification/gas-clearance-paths.json` |
| Regeneration | Independent +2 mm width, height and depth trials passed; original geometry restored | `verification/parameter-regeneration.json` |
| Native archive | Reopened with matching solids, 36 user parameters and 1078 timeline entries | `verification/native-reopen.json` |
| Purchasing quantities | Original definitions and three complete module parents reconciled | `docs/source/native-bom-reconciliation.json` |

The wall survey is a set of selected sections, not a mathematical global minimum.
The USB bezel intentionally has a 1.8 mm local front panel and approximately
1.95 mm cosmetic upper/lower webs. Its 2 mm hidden flange and the separate
cartridge support the connector. The revised bridge capture is 2 mm thick. Four rear-cover locating tabs have
0.30 mm x 45 degree entry chamfers, retaining 2 mm cores with intentional
1.4 mm insertion noses. All four tab cores are included in the wall survey.

The service test samples translations at intervals no greater than 1 mm; it is
not a continuous motion sweep. Nominal driver shafts do not model the user's
hand, handle, torque or a flexible loom. The gas probe verifies an open geometric
route, not sensor exposure, renewal, pressure drop, flow or leakage.

## STEP export exception

The exact delivered STEP reopened with all 102 solids, matching overall bounds
and scale. Its very-high-accuracy total volume differs by 0.45618 mm3, about
1.47 parts per million. The existing 1 ppm numerical comparison therefore
remains **failed** in `verification/step-roundtrip.json`; its tolerance was not
relaxed to turn the result green.

The differences are concentrated in the nominal AO2 thread, the adapter thread
and the revised manifold. A follow-up check sampled 23,022 points in both
directions across those three bodies. Maximum measured point-to-body distance
was 0.001292 mm, within the separate 0.02 mm dimensional diagnostic.
`verification/step-surface-diagnostic.json` records every body summary and the
method. Sampling is not an exhaustive surface-distance proof. The native Fusion
archive is the editable design authority; the STEP is an exchange model with
this numerical exception recorded.

## Printing

See `printing/README.md`, the part/coupon manifests and the per-project layer
inspection records. Each STL uses millimetres and a rigid orientation transform;
no scaling was used. Three editable support blockers protect the manifold's
inaccessible gas cores, while external supports remain removable.

The slicer rounds two local P07 features to its 0.20 mm layers: the 2 mm CO seat
retains ten layers with a nominal +0.1 mm position shift, and the narrow final
outer teardrop crest is omitted. These are recorded fit-check items, not evidence
that a printed passage or sensor seat has passed physical inspection.

## Physical acceptance is pending

Use `docs/FIT_CHECKLIST.md` to record measured results. In particular:

- Select actual M2/M3 heat-set insert SKUs, qualify their coupons and revise full
  part pilot holes before heat installation. Current holes are clearance
  references, not qualified retention pilots.
- Confirm actual module dimensions, sensor support faces, battery connector,
  harness exits, plug overmould clearance and hand-fit AO2 threading.
- Inspect every print for warping, layer integrity, complete support removal,
  open passages, clean threads and damage before fitting modules.
- Establish seal materials, compression and leak tests. The connector's supplier
  IP rating does not establish an enclosure IP rating.
- Test insert retention, USB insertion loads, battery retention, cable handling,
  temperature, creep and gas response with real hardware.
- Complete the main PCB, USB daughterboard, electrical commissioning and real
  sensor firmware separately. The mechanical package does not qualify the
  analyzer for gas-composition decisions.

The project-created CAD and 3D-print design files are licensed under
[CERN-OHL-S-2.0](../../../../LICENSES/CERN-OHL-S-2.0.txt). Project-written
scripts are GPLv3; third-party source terms remain separate in the
[license map](../../../../LICENSE.md).
