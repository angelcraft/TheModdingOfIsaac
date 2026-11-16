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

    def delivery(self, **change):
        data = {"response": {"publishedfiledetails": [{
            "result": 1, "publishedfileid": "123", "consumer_app_id": 250900,
            "title": "Isaac test", "file_url": "", "file_type": 0,
        }]}}
        data["response"]["publishedfiledetails"][0].update(change)
        path = self.base / "delivery.json"
        path.write_text(json.dumps(data), encoding="utf-8")
        return run("delivery", "123", path)

    def test_steampipe_workshop_manifest_is_identified(self):
        p = self.delivery(hcontent_file="1234567890123456789")
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertEqual(p.stdout.strip(), "steampipe:1234567890123456789")

    def test_delivery_never_treats_manifest_as_direct_zip(self):
        self.assertEqual(self.delivery(hcontent_file="12345", file_url="https://cdn.steam.test/mod.zip").stdout.strip(), "https")
        self.assertEqual(self.delivery(hcontent_file="12345", file_url="http://untrusted.test/mod.zip").stdout.strip(), "steampipe:12345")
        self.assertEqual(self.delivery(hcontent_file="18446744073709551616").stdout.strip(), "unavailable")
        self.assertEqual(self.delivery(hcontent_file="123", file_type=2).stdout.strip(), "unavailable")
        self.assertEqual(self.delivery(hcontent_file="0").stdout.strip(), "unavailable")

    def steam_auth(self, kind, response):
        path = self.base / "auth.json"
        path.write_text(json.dumps(response), encoding="utf-8")
        return run(kind, path)

    def test_steam_qr_begin_parses_only_safe_qr_url(self):
        data = {"response": {"client_id": "76561198000000001", "request_id": "b64private+/=",
                             "challenge_url": "https://s.team/q/1/ABC123", "interval": 5.4}}
        p = self.steam_auth("qr-begin", data)
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertEqual(p.stdout.splitlines(), ["https://s.team/q/1/ABC123", "6"])
        self.assertNotIn("b64private", p.stdout)

    def test_steam_qr_begin_rejects_unsafe_challenges(self):
        base = {"client_id": "76561198000000001", "request_id": "b64private+/=",
                "challenge_url": "https://s.team/q/1/ABC123"}
        for update in ({"challenge_url": "https://s.team.evil.test/q/1/ABC"},
                       {"challenge_url": "javascript:alert(1)"},
                       {"request_id": "private\nerror"},
                       {"client_id": "not-a-number"},
                       {"client_id": "18446744073709551616"}):
            with self.subTest(update=update):
                self.assertNotEqual(self.steam_auth("qr-begin", {"response": base | update}).returncode, 0)

    def test_steam_qr_poll_does_not_log_tokens(self):
        p = self.steam_auth("qr-poll", {"response": {
            "access_token": "ACCESS_SECRET_123", "refresh_token": "REFRESH_SECRET_456", "account_name": "myaccount"}})
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertEqual(p.stdout.strip(), "confirmed")
        self.assertNotIn("SECRET", p.stdout + p.stderr)
        self.assertEqual(self.steam_auth("qr-poll", {"response": {"had_remote_interaction": True}}).stdout.strip(), "pending")

    def test_steam_qr_poll_rejects_invalid_types_and_urls(self):
        for response in ({"refresh_token": ["oops"]},
                         {"new_client_id": "NaN"},
                         {"new_challenge_url": "http://s.team/q/1/notsecure"},
                         {"account_name": "abc", "new_challenge_url": "https://notsteam.test/"}):
            with self.subTest(response=response):
                self.assertNotEqual(self.steam_auth("qr-poll", {"response": response}).returncode, 0)

    def test_emoji_in_steam_title(self):
        self.assertIn("🔥", self.metadata(title="Isaac 🔥").stdout)

    def test_public_browse_listing_finds_ids_deduped_in_order(self):
        html = '''<div class="workshopItemPreviewHolder"><a href="https://steamcommunity.com/sharedfiles/filedetails/?id=836319872&amp;searchtext=a">One</a></div>
        <a href="https://steamcommunity.com/sharedfiles/filedetails/?id=123456789&amp;x=y">Two</a>
        <a href="https://steamcommunity.com/sharedfiles/filedetails/?id=836319872">duplicate</a>'''
        path = self.base / "browse.html"
        path.write_text(html)
        p = run("listing", path)
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertEqual(p.stdout.splitlines(), ["836319872", "123456789"])

    def test_browse_html_rejects_bad_numeric_links_and_excess(self):
        path = self.base / "browse.html"
        path.write_text('filedetails/?id=0 filedetails/?id=999999999999999999999999 filedetails/?id=10abc')
        self.assert_failure("listing", path)
        with path.open("wb") as out:
            out.truncate(3 * 1024 * 1024 + 1)
        self.assert_failure("listing", path)

    def test_browse_html_does_not_execute_markup(self):
        path = self.base / "browse.html"
        path.write_text('''<script>alert('bad')</script><a href="filedetails/?id=482">safe</a>''')
        self.assertEqual(run("listing", path).stdout.strip(), "482")

    def test_batch_metadata_validates_app_and_preserves_order(self):
        def entry(id, app=250900, result=1):
            return {"result": result, "publishedfileid": id, "consumer_app_id": app,
                    "title": f"Item {id}", "preview_url": f"https://steamusercontent.test/{id}",
                    "file_description": "A sample description"}
        data = {"response": {"publishedfiledetails": [entry("333"), entry("222", 480), entry("111")]}}
        path = self.base / "batch.json"
        path.write_text(json.dumps(data))
        p = run("batch", "111,222,333", path)
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertEqual([line.split(" | ")[0] for line in p.stdout.splitlines()], ["111", "333"])

    def test_batch_wrong_id_and_duplicate_requested_ids_rejected(self):
        data = {"response": {"publishedfiledetails": [
            {"result": 1, "publishedfileid": "999", "consumer_app_id": 250900, "title": "bad"}]}}
        path = self.base / "batch.json"
        path.write_text(json.dumps(data))
        self.assert_failure("batch", "123", path)
        self.assert_failure("batch", "123,123", path)

    def test_control_characters_stripped_before_console(self):
        self.assertEqual(run("clean-text", "Nice\x1b[2J\n\tMod").stdout.strip(), "Nice [2J Mod")

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
