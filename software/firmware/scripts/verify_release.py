#!/usr/bin/env python3
"""Validate locally built OTA applications and optional GitHub draft metadata.

No network, publishing, image modification, partition modification or flashing.
ESP image/partition layouts follow the pinned ESP-IDF esp_app_format.h and
esp_partition.h. The device installer remains responsible for full image checks.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


def canonical_version(value):
    if not re.fullmatch(r"(0|[1-9][0-9]{0,4})\.(0|[1-9][0-9]{0,4})\.(0|[1-9][0-9]{0,4})", value):
        raise ValueError("Version must be canonical MAJOR.MINOR.PATCH")
    if any(int(part) > 65535 for part in value.split(".")):
        raise ValueError("Version component exceeds device OTA limit 65535")
    return value


def c_string(raw):
    if b"\0" not in raw:
        raise ValueError("Unterminated application descriptor")
    return raw.split(b"\0", 1)[0].decode("ascii")


def slot_capacity(table):
    slots = {}
    for pos in range(0, len(table) - 31, 32):
        magic, kind, subtype, offset, size = struct.unpack_from("<HBBII", table, pos)
        if magic != 0x50AA:
            break
        if kind == 0 and subtype in (0x10, 0x11):
            if subtype in slots or not size:
                raise ValueError("Invalid OTA partition table")
            slots[subtype] = size
    if len(slots) != 2:
        raise ValueError("Both installed OTA slots must exist")
    return min(slots.values())


def application_info(data, family, version, capacity):
    canonical_version(version)
    if len(data) < 288 or len(data) > capacity or data[0] != 0xE9:
        raise ValueError("Not an application image fitting both OTA slots")
    chip = struct.unpack_from("<H", data, 12)[0]
    minimum, maximum = struct.unpack_from("<HH", data, 15)
    if chip != 18:
        raise ValueError("Release application is not ESP32-P4")
    if family == "pre3":
        compatible = minimum < 300 and minimum <= maximum < 300
    elif family == "v3":
        compatible = 300 <= minimum <= maximum
    else:
        compatible = False
    if not compatible:
        raise ValueError("Silicon family does not match asset name")
    if struct.unpack_from("<I", data, 28)[0] < 256 or struct.unpack_from("<I", data, 32)[0] != 0xABCD5432:
        raise ValueError("Missing application descriptor; factory/C6 images are not OTA applications")
    if c_string(data[48:80]) != version or c_string(data[80:112]) != "Trimix_analyzer":
        raise ValueError("Application descriptor project/version does not match release")
    return {"name": f"Trimix_analyzer_esp32p4_{family}_v{version}.bin",
            "size": len(data), "digest": "sha256:" + hashlib.sha256(data).hexdigest(),
            "project": "Trimix_analyzer", "version": version,
            "minimum_chip_revision": minimum, "maximum_chip_revision": maximum,
            "ota_slot_capacity": capacity}


def verify_metadata(raw, manifest):
    if len(raw) > 32768:
        raise ValueError("Release metadata exceeds device 32 KiB limit")
    release = json.loads(raw)
    if release.get("tag_name") != manifest["tag"] or release.get("prerelease") is not False:
        raise ValueError("Release tag/prerelease differs from installable release")
    assets = release.get("assets")
    if not isinstance(assets, list):
        raise ValueError("Missing GitHub assets")
    for expected in manifest["applications"]:
        matched = [asset for asset in assets if asset.get("name") == expected["name"]]
        if len(matched) != 1:
            raise ValueError("Missing or duplicate exact OTA application asset")
        asset = matched[0]
        url = f'https://github.com/{manifest["repository"]}/releases/download/{manifest["tag"]}/{expected["name"]}'
        if (asset.get("state") != "uploaded" or asset.get("size") != expected["size"] or
                asset.get("digest") != expected["digest"] or asset.get("browser_download_url") != url):
            raise ValueError("GitHub asset identity, size, URL or server digest differs from tested bytes")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True)
    parser.add_argument("--repository", default="magnus188/aegir")
    parser.add_argument("--build-root", type=Path, default=Path("build"))
    parser.add_argument("--asset-dir", type=Path)
    parser.add_argument("--release-json", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    version = canonical_version(args.version)
    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", args.repository):
        raise ValueError("Invalid GitHub repository")
    manifest = {"schema": 1, "repository": args.repository, "tag": "v" + version, "applications": []}
    for family in ("pre3", "v3"):
        build = args.build_root / ("esp32p4-" + family)
        if "CONFIG_TRIMIX_UPDATE_C6_ON_BOOT=y" in (build / "sdkconfig").read_text():
            raise ValueError("C6 recovery build must never be published as ordinary OTA")
        data = (build / "Trimix_analyzer.bin").read_bytes()
        capacity = slot_capacity((build / "partition_table/partition-table.bin").read_bytes())
        app = application_info(data, family, version, capacity)
        if args.asset_dir and (args.asset_dir / app["name"]).read_bytes() != data:
            raise ValueError("Named OTA asset differs from verified build")
        manifest["applications"].append(app)
    if args.release_json:
        verify_metadata(args.release_json.read_bytes(), manifest)
    if args.output:
        args.output.write_text(json.dumps(manifest, indent=2) + "\n")
    print("PASS release applications, silicon identity, descriptor versions and installed slot sizes" +
          ("; GitHub asset digests verified" if args.release_json else ""))


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, UnicodeError) as error:
        raise SystemExit("Release validation failed: " + str(error)) from error
