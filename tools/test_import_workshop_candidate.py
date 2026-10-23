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
    def test_stages_unmodified_main_and_local_modules_without_overwriting(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as temp:
            root = Path(temp)
            source = root / "workshop"
            (source / "helpers").mkdir(parents=True)
            main = b'local h=require("helpers.quality_filter")\nlocal m=RegisterMod("Demo",1)\nm:AddCallback(ModCallbacks.MC_POST_UPDATE,function(self) end)\n'
            helper = b'return function(id) return id == 4 end\n'
            (source / "main.lua").write_bytes(main)
            (source / "helpers" / "quality_filter.lua").write_bytes(helper)
            report = candidate.inspect(source)
            self.assertTrue(report["candidate"], report["blockers"])
            self.assertEqual(report["required_modules"], ["helpers.quality_filter"])
            self.assertEqual(report["module_files"], ["helpers/quality_filter.lua"])
            output = root / "sd-package"
            candidate.stage(report, source, output, "demo")
            base = output / "switch/TheModdingOfIsaac"
            self.assertEqual((base / "mods/demo/main.lua").read_bytes(), main)
            self.assertEqual((base / "mods/demo/helpers/quality_filter.lua").read_bytes(), helper)
            self.assertEqual((base / "mods.txt").read_text(), "demo\n")
            with self.assertRaises(FileExistsError):
                candidate.stage(report, source, output, "demo")

    def test_rejects_missing_or_dynamic_modules(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as temp:
            source = Path(temp)
            (source / "main.lua").write_text('local a=require("missing.module")\nlocal b=require(name)\n')
            report = candidate.inspect(source)
            self.assertFalse(report["candidate"])
            self.assertTrue(any("missing" in item for item in report["blockers"]))
            self.assertTrue(any("dynamic or non-literal require" in item for item in report["blockers"]))

    def test_rejects_unsupported_event_apis_and_assets(self):
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
