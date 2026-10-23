"""Triage and stage a narrow, Lua-only Workshop mod for the Switch bridge.

This does not assert behavioral compatibility. It never converts or installs
assets, and it never changes a game binary or an existing SD installation.
"""

import argparse
import hashlib
import json
import re
from pathlib import Path

MAX_MAIN_LUA = 64 * 1024
MAX_MODULE_LUA = 64 * 1024
MAX_MODULE_FILES = 16
MAX_TOTAL_LUA = 256 * 1024
ALLOWED_CALLBACKS = {"MC_POST_UPDATE", "MC_POST_NEW_ROOM"}
ALLOWED_ISAAC = {"GetItemConfig"}
ALLOWED_BRIDGE = {"GetRoomCollectibles", "SetSpecialistDance", "SetSpecialistMusic"}
IGNORED_FILES = {"main.lua", "metadata.xml", "changelog.txt", "installation.txt"}
MODULE_SEGMENT = re.compile(r"^[A-Za-z0-9_+-]+$")
REQUIRE_ARGS = re.compile(r"""^\(\s*(['"])([^'"\\]+)\1\s*\)$""")
REQUIRE_BARE = re.compile(r"""^(['"])([^'"\\]+)\1(?:\s|$)""")


def _read_lua(path: Path, blockers: list[str]) -> tuple[bytes, str]:
    data = path.read_bytes()
    rel = str(path)
    if not data:
        blockers.append(f"{rel} is empty")
    if b"\0" in data:
        blockers.append(f"{rel} contains a NUL byte")
    text = data.decode("utf-8", errors="replace")
    if "\ufffd" in text:
        blockers.append(f"{rel} is not valid UTF-8")
    return data, text


def _module_name(relative: Path) -> str | None:
    parts = relative.with_suffix("").parts
    if not parts or not all(MODULE_SEGMENT.fullmatch(part) for part in parts):
        return None
    return ".".join(parts)


