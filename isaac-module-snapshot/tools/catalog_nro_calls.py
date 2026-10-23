#!/usr/bin/env python3
"""Offline, read-only static call inventory for the user's OWN Repentance.nro.

No binary code or game assets are published. This is NOT a live function trace,
nor a reconstruction of the PC Lua mod API. It enumerates every direct AArch64
BL site in the file-backed executable text and names verified PLT imports. Calls
through BLR and pointers have no static destination and remain unresolved.

Usage:
  python isaac-module-snapshot/tools/catalog_nro_calls.py \
    /path/to/Repentance.nro --output /path/to/isaac-call-catalog
"""
from __future__ import annotations

import argparse
from bisect import bisect_right
import csv
import json
import struct
from collections import Counter
from pathlib import Path

from nro_plt import EXPECTED_SHA256, decode_direct_branch, resolve_plt_imports


def indirect_branch_register(word: int) -> int | None:
    # BLR Xn (not BR or RET): mask out bits 9..5 for register number.
    if (word & 0xFFFFFC1F) == 0xD63F0000:
        return (word >> 5) & 31
    return None


def export_symbols(data: bytes, resolved: object) -> list[dict[str, object]]:
    symbols = []
    string_begin = resolved.string_table_offset
    string_end = string_begin + resolved.string_table_size
    for index in range(resolved.symbol_count):
        offset = resolved.symbol_table_offset + index * 24
        name_offset, info, other, section, value, size = struct.unpack_from(
            "<IBBHQQ", data, offset
        )
        if not name_offset:
            continue
        if name_offset >= resolved.string_table_size:
            raise ValueError(f"invalid dynamic symbol name offset at {index}")
        start = string_begin + name_offset
        end = data.find(b"\0", start, string_end)
        if end < 0:
            raise ValueError("unterminated dynamic symbol")
        name = data[start:end].decode("utf-8")
        if not name:
            continue
        symbols.append(
            {
                "index": index,
                "name": name,
                "type": info & 0xF,
                "binding": info >> 4,
                "visibility": other & 3,
                "section": section,
                "offset": f"0x{value:X}",
                "size": size,
                "defined": section != 0,
            }
        )
    return symbols


def build_function_index(symbols: list[dict[str, object]]) -> tuple[dict[int, list[str]], list[tuple[int, int, str]], list[int], list[int], dict[str, list[int]]]:
    """Index defined STT_FUNC symbols for cautious call-site name hints.

    Dynamic symbol sizes can overlap or be absent. Exact entry addresses win;
    otherwise resolve only to the smallest known containing function range.
    """
    exact: dict[int, list[str]] = {}
    ranges: list[tuple[int, int, str]] = []
    body_by_name: dict[str, list[int]] = {}
    for symbol in symbols:
        if symbol["type"] != 2 or not symbol["defined"] or not symbol["size"]:
            continue
        start = int(str(symbol["offset"]), 16)
        size = int(symbol["size"])
        name = str(symbol["name"])
        exact.setdefault(start, []).append(name)
        ranges.append((start, start + size, name))
        body_by_name.setdefault(name, []).append(start)
    ranges.sort(key=lambda item: (item[0], item[1], item[2]))
    prefix_max_end: list[int] = []
    maximum = 0
    for _start, end, _name in ranges:
        maximum = max(maximum, end)
        prefix_max_end.append(maximum)
    for names in exact.values():
        names.sort()
    for starts_for_name in body_by_name.values():
        starts_for_name.sort()
    return exact, ranges, prefix_max_end, [item[0] for item in ranges], body_by_name


def resolve_function_symbol(offset: int, index: tuple[dict[int, list[str]], list[tuple[int, int, str]], list[int], list[int], dict[str, list[int]]]) -> str:
    """Return an exact or best containing defined function name, if known."""
    exact, ranges, prefix_max_end, starts, _body_by_name = index
    if offset in exact:
        return "|".join(exact[offset])
    position = bisect_right(starts, offset) - 1
    candidates: list[tuple[int, str]] = []
    while position >= 0 and prefix_max_end[position] > offset:
        start, end, name = ranges[position]
        if start <= offset < end:
            candidates.append((end - start, name))
        position -= 1
    if not candidates:
        return ""
    smallest_range = min(size for size, _name in candidates)
    names = sorted({name for size, name in candidates if size == smallest_range})
    return "|".join(names)


def resolve_direct_target(offset: int, plt_name: str, index: tuple[dict[int, list[str]], list[tuple[int, int, str]], list[int], list[int], dict[str, list[int]]]) -> tuple[str, str, str]:
    """Label a direct BL target and preserve how that label was obtained."""
    exact, _ranges, _prefix_max_end, _starts, body_by_name = index
    if plt_name:
        body_offsets = body_by_name.get(plt_name, [])
        return plt_name, "plt-import", "|".join(f"0x{value:X}" for value in body_offsets)
    function = resolve_function_symbol(offset, index)
    body_offsets = []
    for name in function.split("|") if function else []:
        body_offsets.extend(body_by_name.get(name, []))
    if offset in exact:
        resolution = "defined-symbol-entry"
    elif function:
        resolution = "defined-symbol-range"
    else:
        resolution = "unresolved"
    return function, resolution, "|".join(f"0x{value:X}" for value in sorted(set(body_offsets)))


