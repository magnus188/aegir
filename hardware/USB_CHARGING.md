# USB input and charging — system-review revision

**Engineering draft; order and charging remain on hold.** The selected port is
GCT USB4720-03-A on a separate 0.60 mm daughterboard. Its six-wire harness
connects to the main PCB. Earlier two-wire/passive-Rd instructions are
superseded and available through Git history.

## Connections

| Daughter J902 → main J101 | Net | Function |
|---:|---|---|
| 1 → 1 | USB_5V | Input VBUS |
| 2 → 2 | GND | Protected common return |
| 3 → 3 | USB_CC1 | Separate Type-C CC1 |
| 4 → 4 | USB_CC2 | Separate Type-C CC2 |
| 5 → 5 | USB_D_P | D+ to BC1.2 detector |
| 6 → 6 | USB_D_M | D− to BC1.2 detector |

The actual GCT contact mapping and solder-pad views are in the native schematic
and [interface packet](system-review/interface-packet.pdf). Do not confuse a
mating-face view with a rear solder view. Proposed power/signal wire gauges
and strain relief still require the measured harness route.

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
specified conditions. A latched permission circuit allows a higher limit
only after a fresh software-controlled edge; source loss and CC events clear
permission independently of a stalled processor. See the exact evolving
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

## Mechanical and protection checks

GCT specifies 0.60 ± 0.10 mm PCB thickness. The daughterboard has concealed
mechanical support and a gasket interface; final land/cutout edge clearance,
assembly fixture, plug insertion and sealing still need acceptance. The
connector's component IP rating is not an enclosure rating or charging result.
[GCT drawing](https://www.mouser.com/pdfDocs/USB4720-ProductDrawing.pdf),
[GCT product](https://gct.co/connector/usb4720)

The review specifies four-line ESD suppression for CC and data plus VBUS ESD
suppression beside the connector. Standoff voltage is not clamp voltage.
The upstream TPS259470ARPWR adds a DC overvoltage disconnect and clears
current permission through AUXOFF. Its calculated threshold is an engineering
check; its typical response time does not prove transient survival. A reviewed
2.2 A hot-plug example can exceed the downstream limiter’s 6 V absolute maximum.
Transient protection therefore remains a release hold. See the
[overvoltage bounds and limitations](system-review/electrical/usb-protection-research/upstream-ovp-review.md).

The original GCT lands/cutout leave approximately 0.10/0.15 mm copper-to-edge
clearances. They have not been qualified against a manufacturing process. The
DRC exceptions remain visible; a fabrication agreement or approved geometry
change is required before ordering the daughterboard.

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
