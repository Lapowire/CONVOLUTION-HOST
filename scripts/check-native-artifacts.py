#!/usr/bin/env python3
import argparse, ctypes, plistlib, subprocess
from build_artifacts import collect

parser = argparse.ArgumentParser()
parser.add_argument("--platform", choices=["linux", "windows", "macos"], required=True)
parser.add_argument("--artifacts", required=True)
args = parser.parse_args()
platform = {"linux": "Linux", "windows": "Windows", "macos": "macOS"}[args.platform]
for kind, path, binary in collect(args.artifacts, platform):
    expected = {
        "VST3": ["GetPluginFactory"],
        "CLAP": ["clap_entry"],
        "AAX": ["ACFRegisterPlugin", "ACFGetClassFactory"],
    }.get(kind, [])
    if kind == "AU":
        info = plistlib.loads((path / "Contents/Info.plist").read_bytes())
        expected = [entry["factoryFunction"] for entry in info["AudioComponents"]]
    if expected:
        if platform == "Windows":
            library = ctypes.WinDLL(str(binary.resolve()))
            for symbol in expected:
                getattr(library, symbol)
        else:
            symbols = subprocess.check_output(
                ["nm", "-gU" if platform == "macOS" else "-D", str(binary)], text=True
            )
            for symbol in expected:
                if not any(
                    line.split()[-1]
                    == ("_" + symbol if platform == "macOS" else symbol)
                    for line in symbols.splitlines()
                    if line.split()
                ):
                    raise SystemExit(f"Missing {kind} entry point: {symbol}")
    print(f"PASS {platform} {kind}: {path}")
