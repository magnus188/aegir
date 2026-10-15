# USB input and charging — integrated PCB 2.0

The current port is **GCT USB4105-GF-A (J901)**, mounted on the same four-layer,
1.6 mm PCB as the main circuit and facing the bottom of the enclosure. There
is no separate USB daughterboard or internal USB harness. Open the
[current KiCad project](pcb/analyzer-v2-bottom/Trimix_Analyzer.kicad_pro) and
read its [delivery notes](pcb/analyzer-v2-bottom/LES-MEG.txt).

The package is for prototype quotation and engineering review; fabrication
approval and charging qualification remain outstanding. The earlier human
review does not cover the new connector footprint, USB routing or case support.
Earlier two-board designs and their source evidence are in Git history at
`1fada5c`.

## Connections

| PCB net from J901 | Function |
|---|---|
| USB_5V | Input VBUS to the protection and current-limiter chain |
| GND | Circuit return and connector shell |
| USB_CC1 | Separate Type-C CC1 |
| USB_CC2 | Separate Type-C CC2 |
| USB_D_P | Joined duplicated D+ contacts, through the ESD region to BC1.2 detection |
| USB_D_M | Joined duplicated D− contacts, through the ESD region to BC1.2 detection |

These are PCB connections, not wires to install. J101 and J902 are not fitted.
Use the current native schematic/footprint and
[populated BOM](pcb/analyzer-v2-bottom/manufacturing/bom-populated-by-reference.csv)
for contact mapping and assembly. References to the old six-wire USB interface
in retained review records are historical. SBU contacts are unused. Do not
confuse the connector mating-face view with its PCB land pattern.

