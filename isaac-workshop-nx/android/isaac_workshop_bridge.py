#!/usr/bin/env python3
"""Android/Termux Steam Workshop fetcher and paired local ZIP bridge.

Steam's real file transport is handled by SteamRE DepotDownloader (SteamKit2).
This script never takes passwords, cookies, tokens or Steam Guard codes.
No mod data leaves the local phone except via the opt-in LAN HTTP server.
"""

import argparse
import hashlib
import http.server
import ipaddress
import os
from pathlib import Path
import re
import secrets
import shutil
import socket
import stat
import subprocess
import sys
import tempfile
import threading
import zipfile


APP_ID = "250900"
MAX_ARCHIVE = 32 * 1024 * 1024
MAX_EXPANDED = 96 * 1024 * 1024
MAX_SINGLE = 24 * 1024 * 1024
MAX_ENTRIES = 1500
MAX_DEPTH = 14
BRIDGE_PORT = 8765


class BridgeError(Exception):
    pass


def validate_id(text: str) -> str:
    if not re.fullmatch(r"[0-9]{1,20}", text) or not 0 < int(text) <= 2**64 - 1:
        raise BridgeError("El ID de Workshop no es valido")
    return text


def check_mod_root(root: Path) -> list[tuple[str, Path]]:
    if root.is_symlink() or not root.is_dir():
        raise BridgeError("La raiz del mod no es una carpeta normal")
    found: list[tuple[str, Path]] = []
    seen: set[str] = set()
    expanded = 0
    for parent, directories, files in os.walk(root, followlinks=False):
        for name in directories + files:
            item = Path(parent) / name
            relative = item.relative_to(root)
            parts = relative.parts
            if len(parts) > MAX_DEPTH or len(relative.as_posix().encode("utf-8")) > 220:
                raise BridgeError("Ruta demasiado larga para el instalador Switch")
            if any(p in ("", ".", "..") or any(ord(c) < 32 or c in ':\\' for c in p)
                   for p in parts):
                raise BridgeError("Nombre de archivo no admitido en Switch")
            folded = relative.as_posix().casefold()
            if folded in seen:
                raise BridgeError("El mod contiene nombres duplicados")
            seen.add(folded)
            mode = item.lstat().st_mode
            if stat.S_ISLNK(mode) or not (stat.S_ISREG(mode) or stat.S_ISDIR(mode)):
                raise BridgeError("No se admiten enlaces ni archivos especiales")
            if stat.S_ISREG(mode):
                expanded += item.stat().st_size
                if item.stat().st_size > MAX_SINGLE or expanded > MAX_EXPANDED:
                    raise BridgeError("El mod supera el limite del instalador Switch")
                found.append((relative.as_posix(), item))
            if len(seen) > MAX_ENTRIES:
                raise BridgeError("Demasiados archivos para la Switch")
    filenames = {name.casefold() for name, _ in found}
    if "main.lua" not in filenames or "metadata.xml" not in filenames:
        raise BridgeError("No hay main.lua y metadata.xml juntos en la raiz")
    return sorted(found)


def detect_mod_root(download_dir: Path) -> Path:
    if not download_dir.is_dir():
        raise BridgeError("No encuentro el resultado de DepotDownloader")
    candidates = []
    # Never follow symlink directories when looking for the actual mod root.
    for parent, dirs, files in os.walk(download_dir, followlinks=False):
        dirs[:] = [d for d in dirs if not (Path(parent) / d).is_symlink()]
        if "main.lua" in files and "metadata.xml" in files:
            candidates.append(Path(parent))
            if len(candidates) > 1:
                break
    if len(candidates) != 1:
        raise BridgeError("Se esperaba un solo mod con main.lua y metadata.xml")
    return candidates[0]


