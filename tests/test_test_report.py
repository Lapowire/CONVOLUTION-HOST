import json, os, pathlib, platform, subprocess, sys, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class ReportTests(unittest.TestCase):
    def report(self, xml):
        with tempfile.TemporaryDirectory() as tmp:
            source = pathlib.Path(tmp) / "report.xml"
            dest = pathlib.Path(tmp) / "report.json"
            source.write_text(xml)
            run = subprocess.run(
                [
                    sys.executable,
                    str(ROOT / "scripts/test-report.py"),
                    "--junit",
                    str(source),
                    "--output",
                    str(dest),
                ],
                cwd=ROOT,
                capture_output=True,
                text=True,
            )
            return run, json.loads(dest.read_text()) if dest.exists() else None

    def test_counts_cpp_and_python_cases_without_losing_groups(self):
        run, data = self.report(
            '<testsuite><testcase name="core"><system-out>45/45 passed</system-out></testcase><testcase name="plugin"><system-out>19/19 passed</system-out></testcase><testcase name="python"><system-out>Ran 9 tests in 1s</system-out></testcase><testcase name="wrapper"/></testsuite>'
        )
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(data["tests"], 74)
        self.assertEqual(data["test_groups"], 4)

    def test_importing_old_xml_does_not_stamp_it_as_current_execution(self):
        run, data = self.report('<testsuite><testcase name="old-wrapper"/></testsuite>')
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertFalse(data["execution"])
        self.assertIsNone(data["commit"])

    def test_truncated_cpp_counts_cannot_be_claimed_as_passed(self):
        run, data = self.report(
            '<testsuite><testcase name="core"><system-out>PASS first...output removed</system-out></testcase></testsuite>'
        )
        self.assertNotEqual(run.returncode, 0)

    def test_failed_fresh_execution_invalidates_previous_success_report(self):
        artifacts = os.environ.get("CH_STAGE_TEST_ARTIFACTS")
        if not artifacts:
            self.skipTest("CTest supplies real native artifacts")
        with tempfile.TemporaryDirectory(prefix="ch-report-failure-") as tmp:
            root = pathlib.Path(tmp)
            (root / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.22)\nproject(Failure NONE)\nenable_testing()\nadd_test(NAME known_failure COMMAND "${CMAKE_COMMAND}" -E false)\n'
            )
            subprocess.run(
                ["cmake", "-S", tmp, "-B", str(root / "out")],
                check=True,
                capture_output=True,
            )
            evidence = root / "results.json"
            evidence.write_text('{"status":"passed"}')
            provenance = root / "provenance.json"
            provenance.write_text(
                json.dumps(
                    {
                        "commit": subprocess.check_output(
                            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
                        ).strip(),
                        "dirty": True,
                        "platform": {"Darwin": "macOS"}.get(
                            platform.system(), platform.system()
                        ),
                    }
                )
            )
            run = subprocess.run(
                [
                    sys.executable,
                    str(ROOT / "scripts/test-report.py"),
                    "--run",
                    "--build-dir",
                    str(root / "out"),
                    "--junit",
                    str(root / "tests.xml"),
                    "--output",
                    str(evidence),
                    "--artifacts",
                    artifacts,
                    "--provenance",
                    str(provenance),
                ],
                cwd=ROOT,
                capture_output=True,
                text=True,
            )
            self.assertNotEqual(run.returncode, 0)
            self.assertFalse(evidence.exists(), run.stdout + run.stderr)
            self.assertIn("::error title=CTest known_failure::", run.stdout)

    def test_failed_group_stops_publication(self):
        run, data = self.report(
            '<testsuite><testcase name="wrapper"><failure>bad audio</failure></testcase></testsuite>'
        )
        self.assertNotEqual(run.returncode, 0)
        self.assertEqual(data["status"], "failed")


if __name__ == "__main__":
    unittest.main()
