#!/bin/bash
# Validate firmware artifact size against the configured app partition.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
PARTITIONS_FILE="${PARTITIONS_FILE:-$PROJECT_DIR/partitions.csv}"
BUILD_DIR="${BUILD_DIR:-$PROJECT_DIR/build}"
APP_BIN="${APP_BIN:-$BUILD_DIR/Trimix_analyzer.bin}"
APP_ELF="${APP_ELF:-$BUILD_DIR/Trimix_analyzer.elf}"
APP_MAP="${APP_MAP:-$BUILD_DIR/Trimix_analyzer.map}"

if [ ! -f "$PARTITIONS_FILE" ]; then
    echo "Partition table not found: $PARTITIONS_FILE" >&2
    exit 1
fi

if [ ! -f "$APP_BIN" ]; then
    echo "Firmware binary not found: $APP_BIN" >&2
    exit 1
fi

APP_SIZE_HEX="$(awk -F, '
    /^[[:space:]]*#/ { next }
    /^[[:space:]]*(factory|ota_0)[[:space:]]*,/ {
        gsub(/[[:space:]]/, "", $5)
        print $5
        exit
    }
' "$PARTITIONS_FILE")"

if [ -z "$APP_SIZE_HEX" ]; then
    echo "Could not determine app partition size from $PARTITIONS_FILE" >&2
    exit 1
fi

APP_LIMIT_BYTES=$((APP_SIZE_HEX))
BIN_BYTES="$(wc -c <"$APP_BIN" | tr -d ' ')"
BIN_PERCENT=$((BIN_BYTES * 100 / APP_LIMIT_BYTES))
BIN_REMAINING=$((APP_LIMIT_BYTES - BIN_BYTES))

echo "Firmware binary: $BIN_BYTES bytes"
echo "App partition:   $APP_LIMIT_BYTES bytes"
echo "Usage:           $BIN_PERCENT%"
echo "Remaining:       $BIN_REMAINING bytes"

if [ -f "$APP_ELF" ]; then
    echo "ELF artifact:    $(wc -c <"$APP_ELF" | tr -d ' ') bytes"
fi

if [ -f "$APP_MAP" ]; then
    echo "Map artifact:    $(wc -c <"$APP_MAP" | tr -d ' ') bytes"
fi

if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
    {
        echo "## Firmware Size"
        echo ""
        echo "| Artifact | Bytes |"
        echo "| --- | ---: |"
        echo "| App partition | $APP_LIMIT_BYTES |"
        echo "| Firmware binary | $BIN_BYTES |"
        echo "| Remaining | $BIN_REMAINING |"
        if [ -f "$APP_ELF" ]; then
            echo "| ELF | $(wc -c <"$APP_ELF" | tr -d ' ') |"
        fi
        if [ -f "$APP_MAP" ]; then
            echo "| Map | $(wc -c <"$APP_MAP" | tr -d ' ') |"
        fi
    } >> "$GITHUB_STEP_SUMMARY"
fi

if [ "$BIN_BYTES" -gt "$APP_LIMIT_BYTES" ]; then
    echo "Firmware exceeds app partition size." >&2
    exit 1
fi

if [ "$BIN_PERCENT" -ge 90 ]; then
    echo "Warning: firmware uses at least 90% of the app partition." >&2
fi
