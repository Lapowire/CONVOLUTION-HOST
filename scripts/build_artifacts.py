"""Native artifact discovery and architecture checks shared by validation/staging."""

from pathlib import Path
import struct

PRODUCT = "CONVOLUTION HOST"
FORMATS = {
    "Linux": ["VST3", "CLAP", "Standalone"],
    "Windows": ["VST3", "CLAP", "AAX", "Standalone"],
    "macOS": ["VST3", "CLAP", "AU", "AAX", "Standalone"],
}


def check_binary(path, platform, architectures):
    data = Path(path).read_bytes()
    if platform == "Linux":
        if (
            len(data) < 20
            or data[:6] != b"\x7fELF\x02\x01"
            or struct.unpack_from("<H", data, 18)[0] != 62
            or architectures != ["x86_64"]
        ):
            raise ValueError(f"Expected ELF x86_64: {path}")
    elif platform == "Windows":
        if len(data) < 64 or data[:2] != b"MZ":
            raise ValueError(f"Expected Windows PE: {path}")
        offset = struct.unpack_from("<I", data, 60)[0]
        if (
            offset + 6 > len(data)
            or data[offset : offset + 4] != b"PE\0\0"
            or struct.unpack_from("<H", data, offset + 4)[0] != 0x8664
            or architectures != ["x86_64"]
        ):
            raise ValueError(f"Expected PE x64: {path}")
    elif platform == "macOS":
        if len(data) < 8:
            raise ValueError(f"Truncated Mach-O: {path}")
        magic = struct.unpack_from(">I", data)[0]
        cpus = []
        if magic in (0xCAFEBABE, 0xCAFEBABF):
            count = struct.unpack_from(">I", data, 4)[0]
            stride = 32 if magic == 0xCAFEBABF else 20
            if count > 32 or len(data) < 8 + count * stride:
                raise ValueError(f"Invalid fat Mach-O: {path}")
            cpus = [
                struct.unpack_from(">I", data, 8 + i * stride)[0] for i in range(count)
            ]
        elif magic == 0xCFFAEDFE:
            cpus = [struct.unpack_from("<I", data, 4)[0]]
        elif magic == 0xFEEDFACF:
            cpus = [struct.unpack_from(">I", data, 4)[0]]
        wanted = {"x86_64": 0x01000007, "arm64": 0x0100000C}
        if any(
            arch not in wanted or wanted[arch] not in cpus for arch in architectures
        ):
            raise ValueError(f"Missing Mach-O architecture: {path}")
    else:
        raise ValueError(f"Unsupported platform: {platform}")


def collect(root, platform, formats=None):
    root = Path(root)
    formats = FORMATS[platform] if formats is None else formats
    result = []
    for kind in formats:
        if kind not in FORMATS[platform]:
            raise ValueError(f"{kind} is unsupported on {platform}")
        suffix = {
            "VST3": ".vst3",
            "CLAP": ".clap",
            "AU": ".component",
            "AAX": ".aaxplugin",
            "Standalone": (
                ".app"
                if platform == "macOS"
                else ".exe" if platform == "Windows" else ""
            ),
        }[kind]
        source = root / kind / (PRODUCT + suffix)
        if not source.exists():
            raise ValueError(f"Missing requested {kind} artifact: {source}")
        if platform == "macOS":
            binary = source / "Contents/MacOS" / PRODUCT
        elif kind == "VST3":
            binary = source / (
                "Contents/x86_64-linux/" + PRODUCT + ".so"
                if platform == "Linux"
                else "Contents/x86_64-win/" + PRODUCT + ".vst3"
            )
        elif kind == "AAX":
            binary = source / "Contents/x64" / (PRODUCT + ".aaxplugin")
        else:
            binary = source
        if not binary.is_file():
            raise ValueError(f"Missing native binary: {binary}")
        check_binary(
            binary, platform, ["arm64", "x86_64"] if platform == "macOS" else ["x86_64"]
        )
        if source.is_dir() and not (source / "Contents").is_dir():
            raise ValueError(f"Incomplete bundle: {source}")
        result.append((kind, source, binary))
    return result
