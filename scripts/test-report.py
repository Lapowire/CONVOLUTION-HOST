#!/usr/bin/env python3
import argparse, json, pathlib, platform, re, subprocess, xml.etree.ElementTree as ET

parser = argparse.ArgumentParser()
parser.add_argument("--junit", required=True)
parser.add_argument("--output", required=True)
args = parser.parse_args()
root = ET.parse(args.junit).getroot()
cases = list(root.iter("testcase"))
failed = any(
    list(case.iter("failure")) or list(case.iter("error")) or list(case.iter("skipped"))
    for case in cases
)
if not cases:
    raise SystemExit("CTest report has no cases")
commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
arch = {"AMD64": "x86_64", "aarch64": "arm64"}.get(
    platform.machine(), platform.machine()
)
total = 0
for case in cases:
    log = "".join(case.itertext())
    matches = re.findall(r"(\d+)/(\d+) passed", log)
    if case.attrib.get("name") in ("core", "plugin") and not matches:
        raise SystemExit(
            "Detailed C++ test counts are missing or truncated; publication blocked"
        )
    python_count = re.search(r"Ran (\d+) tests?", log)
    total += (
        int(matches[-1][1]) if matches else int(python_count[1]) if python_count else 1
    )
pathlib.Path(args.output).write_text(
    json.dumps(
        {
            "commit": commit,
            "status": "failed" if failed else "passed",
            "tests": total,
            "test_groups": len(cases),
            "architectures": [arch],
            "cases": [c.attrib["name"] for c in cases],
        },
        indent=2,
    )
    + "\n",
    encoding="utf-8",
)
if failed:
    raise SystemExit("Tests failed or were skipped; delivery is blocked")
