# A3 bottom USB interface — source and limits

The purchased connector reference is **GCT USB4720-03-A**. Geometry in
`usb_a3.py` is reconstructed from the [GCT USB4720 Rev B drawing](https://gct.co/files/drawings/usb4720.pdf),
not an authenticated supplier STEP model. The retained source PDF is
`../../../pcb/usb-input/reference/GCT_USB4720_RevB_drawing.pdf`; its SHA-256 is
`b3347df8cf39cc4f72e60f88708d3ddedec681d77bfcbbc9a3a5fd53975a5be6`.

The A3 assembly opens toward −Y. The small **14 × 8 mm metal bezel** is flush
with the bottom face; its larger 20 × 11 mm flange is hidden inside. A rear
relief leaves the drawing's local **1.80 mm reference panel section**, rather
than adding a printed tunnel in front of that section.

| Drawing section | Nominal CAD reference |
|---|---|
| Throat | 8.44 × 2.66 mm, R1.05 |
| Inner lip | 9.14 × 3.35 mm, R1.39; 0.17 mm depth |
| Gasket seat | 9.64 × 3.86 mm, R1.65; 0.50 mm depth |
| External entry | 1.13 mm depth, 10° lead-in |
| Daughterboard | 0.60 mm, drawing tolerance ±0.10 mm |

R0.20 transitions are omitted. Nominal shell, insulator, gasket and grounding
wings are drawing references; contact stripes are illustrative. The PCB
outline has support and connector clearances but is **not a routed or released
footprint**. Drawing tolerances remain requirements for the final interface.

Two internal M2×5 screws hold one common bridge over the supported board and
hidden metal flange. The nominal screw/insert axial overlap is 3 mm, with 1 mm
to the insert's front end. Printed insert bosses have a 7.3 mm outer diameter
around a 3.3 mm pilot, giving a local nominal 2 mm radial wall. The printed
support floor is 2 mm thick. These selected dimensions do not establish a
global minimum wall or heat-set process qualification.

The housing supports the cartridge from the front. The single rear cover
carries an L-shaped key with 0.5 mm nominal inward and rearward stop gaps.
That key bounds movement; it supplies no claimed gasket preload. After the
cover, battery and USB cable are removed, the candidate service path is
2.5 mm inward (+Y), then rearward (+Z). A notch in the provisional main PCB
and carrier must clear the right insert boss during that movement.

Physical plug loads pass through the connector's soldered grounding tabs to
the closely supported board and carrier. The connector has no invented rear
clamp shoulder in this model. Solder-tab strength, board flex, printed creep,
bezel manufacture, gasket compression and the separate bezel-to-housing seam
need physical validation. The connector's advertised IP67 rating does not
qualify the complete enclosure, and the PLA fit prototype is not a validated
waterproof charging assembly.
