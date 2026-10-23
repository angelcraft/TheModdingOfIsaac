"""Offline unit checks. No proprietary NRO data or game process required."""
import unittest

from nro_plt import decode_direct_branch
from catalog_nro_calls import (
    build_function_index,
    indirect_branch_register,
    resolve_direct_target,
    resolve_function_symbol,
)


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

    def test_function_symbol_exact_start_and_containing_range(self):
        index = build_function_index([
            {"type": 2, "defined": True, "offset": "0x1000", "size": 0x40, "name": "outer"},
            {"type": 2, "defined": True, "offset": "0x1010", "size": 0x10, "name": "inner"},
        ])
        self.assertEqual(resolve_function_symbol(0x1010, index), "inner")
        self.assertEqual(resolve_function_symbol(0x1018, index), "inner")
        self.assertEqual(resolve_function_symbol(0x1030, index), "outer")
        self.assertEqual(resolve_function_symbol(0x2000, index), "")

    def test_function_symbol_aliases_are_preserved(self):
        index = build_function_index([
            {"type": 2, "defined": True, "offset": "0x2000", "size": 8, "name": "alias_b"},
            {"type": 2, "defined": True, "offset": "0x2000", "size": 8, "name": "alias_a"},
        ])
        self.assertEqual(resolve_function_symbol(0x2000, index), "alias_a|alias_b")

    def test_direct_target_uses_plt_import_name(self):
        index = build_function_index([])
        self.assertEqual(
            resolve_direct_target(0x670000, "IsaacUpdate", index),
            ("IsaacUpdate", "plt-import", ""),
        )

    def test_direct_target_reports_defined_symbol_or_unresolved(self):
        index = build_function_index([
            {"type": 2, "defined": True, "offset": "0x3000", "size": 16, "name": "room_update"},
        ])
        self.assertEqual(
            resolve_direct_target(0x3004, "", index),
            ("room_update", "defined-symbol-range", "0x3000"),
        )
        self.assertEqual(resolve_direct_target(0x3000, "", index), ("room_update", "defined-symbol-entry", "0x3000"))
        self.assertEqual(resolve_direct_target(0x5000, "", index), ("", "unresolved", ""))

    def test_plt_import_resolves_matching_defined_function_body(self):
        index = build_function_index([
            {"type": 2, "defined": True, "offset": "0x353FE0", "size": 140, "name": "Game::ChangeRoom"},
        ])
        self.assertEqual(
            resolve_direct_target(0x670100, "Game::ChangeRoom", index),
            ("Game::ChangeRoom", "plt-import", "0x353FE0"),
        )


if __name__ == "__main__":
    unittest.main()
