# USB daughterboard engineering completion

The USB circuit and routing are complete for integration review. **Fabrication is held.**
The native board retains the original GCT recommended copper lands and exact
cutout/connector pose. Its full manufacturing DRC deliberately exposes the
source geometry conflicts; no exception converts those findings into a pass.

## What changed

The obsolete two-wire PCB and two local 5.1 kΩ CC resistors were replaced with
a six-wire harness and the two protection ICs. J902 maps directly to main J101:
1 USB_5V, 2 GND, 3 CC1, 4 CC2, 5 D+, 6 D−. The main TUSB320LAI provides Rd;
D+/D− serve main-board BC1.2 detection. This is a nominal 5 V power input,
not a high-speed USB data interface or a USB-PD voltage negotiator.

U901 is TI TPD4E05U06DQAR. Pads 1, 2, 4 and 5 protect CC1, CC2, D+ and D−;
3/8 are ground. The PCB explicitly bridges 1–10, 2–9, 4–7 and 5–6 with copper
under the package. TI permits this optional use of NC lands; no internal
pass-through is assumed. D901 is TPD1E10B06DYAR: pad 1 VBUS, pad 2 GND.
[TI TPD4E05U06 Rev O](https://www.ti.com/lit/ds/symlink/tpd4e05u06.pdf),
[TI TPD1E10B06 Rev G](https://www.ti.com/lit/ds/symlink/tpd1e10b06.pdf).

U901 uses the DQA0010B land example (4230307/A): 0.835 mm row pitch,
0.5 mm lead pitch, 0.565 × 0.20 mm signal lands and joined 3/8 ground metal.
The optional centre via is omitted; the ground connection reaches an external
via. D901 uses the DYA0002A example (4224978/B), 0.67 × 0.40 mm lands on
1.48 mm centres. Confirm the quoted DQA package/stencil variant with the
assembler; two DQA variants appear in the datasheet.

There are 94 track segments and 14 vias. Signal copper is 0.15 mm wide with
0.15 mm minimum clearance. VBUS trunks are 0.60 mm; their 0.09 mm pad escapes
are 0.30 mm wide. D901 is a shunt, so its 0.30 mm branch does not form a series
bottleneck for normal input current. Both layers have ground copper, and the
ESD ground paths use short external vias. The D901 ground land to via distance
is 0.51 mm. U901's ground return uses the external 3/8 bridge and a 0.7825 mm
trace from pad 8 to its via. These dimensions do not establish transient loop
inductance or assembled-port discharge performance. Source-current limiting
and overvoltage coordination belong to the separately reviewed main board.

## Verification and fabrication holds

Final native ERC: **0 violations**. Native PCB check: **0 unconnected items,
0 schematic parity issues**, with **22 manufacturing violations retained**:
20 copper-edge instances and 2 PTH annular-width instances. Some connector
pad aliases share physical copper and are counted separately by KiCad.
All remaining violations concern the unchanged GCT footprint/edge geometry.
There are no routing clearances, shorts, extra tracks crossing the edge,
co-located holes, silkscreen overlaps or unmapped schematic pins in that result.
See `usb-drc.json`, `usb-erc.json` and `audit.json`; DRC is not a physical test.

The current [JLCPCB published capabilities](https://jlcpcb.com/capabilities/pcb-capabilities/)
support the chosen 1 oz trace/space and via sizes. However, its ordinary routed
edge requirement is 0.20 mm, while the GCT lands give 0.10 mm at the SMT row and
0.15 mm at several ground lands. JLCPCB also gives a two-layer PTH minimum ring
of 0.18 mm, while the GCT two 0.80 mm lands with 0.50 mm holes give 0.15 mm.
Its quoted high-precision route tolerance is ±0.10 mm; the GCT recommended
layout gives ±0.05 mm. That mismatch requires a confirmed manufacturing
process. The TI 0.20 mm-wide signal lands also need explicit assembly/process
review against the generic pad-size line in the capability table. Panel tooling,
flat finish, stencil, reflow and inspection requirements must be agreed before
an order. The nominal board thickness requirement is independently satisfied
by 0.60 ±0.10 mm; that alone does not resolve the other conflicts.

The source [GCT Rev B drawing](https://gct.co/files/drawings/usb4720.pdf) is saved
unchanged at `hardware/pcb/usb-input/reference/GCT_USB4720_RevB_drawing.pdf`.
A rearward SMT-land shift was considered but rejected because solder-tail
coverage and tolerance could not be proved from the available drawing. Moving
the rear cutout to one end of its tolerance consumes the available tolerance
and reduces the current CAD plastic clearance. Neither was accepted as a
fabrication fix. The rejected land trial is preserved only in `before/`.

**Required closure:** obtain manufacturer/assembler approval of a real alternate
land/cutout layout, or choose a supplier/process that explicitly accepts and
meets the original edge, annular, slot, placement and routing tolerances. A
compatible alternate connector would require a new mechanical design review.
No quote, order or outside message has been sent. `manufacturing-diagnostic/`
is labelled accordingly; those exports are for inspection, not release.

D901's surge clamp is not limited to 5.5 V. Its published clamping voltages can
exceed the downstream limiter's input rating, so device-level IEC specifications
cannot prove coordinated VBUS protection or a system ESD rating. Verify the
main-board protection chain and then measure the assembled port's hot-plug and
transient behaviour. No charger, connector or gas-analyser safety pass is made.

## Mechanical and STEP contract

No connector position, purchased component scale or board outline changed.
J901 remains at KiCad (108, 110.14) mm, 0°. J902 is the intended six-wire row
at (108, 104.5), giving pad centres X103.5…112.5 mm at 1.8 mm pitch.
The earlier two-pad board had J902 at (105.9,104.0); that obsolete pose was
replaced with the already approved six-wire/Fusion row.

| Part | KiCad XY mm | Rotation | Maximum purchased envelope |
| --- | --- | --- | --- |
| U901 | 109.5, 106.8 | 90° | 2.6 × 1.1 mm in installed XY; height 0.55 mm |
| D901 | 104.0, 108.0 | 0° | 1.70 mm terminal span × 1.15 mm conservative width including mould-flash allowance; height 0.77 mm |
| J902 | 108.0, 104.5 | 0° | Wires/solder/bends unmeasured; 4.5 mm above-board allowance is provisional |

For the established nominal Fusion PCB top Z25.5 and support underside Z31.2,
U901 reaches Z26.05 and D901 Z26.27 before solder/placement allowance. A separate
0.10 mm assembly allowance would give Z26.15 and Z26.37, respectively; it is
an engineering allowance, not a measured solder thickness. The provisional
harness allowance reaches Z30.0, but physical bends and solder tips must be
measured rather than treated as a guaranteed fit. The nominal PCB stack is
Z24.9…25.5 and the connector/factory stake reconstruction remains separate.

`Trimix_USB_Input.step` contains the PCB, copper and unchanged KiCad nominal
package models for U901/D901. The connector and harness remain separate native
Fusion assemblies so they are not duplicated. The STEP uses the drill origin
(100,100) with the KiCad Y sign convention. Existing Fusion registration is
X+34.5, Y+18 with the previous Z+24.945 translation; the CAD owner must confirm
actual exported face elevations when reimporting. `height-contract.json` records
source maxima separately from the illustrative models. The generic models have
nominal heights about 0.53 and 0.63 mm; those smaller visual bodies must not replace
the maximum envelope check. Existing KiCad model license notices are preserved.

## Main hierarchy handoff and reproducibility

The main-PCB owner can copy `USB_Input-integration.kicad_sch` to its existing
`USB_Input.kicad_sch`, preserving the checked hierarchy UUID/path. All four
USB parts are excluded from the main PCB and main-board purchasing quantities;
the separate USB BOM lists their quantities. Add local library aliases:

- `Trimix_USB` symbols: `${KIPRJMOD}/../usb-input/Trimix_USB.kicad_sym`
- `Trimix_USB` footprints: `${KIPRJMOD}/../usb-input/Trimix_USB.pretty`

Then regenerate the main XML/ERC/BOM and verify exclusion and hierarchy paths.
No main-PCB files or live MCP board state were edited by this task.

`before/` preserves the original USB project. `build_usb_final.py` builds its
local libraries and schematic; `autoroute_usb.py` constructs the USB board
using the frozen original outline. They run with the existing KiCad MCP Python
virtual environment as an **offline Python interpreter**, never a live MCP
board. After that process exits, run `configure_rules.py`, because SWIG retains
in-memory project settings that it saves with the board. Run native DRC with
zone refill/save before `export_usb.py`. `final_silkscreen.py` is the recorded
incremental marking adjustment; the builder now generates those dimensions.
`export-commands.json`/`export.log` contain exact export commands and statuses.

Top/bottom native renders and the schematic were visually reviewed. The PCB and
schematic were reopened by native CLI checks. STEP importer/clearance acceptance
is the CAD integration step, not an implied pass from exporting the file. Final
source and artifact hashes are bound by `audit.json`. Physical fit, solderability,
slot precision, insertion loads, strain relief, source orientation/detach, load
heating, hot plug, ESD and leakage/sealing checks remain pending.
