"""Host-only checks for the conservative Workshop staging tool."""

import importlib.util
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).with_name("import_workshop_candidate.py")
spec = importlib.util.spec_from_file_location("import_workshop_candidate", SCRIPT)
candidate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(candidate)


class ImportCandidateTest(unittest.TestCase):
    def test_stages_unmodified_lua_without_overwriting(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as temp:
            root = Path(temp)
            source = root / "workshop"
            source.mkdir()
            data = b'local m=RegisterMod("Demo",1)\nm:AddCallback(ModCallbacks.MC_POST_UPDATE,function(self) end)\n'
            (source / "main.lua").write_bytes(data)
            report = candidate.inspect(source)
            self.assertTrue(report["candidate"])
            output = root / "sd-package"
            candidate.stage(report, source, output, "demo")
            self.assertEqual((output / "switch/TheModdingOfIsaac/mods/demo/main.lua").read_bytes(), data)
            self.assertEqual((output / "switch/TheModdingOfIsaac/mods.txt").read_text(), "demo\n")
            with self.assertRaises(FileExistsError):
                candidate.stage(report, source, output, "demo")

    def test_rejects_unsupported_event_and_assets(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as temp:
            source = Path(temp)
            (source / "main.lua").write_text(
                'm:AddCallback(ModCallbacks.MC_POST_NEW_LEVEL,fn)\nIsaac.Spawn(1)\n')
            (source / "resources").mkdir()
            (source / "resources" / "image.png").write_bytes(b"placeholder")
            report = candidate.inspect(source)
            self.assertFalse(report["candidate"])
            self.assertIn("unsupported callback MC_POST_NEW_LEVEL", report["blockers"])
            self.assertIn("unsupported Isaac.Spawn", report["blockers"])
            with self.assertRaises(ValueError):
                candidate.stage(report, source, source / "output", "demo")


if __name__ == "__main__":
    unittest.main()

