#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${TRIMIX_WEB_BUILD_DIR:-$PROJECT_DIR/web/build}"

if ! command -v emcmake >/dev/null 2>&1; then
    echo "Emscripten is required. Activate emsdk so emcmake and em++ are in PATH." >&2
    exit 1
fi

emcmake cmake \
    -S "$PROJECT_DIR/web" \
    -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release

cmake --build "$BUILD_DIR" --target index --parallel

test -s "$BUILD_DIR/index.html"
test -s "$BUILD_DIR/index.js"
test -s "$BUILD_DIR/index.wasm"

echo "Web demo built at $BUILD_DIR/index.html"
