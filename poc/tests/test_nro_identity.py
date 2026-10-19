import struct
import tempfile
import unittest
from pathlib import Path

from tools.nro_identity import NROFormatError, inspect_nro


def make_nro(*, declared_size=None, magic=b"NRO0", segments=None):
    data = bytearray(0x180)
    data[0x10:0x14] = magic
    struct.pack_into("<I", data, 0x14, 0)  # version
    struct.pack_into("<I", data, 0x18, len(data) if declared_size is None else declared_size)
    struct.pack_into("<I", data, 0x1C, 0)  # flags
    if segments is not None:
        struct.pack_into("<7I", data, 0x20, *segments)
    data[0x40:0x54] = bytes(range(1, 21))
    return bytes(data)


class NROIdentityTests(unittest.TestCase):
    def inspect_bytes(self, data):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "fixture.nro"
            path.write_bytes(data)
            return inspect_nro(path)

    def test_valid_header_reports_identity(self):
        result = self.inspect_bytes(make_nro())
        self.assertTrue(result["valid_nro0_header"])
        self.assertEqual(result["file_size"], 0x180)
        self.assertEqual(result["build_id"], bytes(range(1, 21)).hex().upper())
        self.assertIsNone(result["known_reference_target"])

    def test_rejects_wrong_magic(self):
        with self.assertRaisesRegex(NROFormatError, "magic"):
            self.inspect_bytes(make_nro(magic=b"NOPE"))

    def test_rejects_declared_size_mismatch(self):
        with self.assertRaisesRegex(NROFormatError, "does not match"):
            self.inspect_bytes(make_nro(declared_size=0x200))

    def test_rejects_segment_past_file_end(self):
        # text starts at 0x100 and extends beyond the 0x180-byte fixture
        segments = (0x100, 0x100, 0, 0, 0, 0, 0)
        with self.assertRaisesRegex(NROFormatError, "text segment end"):
            self.inspect_bytes(make_nro(segments=segments))


if __name__ == "__main__":
    unittest.main()
