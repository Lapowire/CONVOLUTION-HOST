#!/usr/bin/env python3
import argparse, json, pathlib, platform, re, subprocess, xml.etree.ElementTree as ET
from build_artifacts import fingerprints
from ci_diagnostics import error as annotate_error

parser = argparse.ArgumentParser()
parser.add_argument("--junit", required=True)
parser.add_argument("--output", required=True)
parser.add_argument(
    "--run",
    action="store_true",
    help="Execute CTest now and bind evidence to native files",
)
parser.add_argument("--build-dir")
parser.add_argument("--artifacts")
parser.add_argument("--provenance")
args = parser.parse_args()
commit = None
identities = {}
if args.run:
    if not all([args.build_dir, args.artifacts, args.provenance]):
        parser.error("--run requires --build-dir, --artifacts and --provenance")
    pathlib.Path(args.output).unlink(missing_ok=True)
    provenance = json.loads(pathlib.Path(args.provenance).read_text())
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    expected_identity = commit + ("-dirty" if provenance["dirty"] else "")
    if provenance["commit"] != commit:
        raise SystemExit("Build provenance is stale")
    identities = fingerprints(args.artifacts, provenance["platform"])
    source = pathlib.Path(args.junit).resolve()
    source.unlink(missing_ok=True)
    run = subprocess.run(
        [
            "ctest",
            "--test-dir",
            args.build_dir,
            "-C",
            "Release",
            "--output-on-failure",
            "--output-junit",
            str(source),
        ]
    )
    if run.returncode:
        if source.exists():
            for case in ET.parse(source).getroot().iter("testcase"):
                if list(case.iter("failure")) or list(case.iter("error")):
                    annotate_error(
                        "CTest " + case.attrib.get("name", "unknown"),
                        "".join(case.itertext()),
                    )
        raise SystemExit(run.returncode)
    if (
        identities != fingerprints(args.artifacts, provenance["platform"])
        or commit
        != subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    ):
        raise SystemExit("Build changed while tests were running")
root = ET.parse(args.junit).getroot()
cases = list(root.iter("testcase"))
failed = any(
    list(case.iter("failure")) or list(case.iter("error")) or list(case.iter("skipped"))
    for case in cases
)
if not cases:
    raise SystemExit("CTest report has no cases")
if args.run:
    required = {"core", "plugin", "wrapper_vst3", "wrapper_clap"}
    named = {case.attrib.get("name"): case for case in cases}
    if not required.issubset(named):
        raise SystemExit("Required native test groups are missing")
    for name in required:
        if "Build identity: " + expected_identity not in "".join(
            named[name].itertext()
        ):
            raise SystemExit("Compiled build identity is stale: " + name)
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
            "execution": args.run,
            "artifacts": identities,
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
