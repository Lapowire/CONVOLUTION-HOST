#!/usr/bin/env python3
"""Collect verified native plugins, preserving installable bundles and executable bits."""
import argparse, hashlib, json, os, pathlib, re, shutil, subprocess, sys, tarfile, tempfile, uuid, zipfile
from build_artifacts import collect, FORMATS, fingerprints

ROOT = pathlib.Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, data):
    path.write_text(
        json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )


def contained_links(source):
    if source.is_symlink():
        raise ValueError(f"Root artifact must not be a symlink: {source}")
    allowed = source.resolve() if source.is_dir() else source.parent.resolve()
    for path in [source] + (list(source.rglob("*")) if source.is_dir() else []):
        if path.is_symlink() and (
            not path.exists() or not path.resolve().is_relative_to(allowed)
        ):
            raise ValueError(f"External/broken symlink in artifact: {path}")


def stage(args):
    provenance = json.loads(pathlib.Path(args.provenance).read_text(encoding="utf-8"))
    results = json.loads(pathlib.Path(args.test_results).read_text(encoding="utf-8"))
    commit = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
    ).strip()
    if (
        provenance.get("schema") != 1
        or provenance.get("commit") != commit
        or provenance.get("dirty") is not False
    ):
        raise ValueError("Provenance is stale, dirty or invalid")
    if (
        results.get("execution") is not True
        or results.get("commit") != commit
        or results.get("status") != "passed"
        or not isinstance(results.get("tests"), int)
        or results["tests"] < 1
    ):
        raise ValueError("Test results are missing, stale or failed")
    platform = provenance["platform"]
    formats = provenance["formats"]
    if (
        platform not in FORMATS
        or set(formats) != set(FORMATS[platform])
        or len(formats) != len(set(formats))
    ):
        raise ValueError(
            "A final delivery requires every native format for its platform"
        )
    architecture = "universal" if platform == "macOS" else "x86_64"
    expected_arch = ["arm64", "x86_64"] if platform == "macOS" else ["x86_64"]
    if (
        set(provenance.get("architectures", [])) != set(expected_arch)
        or provenance.get("configuration") != "Release"
    ):
        raise ValueError("Unexpected architecture/configuration")
    artifacts = collect(args.artifacts, platform, formats)
    if results.get("artifacts") != fingerprints(args.artifacts, platform):
        raise ValueError("Artifacts differ from the binaries verified by CTest")
    subprocess.run(
        [
            sys.executable,
            str(ROOT / "scripts/check-native-artifacts.py"),
            "--platform",
            {"Linux": "linux", "Windows": "windows", "macOS": "macos"}[platform],
            "--artifacts",
            str(args.artifacts),
        ],
        check=True,
    )
    runtime = {}
    if platform == "Linux" and shutil.which("objdump"):
        symbols = ""
        for _, _, binary in artifacts:
            inspection = subprocess.run(
                ["objdump", "-T", str(binary)], capture_output=True, text=True
            )
            if inspection.returncode == 0:
                symbols += inspection.stdout
        for abi in ("GLIBC", "GLIBCXX"):
            versions = re.findall(r"\b" + abi + r"_([0-9.]+)", symbols)
            if versions:
                runtime[abi] = max(
                    versions, key=lambda value: tuple(map(int, value.split(".")))
                )
    for _, source, _ in artifacts:
        contained_links(source)
    licenses = provenance.get("licenses", {})
    if not licenses or any(
        not pathlib.Path(path).is_file() for path in licenses.values()
    ):
        raise ValueError("Missing dependency license files")
    output = pathlib.Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    relative = pathlib.Path(platform) / architecture
    destination = output / relative
    with tempfile.TemporaryDirectory(prefix=".staging-", dir=output) as tmp:
        temporary = pathlib.Path(tmp)
        delivery = temporary / "delivery"
        delivery.mkdir()
        entries = []
        for kind, source, binary in artifacts:
            target = delivery / kind / source.name
            target.parent.mkdir()
            if source.is_dir():
                shutil.copytree(source, target, symlinks=True)
            else:
                shutil.copy2(source, target)
            native = target / binary.relative_to(source) if source.is_dir() else target
            native.chmod(0o755)
            entries.append(
                {
                    "format": kind,
                    "path": target.relative_to(delivery).as_posix(),
                    "built": True,
                    "automated_tests": {"status": "passed"},
                    "host_validation": {
                        "status": "not_run",
                        "reason": "No DAW session or Pro Tools Developer validation performed",
                    },
                }
            )
        license_dir = delivery / "LICENSES"
        license_dir.mkdir()
        for name, path in licenses.items():
            if not name or pathlib.Path(name).name != name:
                raise ValueError("Invalid license name")
            shutil.copyfile(path, license_dir / (name + ".txt"))
        installation = {
            "Linux": "VST3: ~/.vst3/\nCLAP: ~/.clap/\nStandalone: executable in Standalone/. Requires a compatible Linux desktop with ALSA/X11 runtime libraries.",
            "Windows": "VST3: C:\\Program Files\\Common Files\\VST3\\\nCLAP: C:\\Program Files\\Common Files\\CLAP\\\nAAX: C:\\Program Files\\Common Files\\Avid\\Audio\\Plug-Ins\\\nStandalone: .exe in Standalone/. AAX is unsigned and requires the Avid/PACE release process for normal Pro Tools.",
            "macOS": "VST3: ~/Library/Audio/Plug-Ins/VST3/\nCLAP: ~/Library/Audio/Plug-Ins/CLAP/\nAU: ~/Library/Audio/Plug-Ins/Components/\nAAX: /Library/Application Support/Avid/Audio/Plug-Ins/\nStandalone: drag .app to Applications. Development binaries are not Developer ID signed/notarized; AAX is not Avid/PACE signed.",
        }[platform]
        if runtime:
            installation += "\nMinimum runtime ABI versions: " + ", ".join(
                f"{name}_{version}" for name, version in runtime.items()
            )
        (delivery / "INSTALL.txt").write_text(
            "Copy the WHOLE plugin bundle/file from its format directory into the matching DAW scan location.\nDo not copy only its inner binary and do not rename formats.\n\n"
            + installation
            + "\n\nSource: https://github.com/Lapowire/CONVOLUTION-HOST/tree/"
            + commit
            + "\n",
            encoding="utf-8",
        )
        files = [
            {
                "path": p.relative_to(delivery).as_posix(),
                "size": p.stat().st_size,
                "sha256": digest(p),
            }
            for p in sorted(delivery.rglob("*"))
            if p.is_file()
        ]
        manifest = {
            "schema": 1,
            "version": provenance["version"],
            "commit": commit,
            "platform": platform,
            "architectures": expected_arch,
            "tested_architectures": results.get("architectures", []),
            "configuration": "Release",
            "built": True,
            "runtime_requirements": runtime,
            "automated_tests": {"status": "passed", "tests": results["tests"]},
            "host_validation": {
                "status": "not_run",
                "reason": "Automated module tests are not DAW validation",
            },
            "signed": False,
            "notarized": False,
            "artifacts": entries,
            "files": files,
        }
        write_json(delivery / "manifest.json", manifest)
        checksums = "".join(
            f"{digest(p)}  {p.relative_to(delivery).as_posix()}\n"
            for p in sorted(delivery.rglob("*"))
            if p.is_file()
        )
        (delivery / "SHA256SUMS").write_text(checksums, encoding="utf-8")
        archive_name = (
            f'convolution-host-{provenance["version"]}-{platform}-{architecture}'
        )
        archive = temporary / (
            archive_name + (".zip" if platform == "Windows" else ".tar.gz")
        )
        archive_root = pathlib.Path("BUILD") / relative
        if platform == "Windows":
            with zipfile.ZipFile(
                archive, "w", compression=zipfile.ZIP_DEFLATED
            ) as target:
                for path in sorted(delivery.rglob("*")):
                    if path.is_file():
                        target.write(
                            path, (archive_root / path.relative_to(delivery)).as_posix()
                        )
        else:
            with tarfile.open(archive, "w:gz", dereference=False) as target:
                target.add(delivery, arcname=archive_root.as_posix())
        destination.parent.mkdir(parents=True, exist_ok=True)
        backup = destination.parent / (".previous-" + uuid.uuid4().hex)
        existed = destination.exists()
        if existed:
            os.replace(destination, backup)
        try:
            os.replace(delivery, destination)
        except BaseException:
            if existed:
                os.replace(backup, destination)
            raise
        if existed:
            shutil.rmtree(backup)
        archive_dir = output / "archives"
        archive_dir.mkdir(exist_ok=True)
        os.replace(archive, archive_dir / archive.name)
        print(f"Published {len(entries)} native artifacts to {destination}")
        print(f"Download: {archive_dir/archive.name}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--artifacts", required=True)
    parser.add_argument("--provenance", required=True)
    parser.add_argument("--test-results", required=True)
    parser.add_argument("--output", default="BUILD")
    arguments = parser.parse_args()
    try:
        stage(arguments)
    except (ValueError, KeyError, OSError, subprocess.SubprocessError) as error:
        print(f"Staging failed: {error}", file=sys.stderr)
        sys.exit(1)
