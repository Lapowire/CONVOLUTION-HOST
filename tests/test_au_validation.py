import importlib.util
import pathlib
import plistlib
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))


class AuValidationTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="ch-au-registration-")
        self.addCleanup(self.tmp.cleanup)
        root = pathlib.Path(self.tmp.name)
        self.source = root / "build/Test.component"
        (self.source / "Contents").mkdir(parents=True)
        (self.source / "Contents/Info.plist").write_bytes(
            plistlib.dumps(
                {
                    "AudioComponents": [
                        {"type": "aufx", "subtype": "Test", "manufacturer": "Unit"}
                    ]
                }
            )
        )
        self.parent = root / "user-components"
        self.target = self.parent / self.source.name
        self.target.mkdir(parents=True)
        (self.target / "previous.txt").write_text("preserve this plugin")

    def validator(self):
        spec = importlib.util.spec_from_file_location(
            "au_validation", ROOT / "scripts/validate-au.py"
        )
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module.validate

    def test_registration_delay_and_plist_codes_are_respected(self):
        calls, waits = [], []

        def run(command, **kwargs):
            calls.append(command)
            if command[0] == "killall":
                self.assertEqual(command[1], "-u")
                self.assertEqual(command[-1], "AudioComponentRegistrar")
                return subprocess.CompletedProcess(command, 0, "", "")
            if command[1] == "-a":
                ready = sum(call[1] == "-a" for call in calls) > 1
                return subprocess.CompletedProcess(
                    command, 0, "aufx Test Unit: Registered" if ready else "", ""
                )
            self.assertEqual(command, ["auval", "-v", "aufx", "Test", "Unit"])
            self.assertTrue((self.target / "Contents/Info.plist").exists())
            return subprocess.CompletedProcess(
                command, 0, "AU VALIDATION SUCCEEDED", ""
            )

        self.assertEqual(
            self.validator()(
                self.source,
                self.parent,
                runner=run,
                wait=waits.append,
                attempts=3,
                refresh_registrar=True,
            ),
            0,
        )
        self.assertEqual(len(waits), 1)
        self.assertEqual(
            (self.target / "previous.txt").read_text(), "preserve this plugin"
        )

    def test_unregistered_component_is_reported_and_previous_copy_restored(self):
        calls = []

        def run(command, **kwargs):
            calls.append(command)
            return subprocess.CompletedProcess(command, 0, "", "")

        result = self.validator()(
            self.source, self.parent, runner=run, wait=lambda _: None, attempts=2
        )
        self.assertNotEqual(result, 0)
        self.assertTrue(all(command[1] == "-a" for command in calls))
        self.assertTrue((self.target / "previous.txt").is_file())

    def test_audio_validation_failure_is_not_hidden_and_restores_old_copy(self):
        def run(command, **kwargs):
            return subprocess.CompletedProcess(
                command,
                0 if command[1] == "-a" else 6,
                "aufx Test Unit: Registered",
                "",
            )

        self.assertEqual(
            self.validator()(self.source, self.parent, runner=run, wait=lambda _: None),
            6,
        )
        self.assertTrue((self.target / "previous.txt").is_file())


if __name__ == "__main__":
    unittest.main()
