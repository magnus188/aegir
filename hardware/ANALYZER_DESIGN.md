# Ægir — whole-system review

**Order status: HOLD.** Open `pcb/analyzer/Trimix_Analyzer.kicad_pro` for the
current main design and `pcb/usb-input/Trimix_USB_Input.kicad_pro` for the
thin USB daughterboard. The [system-review package](system-review/README.md)
contains current electrical, software and mechanical evidence. Earlier designs
are preserved, including this document's
[previous version](system-review/baseline-docs/ANALYZER_DESIGN.md).

The approved A3 exterior remains 85 × 180 × 43 mm. The separate Fusion design
is `Trimix_Enclosure_A3_SystemReview`; earlier PCBFit geometry is a preserved
baseline. A clear placement or CAD check is not a routed fabrication release.

## Installed modules and supply domains

| Assembly | Current basis |
|---|---|
| Host | Guition JC4880P443C_I_W, 4.3-inch portrait ESP32-P4 display |
| Pack | Two owner-confirmed 3400 mAh 18650 cells, 1S2P, 6800 mAh nominal; exact cell ratings remain unverified |
| Holder | Protected FMA FPML1S2P050C; retain its actual protected plug, identify mate and polarity before wiring |
| USB | GCT USB4720-03-A, 0.60 mm daughterboard, six-wire connection to main board |
| Oxygen | One installed AO₂ or R17JJ-CCR; separate J401/J402 differential input paths |
| Other sensors | MD62, actual BME280 humidity module, ZE07-CO |
| User button | Owned illuminated momentary 1NO button; hardware long-press shutdown retained |

All ground returns use protected holder P−, never raw cell negative. BQ25895,
MAX17048 and LTC2954 remain in the always-connected domain. LTC2954 controls
the TPS63020 switched 5 V supply. LM66100 provides reverse blocking on the host
5 V output, but the Guition's own USB/IP5306 power arrangement still needs
qualification before simultaneous external supplies are connected.

J104 charge ARM remains **open** and current firmware inhibits cell charging.
This stays in force until actual cell limits, holder ratings, pack NTC and
charging behaviour are qualified. USB input-current permission is separate
from permission to charge the cells. SW101 is an internal charger QON service
switch, not the normal user power button. The NTC must thermally contact the
pack; the chamber humidity sensor cannot substitute for it.

Read [USB charging integration](USB_CHARGING.md) for the CC/BC1.2 controllers,
independent low-current startup limiter and unresolved transient checks.

## Measurement paths

U401 and U502 are **ADS122C04IPWR**, TSSOP-16, at 0x40 and 0x41. Both use the
internal 2.048 V reference and 20 SPS conversion setting with excitation-current
outputs disabled. Oxygen uses gain 8/PGA; helium gain 1/bypass. Diagnostics have
separate settings. Sequential conversions and settling make the delivered
rate lower than 20 samples per channel per second.
[TI ADC datasheet](https://www.ti.com/lit/ds/symlink/ads122c04.pdf)

J401/J402 retain the approved positions. The SMB shell is the negative sensor
signal, **not ground or chassis**, and must not receive a 50 Ω termination.
The newly selected J402 part is Amphenol RF 142138; its drawing-derived model
does not identify or qualify the owned 90° cable. The common thread and
relative sensor dimensions are owner measurements; seal/shoulder/cable bounds
are still required. AO₂'s larger diameter and JJ-CCR's extra 2 mm length govern
the alternative chamber envelopes.

MD62 retains regulated 3.0 V excitation through TPS7A2030PDBVR. RN501 is the
fixed matched 2 kΩ/2 kΩ reference divider. U502 measures **HE_REF−HE_SENSE**;
R506/R507 are 680 Ω. The selected supply must meet the sensor's 3.0 ± 0.1 V
requirement at its actual leads under load. Helium interpretation of MD62
remains experimental and must be characterized with reference mixtures.
[Winsen MD62 manual](https://www.winsen-sensor.com/d/files/thermal/md62.pdf)

The humidity driver verifies a BME280, including its factory trim; a BMP280
does not provide humidity. ZE07-CO stays distinct from CO₂ and receives no
invented calibration commands. Its humidity/use restrictions and the absence
of a specified startup-current maximum remain qualification inputs.

## Software and harness contract

The authoritative logical mapping is
[`main/hardware_contract.h`](../main/hardware_contract.h), checked against
the [schematic contract](system-review/electrical/interface-contract.md).
JP1 physical pitch, orientation and mating height are still unconfirmed.

| Signal | J301/Guition logical pin | P4 GPIO |
|---|---:|---:|
| Sensor I²C SDA/SCL | 21 / 14 | 28 / 29 |
| CO UART TX/RX | 12 / 10 | 30 / 31 |
| Power interrupt/KILL | 19 / 8 | 32 / 33 |
| MD62 enable | 17 | 34 |
| Charger IRQ / USB latch feedback | 13 | 49 |
| USB current permission | 11 | 50 |
| CO translator enable | 9 | 51 |
| Unused | 7 | 52 |

The managed 100 kHz sensor bus serves gauge 0x36, oxygen 0x40, helium 0x41,
charger 0x6A, CC controller 0x47, BC1.2 detector 0x5F and BME280 0x76/0x77.
Touch GPIO7/8 uses its own bus. Gauge alert is polled; GPIO50 is now the USB
permission output. Hardware builds report unavailable data when reads fail;
they do not replace it with simulator readings.

Oxygen selection, calibration, diagnostics and normal operating policies are
software-defined. Selection persists across compatible OTA updates, never
changes automatically during measurement, and keeps separate calibration
identities. The known-air screen currently remains manual/inconclusive
because exact owned-sensor response bounds are missing. See
[software calibration](SOFTWARE_CALIBRATION.md).

OTA and shutdown share a maintenance coordinator that stops acquisition,
disables heating and drains or rejects pending storage writes. Storage is
centralized and versioned without automatic whole-partition erasure. The
firmware retains application-only HTTPS OTA and separate P4 pre-v3/v3 builds.
No device was flashed or updated during the review.

## Acceptance

Use the [verification receipt](system-review/verification/README.md), fresh
native ERC/DRC/netlist checks and final CAD imports together. Unsupported
models, unresolved mates, component tolerances and unfinished routing remain
visible release gates. Physical charging, thermal, fit, seal and reference-gas
tests remain pending. The ±0.2-point O₂ and ±0.5-point He figures are unproven
targets. Showcase, production print preparation and purchasing remain paused.

The project remains inspired by
[captainigloo/Trimix-analyzer](https://github.com/captainigloo/Trimix-analyzer).
Existing attribution and licensing remain unchanged; manufacturer evidence
and third-party software retain their own terms.
