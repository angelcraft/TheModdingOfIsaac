"""Host-side safety and compatibility tests. No Nintendo SDK required."""
import io
import json
import os
from pathlib import Path
import stat
import subprocess
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
CLI = Path(os.environ.get("ISAACNX_TEST_CLI", str(ROOT / "build" / "host_cli")))


def run(*args):
    return subprocess.run([str(CLI), *map(str, args)], capture_output=True, text=True)


def archive(path, items, *, mode=zipfile.ZIP_DEFLATED):
    with zipfile.ZipFile(path, "w", compression=mode) as out:
        for name, payload in items.items():
            out.writestr(name, payload)


class IsaacWorkshopTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="isaacnx-")
        self.addCleanup(self.tmp.cleanup)
        self.base = Path(self.tmp.name)
        self.mods = self.base / "isaac_mods" / "mods"
        self.mods.mkdir(parents=True)
        self.zip_path = self.base / "test.zip"

    def assert_failure(self, *args):
        p = run(*args)
        self.assertNotEqual(p.returncode, 0, p.stdout)
        return p

    def test_numeric_id(self):
        self.assertEqual(run("id", "123456789").stdout.strip(), "123456789")

    def test_url_id(self):
        self.assertEqual(run("id", "https://steamcommunity.com/sharedfiles/filedetails/?foo=bar&id=24681&x=y").stdout.strip(), "24681")

    def test_wrong_hosts_and_bad_ids(self):
        for bad in ["https://steamcommunity.com.evil.test/sharedfiles/filedetails/?id=1", "-1", "0", "1a", "1" * 22, "https://evil.com/?id=3", "file:///etc/passwd", "https://steamcommunity.com/other/?id=1"]:
            with self.subTest(bad=bad):
                self.assert_failure("id", bad)

    def metadata(self, **change):
        data = {"response": {"publishedfiledetails": [{"result": 1, "publishedfileid": "123", "consumer_app_id": 250900, "title": "Isaac \u2605", "file_url": "", "file_size": "2048"}]}}
        data["response"]["publishedfiledetails"][0].update(change)
        path = self.base / "meta.json"
        path.write_text(json.dumps(data), encoding="utf-8")
        return run("metadata", "123", path)

    def test_metadata(self):
        p = self.metadata()
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertIn("Isaac ★", p.stdout)

    def test_emoji_in_steam_title(self):
        self.assertIn("🔥", self.metadata(title="Isaac 🔥").stdout)

    def test_wrong_game_rejected(self):
        self.assertNotEqual(self.metadata(consumer_app_id=480).returncode, 0)

    def test_wrong_id_rejected(self):
        self.assertNotEqual(self.metadata(publishedfileid="124").returncode, 0)

    def test_missing_item_rejected(self):
        self.assertNotEqual(self.metadata(result=9).returncode, 0)

    def test_json_duplicate_keys_rejected(self):
        path = self.base / "duplicate.json"
        path.write_text('{"response":{},"response":{}}')
        self.assert_failure("metadata", "123", path)

    def test_malformed_json_rejected(self):
        path = self.base / "bad.json"
        path.write_text('{"response": untrusted}')
        self.assert_failure("metadata", "123", path)

    def test_flat_zip_installs(self):
        archive(self.zip_path, {"main.lua": "print(1)", "metadata.xml": "<metadata/>", "content/effect.txt": "hi"})
        p = run("install", self.zip_path, self.mods, "Test Mod")
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertEqual((self.mods / "test_mod" / "content" / "effect.txt").read_text(), "hi")

    def test_nested_zip_installs_without_extra_folder(self):
        archive(self.zip_path, {"mod-name/main.lua": "-- hi", "mod-name/metadata.xml": "<metadata/>"})
        p = run("install", self.zip_path, self.mods, "steam_123")
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertTrue((self.mods / "steam_123" / "main.lua").exists())
        self.assertFalse((self.mods / "steam_123" / "mod-name").exists())

    def test_stored_zip_installs(self):
        archive(self.zip_path, {"main.lua": "--test", "metadata.xml": "ok"}, mode=zipfile.ZIP_STORED)
        self.assertEqual(run("install", self.zip_path, self.mods, "plain").returncode, 0)

    def test_no_overwrite_existing_mod(self):
        existing = self.mods / "same"
        existing.mkdir()
        (existing / "main.lua").write_text("USER_DATA")
        archive(self.zip_path, {"main.lua": "attacker", "metadata.xml": "ok"})
        self.assert_failure("install", self.zip_path, self.mods, "same")
        self.assertEqual((existing / "main.lua").read_text(), "USER_DATA")

    def test_traversal_is_rejected(self):
        archive(self.zip_path, {"main.lua": "x", "metadata.xml": "y", "../stolen": "z"})
        self.assert_failure("install", self.zip_path, self.mods, "bad")
        self.assertFalse((self.base / "stolen").exists())
        self.assertFalse((self.mods / "bad").exists())

    def test_absolute_path_is_rejected(self):
        archive(self.zip_path, {"main.lua": "x", "metadata.xml": "y", "/etc/passwd": "z"})
        self.assert_failure("install", self.zip_path, self.mods, "bad")

    def test_backslash_path_is_rejected(self):
        archive(self.zip_path, {"main.lua": "x", "metadata.xml": "y", "..\\evil": "z"})
        self.assert_failure("install", self.zip_path, self.mods, "bad")

    def test_missing_lua_or_metadata(self):
        archive(self.zip_path, {"main.lua": "x"})
        self.assert_failure("install", self.zip_path, self.mods, "bad")

    def test_multiple_roots_rejected(self):
        archive(self.zip_path, {"mod1/main.lua": "x", "mod1/metadata.xml": "y", "mod2/main.lua": "x"})
        self.assert_failure("install", self.zip_path, self.mods, "bad")

    def test_symlink_is_rejected(self):
        with zipfile.ZipFile(self.zip_path, "w") as out:
            out.writestr("main.lua", "ok")
            out.writestr("metadata.xml", "ok")
            link = zipfile.ZipInfo("link")
            link.create_system = 3
            link.external_attr = (stat.S_IFLNK | 0o777) << 16
            out.writestr(link, "../../etc")
        self.assert_failure("install", self.zip_path, self.mods, "bad")

    def test_crc_error_rolls_back(self):
        archive(self.zip_path, {"main.lua": "good", "metadata.xml": "abcd"}, mode=zipfile.ZIP_STORED)
        content = bytearray(self.zip_path.read_bytes())
        offset = content.find(b"abcd")
        self.assertGreater(offset, 0)
        content[offset] = ord("X")
        self.zip_path.write_bytes(content)
        self.assert_failure("install", self.zip_path, self.mods, "bad")
        self.assertFalse((self.mods / "bad").exists())
        self.assertFalse(list(self.mods.glob(".isaacnx_tmp*")))

    def test_zip_too_large_rejected(self):
        with self.zip_path.open("wb") as file:
            file.truncate(33 * 1024 * 1024)
        self.assert_failure("install", self.zip_path, self.mods, "bad")

    def test_missing_runtime_path(self):
        archive(self.zip_path, {"main.lua": "x", "metadata.xml": "y"})
        self.assert_failure("install", self.zip_path, self.base / "missing", "bad")

    def test_root_overlay_and_ambiguity(self):
        p = run("root", self.base)
        self.assertEqual(p.stdout.strip(), str(self.mods))
        overlay = self.base / "atmosphere" / "contents" / "010021C000B6A000" / "romfs" / "isaac_mods" / "mods"
        overlay.mkdir(parents=True)
        self.assertEqual(run("root", self.base).stdout.strip(), "")
        self.mods.rmdir()
        self.assertEqual(run("root", self.base).stdout.strip(), str(overlay))


if __name__ == "__main__":
    unittest.main()
