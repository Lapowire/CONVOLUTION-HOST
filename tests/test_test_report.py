import json, pathlib, subprocess, sys, tempfile, unittest

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

    def test_truncated_cpp_counts_cannot_be_claimed_as_passed(self):
        run, data = self.report(
            '<testsuite><testcase name="core"><system-out>PASS first...output removed</system-out></testcase></testsuite>'
        )
        self.assertNotEqual(run.returncode, 0)

    def test_failed_group_stops_publication(self):
        run, data = self.report(
            '<testsuite><testcase name="wrapper"><failure>bad audio</failure></testcase></testsuite>'
        )
        self.assertNotEqual(run.returncode, 0)
        self.assertEqual(data["status"], "failed")


if __name__ == "__main__":
    unittest.main()
