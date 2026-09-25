#!/usr/bin/env python3
"""Offline fixtures for the validator used by release publication."""
import copy
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("verify_release", ROOT / "scripts/verify_release.py")
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


def image(family="pre3", version="1.2.3"):
    data = bytearray(512)
    data[0] = 0xE9
    struct.pack_into("<H", data, 12, 18)
    struct.pack_into("<HH", data, 15, *( (1, 199) if family == "pre3" else (300, 399)))
    struct.pack_into("<I", data, 28, 256)
    struct.pack_into("<I", data, 32, 0xABCD5432)
    data[48:48 + len(version)] = version.encode()
    data[80:95] = b"Trimix_analyzer"
    return data


class ReleaseValidation(unittest.TestCase):
    def setUp(self):
        self.manifest = {"repository": "magnus188/ziphius", "tag": "v1.2.3", "applications": [
            release.application_info(image(f), f, "1.2.3", 1024) for f in ("pre3", "v3")]}
        self.metadata = {"tag_name": "v1.2.3", "draft": True, "prerelease": False, "assets": []}
        for app in self.manifest["applications"]:
            self.metadata["assets"].append({**app, "state": "uploaded", "browser_download_url":
                f'https://github.com/{self.manifest["repository"]}/releases/download/v1.2.3/{app["name"]}'})

    def test_valid_family_images(self):
        self.assertEqual([a["minimum_chip_revision"] for a in self.manifest["applications"]], [1, 300])

    def test_wrong_silicon_or_c6_rejected(self):
        with self.assertRaises(ValueError): release.application_info(image("v3"), "pre3", "1.2.3", 1024)
        data = image(); struct.pack_into("<H", data, 12, 13)
        with self.assertRaises(ValueError): release.application_info(data, "pre3", "1.2.3", 1024)

    def test_wrong_descriptor_rejected(self):
        with self.assertRaises(ValueError): release.application_info(image(version="1.2.2"), "pre3", "1.2.3", 1024)
        data = image(); data[80] = ord("X")
        with self.assertRaises(ValueError): release.application_info(data, "pre3", "1.2.3", 1024)

    def test_factory_or_short_image_rejected(self):
        for data in (image()[:250], b"\xff" * 8192 + image()):
            with self.assertRaises(ValueError): release.application_info(data, "pre3", "1.2.3", 20000)

    def test_unterminated_descriptor_rejected(self):
        data = image(); data[48:80] = b"x" * 32
        with self.assertRaises(ValueError): release.application_info(data, "pre3", "1.2.3", 1024)

    def test_oversize_rejected(self):
        with self.assertRaises(ValueError): release.application_info(image(), "pre3", "1.2.3", 511)

    def test_smallest_real_ota_slot_controls_limit(self):
        table = b"".join(struct.pack("<HBBII16sI", 0x50AA, 0, subtype, 0x20000, size, b"app", 0)
                         for subtype, size in ((0, 0x600000), (0x10, 0x400000), (0x11, 0x300000)))
        self.assertEqual(release.slot_capacity(table), 0x300000)
        with self.assertRaises(ValueError): release.slot_capacity(table[:64])

    def test_versions_are_canonical_and_bounded(self):
        self.assertEqual(release.canonical_version("65535.0.1"), "65535.0.1")
        for value in ("01.2.3", "65536.0.0", "1.2", "v1.2.3", "1.2.3-beta", "1.2.3\n"):
            with self.subTest(value=value), self.assertRaises(ValueError): release.canonical_version(value)

    def test_version_script_rejects_without_touching_header(self):
        header = ROOT / "main/version.h"; before = header.read_bytes()
        for value in ("01.2.3", "65536.0.0", "1.2.3-beta"):
            result = subprocess.run(["bash", str(ROOT / "scripts/set_version.sh"), value], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
        self.assertEqual(header.read_bytes(), before)

    def test_complete_draft_metadata_matches(self):
        release.verify_metadata(json.dumps(self.metadata).encode(), self.manifest)

    def test_missing_or_duplicate_asset_rejected(self):
        for assets in ([], self.metadata["assets"] * 2):
            fixture = {**self.metadata, "assets": assets}
            with self.assertRaises(ValueError): release.verify_metadata(json.dumps(fixture).encode(), self.manifest)

    def test_bad_digest_size_url_or_upload_state_rejected(self):
        for key, value in (("digest", None), ("digest", "sha256:" + "0" * 64), ("size", 511),
                           ("browser_download_url", "https://github.com/other/download.bin"), ("state", "new")):
            fixture = copy.deepcopy(self.metadata); fixture["assets"][0][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError): release.verify_metadata(json.dumps(fixture).encode(), self.manifest)

    def test_prerelease_wrong_tag_and_large_metadata_rejected(self):
        for fixture in ({**self.metadata, "prerelease": True}, {**self.metadata, "tag_name": "v1.2.2"},
                        {**self.metadata, "body": "x" * 32768}):
            with self.assertRaises(ValueError): release.verify_metadata(json.dumps(fixture).encode(), self.manifest)


if __name__ == "__main__":
    unittest.main(verbosity=2)
