# Guition shared SDMMC controller

This owned patch binds ESP-Hosted **2.12.9** (upstream commit
`09d9e983c9fc9d425d106177110165a89072533a`) to the board coordinator in
`main/board/guition_sdmmc.c`, for **ESP-IDF 5.5.4 / ESP32-P4**. Upgrading either
requires review. It adds no logger, format operation, automatic card-detect,
firmware flashing or electrical modification.

The four-file unified patch and `manifest.json` are the reviewed source. The
build includes `cmake/guition_hosted_sdmmc.cmake` after `project(...)`; that runs
`scripts/apply_hosted_sdmmc_patch.py`, verifies exact original hashes and hunk
contents, writes patched copies under the build directory, and replaces exactly
those four source entries in the Hosted target. All managed dependency files and
`dependencies.lock` remain unchanged. A changed upstream file, version, patch,
postimage or source inventory fails configuration. The build copy includes a
`patch-receipt.json` binding its inputs and outputs.

## Ownership and failure behavior

- Card slot 0 and C6 SDIO slot 1 share a single recursive FreeRTOS mutex. The
  complete FAT mount/unmount operation holds it because IDF's FAT helper invokes
  `sdmmc_host_init_slot()` directly, outside the configurable host callback table.
- Card transactions and configuration callbacks, all C6 SDIO port entry points,
  and C6 setup/teardown participate in the same arbitration. Ordinary card-side
  guard waits are limited to 250 ms; per-command card and C6 timeouts are 1000 ms.
  A full mount/probe or filesystem operation has no claimed wall-clock deadline.
  Calls belong on the storage worker, not the UI or acquisition task.
- C6 interrupt waiting polls the native semaphore without blocking while holding
  the board mutex, releases it, and sleeps one tick between unsuccessful polls.
  Transport teardown acquires the board mutex before cancelling workers. C6
  lifetime initialization and teardown wait for the current guarded operation;
  cancellation cannot strand a mutex owned by an in-flight port callback.
- Each successful slot initialization is released with `sdmmc_host_deinit_slot`.
  Forceful global deinit is permitted only for a failed initialization with zero
  registered slots and no other owner. Initializing a slot twice is rejected.
  IDF 5.5.4's deinit-slot call does not check whether that particular slot ever
  completed initialization, so the coordinator checks the actual slot count.
- An incomplete slot init while the other slot remains active is quarantined
  until the last active owner exits. Card absence after successful slot init
  releases only slot 0 and LDO4 and permits an ordinary retry while C6 stays alive.
- FATFS ignores the host-deinit callback's return and frees the card anyway.
  The coordinator separately records the consumed card pointer and any remaining
  controller/LDO ownership. A cleanup error is reported, the freed card is never
  presented as mounted, and uncertain residual hardware is quarantined until
  restart. A failed LDO release retains its handle, preventing double acquisition.
  `guition_sdmmc_is_mounted()` addresses pointer lifetime; it is not a successful
  eject signal. Only a successful unmount permits the service to report eject.
- C6 card-init retry frees the prior IDF DMA buffer; failed port allocation
  releases its acquired slot. SDIO function-probe errors propagate instead of
  aborting the P4. Failed bus setup cleans up its resources and propagates through
  `setup_transport` / `esp_hosted_init`. Unrelated upstream allocation assertions
  outside this lifecycle path are not claimed to have been removed.

API: `guition_sdmmc_mount(path, &config, &card)`,
`guition_sdmmc_unmount(path, card)`, and `guition_sdmmc_is_mounted(card)`.
The caller closes every file and stops filesystem users before unmount. Mount
rejects `format_if_mount_failed=true`. Slot 0 uses four data lines, a 20 MHz
maximum, 3.3 V default IO and no DDR/UHS request. Hosted runtime slot/pins/width
must match the board binding; Hosted must not acquire the card's LDO4.

## Board source and power review

The manufacturer [model-selection page](https://www.guition.com/model-selection)
links the [JC4880P443C_I_W development archive](https://pan.jczn1688.com/directlink/1/HMI%20display/JC4880P443C_I_W.zip).
The retained `hardware/system-review/integration-photo/guition-manufacturer/JC4880P443_V1.0.pdf`
has SHA256 `c7edaaff96dba106fc53f924db0c3054b0b2b81ec9807d59787778cc6fa48c05`.
Its single schematic sheet was visually checked at the U3 pin map and J1 TF-card
block. The adjacent source receipt records verified archive shards and ZIP CRC;
it does not claim a complete archive download.

| Item | Source-supported binding |
|---|---|
| Card CLK / CMD | U3 GPIO43 / GPIO44 |
| Card DATA0 / DATA1 / DATA2 / DATA3 | U3 GPIO39 / GPIO40 / GPIO41 / GPIO42 |
| Card supply | `ESP_LDO_VO4` through R4=0R and Q1 AO3401 to `TF_VCC` / J1 VDD |
| Q1 gate | R13=10k to GND; GPIO45 link R10 marked NC, as is parallel R7 |
| External pull-ups | R2 10k array: CMD, CLK, D2, D3; R11/R12 5.1k: D0/D1, to TF_VCC |
| Card detect / write protect | No connected controller signal shown; J1 K/pin9 unconnected |
| C6 SDIO | Existing resolved firmware: slot 1, CLK18/CMD19, D0-3=14-17, four-bit; checked at runtime |

The carrier schematic represents the P4/C6 assembly as U3 and does not expose
its internal C6 SDIO connections. Thus the retained running-target configuration
and Hosted P4 defaults support the C6 binding; this sheet alone does not prove
those internal connections. The display driver already owns **LDO3** for DSI;
the card owns **LDO4**, and Hosted LDO control is forbidden in this patch.
GPIO45 is not used as a power switch because its gate link is shown unfitted.
No weak internal pull-up is substituted for the schematic's external network.

Actual-unit PCB revision, resistor population, continuity, LDO4 voltage under
load, card behavior, concurrent radio traffic, removal and power-loss behavior
still require hardware testing. This source review is not a claim that hotplug,
abrupt removal or operation during brownout preserves files.

## Primary software sources and validation

The [IDF 5.5.4 SDMMC host documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32p4/api-reference/peripherals/sdmmc_host.html)
documents that controller initialization/configuration is not thread-safe with
transactions, per-slot deinit preserves another active slot, and global deinit
immediately removes the whole controller. The exact installed source was also
read, including `esp_driver_sdmmc/src/sdmmc_host.c` and
`fatfs/vfs/vfs_fat_sdmmc.c`.

The bundled official `examples/host_sdcard_with_hosted` establishes the two-slot
arrangement. Its IDF6 dummy-init workaround is not used here: it does not provide
this project's full lifecycle/transaction arbitration or failed-mount ownership.

Run the focused suite with:

```sh
python3 tests/test_guition_sdmmc.py
```

It compiles the actual coordinator and the exact patched interrupt-wait function
against IDF-shaped fault fixtures with real pthread mutexes. The 23 tests cover
both owner orders, absent-card retries, partial slot init, rejected duplicates,
power failures, ignored callback errors with freed cards, concurrent transfers,
bounded contention, cancellable interrupt waiting, exact patch reproducibility
and fail-closed rejection. These are host tests; real ESP-IDF target builds and
hardware validation remain separately reported by the integration owner.