def inspect(source: Path) -> dict:
    source = source.resolve()
    main = source / "main.lua"
    blockers: list[str] = []
    if not main.is_file():
        return {"source": str(source), "candidate": False, "blockers": ["main.lua is missing"]}

    lua_paths = sorted(p for p in source.rglob("*.lua") if p.is_file())
    if any(p.is_symlink() for p in lua_paths):
        blockers.append("Lua symlinks are not supported")
    if main not in lua_paths:
        lua_paths.insert(0, main)

    contents: dict[str, tuple[bytes, str]] = {}
    module_by_name: dict[str, Path] = {}
    main_data = b""
    total_bytes = 0
    for path in lua_paths:
        rel_path = path.relative_to(source)
        rel = rel_path.as_posix()
        data, text = _read_lua(path, blockers)
        contents[rel] = (data, text)
        total_bytes += len(data)
        if path == main:
            main_data = data
            if len(data) > MAX_MAIN_LUA:
                blockers.append(f"main.lua exceeds {MAX_MAIN_LUA} bytes")
        else:
            if len(data) > MAX_MODULE_LUA:
                blockers.append(f"module {rel} exceeds {MAX_MODULE_LUA} bytes")
            name = _module_name(rel_path)
            if name is None:
                blockers.append(f"module path {rel} cannot be addressed with dotted require")
            else:
                module_by_name[name] = path

    module_files = [rel for rel in contents if rel != "main.lua"]
    if len(module_files) > MAX_MODULE_FILES:
        blockers.append(f"mod has {len(module_files)} modules; limit is {MAX_MODULE_FILES}")
    if total_bytes > MAX_TOTAL_LUA:
        blockers.append(f"Lua source totals {total_bytes} bytes; limit is {MAX_TOTAL_LUA}")

    texts = "\n".join(text for _, text in contents.values())
    callbacks = sorted(set(re.findall(r"\bModCallbacks\s*\.\s*(MC_[A-Z0-9_]+)", texts)))
    for name in callbacks:
        if name not in ALLOWED_CALLBACKS:
            blockers.append(f"unsupported callback {name}")

    required_modules: set[str] = set()
    for match in re.finditer(r"\brequire\b", texts):
        tail = texts[match.end():].lstrip()
        parsed = None
        if tail.startswith("("):
            close = tail.find(")")
            if close >= 0:
                args = tail[:close + 1]
                parsed = REQUIRE_ARGS.fullmatch(args)
        else:
            parsed = REQUIRE_BARE.match(tail)
        if parsed is None:
            blockers.append("dynamic or non-literal require is unsupported")
            continue
        module = parsed.group(2)
        if not module or not all(MODULE_SEGMENT.fullmatch(part) for part in module.split(".")):
            blockers.append(f"invalid dotted module name {module!r}")
            continue
        required_modules.add(module)
    for module in sorted(required_modules):
        if module not in module_by_name:
            blockers.append(f"required local module {module!r} is missing")

    for pattern, label in [
        (r"\b(?:dofile|loadfile|loadstring)\s*\(", "dynamic Lua loading"),
        (r"\b(?:Game|MusicManager|Vector)\s*\(", "unimplemented game constructor"),
        (r"\b(?:io|os|debug|package)\s*\.", "unavailable Lua library"),
    ]:
        if re.search(pattern, texts):
            blockers.append(label)
    isaac_calls = sorted(set(re.findall(r"\bIsaac\s*[.:]\s*([A-Za-z_][A-Za-z_0-9]*)", texts)))
    bridge_calls = sorted(set(re.findall(r"\bIsaacBridge\s*[.:]\s*([A-Za-z_][A-Za-z_0-9]*)", texts)))
    for name in isaac_calls:
        if name not in ALLOWED_ISAAC:
            blockers.append(f"unsupported Isaac.{name}")
    for name in bridge_calls:
        if name not in ALLOWED_BRIDGE:
            blockers.append(f"unsupported IsaacBridge.{name}")

    lua_set = set(contents)
    assets = sorted(
        p.relative_to(source).as_posix()
        for p in source.rglob("*")
        if p.is_file()
        and p.relative_to(source).as_posix() not in lua_set
        and p.name.lower() not in IGNORED_FILES
    )
    if assets:
        blockers.append(f"{len(assets)} additional non-Lua file(s) need resource review")

    return {
        "source": str(source),
        "candidate": not blockers,
        "blockers": blockers,
        "main_lua_sha256": hashlib.sha256(main_data).hexdigest(),
        "main_lua_bytes": len(main_data),
        "lua_files": sorted(contents),
        "module_files": sorted(module_files),
        "required_modules": sorted(required_modules),
        "module_count": len(module_files),
        "total_lua_bytes": total_bytes,
        "lua_sha256": {rel: hashlib.sha256(data).hexdigest() for rel, (data, _) in sorted(contents.items())},
        "callbacks": callbacks,
        "isaac_calls": isaac_calls,
        "bridge_calls": bridge_calls,
        "additional_files": assets,
        "note": "Static triage only; source scanning can flag comments/strings and cannot prove runtime compatibility",
    }


def stage(report: dict, source: Path, output: Path, mod_id: str) -> None:
    if not report["candidate"]:
        raise ValueError("Refusing to stage a mod with known unsupported requirements")
    if not re.fullmatch(r"[a-z0-9_-]{1,40}", mod_id):
        raise ValueError("mod id must match the bridge's lowercase folder-id rule")
    if output.exists():
        raise FileExistsError(f"Refusing to overwrite {output}")
    source = source.resolve()
    destination = output / "switch" / "TheModdingOfIsaac"
    mod_root = destination / "mods" / mod_id
    mod_root.mkdir(parents=True)
    for relative in report["lua_files"]:
        target = mod_root / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes((source / relative).read_bytes())
    (destination / "mods.txt").write_text(mod_id + "\n", encoding="ascii")
    (output / "IMPORT-REPORT.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="one unpacked Steam Workshop mod folder")
    parser.add_argument("--stage", type=Path, help="new output directory for an opt-in SD package")
    parser.add_argument("--mod-id", help="lowercase SD folder id; required with --stage")
    args = parser.parse_args()
    report = inspect(args.source)
    print(json.dumps(report, indent=2))
    if args.stage:
        if not args.mod_id:
            parser.error("--mod-id is required with --stage")
        stage(report, args.source, args.stage, args.mod_id)


if __name__ == "__main__":
    main()
