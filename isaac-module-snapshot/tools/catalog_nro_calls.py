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
    plt_counts: Counter[str] = Counter()
    with (output / "calls.csv").open("w", encoding="utf-8", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(("site_offset", "kind", "target_offset", "plt_symbol", "target_register"))
        for pc in range(resolved.text_start, resolved.text_end - 3, 4):
            word = struct.unpack_from("<I", data, pc)[0]
            branch = decode_direct_branch(word, pc)
            if branch is not None and branch.kind == "BL":
                branch_sites += 1
                imported = resolved.entries.get(branch.target)
                name = imported.name if imported is not None else ""
                if name:
                    named_plt_sites += 1
                    plt_counts[name] += 1
                writer.writerow((f"0x{pc:X}", "BL", f"0x{branch.target:X}", name, ""))
            else:
                reg = indirect_branch_register(word)
                if reg is not None:
                    unresolved_register_calls += 1
                    writer.writerow((f"0x{pc:X}", "BLR", "", "", f"X{reg}"))

    summary = {
        "schema_version": 1,
        "source_nro_sha256": resolved.sha256,
        "scope": "file-backed executable text, static calls; not live execution",
        "text_offset_start": f"0x{resolved.text_start:X}",
        "text_offset_end": f"0x{resolved.text_end:X}",
        "dynamic_symbol_count": resolved.symbol_count,
        "named_dynamic_symbols": len(symbols),
        "plt_import_count": len(imports),
        "direct_bl_sites": branch_sites,
        "bl_sites_to_named_plt": named_plt_sites,
        "indirect_blr_sites_without_resolved_target": unresolved_register_calls,
        "plt_imports_by_static_bl_site": dict(sorted(plt_counts.items())),
        "limitations": [
            "BLR target unknown statically; never infer it from the register number.",
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
