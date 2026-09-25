#!/bin/bash
# Update the semantic version macros without replacing the rest of version.h.

set -euo pipefail

VERSION="${1:-}"
if [[ ! "$VERSION" =~ ^(0|[1-9][0-9]{0,4})\.(0|[1-9][0-9]{0,4})\.(0|[1-9][0-9]{0,4})$ ]]; then
    echo "Usage: $0 MAJOR.MINOR.PATCH" >&2
    exit 2
fi

IFS='.' read -r MAJOR MINOR PATCH <<< "$VERSION"
if (( MAJOR > 65535 || MINOR > 65535 || PATCH > 65535 )); then
    echo "Version components must fit the device OTA limit 65535." >&2
    exit 2
fi
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VERSION_HEADER="$(dirname "$SCRIPT_DIR")/main/version.h"

perl -0pi -e "s/#define TRIMIX_ANALYZER_VERSION_MAJOR \\d+/#define TRIMIX_ANALYZER_VERSION_MAJOR $MAJOR/; s/#define TRIMIX_ANALYZER_VERSION_MINOR \\d+/#define TRIMIX_ANALYZER_VERSION_MINOR $MINOR/; s/#define TRIMIX_ANALYZER_VERSION_PATCH \\d+/#define TRIMIX_ANALYZER_VERSION_PATCH $PATCH/; s/#define TRIMIX_ANALYZER_VERSION \"[^\"]+\"/#define TRIMIX_ANALYZER_VERSION \"$VERSION\"/" "$VERSION_HEADER"

echo "Updated firmware version to $VERSION"
