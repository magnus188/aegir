#!/usr/bin/env python3
"""Run actual coordinator races/failures and exact managed-patch rejection tests."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("hosted_patch", ROOT / "scripts/apply_hosted_sdmmc_patch.py")
PATCHER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PATCHER)
MANIFEST = ROOT / "patches/esp_hosted/manifest.json"
SOURCE = ROOT / "managed_components/espressif__esp_hosted"
CASES = """both_orders absent_retry slot_failure_quarantine hosted_failure_card_live
no_slots_failure ignored_unmount_deinit_error ignored_failed_mount_deinit_error
unregister_error_consumes_card ldo_cleanup_failure early_allocation_failure
configuration_and_duplicate_guards transfer_vs_teardown hosted_transfer_vs_mount
interrupt_wait_releases_guard cancel_waiter_under_teardown_guard""".split()


class CoordinatorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix="trimix-sdmmc-test-")
        cls.folder = Path(cls.tmp.name)
        PATCHER.prepare(SOURCE, cls.folder / "patched", MANIFEST)
        source = (cls.folder / "patched/host/port/esp/freertos/src/port_esp_hosted_host_sdio.c").read_text()
        wait = source[source.index("int hosted_sdio_wait_slave_intr(void *ctx, uint32_t ticks_to_wait)\n{"):]
        harness = '#include "board/guition_sdmmc.h"\n#include "freertos/task.h"\n'
        harness += 'extern int fake_wait_for_interrupt(void*,uint32_t);\n'
        harness += '#define hosted_sdio_wait_slave_intr_impl fake_wait_for_interrupt\n' + wait
        (cls.folder / "actual_interrupt_wait.c").write_text(harness)
        cls.binary = cls.folder / "test-coordinator"
        sanitize = os.environ.get("TRIMIX_SDMMC_SANITIZE", "")
        if sanitize not in ("", "address,undefined"):
            raise ValueError("TRIMIX_SDMMC_SANITIZE must be unset or exactly address,undefined")
        flags = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"] if sanitize else []
        subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-pthread", *flags,
                        "-I" + str(ROOT / "tests/sdmmc_stubs"), "-I" + str(ROOT / "main"),
                        str(ROOT / "tests/test_guition_sdmmc.c"),
                        str(ROOT / "main/board/guition_sdmmc.c"),
                        str(cls.folder / "actual_interrupt_wait.c"), "-o", str(cls.binary)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()


def scenario(name):
    def test(self):
        result = subprocess.run([str(self.binary), name], check=True, capture_output=True,
                                text=True, timeout=8)
        self.assertEqual(result.stdout.strip(), "PASS " + name)
    return test


for case in CASES:
    setattr(CoordinatorTests, "test_" + case, scenario(case))


class CheckedPatchTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="trimix-patch-test-")
        self.addCleanup(self.tmp.cleanup)
        self.folder = Path(self.tmp.name)
        self.source = self.folder / "managed"
        self.output = self.folder / "build"
        self.manifest = json.loads(MANIFEST.read_bytes())
        for relative in ["idf_component.yml"] + [r["path"] for r in self.manifest["files"]]:
            target = self.source / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(SOURCE / relative, target)

    def hashes(self, path):
        return {str(p.relative_to(path)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in path.rglob("*") if p.is_file()}

    def test_exact_reproducible_build_copy_preserves_managed(self):
        before = self.hashes(self.source)
        one = PATCHER.prepare(self.source, self.output, MANIFEST)
        two = PATCHER.prepare(self.source, self.output, MANIFEST)
        self.assertEqual(one, two)
        self.assertEqual(before, self.hashes(self.source))
        for row in self.manifest["files"]:
            self.assertEqual(PATCHER.digest((self.output / row["path"]).read_bytes()), row["patched_sha256"])

    def test_reject_upstream_drift_before_any_output(self):
        target = self.source / self.manifest["files"][-1]["path"]
        target.write_bytes(target.read_bytes() + b"\n/* drift */\n")
        with self.assertRaisesRegex(ValueError, "managed source drift"):
            PATCHER.prepare(self.source, self.output, MANIFEST)
        self.assertFalse(self.output.exists())

    def test_reject_version_drift(self):
        (self.source / "idf_component.yml").write_text("version: 2.13.0\n")
        with self.assertRaisesRegex(ValueError, "version mismatch"):
            PATCHER.prepare(self.source, self.output, MANIFEST)
        self.assertFalse(self.output.exists())

    def test_reject_cache_only_patched_input(self):
        PATCHER.prepare(self.source, self.output, MANIFEST)
        row = self.manifest["files"][0]
        shutil.copyfile(self.output / row["path"], self.source / row["path"])
        with self.assertRaisesRegex(ValueError, "managed source drift"):
            PATCHER.prepare(self.source, self.folder / "second", MANIFEST)

    def test_reject_bad_patch_hash(self):
        patch = MANIFEST.parent / self.manifest["patch"]
        (self.folder / patch.name).write_bytes(patch.read_bytes() + b"\n")
        manifest = self.folder / "manifest.json"
        manifest.write_bytes(MANIFEST.read_bytes())
        with self.assertRaisesRegex(ValueError, "patch hash mismatch"):
            PATCHER.prepare(self.source, self.output, manifest)
        self.assertFalse(self.output.exists())

    def test_reject_wrong_postimage_hash(self):
        patch = MANIFEST.parent / self.manifest["patch"]
        shutil.copyfile(patch, self.folder / patch.name)
        self.manifest["files"][-1]["patched_sha256"] = "0" * 64
        manifest = self.folder / "manifest.json"
        manifest.write_text(json.dumps(self.manifest))
        with self.assertRaisesRegex(ValueError, "patched source hash mismatch"):
            PATCHER.prepare(self.source, self.output, manifest)
        self.assertFalse(self.output.exists())

    def test_reject_managed_output(self):
        with self.assertRaisesRegex(ValueError, "separate"):
            PATCHER.prepare(self.source, self.source / "patched", MANIFEST)

    def test_hunks_reject_changed_context_even_with_valid_hash(self):
        with self.assertRaisesRegex(ValueError, "context mismatch"):
            PATCHER.apply_exact("wrong\n", ["@@ -1 +1 @@\n", "-expected\n", "+changed\n"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