def pack_mod(root: Path, output: Path) -> Path:
    files = check_mod_root(root)
    output.parent.mkdir(parents=True, exist_ok=True)
    # Never replace a ZIP the user already downloaded or installed.
    try:
        archive_fd = os.open(output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    except FileExistsError as exc:
        raise BridgeError(f"ZIP ya existente (no sobrescribo): {output}") from exc
    try:
        with os.fdopen(archive_fd, "wb") as output_file:
            with zipfile.ZipFile(output_file, "w", compression=zipfile.ZIP_DEFLATED,
                                 compresslevel=6, allowZip64=False) as archive:
                for name, path in files:
                    archive.write(path, arcname=name)
                    if output_file.tell() > MAX_ARCHIVE:
                        raise BridgeError("ZIP comprimido supera 32 MiB")
        if output.stat().st_size > MAX_ARCHIVE:
            raise BridgeError("ZIP comprimido supera 32 MiB")
    except Exception:
        # This exact file was created with O_EXCL by this invocation.
        output.unlink(missing_ok=True)
        raise
    return output


def output_digest(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def download_mod(item_id: str, output_dir: Path, username: str | None, qr: bool) -> Path:
    if shutil.which("depotdownloader") is None:
        raise BridgeError("Falta DepotDownloader. En Termux: pkg install tur-repo depotdownloader")
    if username and (not re.fullmatch(r"[A-Za-z0-9_]{1,64}", username)):
        raise BridgeError("Nombre de usuario Steam no valido")
    output_dir.mkdir(parents=True, exist_ok=True)
    target = output_dir / f"steam_{item_id}.zip"
    if target.exists():
        raise BridgeError(f"Ya existe {target.name}. No se sobrescribe.")
    stage_root = output_dir / "staging"
    stage_root.mkdir(parents=True, exist_ok=True)
    staged = Path(tempfile.mkdtemp(prefix=f"steam_{item_id}_", dir=stage_root))
    args = ["depotdownloader", "-app", APP_ID, "-pubfile", item_id,
            "-dir", str(staged)]
    if username:
        args += ["-username", username]
    if qr:
        args += ["-qr"]
    print("Descargando desde Steam. Si hace falta, autentica tu cuenta en DepotDownloader.")
    # Inherit terminal input for Steam Guard. Never pass a Steam password as an argument.
    try:
        result = subprocess.run(args, check=False, timeout=600)
    except subprocess.TimeoutExpired as exc:
        raise BridgeError("Steam no termino la descarga en 10 minutos") from exc
    if result.returncode != 0:
        raise BridgeError(f"DepotDownloader fallo ({result.returncode}); diagnostico en {staged}")
    root = detect_mod_root(staged)
    return pack_mod(root, target)


def load_pairing_token(output_dir: Path) -> str:
    secret_file = output_dir / ".bridge_token"
    if secret_file.exists():
        if secret_file.is_symlink() or secret_file.stat().st_size > 128:
            raise BridgeError("Archivo de emparejamiento invalido")
        token = secret_file.read_text(encoding="ascii").strip()
        if not re.fullmatch(r"[A-Za-z0-9_-]{24,64}", token):
            raise BridgeError("Token de emparejamiento invalido")
        return token
    token = secrets.token_urlsafe(24)
    try:
        fd = os.open(secret_file, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    except FileExistsError:
        return load_pairing_token(output_dir)
    with os.fdopen(fd, "w", encoding="ascii") as destination:
        destination.write(token + "\n")
    return token


def make_handler(output_dir: Path, token: str, auto_fetch=False, username=None):
    fetch_lock = threading.Lock()

    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, _fmt, *_args):
            # Do not log the pairing secret into Android terminals/files.
            pass

        def do_GET(self):
            prefix = "/" + token + "/"
            if not self.path.startswith(prefix):
                self.send_error(404)
                return
            name = self.path[len(prefix):]
            if not re.fullmatch(r"steam_[0-9]{1,20}\.zip", name):
                self.send_error(404)
                return
            path = output_dir / name
            if auto_fetch and not path.exists():
                with fetch_lock:
                    if not path.exists():
                        try:
                            item_id = name[len("steam_"):-len(".zip")]
                            download_mod(validate_id(item_id), output_dir, username, False)
                        except BridgeError as exc:
                            print(f"Descarga solicitada desde Switch: {exc}", file=sys.stderr)
                            self.send_error(503, "Steam no entrego el mod; revisa Termux")
                            return
            try:
                fd = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0))
                details = os.fstat(fd)
                if not stat.S_ISREG(details.st_mode) or not 0 < details.st_size <= MAX_ARCHIVE:
                    os.close(fd)
                    self.send_error(404)
                    return
            except OSError:
                self.send_error(404)
                return
            with os.fdopen(fd, "rb") as source:
                self.send_response(200)
                self.send_header("Content-Type", "application/zip")
                self.send_header("Content-Length", str(details.st_size))
                self.send_header("Cache-Control", "no-store")
                self.end_headers()
                shutil.copyfileobj(source, self.wfile, 64 * 1024)

    return Handler


