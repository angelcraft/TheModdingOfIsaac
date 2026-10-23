"""Offline unit checks. No proprietary NRO data or game process required."""
import unittest

from nro_plt import decode_direct_branch
from catalog_nro_calls import indirect_branch_register


class CallCatalogTests(unittest.TestCase):
    def test_direct_bl_forward(self):
        call = decode_direct_branch(0x94000002, 0x1000)
        self.assertEqual(call.kind, "BL")
        self.assertEqual(call.target, 0x1008)

    def test_direct_bl_backward(self):
        call = decode_direct_branch(0x97FFFFFF, 0x1000)
        self.assertEqual(call.kind, "BL")
        self.assertEqual(call.target, 0xFFC)

    def test_direct_b_not_call(self):
        self.assertEqual(decode_direct_branch(0x14000001, 0x1000).kind, "B")

    def test_blr_only(self):
        self.assertEqual(indirect_branch_register(0xD63F0060), 3)
        self.assertIsNone(indirect_branch_register(0xD61F0060))
        self.assertIsNone(indirect_branch_register(0xD65F03C0))


if __name__ == "__main__":
    unittest.main()
