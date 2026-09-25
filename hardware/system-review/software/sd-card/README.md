# Optional SD recording

This feature uses the onboard microSD socket in the Guition display board.
It adds no socket to the main PCB. Access is behind the rear cover; sideways card
ejection may also require releasing the display, pending the mechanical fit check.
The firmware has not been flashed during this work and the installed card has
not been mounted, inspected, formatted or written by these development tools.

## Everyday use

Opening **Analyse Mix** automatically records results and raw samples. Opening
**Gas calibration** starts a separate calibration session and records capture/save
events. Leaving those screens closes the session. Going back starts new files.
Measurement faults are recorded with validity/fault fields and unavailable values;
logging does not turn them into valid concentrations.

**Device Settings** shows card status, capacity when known, rows submitted to the
filesystem, rows acknowledged by filesystem sync, missing records and the current
session path. **Safely eject SD card** requests a background close and unmount.
Wait for **SD safe to remove** before removing it. A failed unmount reports
unavailable and does not claim safe removal. **Retry SD card (preserve files)**
attempts a new mount; there is no automatic format, deletion or overwrite policy.
An unsupported, absent, corrupt or full card leaves the analyzer available, with
logging unavailable. Existing card contents are preserved by the software path.

## Files and interpretation

Files are created below `TRIMIX/<boot-id>/<session-id>/`, using 8.3-compatible
names. The boot identifier is a collision-checked identifier, not a date. Each
directory contains `RESULTS.CSV`, `RAW.CSV` and `EVENTS.CSV`. An existing session
directory is never reused; files are created exclusively. Failed/incomplete
sessions are left available for inspection.

The first two lines begin with `#` and describe schema (currently 2), firmware, session and
activity. The next line contains column names and units. CSV readers should
ignore comment lines. The final incomplete line after power loss must not be
treated as a complete measurement. There is no automatic repair of existing files.

- Results contain sensor/source/selection identity, both applied calibration revisions,
  measured and analyzed gas fractions, CO validity, chamber environment,
  gas mode, planned depth and advisory state. Periodic analysis rows normally
  arrive every second; calibration result snapshots every half second. Each
  applied oxygen/helium revision is returned under the same calibration lock as
  its converted concentration. A concurrent save cannot relabel that value.
- Raw rows retain ADC code, voltage, gain, excitation and reference bias,
  acquisition revision, fault mask, warm-up, selection generation and observed
  calibration revision observed when the raw row was collected. That observed
  revision is diagnostic context, distinct from the applied revisions in a
  converted result. This follows actual acquisition timing, not a claim of
  20 complete channel scans per second. Raw records are not calibrated gas results.
- Events record baseline/span capture attempts and save outcomes, numeric
  reference concentrations/uncertainty, and the last saved coefficient/revision.
  They are diagnostic context; the CSV files are not an importable calibration
  backup and are never read back to overwrite calibration.
- `uptime_ms` is 64-bit monotonic logging time. `sample_ms` retains the original
  32-bit acquisition clock, which wraps after about 49.7 days. No UTC timestamp
  is invented when the device has no verified wall clock.
- `dropped_total` is a cumulative count for this boot. Queue contention,
  overflow, oversized records and unavailable media are observable gaps.
  Gaps are not interpolated. `nan` and validity flags must be retained during
  analysis. `calibration_unvalidated` remains explicit.

Authoritative calibration, selected O2 cell, settings and the latest 20 manually
saved analyses remain in the existing versioned NVS journals. SD faults do not
erase or migrate that storage. OTA images and rollback storage do not depend
on the card. Ordinary missing optional media is outside startup probation health.

## Implementation and power transitions

The production logger has a 64-record fixed queue (768-byte row budget) and one
low-priority writer. Acquisition/UI producers try-lock only the in-memory queue;
they never mount, write or sync a card. All file operations run in the writer.
Files are synced approximately once per second and closed at session boundaries.
Filesystem sync is an acknowledgement from the stack, not proof of power-loss
protection inside an arbitrary card.

The existing maintenance barrier pauses producers and waits within its supplied
budget for the writer to finish and close. OTA/shutdown cannot report that barrier
held while card I/O is still in flight. If startup is interrupted before mounting,
unsaved RAM rows are counted and discarded. Failed optional media with no active
file operation does not permanently prevent OTA. Maintenance cancellation resumes
logging in a new session. Standby suppresses logging alongside acquisition; waking
resumes it. Hardware forced shutdown remains independent and can interrupt a write.

