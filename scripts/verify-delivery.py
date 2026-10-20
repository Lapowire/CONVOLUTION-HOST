#!/usr/bin/env python3
"""Extract a delivery and check checksums, native architectures and executable modes.

Uses Python 3.12+ safe tar extraction. Run on each native CI platform after staging.
"""
import argparse
import hashlib
import json
import pathlib
import stat
import tarfile
import tempfile
import zipfile
from build_artifacts import collect


def verify(archive):
    with tempfile.TemporaryDirectory(prefix="ch-delivery-") as tmp:
        root = pathlib.Path(tmp).resolve()
        if zipfile.is_zipfile(archive):
            with zipfile.ZipFile(archive) as source:
                for item in source.infolist():
                    if not (root / item.filename).resolve().is_relative_to(root):
                        raise ValueError("Archive path escapes extraction directory")
                source.extractall(root)
        else:
            with tarfile.open(archive) as source:
                source.extractall(root, filter="data")
        manifests = list(root.glob("BUILD/*/*/manifest.json"))
        if len(manifests) != 1:
            raise ValueError("Expected one native platform delivery")
        delivery = manifests[0].parent
        manifest = json.loads(manifests[0].read_text(encoding="utf-8"))
        expected = set()
        for line in (delivery / "SHA256SUMS").read_text(encoding="utf-8").splitlines():
            digest, name = line.split("  ", 1)
            path = delivery / name
            if not path.resolve().is_relative_to(delivery.resolve()):
                raise ValueError("Checksum path escapes delivery")
            if (
                name in expected
                or hashlib.sha256(path.read_bytes()).hexdigest() != digest
            ):
                raise ValueError("Checksum mismatch: " + name)
            expected.add(name)
        actual = {
            p.relative_to(delivery).as_posix()
            for p in delivery.rglob("*")
            if p.is_file() and p.name != "SHA256SUMS"
        }
        if expected != actual:
            raise ValueError("Checksum inventory differs from extracted files")
        platform = manifest["platform"]
        artifacts = collect(delivery, platform)
        if platform != "Windows":
            for _, _, binary in artifacts:
                if not binary.stat().st_mode & stat.S_IXUSR:
                    raise ValueError(
                        "Extracted native file is not executable: " + str(binary)
                    )
        print(
            f"Verified extracted {platform} delivery: {len(artifacts)} native formats, {len(expected)} checksums"
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", required=True)
    args = parser.parse_args()
    verify(args.archive)
