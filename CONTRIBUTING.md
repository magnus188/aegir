# Contributing

Start with the [project overview](README.md). For details, use the
[firmware guide](software/firmware/README.md), the
[enclosure CAD](hardware/cad/rev04/3d-print/Trimix_Enclosure_A3_PrintReview.f3d),
or the [main PCB project](hardware/pcb/analyzer/Trimix_Analyzer.kicad_pro).

Before contributing, read the [license map](LICENSE.md). Submit original work
under the license for its area: GPL-3.0-only for code and CERN-OHL-S-2.0 for
hardware designs. Keep third-party notices with any external files and include
their source and redistribution terms. Do not add brand assets or external
fonts as if they were covered by the code or hardware licenses.

## Start with an issue

Search the [existing issues](https://github.com/magnus188/aegir/issues) before
opening a new one. Bug reports, measurements, design reviews, documentation and
small fixes are all useful contributions; you do not need to design a PCB.

For a new idea, describe the problem, the proposed option and what still needs
investigation. The [v2 investigation milestone](https://github.com/magnus188/aegir/milestone/1)
collects possibilities, with no promised implementation or release date. Discuss
larger changes in an issue before investing in an implementation.

## Make a pull request

1. Fork the repository and create a branch from current `main`.
2. Keep the change focused and link the issue it addresses. Open a draft pull
   request early if you want feedback on the approach.
3. Explain the problem, your change, and the evidence used to check it. Name
   anything you could not test; do not mark a check complete because AI or a
   simulator predicted a result.
4. Wait for the automated checks and address review comments. A maintainer
   reviews contributions and merges the pull request.

Use a clear PR title, such as `fix: reject an invalid calibration value` or
`docs: explain the sample inlet`. The title is used for squash merges and
automatic release versioning: `feat:` selects a minor release, `!:` or
`BREAKING` selects a major release, and other titles select a patch release.

AI-assisted contributions are welcome when the author can explain the change
and verify its claims. Say where AI assistance materially affected the design
or implementation, and identify assumptions that still need human review.
Be respectful and specific when discussing someone else's work.

## Software checks

For software changes, run `make test` from the repository root. If you have
the pinned ESP-IDF toolchain, also build the affected P4 revision with
`make build P4_REV=pre3` or `make build P4_REV=v3`. The desktop simulator and
Hosted patch integration require downloaded managed components; the test
runner reports when they are unavailable.

CI builds both P4 revisions and runs the simulator tests. Passing those checks
does not validate sensor accuracy, calibration, gas handling or physical safety.

## Hardware and measurement changes

Keep editable CAD and KiCad sources with their design area. Include a STEP,
preview, CAM, or print project when it helps someone review or reproduce the
current design. Do not add local editor state, build outputs, logs, G-code,
autosaves, or intermediate copies of a whole project. Git history retains
earlier design checkpoints.

Hardware changes should update the relevant interface and acceptance notes.
The [whole-system review](hardware/system-review/README.md) records unresolved
physical and supplier checks. A passing digital check does not authorize a
prototype order or establish breathing-gas safety.

For changes to measurement, calibration, alarms, power or gas handling, describe
failure cases and provide the relevant calculations, datasheets and reproducible
test evidence. Seek review from someone competent in the affected area before
accepting a design as validated. Record simulator results, bench measurements
and comparisons with reference instruments separately. If physical validation
is pending, keep that limitation explicit in the review and acceptance notes.

Report exploitable vulnerabilities through the private route in
[SECURITY.md](SECURITY.md). Ordinary bugs and hardware review findings belong in
public issues. Maintainers can follow the [repository guide](.github/MAINTAINING.md).
