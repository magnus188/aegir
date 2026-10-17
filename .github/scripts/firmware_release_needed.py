#!/usr/bin/env python3
"""Gate automatic releases on changes to inputs of the ESP-IDF firmware build."""

import argparse
from pathlib import PurePosixPath
import re
import subprocess


def is_firmware_input(path):
    prefix = "software/firmware/"
    if not path.startswith(prefix):
        return False
    relative = PurePosixPath(path[len(prefix):])
    name = relative.name.lower()
    if relative.suffix.lower() in {".md", ".rst"} or name in {
        "readme", "license", "copying", "notice",
        "readme.txt", "license.txt", "copying.txt", "notice.txt",
    }:
        return False
    if relative.parts[0] in {"main", "cmake", "patches"}:
        return True
    if len(relative.parts) == 1:
        return (
            name in {"cmakelists.txt", "dependencies.lock"}
            or name.startswith("sdkconfig.defaults")
            or (name.startswith("partitions") and name.endswith(".csv"))
        )
    return str(relative) == "scripts/apply_hosted_sdmmc_patch.py"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--event", required=True, choices=["push", "workflow_dispatch"])
    parser.add_argument("--before", default="")
    parser.add_argument("--after", default="")
    args = parser.parse_args()

    if args.event == "workflow_dispatch":
        needed = True
    else:
        if not all(re.fullmatch(r"[0-9a-f]{40}", ref) for ref in (args.before, args.after)):
            parser.error("push comparisons require full before and after commit SHAs")
        # Compare the entire push without GitHub's path-filter diff limits.
        # Disabling rename detection includes both the old and new paths, so a
        # deletion or a move out of the firmware tree still triggers a release.
        changed = subprocess.check_output([
            "git", "diff", "--name-only", "--no-renames", "-z",
            args.before, args.after, "--",
        ])
        needed = any(is_firmware_input(path.decode("utf-8", errors="surrogateescape"))
                     for path in changed.split(b"\0") if path)

    # A failed git comparison exits before producing an output; the release
    # job therefore cannot publish on incomplete change information.
    print(f"release={str(needed).lower()}")


if __name__ == "__main__":
    main()
