# Ægir

![Ægir 1 logo](branding/aegir-lockup-dark.png)

Ægir is a prototype trimix gas analyzer project with editable hardware
design files and ESP32-P4 software. The project is **not qualified as a
breathing-gas safety instrument**. The current
[whole-system review](hardware/system-review/README.md) lists the unresolved
electrical, mechanical, sensor, and physical-test work. **Prototype order:
HOLD.**

## The name

In Norse myth, [Ægir](https://snl.no/%C3%86ge) is a sea giant who welcomes the
gods to a hall lit by gold. His nine daughters with Rán are the waves—a fitting
namesake for a diving instrument.

Say **Ægir** roughly **AG-eer**: the **æ** sounds like the *a* in *cat*, and the
**g** is hard. The runes **ᛅᛁᚾ** are our Viking Age-style nod to
[*ein/einn*](https://ordbokene.no/nob/nn/14065), meaning “one”: **Ægir 1**.

## Project map

| Area | Contents |
| --- | --- |
| [Software](software/README.md) | ESP32-P4 firmware, desktop simulator, web demo, host tests, and development scripts |
| [CAD](hardware/cad/README.md) | Editable Fusion enclosure, STEP export, drawings, and 3D-print package |
| [PCB](hardware/pcb/README.md) | KiCad analyzer and USB-input projects, libraries, and current manufacturing review |
| [Hardware review](hardware/system-review/README.md) | Interface contracts, qualification status, and current engineering evidence |
| [Branding](branding/README.md) | Selected Ægir 1 logo, wordmark, and device splash |

The active firmware workspace is [`software/firmware/`](software/firmware/).
From the repository root, `make help` lists the common commands, `make test`
runs host validation, and `make build P4_REV=pre3` builds a firmware profile
when the pinned ESP-IDF toolchain is installed. See the
[firmware guide](software/firmware/README.md) for setup and board-specific
instructions.

The active PCB source is
[`hardware/pcb/analyzer/Trimix_Analyzer.kicad_pro`](hardware/pcb/analyzer/Trimix_Analyzer.kicad_pro).
The latest enclosure work and print files are indexed from
[`hardware/cad/README.md`](hardware/cad/README.md). Earlier design checkpoints
remain available through Git history instead of being duplicated throughout
the working tree.

See [CONTRIBUTING.md](CONTRIBUTING.md) for where new files belong and which
checks to run. Generated binaries, logs, local KiCad state and printer G-code
are ignored.

## License

Ægir software is [GPLv3](LICENSES/GPL-3.0-only.txt); project-created PCB and
enclosure/3D-print designs are [CERN-OHL-S-2.0](LICENSES/CERN-OHL-S-2.0.txt).
Both allow commercial use and require source sharing when covered work or
products are distributed. The Ægir name and logo have a separate
[brand policy](branding/LICENSE.md). See the [license map](LICENSE.md) for
third-party exceptions.