TUSB320LAI supplies the sink CC terminations and reports source-current
advertisement; the old external R901/R902 pull-downs are removed. CC1 and CC2
remain separate. PI3USB9201 handles BC1.2 detection on D+/D−. BQ25895 pins 2/3
remain unconnected, avoiding its autonomous high-voltage source request.
This design does not negotiate USB-PD voltage above 5 V.
[TI TUSB320LAI](https://www.ti.com/lit/ds/symlink/tusb320lai.pdf),
[Diodes PI3USB9201](https://www.diodes.com/assets/Datasheets/PI3USB9201.pdf),
[TI BQ25895](https://www.ti.com/lit/ds/symlink/bq25895.pdf)

## Current permission and charging are separate

A hardware low-current limiter precedes the charger. The selected accessible
variant is **TPS22950CQDDCRQ1**, SOT-23-THIN. Its exact Q1 specification supports
the 50 mA nominal startup setting; the ordinary industrial C variant is not an
automatic substitute. The 19.2 kΩ setting has published 34/50/66 mA limits under
specified conditions; the fitted R119 is 19.1 kΩ, so those bounds are not a
qualified guarantee for the populated circuit. A latched permission circuit
allows a higher limit only after a fresh software-controlled edge; source loss
and CC events clear permission independently of a stalled processor. See the current
native circuit and [Q1 audit](system-review/electrical/usb-protection-research/tps22950-q1-review.md).

Firmware starts with BQ input isolation (`EN_HIZ=1`) verified by readback and
a 100 mA register ceiling. It reconnects input and can raise that ceiling to
1400 mA after consistent Type-C 1.5/3 A advertisement or a matching BC1.2
CDP/DCP classification with fresh attachment health. Default, SDP, proprietary
and unknown sources remain isolated; no USB enumeration is implemented.
The low register setting is a ceiling, not permission to draw that current.
[Input isolation and source limits](system-review/software/power/input-isolation/README.md). The lower of the
upstream limiter, charger hardware limit and register setting governs actual
current. A 1400 mA register value is not a promise of 1400 mA delivered power.

GPIO50/J301.11 now controls current permission. The gauge alert is polled.
The software removes permission during faults, stale source evidence, source
changes, maintenance and shutdown. Source classification is refreshed before
permission is asserted, and maintenance epochs invalidate previously cached
permission even if maintenance finishes between worker polls. GPIO49/J301.13
now reads the shared charger IRQ and inverted latch feedback. A sustained LOW
triggers fresh qualification; brief charger IRQ pulses receive a settling
interval. The software distinguishes commanded from confirmed permission.
[Feedback review](system-review/software/power/usb-latch-feedback-review.md)

The low-current default cannot run the whole touchscreen/heater load. A charged
pack may supplement insufficient USB power. The implemented standby path can
keep the host's source worker running with a dark screen and stopped heater,
but requires startup acceptance, battery reserve and a future qualified charge
profile. Recovery of an empty/protection-disconnected pack from true off remains
unverified; firmware cannot bootstrap it simply by requesting more current.
[Standby implementation and fault tests](system-review/software/power/off-charge-review/standby-implementation.md)

**J104 remains open and cell charging is inhibited in the current firmware.**
Exact cell charging limits, pack NTC, holder/protection ratings, connector
polarity and charging fault behaviour must be qualified before arming.
Successful USB attachment or current detection does not establish cell safety.

## Mechanical, assembly and protection checks

USB4105-GF-A is top-mounted on the 1.6 mm main board. Its nominal 0.95 mm shell
stakes do not protrude through that board. The assembler must approve soldering
and inspection of the plated shell slots; an ordinary protruding-pin process
cannot simply be assumed. The footprint's four ground lands have a documented
0.025 mm rearward offset, with supplier DFM acceptance still pending. See the
[archived GCT drawing](pcb/analyzer-v2-bottom/reference/USB4105-current.pdf),
[adaptation record](pcb/analyzer-v2-bottom/reference/local-adaptation-review.json)
and [fabrication/assembly requirements](pcb/analyzer-v2-bottom/manufacturing/FABRICATION_AND_ASSEMBLY_REVIEW_NOTES.txt).

The matching [bottom-port enclosure](cad/pcb-v2-bottom/LES-MEG.txt) provides
board supports and insertion/removal stops. Cable loads still pass through the
connector's soldered shell joints and board mounts. Physical plug fit, insertion,
withdrawal, side-load and support-strength tests remain necessary. This port
and enclosure are not qualified as waterproof.

U901 (TPD4E05U06) provides four-line ESD suppression for CC and data; D901
(TPD1E10B06) provides VBUS ESD suppression beside the connector. U901's paired
pads require the explicit PCB copper bridges; the IC does not internally
provide these pass-through connections. Standoff voltage is not clamp voltage.
The upstream TPS259470ARPWR adds a DC overvoltage disconnect and clears
current permission through AUXOFF. Its calculated threshold is an engineering
check; its typical response time does not prove transient survival. A reviewed
2.2 A hot-plug example can exceed the downstream limiter’s 6 V absolute maximum.
Transient protection therefore remains a release hold. See the
[overvoltage bounds and limitations](system-review/electrical/usb-protection-research/upstream-ovp-review.md).

The current [USB path checks](pcb/analyzer-v2-bottom/reference/verification/usb-path-review.json)
and [native DRC](pcb/analyzer-v2-bottom/reference/final-drc.json) concern the
integrated layout. They do not establish transient immunity, solder-joint
strength or factory acceptance. The current manufacturing package retains
the four-layer stack and 27 specifically selected resin-filled/copper-capped
holes; ordinary tenting is not a substitute for that selected-hole process.

## Physical acceptance — all pending

Keep cells and external modules disconnected and J104 open for initial tests.
Inspect solder, pin mapping and protected-ground continuity with power removed.
Use a controlled source and load; record instruments and source/cable identity.
A multimeter cannot capture hot-plug overshoot, inrush or brief bus faults.

| Test | Required observation |
|---|---|
| A-to-C, both C orientations | Correct attachment, source classification and permitted current |
| C-to-C, all end orientations and marked cable | Correct attachment and CC advertisement |
| No battery / host off | Hardware attachment and conservative input behaviour without firmware |
| Default/SDP/unknown source | No high-current permission |
| Native 1.5/3 A or qualified CDP/DCP | Fresh evidence and bounded permission sequence |
| Disconnect, source swap, controller reset, stale evidence | Permission removed; stalled-high GPIO cannot silently rearm |
| Hot plug, weak cable, brownout, overvoltage | Bounded input current/voltage, no unsafe transient or reset loop |
| Cell charging after separate qualification | Correct current, voltage, termination, temperature and fault inhibition |

The full [measurement and first-power checklist](system-review/electrical/measurement-and-bringup.md)
also covers reverse current, Guition dual supplies, shutdown and pack NTC faults.
No USB or charging result has been physically verified in this review.