def write_csv(path: Path, fields: list[str], rows: list[dict]) -> None:
    with path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def build_catalog(nro: Path, output: Path) -> dict:
    data = nro.read_bytes()
    resolved = resolve_plt_imports(data, EXPECTED_SHA256)
    output.mkdir(parents=True, exist_ok=True)

    imports = [
        {
            "name": p.name,
            "plt_offset": f"0x{p.address:X}",
            "got_offset": f"0x{p.got_address:X}",
            "relocation_offset": f"0x{p.relocation_offset:X}",
            "symbol_index": p.symbol_index,
        }
        for p in sorted(resolved.entries.values(), key=lambda e: e.address)
    ]
    write_csv(
        output / "plt_imports.csv",
        ["name", "plt_offset", "got_offset", "relocation_offset", "symbol_index"],
        imports,
    )

    symbols = export_symbols(data, resolved)
    function_index = build_function_index(symbols)
    write_csv(
        output / "dyn_symbols.csv",
        ["index", "name", "type", "binding", "visibility", "section", "offset", "size", "defined"],
        symbols,
    )

    # Stream the very large call table directly to disk, without keeping all
    # call sites in RAM. A static BLR cannot provide a target; record registers.
    branch_sites = 0
    unresolved_register_calls = 0
    named_plt_sites = 0
    named_caller_sites = 0
    named_internal_targets = 0
    named_direct_targets = 0
    direct_targets_with_body = 0
    plt_counts: Counter[str] = Counter()
    with (output / "calls.csv").open("w", encoding="utf-8", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(("site_offset", "kind", "caller_function", "target_offset",
                         "target_function", "target_resolution", "target_body_offset",
                         "plt_symbol", "target_register"))
        for pc in range(resolved.text_start, resolved.text_end - 3, 4):
            word = struct.unpack_from("<I", data, pc)[0]
            branch = decode_direct_branch(word, pc)
            if branch is not None and branch.kind == "BL":
                branch_sites += 1
                imported = resolved.entries.get(branch.target)
                name = imported.name if imported is not None else ""
                caller = resolve_function_symbol(pc, function_index)
                target, resolution, body_offset = resolve_direct_target(branch.target, name, function_index)
                named_caller_sites += bool(caller)
                if target:
                    named_direct_targets += 1
                    named_internal_targets += resolution.startswith("defined-symbol")
                if body_offset:
                    direct_targets_with_body += 1
                if name:
                    named_plt_sites += 1
                    plt_counts[name] += 1
                writer.writerow((f"0x{pc:X}", "BL", caller, f"0x{branch.target:X}",
                                 target, resolution, body_offset, name, ""))
            else:
                reg = indirect_branch_register(word)
                if reg is not None:
                    unresolved_register_calls += 1
                    caller = resolve_function_symbol(pc, function_index)
                    named_caller_sites += bool(caller)
                    writer.writerow((f"0x{pc:X}", "BLR", caller, "", "", "", "", "", f"X{reg}"))

    summary = {
        "schema_version": 2,
        "source_nro_sha256": resolved.sha256,
        "scope": "file-backed executable text, static calls; not live execution",
        "text_offset_start": f"0x{resolved.text_start:X}",
        "text_offset_end": f"0x{resolved.text_end:X}",
        "dynamic_symbol_count": resolved.symbol_count,
        "named_dynamic_symbols": len(symbols),
        "plt_import_count": len(imports),
        "direct_bl_sites": branch_sites,
        "bl_sites_to_named_plt": named_plt_sites,
        "call_sites_with_named_caller": named_caller_sites,
        "direct_bl_sites_with_named_target": named_direct_targets,
        "direct_bl_sites_to_defined_function_symbols": named_internal_targets,
        "direct_bl_sites_with_defined_body_offset": direct_targets_with_body,
        "indirect_blr_sites_without_resolved_target": unresolved_register_calls,
        "plt_imports_by_static_bl_site": dict(sorted(plt_counts.items())),
        "limitations": [
            "BLR target unknown statically; never infer it from the register number.",
            "Caller/target function labels use defined dynamic STT_FUNC symbol ranges; exact entries win and ambiguous aliases are joined with '|'.",
            "A function label is a static symbol hint, not a prototype, verified call contract, runtime trace, or callback mapping.",
            "Only file-backed executable text and the verified dynamic symbol/PLT tables.",
            "No runtime execution counts, native function prototypes, Lua callbacks or PC mod API implied.",
            "Potential inlined calls and code reached through data tables are not enumerated.",
        ],
    }
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(
        f"Call catalog: {branch_sites} direct BL, {unresolved_register_calls} "
        f"indirect BLR, {len(imports)} PLT imports, {len(symbols)} named symbols"
    )
    print(f"Saved: {output.resolve()}")
    return summary


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("nro", type=Path, help="Your locally extracted Repentance.nro")
    parser.add_argument("--output", "-o", type=Path, required=True, help="Local output directory")
    args = parser.parse_args()
    build_catalog(args.nro, args.output)


if __name__ == "__main__":
    main()
