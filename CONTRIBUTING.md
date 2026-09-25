# Contributing

Start with the [project map](README.md) and the relevant area guide:
[firmware](software/firmware/README.md), [CAD](hardware/cad/README.md), or
[PCB](hardware/pcb/README.md).

For software changes, run `make test` from the repository root. If you have
the pinned ESP-IDF toolchain, also build the affected P4 revision with
`make build P4_REV=pre3` or `make build P4_REV=v3`. The desktop simulator and
Hosted patch integration require downloaded managed components; the test
runner reports when they are unavailable.

Keep editable CAD and KiCad sources with their design area. Include a STEP,
preview, CAM, or print project when it helps someone review or reproduce the
current design. Do not add local editor state, build outputs, logs, G-code,
autosaves, or intermediate copies of a whole project. Git history retains
earlier design checkpoints.

Hardware changes should update the relevant interface and acceptance notes.
The [whole-system review](hardware/system-review/README.md) records unresolved
physical and supplier checks. A passing digital check does not authorize a
prototype order or establish breathing-gas safety.