SD slot0 and C6 slot1 share the ESP32-P4 SDMMC controller. The owned board layer
serializes configuration/transactions/lifecycle, preserves the other slot on
teardown, and handles partially failed mounts without retaining freed card pointers.
The pinned ESP-Hosted patch is generated into each build directory after exact
source/hash checks; managed component originals are unchanged. Dependency drift
fails configuration rather than silently using an incompatible patch. See
[the patch and board-source review](../../../../software/firmware/patches/esp_hosted/README.md).

The authoritative Guition schematic is retained at
`hardware/system-review/integration-photo/guition-manufacturer/JC4880P443_V1.0.pdf`,
with archive provenance in its adjacent `source-receipt.json`. It shows card
CLK43/CMD44/D0–3=39–42, LDO4 supply, an unfitted GPIO45 power-control link, and
no connected card-detect signal. The display keeps LDO3. Firmware therefore uses
the board's native four-bit slot, conservative 20 MHz maximum, and no invented
GPIO45/card-detect control.

The internal C6 SDIO link uses slot1, D0–3 GPIO14–17, CLK18, CMD19 and reset54;
the card uses slot0. The external main board uses GPIO28–34 and 49–51. These
allocations do not overlap; GPIO52 remains reserved and unused. The generated
pre3/v3 configurations, source hashes and unchanged native PCB hashes are bound
in `evidence/gpio-allocation.json`. The card pin/supply assignment is grounded in
the manufacturer schematic; the C6 assignment is also checked against the pinned
firmware configuration and runtime guards. Actual board population remains pending.

## Verification and limits

The final source-bound results are in `evidence/receipt.json`; reproduction
commands are in `evidence/commands.md`.
`evidence/source-before.json` was captured before those checks; the receipt refuses
source drift. Earlier pre-SD successful files remain in
`hardware/system-review/verification/before-sd-storage/`. Preliminary SD checks
preceding the final pause/eject corrections are separately preserved under
`evidence/before-final-edge-fixes/`; they are not final acceptance evidence.
`evidence/before-idle-pump-fix/` also preserves a failed repeated maintenance test:
an idle writer briefly reported itself busy after the barrier had drained.
The final writer skips those no-op wakeups. The revised test checks 1,000 idle
wakeups, and 101 consecutive executions of that fixture passed.
The independent review also caught a combined close/unmount failure during a
session change and a split conversion/revision lookup. Both have deterministic
regressions with isolated negative controls that fail against the old behavior.
Real LVGL screen transitions are checked so loading calibration before analysis
finishes unloading does not turn off calibration recording. Earlier passing
results before these final corrections are under `evidence/before-compound-close-fix/`.

Final checks passed: both native firmware builds, 53 CTest cases normally and
under AddressSanitizer/UndefinedBehaviorSanitizer, 98 repository checks and
22 portable Clang analysis units. ThreadSanitizer passed the production SD
queue/files/format suite (230 assertions) and NVS/maintenance suite (28 functional
assertions plus 1,000 idle-wakeup checks). The controller/Hosted fixture contributes
23 tests, including lifecycle faults, teardown races and checked patch provenance.
The analysis, calibration and Device Settings captures were inspected for readable
status and controls; they are simulator views, not a connected card demonstration.

| Image | Application bytes | Smallest OTA slot | Static DIRAM sections | Data + BSS |
|---|---:|---:|---:|---:|
| P4 pre3 | 2,051,056 | 4,194,304 | 202,962 | 109,180 |
| P4 v3 | 2,056,432 | 6,291,456 | 203,432 | 109,196 |

Both images fit both installed OTA slots and preserve exact silicon-family
identity. Builds emitted no compiler warnings. Static allocations are not free
runtime heap: the fixed logger occupies about 50 KiB, with a further 6 KiB writer
task stack and dynamic filesystem/controller buffers. The installed size tool's
older chip tables warn about v3 region classification; original outputs are kept,
and their total/free-memory estimates are not used as a runtime-memory guarantee.

Tests exercise the actual queue, CSV formatters and POSIX file writer, the actual
NVS/maintenance coordinator, and the actual board coordinator with explicit IDF
hardware faults. Existing power/CO fixtures substitute an unavailable SD boundary;
they do not independently execute the new ESP SD writer task. Native pre3/v3 builds
verify compilation/linking of that task, board layer and patched Hosted transport.

Still pending on the real unit: card capacity/filesystem identification, board
revision/population, LDO4 voltage and current during card writes, memory headroom
under real display/radio workload, simultaneous Wi-Fi/SD traffic, insertion/eject,
forced-cutoff/card power-loss behavior and data recovery. No filesystem promises
zero data loss under abrupt power removal. Existing battery commissioning,
charging inhibition, gas-sensor qualification and PCB order gates are unchanged.
