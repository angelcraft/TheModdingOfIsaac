"""Offline integration tests for the Android helper and Switch ZIP importer."""

import importlib.util
import io
from pathlib import Path
import subprocess
import tempfile
import threading
import unittest
from unittest import mock
from urllib.error import HTTPError
from urllib.request import urlopen
import http.server
import zipfile

from test_isaacnx import run


MODULE = Path(__file__).resolve().parents[1] / "android" / "isaac_workshop_bridge.py"
spec = importlib.util.spec_from_file_location("isaacnx_android_bridge", MODULE)
bridge = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bridge)


class AndroidBridgeTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="isaacnx-android-")
        self.addCleanup(self.tmp.cleanup)
        self.base = Path(self.tmp.name)
        self.source = self.base / "incoming" / "workshop" / "250900" / "123456"
        self.source.mkdir(parents=True)
        (self.source / "main.lua").write_text("-- run mod")
        (self.source / "metadata.xml").write_text("<metadata/>")
        (self.source / "resources").mkdir()
        (self.source / "resources" / "sprite.anm2").write_bytes(b"animation")
        self.zip_path = self.base / "steam_123456.zip"

    def test_real_zip_accepted_by_switch_importer(self):
        self.assertEqual(bridge.detect_mod_root(self.base / "incoming"), self.source)
        bridge.pack_mod(self.source, self.zip_path)
        with zipfile.ZipFile(self.zip_path) as f:
            self.assertIn("main.lua", f.namelist())
            self.assertIn("resources/sprite.anm2", f.namelist())
        mods = self.base / "mods"
        mods.mkdir()
        p = run("install", self.zip_path, mods, "steam_123456")
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertEqual((mods / "steam_123456" / "main.lua").read_text(), "-- run mod")

    def test_no_overwrite_on_existing_download(self):
        self.zip_path.write_bytes(b"PRESERVE_USER_ZIP")
        with self.assertRaises(bridge.BridgeError):
            bridge.pack_mod(self.source, self.zip_path)
        self.assertEqual(self.zip_path.read_bytes(), b"PRESERVE_USER_ZIP")

    def test_bad_id_and_private_bridge_config(self):
        for bad in ("0", "123a", "1" * 21, "-42", "42&username=abc"):
            with self.subTest(bad=bad), self.assertRaises(bridge.BridgeError):
                bridge.validate_id(bad)
        token = "a" * 32
        for ip in ("192.168.2.45", "10.20.30.40", "172.20.1.2"):
            address = bridge.bridge_address(ip, 8765, token)
            self.assertEqual(run("bridge", address).stdout.strip(), address)
        for ip in ("8.8.8.8", "127.0.0.1", "169.254.1.1", "172.32.0.1"):
            with self.assertRaises(bridge.BridgeError):
                bridge.bridge_address(ip, 8765, token)

    def test_switch_rejects_ssrf_or_incomplete_tokens(self):
        bad_urls = [
            "https://192.168.1.2:8765/" + "a" * 32,
            "http://evil.com:8765/" + "a" * 32,
            "http://127.0.0.1:8765/" + "a" * 32,
            "http://192.168.1.2:9999/" + "a" * 32,
            "http://192.168.1.2:8765/short",
            "http://192.168.1.2:8765/" + "a" * 32 + "/../../bad",
            "http://192.168.1.2:8765/" + "a" * 32 + "?next=http://evil.com",
        ]
        for address in bad_urls:
            with self.subTest(address=address):
                self.assertNotEqual(run("bridge", address).returncode, 0)

    def test_download_argv_has_no_password(self):
        capture = {}

        def fake_depot(args, check, timeout):
            self.assertFalse(check)
            self.assertEqual(timeout, 600)
            capture["argv"] = args
            staged = Path(args[args.index("-dir") + 1])
            (staged / "mod").mkdir()
            (staged / "mod" / "main.lua").write_text("print(1)")
            (staged / "mod" / "metadata.xml").write_text("<metadata/>")
            return subprocess.CompletedProcess(args, 0)

        with mock.patch.object(bridge.shutil, "which", return_value="/bin/depotdownloader"), \
             mock.patch.object(bridge.subprocess, "run", side_effect=fake_depot):
            output = bridge.download_mod("123456", self.base / "out", "owner123", True)
        self.assertEqual(output.name, "steam_123456.zip")
        self.assertEqual(capture["argv"][:5], ["depotdownloader", "-app", "250900", "-pubfile", "123456"])
        self.assertIn("-qr", capture["argv"])
        self.assertNotIn("-password", capture["argv"])

    def test_symlinks_and_oversize_rejected(self):
        (self.source / "link").symlink_to(self.base)
        with self.assertRaises(bridge.BridgeError):
            bridge.pack_mod(self.source, self.zip_path)
        self.assertFalse(self.zip_path.exists())
        (self.source / "link").unlink()
        with (self.source / "giant.bin").open("wb") as out:
            out.truncate(24 * 1024 * 1024 + 1)
        with self.assertRaises(bridge.BridgeError):
            bridge.pack_mod(self.source, self.zip_path)
        self.assertFalse(self.zip_path.exists())

    def test_lan_http_serves_only_paired_archive(self):
        bridge.pack_mod(self.source, self.zip_path)
        token = "x" * 32
        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), bridge.make_handler(self.base, token))
        worker = threading.Thread(target=server.serve_forever, daemon=True)
        worker.start()
        self.addCleanup(worker.join, 2)
        self.addCleanup(server.server_close)
        self.addCleanup(server.shutdown)
        root = f"http://127.0.0.1:{server.server_port}"
        with urlopen(f"{root}/{token}/steam_123456.zip", timeout=3) as response:
            self.assertEqual(response.read(), self.zip_path.read_bytes())
        for suffix in ("wrong/steam_123456.zip", f"{token}/../../test", f"{token}/steam_999999.zip"):
            with self.subTest(suffix=suffix), self.assertRaises(HTTPError):
                urlopen(f"{root}/{suffix}", timeout=3)

    def test_optional_fetch_on_request_uses_only_paired_id(self):
        token = "z" * 32
        called = []

        def fake_fetch(item_id, destination, username, qr):
            called.append((item_id, username, qr))
            return bridge.pack_mod(self.source, destination / f"steam_{item_id}.zip")

        with mock.patch.object(bridge, "download_mod", side_effect=fake_fetch):
            server = http.server.ThreadingHTTPServer(("127.0.0.1", 0),
                bridge.make_handler(self.base, token, auto_fetch=True, username="owner123"))
            worker = threading.Thread(target=server.serve_forever, daemon=True)
            worker.start()
            try:
                root = f"http://127.0.0.1:{server.server_port}"
                with self.assertRaises(HTTPError):
                    urlopen(f"{root}/wrong/steam_987654.zip", timeout=3)
                with urlopen(f"{root}/{token}/steam_987654.zip", timeout=3) as response:
                    self.assertTrue(response.read().startswith(b"PK"))
                self.assertEqual(called, [("987654", "owner123", False)])
            finally:
                server.shutdown()
                server.server_close()
                worker.join(2)


if __name__ == "__main__":
    unittest.main()
