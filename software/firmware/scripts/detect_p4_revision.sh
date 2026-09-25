#!/bin/bash
# Detect which mutually incompatible ESP32-P4 silicon family is connected.

set -euo pipefail

PORT="${1:-}"
if [ -z "$PORT" ]; then
    echo "Usage: $0 /dev/cu.usbserial-..." >&2
    exit 2
fi

if ! command -v esptool.py >/dev/null 2>&1; then
    echo "esptool.py is unavailable; source the pinned ESP-IDF environment first." >&2
    exit 2
fi

CHIP_INFO="$(esptool.py --port "$PORT" chip_id 2>&1)" || {
    printf '%s\n' "$CHIP_INFO" >&2
    exit 1
}
printf '%s\n' "$CHIP_INFO" >&2

if ! printf '%s\n' "$CHIP_INFO" | grep -qE 'Chip is ESP32-P4|Detecting chip type\.\.\. ESP32-P4'; then
    DETECTED_CHIP="$(printf '%s\n' "$CHIP_INFO" | sed -nE 's/^Chip is ([^ ]+).*/\1/p' | head -n 1)"
    printf 'Expected an ESP32-P4, but %s is attached. Refusing to select a P4 image.\n' \
        "${DETECTED_CHIP:-a different chip}" >&2
    exit 3
fi

MAJOR_REV="$(printf '%s\n' "$CHIP_INFO" | sed -nE 's/.*revision v([0-9]+)\.[0-9]+.*/\1/p' | head -n 1)"
if [ -z "$MAJOR_REV" ]; then
    echo "Could not parse the ESP32-P4 revision from esptool output." >&2
    exit 1
fi

if [ "$MAJOR_REV" -ge 3 ]; then
    printf 'v3\n'
else
    printf 'pre3\n'
fi
