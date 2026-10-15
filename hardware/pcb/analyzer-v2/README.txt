ÆGIR / TRIMIX — PCB 2.0, integrated USB-C
2026-10-15 | revision 2.0-layout-01 | engineering review / quote candidate

WHAT THIS IS
A separate, routed four-layer PCB candidate combining the existing main board
and USB protection circuit on one 45.1 x 99 x 1.6 mm board. The new USB4105-GF-A
socket faces sideways from a small lower extension. The existing protected
2 x 18650 1S2P battery holder is retained. The enclosure must accommodate the
extension; the original main components, mounting holes and battery datums
are retained.

Open Trimix_Analyzer.kicad_pro in this directory. The engineer-reviewed source
projects in ../analyzer and ../usb-input have NOT been modified. Their hashes
are frozen in reference/original-source-hashes.json.

The copied schematic has only explicit assembly membership, connector part/
footprint metadata, and obsolete explanatory USB annotations updated. Its
wires, labels, pins and electrical connectivity are unchanged. J101 and J902
remain as excluded schematic references; neither is a fitted harness in v2.
This is a layout/connector revision, not a new electrical circuit approval.

MINIMUM CHANGES
- Add J901 USB4105-GF-A and the existing U901 TPD4E05U06DQAR and D901
  TPD1E10B06DYAR to the main PCB extension. Existing charging, CC termination,
  BC1.2 detection, input limiting, OVP and battery circuitry remain intact.
- Remove the six-wire J101 connection and separate USB PCB. Replace the old
  USB4720 connector with USB4105-GF-A. No other functional IC substitution.
- Preserve all original fitted main component positions and all original
  copper except three small VSYS segment adjustments and one redundant D-
  spur inside the removed J101 pad. The affected UUIDs are audited by script.
- Enlarge 15 U201 thermal copper lands from 0.40 to 0.45 mm diameter; keep
  their 0.20 mm holes, solder pad, paste pattern and heat paths. Two adjoining
  VSYS bend vertices move 0.05 mm in X/Y; their 1 mm trace width is retained.
- Keep four layers, the existing stackup and all 24 thermal holes with resin
  fill and copper caps. Three originally selected signal vias remain listed
  for the same treatment. Ordinary new vias remain tented; no new via-in-pad.

WHY THIS SHOULD COST LESS
Only one PCB design and one assembly are needed. The separate 0.6 mm USB
board and its harness disappear. The 99 mm maximum length is retained, and
the larger U201 copper lands meet JLC's ordinary annular-ring category.
The thermal fill/cap process is retained rather than assuming a different
solder process gives equivalent joints and cooling. Current JLC calculator: USD 50.02 for five bare boards, before shipping/tax.
This includes USD 1.05 for manual production-file confirmation. The baseline
without that optional service is USD 48.97. This is not a factory-confirmed
quote or a Norway delivered price. See reference/price-estimate.json for
the full settings and limits; the NOK 400 delivered budget is not established.
Price excludes parts and assembly unless explicitly stated otherwise.

CONNECTOR / ASSEMBLY DETAILS
J901: GCT USB4105-GF-A
https://www.digikey.no/en/products/detail/gct/USB4105-GF-A/11198441
Manufacturer drawing: https://gct.co/files/drawings/usb4105.pdf

The local footprint is explicitly adapted: its two physical ground lands
(four aliased logical pins) are moved rearward 0.025 mm to provide 0.2187 mm
clearance to the locating holes. Land sizes, socket position and holes are
unchanged. A J901-only 0.20 mm NPTH clearance rule is documented. This is
not a manufacturer-approved footprint; supplier DFM acceptance is pending.

The GF-A shell stakes are 0.95 mm long and do not emerge below a 1.6 mm PCB.
The assembler must use a suitable component-side/reflow shell-slot process.
Do not silently substitute a socket, omit shell soldering or expose tented
signal vias beneath the metal body. The socket itself is not waterproof.

VERIFICATION
- Native KiCad 10.0.6: zero DRC violations, zero unconnected items and zero
  schematic-to-PCB parity issues after zone refill on the final board.
- Independent audit: original source hashes, exact circuit net graph,
  original placement/copper preservation, connector mapping, explicit U901
  links, layers, outline, thermal geometry and component membership checked.
- The schematic has the SAME pre-existing ERC messages as the source:
  one U302 ST/open-collector versus PWR_FLAG error and two oxygen net-label
  warnings. No new ERC finding was introduced; none was silently suppressed.
- Independent exported-CAM comparison: 44 passing checks cover copper,
  holes/slots, masks/paste, connectivity and placement/BOM consistency.
  Eleven additional selected-hole/population/profile checks also passed.
- Independent USB and integrated-layout reviews are in reference/.
  These are bounded engineering checks, not assembled functional tests.

DELIVERABLES
review/pcb-v2-top.png                   Current native KiCad top render
review/Trimix_PCB_V2_Mechanical.step     Board/socket model for case adaptation
review/CASE-HANDOFF.txt                 Exact coordinate/mechanical contract
review/case-fit-proposal.svg            Dimensioned placement proposal
reference/final-drc.json                Final native board check
reference/verification/                Independent circuit/preservation checks
manufacturing/                         Quote/review CAM, BOM/POS and hole list

NEXT REQUIRED ENGINEERING WORK
Adapt the case with a supported side-port extension and close the old bottom
USB opening. Preserve the existing board/battery/display datums. The current
STEP omits three pre-existing missing component models (U110, L201, L701);
it is not an assembly collision approval. No native Fusion case edit or
new enclosure fit validation has been performed by this change.

Supplier DFM must confirm the J901 footprint/slots and the explicitly listed
filled/capped thermal holes before production. The existing charging-enable,
firmware, hot-plug/OVP, ESD and thermal qualification holds remain in force.
The new layout does not close them. The earlier enclosure/cable fit issues
also remain. Do not treat a clean DRC report as an order-ready release.

REPRODUCING / REVIEWING
Run scripts/build_variant.py using the KiCad Python environment. This rebuilds
only the derived project, refuses changed source hashes, and overwrites the
derived board/sheet files; do not run it over manual v2 edits without review.
Then run native KiCad pcb drc with --schematic-parity --refill-zones
--save-board, followed by scripts/verify_variant.py. Refresh CAM/STEP/render
after any board change. Preserve the original reviewed projects separately.
