import hashlib, json, os, pathlib, shutil, stat, subprocess, sys, tarfile, tempfile, unittest

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
        elf = bytearray(64)
        elf[:6] = b"\x7fELF\x02\x01"
        elf[18:20] = (62).to_bytes(2, "little")
        for relative in [
            "VST3/CONVOLUTION HOST.vst3/Contents/x86_64-linux/CONVOLUTION HOST.so",
            "CLAP/CONVOLUTION HOST.clap",
            "Standalone/CONVOLUTION HOST",
        ]:
            f = self.artifacts / relative
            f.parent.mkdir(parents=True, exist_ok=True)
            f.write_bytes(elf)
            f.chmod(0o700)
        self.license = self.root / "LICENSE"
        self.license.write_text("Test license text")
        self.provenance = self.root / "provenance.json"
        self.data = {
            "schema": 1,
            "version": "0.2.0",
            "commit": self.commit,
            "dirty": False,
            "platform": "Linux",
            "architectures": ["x86_64"],
            "configuration": "Release",
            "formats": ["VST3", "CLAP", "Standalone"],
            "licenses": {"CONVOLUTION-HOST": str(self.license)},
        }
        self.provenance.write_text(json.dumps(self.data))
        self.results = self.root / "tests.json"
        self.results.write_text(
            json.dumps({"commit": self.commit, "status": "passed", "tests": 62})
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
        dest = self.output / "Linux/x86_64"
        manifest = json.loads((dest / "manifest.json").read_text())
        self.assertEqual(manifest["commit"], self.commit)
        self.assertFalse(manifest["signed"])
        self.assertEqual(manifest["host_validation"]["status"], "not_run")
        self.assertTrue(
            (
                dest
                / "VST3/CONVOLUTION HOST.vst3/Contents/x86_64-linux/CONVOLUTION HOST.so"
            ).is_file()
        )
        self.assertTrue((dest / "CLAP/CONVOLUTION HOST.clap").is_file())
        self.assertTrue(
            (dest / "Standalone/CONVOLUTION HOST").stat().st_mode & stat.S_IXUSR
        )
        for line in (dest / "SHA256SUMS").read_text().splitlines():
            digest, name = line.split("  ", 1)
            self.assertEqual(
                hashlib.sha256((dest / name).read_bytes()).hexdigest(), digest
            )
        archive = next((self.output / "archives").glob("*.tar.gz"))
        extract = self.root / "extracted"
        extract.mkdir()
        with tarfile.open(archive) as tar:
            tar.extractall(extract, filter="data")
        self.assertTrue(
            (extract / "BUILD/Linux/x86_64/Standalone/CONVOLUTION HOST").stat().st_mode
            & stat.S_IXUSR
        )

    def test_missing_format_does_not_overwrite_previous_delivery(self):
        self.assertEqual(self.run_stage().returncode, 0)
        dest = self.output / "Linux/x86_64"
        before = (dest / "manifest.json").read_bytes()
        (self.artifacts / "CLAP/CONVOLUTION HOST.clap").unlink()
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
        self.assertFalse((self.output / "Linux/x86_64").exists())

    def test_external_symlink_and_incorrect_binary_are_rejected(self):
        bundle = self.artifacts / "VST3/CONVOLUTION HOST.vst3"
        (bundle / "outside").symlink_to(self.license)
        self.assertNotEqual(self.run_stage().returncode, 0)
        (bundle / "outside").unlink()
        (self.artifacts / "CLAP/CONVOLUTION HOST.clap").write_bytes(b"MZwrong-platform")
        self.assertNotEqual(self.run_stage().returncode, 0)

    def test_extra_files_are_not_published_and_other_platform_is_preserved(self):
        (self.artifacts / "secret.tmp").write_text("not an artefact")
        other = self.output / "Windows/x86_64/existing"
        other.parent.mkdir(parents=True)
        other.write_text("keep")
        self.assertEqual(self.run_stage().returncode, 0)
        self.assertEqual(other.read_text(), "keep")
        self.assertFalse((self.output / "Linux/x86_64/secret.tmp").exists())

    def test_root_bundle_cannot_link_outside_artifacts(self):
        bundle = self.artifacts / "VST3/CONVOLUTION HOST.vst3"
        outside = self.root / "outside-bundle"
        shutil.move(bundle, outside)
        bundle.symlink_to(outside, target_is_directory=True)
        self.assertNotEqual(self.run_stage().returncode, 0)


if __name__ == "__main__":
    unittest.main()
