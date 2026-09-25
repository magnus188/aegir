# Dated whole-system verification records

This directory contains point-in-time digital review receipts and interface
renders from the September 8 engineering checkpoint. It records the status
and source hashes of that checkout, including two ESP32-P4 builds, normal and
sanitized host tests, static analysis, and hardware interface checks. It is
not a fresh build of the reorganized software workspace.

Run `make test` from the repository root for current host validation. With
ESP-IDF managed components absent, the simulator and Hosted patch integration
are skipped with explicit warnings. Build both P4 profiles and refresh
source-bound receipts before relying on them for a later release or hardware
acceptance decision. See the [whole-system acceptance ledger](../acceptance.csv)
for unresolved physical and supplier gates.

Existing JSON, PDF and image files in this directory are retained as dated
engineering evidence. Earlier intermediate snapshots and generated build logs
are available through Git history.
