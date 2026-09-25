# Ziphius

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="branding/ziphius-lockup-white.svg">
  <img src="branding/ziphius-lockup.svg" alt="Ziphius logo" width="440">
</picture>

Ziphius is an open-source trimix gas analyzer project with editable hardware
design files and ESP32-P4 software. The
project is **not qualified as a breathing-gas safety instrument**. The current
[whole-system review](hardware/system-review/README.md) lists the unresolved
electrical, mechanical, sensor, and physical-test work. **Prototype order:
HOLD.**

## Project map

| Area | Contents |
| --- | --- |
| [Software](software/README.md) | ESP32-P4 firmware, desktop simulator, web demo, host tests, and development scripts |
| [CAD](hardware/cad/README.md) | Editable Fusion enclosure, STEP export, drawings, and 3D-print package |
| [PCB](hardware/pcb/README.md) | KiCad analyzer and USB-input projects, libraries, and current manufacturing review |
| [Hardware review](hardware/system-review/README.md) | Interface contracts, qualification status, and current engineering evidence |
| [Branding](branding/README.md) | One-colour vector logo and wordmark suitable for device marking |

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
