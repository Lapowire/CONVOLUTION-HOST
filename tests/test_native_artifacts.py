import pathlib, struct, sys, tempfile, unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "scripts"))
from build_artifacts import check_binary


class BinaryTests(unittest.TestCase):
    def test_native_architectures_are_checked(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = pathlib.Path(tmp) / "binary"
            elf = bytearray(64)
            elf[:6] = b"\x7fELF\x02\x01"
            elf[18:20] = struct.pack("<H", 62)
            path.write_bytes(elf)
            check_binary(path, "Linux", ["x86_64"])
            with self.assertRaises(ValueError):
                check_binary(path, "Windows", ["x86_64"])
            pe = bytearray(128)
            pe[:2] = b"MZ"
            pe[60:64] = struct.pack("<I", 64)
            pe[64:70] = b"PE\0\0" + struct.pack("<H", 0x8664)
            path.write_bytes(pe)
            check_binary(path, "Windows", ["x86_64"])
            fat = (
                struct.pack(">II", 0xCAFEBABE, 2)
                + struct.pack(">IIIII", 0x01000007, 3, 4096, 0, 0)
                + struct.pack(">IIIII", 0x0100000C, 0, 4096, 0, 0)
            )
            path.write_bytes(fat)
            check_binary(path, "macOS", ["arm64", "x86_64"])
            path.write_bytes(struct.pack("<II", 0xFEEDFACF, 0x01000007) + bytes(24))
            with self.assertRaises(ValueError):
                check_binary(path, "macOS", ["arm64", "x86_64"])


if __name__ == "__main__":
    unittest.main()
