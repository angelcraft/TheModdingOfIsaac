#!/usr/bin/env python3
"""Prepare a source-only Lua 5.1 candidate for the supplied Specialist mod.

This tool makes a copy of a known Workshop source tree, applies a tiny set of
reviewed edits, and writes a clear status note. It does not build an Isaac
loader, alter a game binary, or produce an installable Switch mod.
"""

from __future__ import annotations

import argparse
import hashlib
import re
import shutil
from pathlib import Path


EXPECTED_MAIN_SHA256 = "0623a6dd59603fdf81174d5252db493082ac97b8a6dd351a9b467e17d6d41926"

FLOOR_DIVISION = re.compile(
    r"(?m)^(?P<indent>[ \t]*)step[ \t]*=[ \t]*step[ \t]*//[ \t]*2"
    r"(?P<tail>[ \t]*(?:--[^\r\n]*)?)(?P<newline>\r?)$"
)
CURSE_BIT_TEST = re.compile(
    r"game:GetLevel\(\):GetCurses\(\)[ \t]*&[ \t]*64[ \t]*~=[ \t]*0"
)
ADD_ANIMATION_BUG = re.compile(
    r"(?m)^(?P<indent>[ \t]*)elseif danceCostumes\[playerType\] == nil then\r?$"
)
REMOVE_ANIMATION = re.compile(
    r"(?m)^local function removeAnimation\(table, type\)\r?\n"
    r"[ \t]*-- not sure if this is applicable here\r?\n"
    r"[ \t]*table\[type\] = nil;\r?\n"
    r"end\r?$"
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def transform_main(source: bytes) -> tuple[bytes, list[str]]:
    if sha256(source) != EXPECTED_MAIN_SHA256:
        raise ValueError(
            "main.lua is not the reviewed Workshop 1.5.2 source "
            f"(SHA-256 {sha256(source)}); refusing to apply edits to an unknown revision"
        )
    text = source.decode("utf-8")
    changes: list[str] = []

    text, n = FLOOR_DIVISION.subn(
        lambda m: f"{m.group('indent')}step = math.floor(step / 2){m.group('tail')}{m.group('newline')}",
        text,
    )
    if n != 1:
        raise ValueError(f"expected one floor-division expression, found {n}")
    changes.append("Rewrote positive-integer floor division using math.floor for Lua 5.1.")

    text, n = CURSE_BIT_TEST.subn(
        "(math.floor(game:GetLevel():GetCurses() / 64) % 2) ~= 0", text
    )
    if n != 1:
        raise ValueError(f"expected one Curse of the Blind bit test, found {n}")
    changes.append("Rewrote the 64-bit curse flag check using arithmetic available in Lua 5.1.")

    text, n = ADD_ANIMATION_BUG.subn(
        lambda m: f"{m.group('indent')}elseif table[type] == nil then", text
    )
    if n != 1:
        raise ValueError(f"expected one SpecialistModAPI costume lookup defect, found {n}")
    changes.append("Fixed AddAnimation to check the target table/key instead of an undeclared playerType.")

    text, n = REMOVE_ANIMATION.subn(
        "local function removeAnimation(table, type)\n"
        "    local existed = table[type] ~= nil\n"
        "    table[type] = nil\n"
        "    return existed\n"
        "end",
        text,
    )
    if n != 1:
        raise ValueError(f"expected one RemoveAnimation implementation, found {n}")
    changes.append("Made RemoveAnimation return the documented true/false result.")

    return text.encode("utf-8"), changes


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source_dir", type=Path, help="Extracted Specialist Workshop mod directory")
    parser.add_argument("output_dir", type=Path, help="New output directory; it must not already exist")
    args = parser.parse_args()

    source_dir = args.source_dir.resolve()
    output_dir = args.output_dir.resolve()
    main_path = source_dir / "main.lua"
    if not main_path.is_file():
        parser.error(f"source directory does not contain main.lua: {source_dir}")
    if output_dir.exists():
        parser.error(f"output directory already exists; choose a new path: {output_dir}")

    original = main_path.read_bytes()
    translated, changes = transform_main(original)
    shutil.copytree(source_dir, output_dir)
    (output_dir / "main.lua.original").write_bytes(original)
    (output_dir / "main.lua").write_bytes(translated)

    file_count = sum(1 for path in output_dir.rglob("*") if path.is_file())
    note = [
        "# Specialist Lua 5.1 source candidate",
        "",
        "This is a private source-preparation copy of The Specialist for Good Items 1.5.2.",
        "The original `main.lua` is preserved as `main.lua.original`; the edited copy is `main.lua`.",
        "",
        "## Edits",
        "",
        *[f"- {change}" for change in changes],
        "",
        "## Status",
        "",
        f"- Copied {file_count} files from the supplied mod directory.",
        "- The syntax edits target the Lua 5.1 runtime used by the standalone Switch NRO.",
        "- This folder is **not installable or playable in Isaac on Switch**: the NRO is a separate homebrew app and there is no verified Isaac callback/API bridge or resource registration path yet.",
        "- No game NRO, IPS patch, or original source file was modified.",
        "- The source still depends on Isaac globals, callbacks, entity/player APIs, music, costumes, JSON persistence and optional Mod Config Menu.",
        "",
    ]
    (output_dir / "SWITCH_PORT_STATUS.md").write_text("\n".join(note), encoding="utf-8")
    print(f"Prepared source-only candidate: {output_dir}")
    print(f"Original main.lua SHA-256: {sha256(original)}")
    print(f"Candidate main.lua SHA-256: {sha256(translated)}")
    print(f"Files copied: {file_count}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

