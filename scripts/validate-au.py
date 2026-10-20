#!/usr/bin/env python3
"""Validate the AU on its native macOS runner, restoring any pre-existing user copy."""
import argparse, pathlib, shutil, subprocess, tempfile
from ci_diagnostics import error as annotate_error

parser = argparse.ArgumentParser()
parser.add_argument("--component", required=True)
args = parser.parse_args()
source = pathlib.Path(args.component)
if not source.is_dir():
    raise SystemExit("Missing AU component")
parent = pathlib.Path.home() / "Library/Audio/Plug-Ins/Components"
parent.mkdir(parents=True, exist_ok=True)
target = parent / source.name
with tempfile.TemporaryDirectory(prefix="ch-auval-") as tmp:
    backup = pathlib.Path(tmp) / target.name
    existed = target.exists()
    if existed:
        shutil.move(target, backup)
    try:
        shutil.copytree(source, target, symlinks=True)
        result = subprocess.run(
            ["auval", "-v", "aufx", "Cvht", "Lpwr"], capture_output=True, text=True
        )
        print(result.stdout)
        print(result.stderr)
        if result.returncode:
            annotate_error("Audio Unit validation", result.stdout + result.stderr)
    finally:
        if target.exists():
            shutil.rmtree(target)
        if existed:
            shutil.move(backup, target)
    raise SystemExit(result.returncode)
