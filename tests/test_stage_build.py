import hashlib, json, os, pathlib, platform, shutil, stat, subprocess, sys, tarfile, tempfile, unittest, zipfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "scripts"))
from build_artifacts import fingerprints, collect, FORMATS

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/stage-build.py"


class StagingTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="ch-stage-")
        self.addCleanup(self.tmp.cleanup)
        self.root = pathlib.Path(self.tmp.name)
        self.artifacts = self.root / "artefacts"
        self.output = self.root / "BUILD"
        self.commit = subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip()
        self.platform = {"Darwin": "macOS"}.get(platform.system(), platform.system())
        self.architecture = "universal" if self.platform == "macOS" else "x86_64"
        self.destination = self.output / self.platform / self.architecture
        source = os.environ.get("CH_STAGE_TEST_ARTIFACTS")
        if not source or not pathlib.Path(source).is_dir():
            self.skipTest("Run via configured CTest with native artifacts")
        shutil.copytree(source, self.artifacts, symlinks=True)
        self.native = {
            kind: (bundle, binary)
            for kind, bundle, binary in collect(self.artifacts, self.platform)
        }
        self.license = self.root / "LICENSE"
        self.license.write_text("Test license text")
        self.provenance = self.root / "provenance.json"
        self.data = {
            "schema": 1,
            "version": "0.2.0",
            "commit": self.commit,
            "dirty": False,
            "platform": self.platform,
            "architectures": (
                ["arm64", "x86_64"] if self.platform == "macOS" else ["x86_64"]
            ),
            "configuration": "Release",
            "formats": FORMATS[self.platform],
            "licenses": {"CONVOLUTION-HOST": str(self.license)},
        }
        self.provenance.write_text(json.dumps(self.data))
        self.results = self.root / "tests.json"
        self.results.write_text(
            json.dumps(
                {
                    "commit": self.commit,
                    "status": "passed",
                    "tests": 62,
                    "execution": True,
                    "artifacts": fingerprints(self.artifacts, self.platform),
                }
            )
        )

    def run_stage(self):
        return subprocess.run(
            [
                sys.executable,
                str(SCRIPT),
                "--artifacts",
                str(self.artifacts),
                "--provenance",
                str(self.provenance),
                "--test-results",
                str(self.results),
                "--output",
                str(self.output),
            ],
            capture_output=True,
            text=True,
            cwd=ROOT,
        )

    def test_valid_build_is_directly_installable_and_checksummed(self):
        run = self.run_stage()
        self.assertEqual(run.returncode, 0, run.stderr)
        dest = self.destination
        manifest = json.loads((dest / "manifest.json").read_text())
        self.assertEqual(manifest["commit"], self.commit)
        self.assertFalse(manifest["signed"])
        self.assertEqual(manifest["host_validation"]["status"], "not_run")
        self.assertTrue((dest / "VST3/CONVOLUTION HOST.vst3").is_dir())
        standalone = dest / self.native["Standalone"][1].relative_to(self.artifacts)
        if self.platform != "Windows":
            self.assertTrue(standalone.stat().st_mode & stat.S_IXUSR)
        for line in (dest / "SHA256SUMS").read_text().splitlines():
            digest, name = line.split("  ", 1)
            self.assertEqual(
                hashlib.sha256((dest / name).read_bytes()).hexdigest(), digest
            )
        archive = next((self.output / "archives").iterdir())
        self.assertGreater(archive.stat().st_size, 1000)
        checked = subprocess.run(
            [
                sys.executable,
                str(ROOT / "scripts/verify-delivery.py"),
                "--archive",
                str(archive),
            ],
            capture_output=True,
            text=True,
        )
        self.assertEqual(checked.returncode, 0, checked.stdout + checked.stderr)
        extracted = self.root / "archive-check"
        extracted.mkdir()
        if zipfile.is_zipfile(archive):
            with zipfile.ZipFile(archive) as source:
                source.extractall(extracted)
        else:
            with tarfile.open(archive) as source:
                source.extractall(extracted, filter="data")
        license_file = next(extracted.glob("BUILD/*/*/LICENSES/CONVOLUTION-HOST.txt"))
        license_file.write_text("damaged during transfer")
        damaged = self.root / "damaged.tar.gz"
        with tarfile.open(damaged, "w:gz") as target:
            target.add(extracted / "BUILD", arcname="BUILD")
        checked = subprocess.run(
            [
                sys.executable,
                str(ROOT / "scripts/verify-delivery.py"),
                "--archive",
                str(damaged),
            ],
            capture_output=True,
            text=True,
        )
        self.assertNotEqual(checked.returncode, 0)
        self.assertIn("Checksum mismatch", checked.stderr)

    def test_missing_format_does_not_overwrite_previous_delivery(self):
        self.assertEqual(self.run_stage().returncode, 0)
        dest = self.destination
        before = (dest / "manifest.json").read_bytes()
        (
            shutil.rmtree(self.native["CLAP"][0])
            if self.native["CLAP"][0].is_dir()
            else self.native["CLAP"][0].unlink()
        )
        self.assertNotEqual(self.run_stage().returncode, 0)
        self.assertEqual((dest / "manifest.json").read_bytes(), before)

    def test_stale_commit_failed_tests_and_missing_provenance_are_rejected(self):
        self.data["commit"] = "0" * 40
        self.provenance.write_text(json.dumps(self.data))
        self.assertNotEqual(self.run_stage().returncode, 0)
        self.data["commit"] = self.commit
        self.provenance.write_text(json.dumps(self.data))
        self.results.write_text(
            json.dumps({"commit": self.commit, "status": "failed", "tests": 62})
        )
        self.assertNotEqual(self.run_stage().returncode, 0)
        self.provenance.unlink()
        self.assertNotEqual(self.run_stage().returncode, 0)
        self.assertFalse((self.destination).exists())

    def test_external_symlink_and_incorrect_binary_are_rejected(self):
        bundle = self.native["VST3"][0]
        (bundle / "outside").symlink_to(self.license)
        self.assertNotEqual(self.run_stage().returncode, 0)
        (bundle / "outside").unlink()
        self.native["CLAP"][1].write_bytes(b"MZwrong-platform")
        self.assertNotEqual(self.run_stage().returncode, 0)

    def test_extra_files_are_not_published_and_other_platform_is_preserved(self):
        (self.artifacts / "secret.tmp").write_text("not an artefact")
        other = self.output / "OtherPlatform/existing"
        other.parent.mkdir(parents=True)
        other.write_text("keep")
        self.assertEqual(self.run_stage().returncode, 0)
        self.assertEqual(other.read_text(), "keep")
        self.assertFalse((self.destination / "secret.tmp").exists())

    def test_replaced_native_modules_cannot_reuse_genuine_test_report(self):
        self.assertEqual(self.run_stage().returncode, 0)
        module = self.native["CLAP"][1]
        module.write_bytes(module.read_bytes() + b"changed after tests")
        self.assertNotEqual(self.run_stage().returncode, 0)

    def test_native_executable_is_not_a_plugin_even_with_matching_hashes(self):
        shutil.copyfile(self.native["Standalone"][1], self.native["CLAP"][1])
        data = json.loads(self.results.read_text())
        data["artifacts"] = fingerprints(self.artifacts, self.platform)
        self.results.write_text(json.dumps(data))
        self.assertNotEqual(self.run_stage().returncode, 0)

    def test_imported_junit_is_not_execution_evidence(self):
        data = json.loads(self.results.read_text())
        data.pop("execution", None)
        self.results.write_text(json.dumps(data))
        self.assertNotEqual(self.run_stage().returncode, 0)

    def test_root_bundle_cannot_link_outside_artifacts(self):
        bundle = self.native["VST3"][0]
        outside = self.root / "outside-bundle"
        shutil.move(bundle, outside)
        bundle.symlink_to(outside, target_is_directory=True)
        self.assertNotEqual(self.run_stage().returncode, 0)


if __name__ == "__main__":
    unittest.main()
