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
ALLOWED_CALLBACKS = {"MC_POST_UPDATE", "MC_POST_NEW_ROOM"}
ALLOWED_ISAAC = {"GetItemConfig"}
ALLOWED_BRIDGE = {"GetRoomCollectibles", "SetSpecialistDance", "SetSpecialistMusic"}
IGNORED_FILES = {"main.lua", "metadata.xml", "changelog.txt", "installation.txt"}


def inspect(source: Path) -> dict:
    main = source / "main.lua"
    blockers = []
    if not main.is_file():
        return {"source": str(source), "candidate": False, "blockers": ["main.lua is missing"]}
    data = main.read_bytes()
    if not data or len(data) > MAX_MAIN_LUA:
        blockers.append(f"main.lua size {len(data)} is outside 1..{MAX_MAIN_LUA} bytes")
    if b"\0" in data:
        blockers.append("main.lua contains a NUL byte")
    source_text = data.decode("utf-8", errors="replace")
    if "\ufffd" in source_text:
        blockers.append("main.lua is not valid UTF-8")

    # Conservative source scan: comments and strings can cause false positives.
    # A passing result is a candidate for device testing, never proof of API use.
    callbacks = sorted(set(re.findall(r"\bModCallbacks\s*\.\s*(MC_[A-Z0-9_]+)", source_text)))
    for name in callbacks:
        if name not in ALLOWED_CALLBACKS:
            blockers.append(f"unsupported callback {name}")
    for pattern, label in [
        (r"\brequire\s*\(?\s*['\"]", "require/module loading"),
        (r"\b(?:dofile|loadfile|loadstring)\s*\(", "dynamic Lua loading"),
        (r"\b(?:Game|MusicManager|Vector)\s*\(", "unimplemented game constructor"),
        (r"\b(?:io|os|debug|package)\s*\.", "unavailable Lua library"),
    ]:
        if re.search(pattern, source_text):
            blockers.append(label)
    isaac_calls = sorted(set(re.findall(r"\bIsaac\s*[.:]\s*([A-Za-z_][A-Za-z_0-9]*)", source_text)))
    bridge_calls = sorted(set(re.findall(r"\bIsaacBridge\s*[.:]\s*([A-Za-z_][A-Za-z_0-9]*)", source_text)))
    for name in isaac_calls:
        if name not in ALLOWED_ISAAC:
            blockers.append(f"unsupported Isaac.{name}")
    for name in bridge_calls:
        if name not in ALLOWED_BRIDGE:
            blockers.append(f"unsupported IsaacBridge.{name}")

    assets = sorted(str(p.relative_to(source)).replace('\\', '/') for p in source.rglob('*')
                    if p.is_file() and str(p.relative_to(source)).lower() not in IGNORED_FILES)
    if assets:
        blockers.append(f"{len(assets)} additional file(s) need resource/module review")
    return {
        "source": str(source), "candidate": not blockers, "blockers": blockers,
        "main_lua_sha256": hashlib.sha256(data).hexdigest(), "main_lua_bytes": len(data),
        "callbacks": callbacks, "isaac_calls": isaac_calls, "bridge_calls": bridge_calls,
        "additional_files": assets,
        "note": "Static triage only; actual game behavior and dynamic Lua expressions need device validation",
    }


def stage(report: dict, source: Path, output: Path, mod_id: str) -> None:
    if not report["candidate"]:
        raise ValueError("Refusing to stage a mod with known unsupported requirements")
    if not re.fullmatch(r"[a-z0-9_-]{1,40}", mod_id):
        raise ValueError("mod id must match the bridge's lowercase folder-id rule")
    if output.exists():
        raise FileExistsError(f"Refusing to overwrite {output}")
    destination = output / "switch" / "TheModdingOfIsaac"
    mod_file = destination / "mods" / mod_id / "main.lua"
    mod_file.parent.mkdir(parents=True)
    mod_file.write_bytes((source / "main.lua").read_bytes())
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