def bridge_address(ip_text: str, port: int, token: str) -> str:
    try:
        address = ipaddress.IPv4Address(ip_text)
    except ipaddress.AddressValueError as exc:
        raise BridgeError("IP del movil no valida") from exc
    local_ranges = (ipaddress.IPv4Network("10.0.0.0/8"),
                    ipaddress.IPv4Network("172.16.0.0/12"),
                    ipaddress.IPv4Network("192.168.0.0/16"))
    if not any(address in block for block in local_ranges) or port != BRIDGE_PORT:
        raise BridgeError("Usa la IP privada Wi-Fi del movil y puerto 8765")
    return f"http://{address}:{port}/{token}"


def detect_wifi_ip() -> str:
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.connect(("1.1.1.1", 80))  # Only asks the OS for the outgoing interface.
        return probe.getsockname()[0]


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description="Isaac Workshop NX - puente Steam Android")
    parser.add_argument("--output", type=Path, default=Path.home() / "storage" / "downloads" / "IsaacWorkshopNX")
    subparsers = parser.add_subparsers(dest="action", required=True)
    fetch = subparsers.add_parser("fetch", help="Descargar mod con DepotDownloader y preparar ZIP")
    fetch.add_argument("id")
    fetch.add_argument("--username", help="Usuario Steam; la clave se solicita dentro de DepotDownloader")
    fetch.add_argument("--qr", action="store_true", help="Autorizacion QR con Steam movil")
    pack = subparsers.add_parser("pack", help="Empaquetar mod ya descargado")
    pack.add_argument("id")
    pack.add_argument("folder", type=Path)
    serve = subparsers.add_parser("serve", help="Compartir ZIPs ya preparados con Switch por Wi-Fi local")
    serve.add_argument("--bind", default="0.0.0.0")
    serve.add_argument("--port", type=int, default=BRIDGE_PORT)
    serve.add_argument("--ip", help="IP Wi-Fi privada del Android; si no se indica, se detecta")
    serve.add_argument("--auto-fetch", action="store_true", help="Permitir descarga Steam al solicitar ID desde Switch")
    serve.add_argument("--username", help="Usuario Steam para descargas; Steam Guard se gestiona en Termux")
    options = parser.parse_args(argv)
    try:
        options.output.mkdir(parents=True, exist_ok=True)
        if options.action in ("fetch", "pack"):
            item_id = validate_id(options.id)
            path = (download_mod(item_id, options.output, options.username, options.qr)
                    if options.action == "fetch" else
                    pack_mod(detect_mod_root(options.folder), options.output / f"steam_{item_id}.zip"))
            print(f"ZIP preparado: {path}\nSHA-256: {output_digest(path)}")
        elif options.action == "serve":
            token = load_pairing_token(options.output)
            local_ip = options.ip or detect_wifi_ip()
            url = bridge_address(local_ip, options.port, token)
            config_path = options.output / "bridge.txt"
            # Only overwrite the config generated by this helper; archive and
            # Steam downloads are never overwritten.
            config_path.write_text(url + "\n", encoding="ascii")
            server = http.server.ThreadingHTTPServer((options.bind, options.port),
                make_handler(options.output, token, options.auto_fetch, options.username))
            print(f"Puente local: {url}/steam_<ID>.zip")
            print(f"Copia {config_path} a sdmc:/switch/isaac_workshop_nx/bridge.txt")
            if options.auto_fetch:
                print("Descarga automatica experimental: Steam podra pedir inicio de sesion en Termux.")
            print("Usa solo una red Wi-Fi de confianza. CTRL+C cierra el servidor.")
            try:
                server.serve_forever()
            except KeyboardInterrupt:
                server.server_close()
    except (BridgeError, OSError, zipfile.BadZipFile) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
