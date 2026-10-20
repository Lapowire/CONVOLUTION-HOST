#!/usr/bin/env python3
"""Wait for native AU registration, validate it, and restore the prior user copy."""
import argparse
import getpass
import os
import pathlib
import plistlib
import re
import shutil
import subprocess
import tempfile
import time
from ci_diagnostics import error as annotate_error


def validate(
    component,
    parent=None,
    *,
    runner=subprocess.run,
    wait=time.sleep,
    attempts=30,
    refresh_registrar=False
):
    source = pathlib.Path(component)
    metadata = plistlib.loads((source / "Contents/Info.plist").read_bytes())
    descriptions = metadata["AudioComponents"]
    if len(descriptions) != 1:
        raise ValueError("Expected one AU component description")
    codes = [descriptions[0][key] for key in ("type", "subtype", "manufacturer")]
    if any(
        not isinstance(code, str) or len(code) != 4 or not code.isascii()
        for code in codes
    ):
        raise ValueError("Invalid AU component codes")
    parent = (
        pathlib.Path(parent)
        if parent
        else pathlib.Path.home() / "Library/Audio/Plug-Ins/Components"
    )
    parent.mkdir(parents=True, exist_ok=True)
    target = parent / source.name
    pattern = re.compile(r"\b" + r"\s+".join(map(re.escape, codes)) + r"(?:\s|:|$)")
    print("AU component from Info.plist: " + " / ".join(codes), flush=True)
    with tempfile.TemporaryDirectory(prefix="ch-auval-") as tmp:
        backup = pathlib.Path(tmp) / target.name
        existed = target.exists() or target.is_symlink()
        if existed:
            shutil.move(target, backup)
        try:
            shutil.copytree(source, target, symlinks=True)
            if refresh_registrar:
                # GitHub's disposable native runner can cache the initial AU list.
                # Restart only this runner user's registrar; launchd recreates it.
                runner(
                    ["killall", "-u", getpass.getuser(), "AudioComponentRegistrar"],
                    capture_output=True,
                    text=True,
                )
            for attempt in range(attempts):
                registered = runner(["auval", "-a"], capture_output=True, text=True)
                if registered.returncode == 0 and pattern.search(registered.stdout):
                    break
                if attempt + 1 < attempts:
                    wait(1)
            else:
                annotate_error(
                    "Audio Unit registration",
                    "Component did not appear in auval -a after registration: "
                    + " / ".join(codes),
                )
                return 2
            result = runner(["auval", "-v", *codes], capture_output=True, text=True)
            print(result.stdout)
            print(result.stderr)
            if result.returncode:
                annotate_error("Audio Unit validation", result.stdout + result.stderr)
            return result.returncode
        finally:
            if target.exists():
                shutil.rmtree(target)
            if existed:
                shutil.move(backup, target)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--component", required=True)
    args = parser.parse_args()
    raise SystemExit(
        validate(
            args.component, refresh_registrar=os.environ.get("GITHUB_ACTIONS") == "true"
        )
    )
