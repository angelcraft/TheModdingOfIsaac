"""Read-only import/BL scan for the exact Repentance 1.7.9b Switch NRO."""
from __future__ import annotations

import hashlib
import re
import struct
import sys
from collections import Counter
from pathlib import Path

EXPECTED_SHA256 = "CC363208C220F65547EDDA6D8268B3B6C0F7373B14C91AE6FFB378018DE4B66A"
RELA_OFFSET = 0x6D65D0
RELA_COUNT = 7442
SYMTAB_OFFSET = 0x73FF20
STRTAB_OFFSET = 0x7A5050
PLT_BASE = 0x66FAD0


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def u64(data: bytes, offset: int) -> int:
    return struct.unpack_from("<Q", data, offset)[0]


def decode_bl(word: int, pc: int) -> int | None:
    if word & 0xFC000000 != 0x94000000:
        return None
    imm26 = word & 0x03FFFFFF
    if imm26 & (1 << 25):
        imm26 -= 1 << 26
    return pc + imm26 * 4


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: inspect_nro_loader_imports.py Repentance.nro")
    path = Path(sys.argv[1])
    data = path.read_bytes()
    digest = hashlib.sha256(data).hexdigest().upper()
    if digest != EXPECTED_SHA256:
        raise SystemExit(f"unexpected NRO SHA-256: {digest}")
    if data[0x10:0x14] != b"NRO0":
        raise SystemExit("NRO0 header magic is missing")
    text_offset, text_size = struct.unpack_from("<II", data, 0x20)
    text_start = max(text_offset, 0x80)
    text_end = text_offset + text_size
    if text_size == 0 or text_end > len(data) or text_start >= text_end:
        raise SystemExit("invalid executable text segment range")

    imports: dict[int, str] = {}
    for index in range(RELA_COUNT):
        relocation = RELA_OFFSET + index * 24
        symbol_index = u64(data, relocation + 8) >> 32
        name_offset = u32(data, SYMTAB_OFFSET + symbol_index * 24)
        name_start = STRTAB_OFFSET + name_offset
        name_end = data.find(b"\0", name_start)
        if name_end < 0:
            continue
        name = data[name_start:name_end].decode("utf-8", "replace")
        imports[PLT_BASE + index * 16] = name

    counts = Counter(imports.values())
    terms = re.compile(
        r"LoadModule|UnloadModule|nn::ro|_ZN2nn2ro|NroInfo|roLoad|roUnload|ldrRo",
        re.IGNORECASE,
    )
    candidates = {name for name in counts if terms.search(name)}
    callers: dict[str, list[int]] = {name: [] for name in candidates}
    for pc in range(text_start, text_end - 3, 4):
        target = decode_bl(u32(data, pc), pc)
        name = imports.get(target) if target is not None else None
        if name in callers:
            callers[name].append(pc)

    print(f"sha256={digest}")
    print(f"executable_text=0x{text_start:x}..0x{text_end:x}")
    print(f"rela_entries={RELA_COUNT} imported_symbol_names={len(counts)}")
    print("loader_related_imports:")
    for name in sorted(candidates, key=str.casefold):
        pcs = ",".join(f"0x{pc:x}" for pc in callers[name]) or "none"
        print(f"  {name} count={counts[name]} direct_BL_callers={pcs}")


if __name__ == "__main__":
    main()
