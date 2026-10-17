"""Exercise release decisions against real Git changes, without publishing."""

import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "scripts/firmware_release_needed.py"
SPEC = importlib.util.spec_from_file_location("release_scope", SCRIPT)
SCOPE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SCOPE)


class FirmwareInputs(unittest.TestCase):
    def test_production_inputs_require_a_release(self):
        for path in (
            "main/main.cpp", "main/hardware_contract.h", "main/idf_component.yml",
            "main/ui/notice_screen.cpp", "main/services/license_check.cpp",
            "main/ui/images/menu_icons.c", "cmake/guition_hosted_sdmmc.cmake",
            "patches/esp_hosted/manifest.json", "patches/esp_hosted/fix.patch",
            "CMakeLists.txt", "dependencies.lock", "sdkconfig.defaults",
            "sdkconfig.defaults.esp32p4.v3", "partitions.esp32p4.pre3.csv",
            "scripts/apply_hosted_sdmmc_patch.py",
        ):
            with self.subTest(path=path):
                self.assertTrue(SCOPE.is_firmware_input("software/firmware/" + path))

    def test_non_firmware_changes_do_not_require_a_release(self):
        for path in (
            "README.md", "hardware/cad/enclosure.3mf", "hardware/pcb/board.kicad_pcb",
            "hardware/system-review/electrical/main-final-independent/requirements.lock.txt",
            ".github/workflows/release.yml", "Makefile",
            "software/firmware/README.md", "software/firmware/main/README.txt",
            "software/firmware/main/third_party/bme280/LICENSE",
            "software/firmware/patches/esp_hosted/README.md",
            "software/firmware/docs/screenshots/settings.png",
            "software/firmware/design/ui-redesign/plan.md",
            "software/firmware/tests/test_version.cpp",
            "software/firmware/simulator/src/main.cpp",
            "software/firmware/web/src/main.cpp",
            "software/firmware/scripts/verify_release.py",
        ):
            with self.subTest(path=path):
                self.assertFalse(SCOPE.is_firmware_input(path))


class GitChanges(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.git("init", "--quiet")
        self.git("config", "user.name", "Release test")
        self.git("config", "user.email", "release-test@example.invalid")
        self.write("README.md")
        self.write("software/firmware/main/main.cpp")
        self.before = self.commit()

    def git(self, *args):
        return subprocess.check_output([
            "git", "-c", "core.hooksPath=/dev/null", "-c", "commit.gpgsign=false", *args,
        ], cwd=self.root, text=True).strip()

    def write(self, path, content="original\n"):
        target = self.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(content)

    def commit(self):
        self.git("add", "--all")
        self.git("commit", "--quiet", "-m", "test change")
        return self.git("rev-parse", "HEAD")

    def decision(self, after, before=None, event="push"):
        return subprocess.run([
            sys.executable, str(SCRIPT), "--event", event,
            "--before", before or self.before, "--after", after,
        ], cwd=self.root, text=True, capture_output=True)

    def assert_decision(self, after, expected):
        result = self.decision(after)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), f"release={str(expected).lower()}")

    def test_readme_and_hardware_only(self):
        self.write("README.md", "new documentation\n")
        self.write("hardware/pcb/board.kicad_pcb")
        self.write("hardware/cad/3d-print/enclosure.3mf")
        self.assert_decision(self.commit(), False)

    def test_mixed_push_includes_earlier_firmware_commit(self):
        self.write("software/firmware/main/main.cpp", "updated firmware\n")
        self.commit()
        self.write("README.md", "last commit is documentation\n")
        self.assert_decision(self.commit(), True)

    def test_deleted_firmware(self):
        (self.root / "software/firmware/main/main.cpp").unlink()
        self.assert_decision(self.commit(), True)

    def test_firmware_moved_outside_build(self):
        self.git("mv", "software/firmware/main/main.cpp", "old-main.cpp")
        self.assert_decision(self.commit(), True)

    def test_large_hardware_diff_does_not_hide_firmware(self):
        for index in range(3001):
            self.write(f"hardware/export/{index}.txt")
        self.write("software/firmware/main/new.cpp")
        self.assert_decision(self.commit(), True)

    def test_unchanged_tree(self):
        self.assert_decision(self.before, False)

    def test_missing_commit_fails_without_release_output(self):
        result = self.decision(self.before, before="0" * 40)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "")

    def test_manual_release_does_not_require_changes(self):
        result = self.decision("", event="workflow_dispatch")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), "release=true")


if __name__ == "__main__":
    unittest.main()
