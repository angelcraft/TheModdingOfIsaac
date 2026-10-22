"""Resolve AArch64 NRO PLT entries by their actual GOT relocation targets.

This module only reads the supplied file.  A PLT entry's position is never
used as a relocation-table index: ADRP/LDR/ADD/BR instructions must agree on
the GOT slot, and that slot must have a matching R_AARCH64_JUMP_SLOT record.
"""
from __future__ import annotations

import hashlib
import struct
from dataclasses import dataclass

EXPECTED_SHA256 = "CC363208C220F65547EDDA6D8268B3B6C0F7373B14C91AE6FFB378018DE4B66A"
R_AARCH64_JUMP_SLOT = 1026


@dataclass(frozen=True)
class DirectBranch:
    kind: str
    target: int


def decode_direct_branch(word: int, pc: int) -> DirectBranch | None:
    opcode = word & 0xFC000000
    if opcode not in (0x14000000, 0x94000000):
        return None
    imm26 = word & 0x03FFFFFF
    if imm26 & (1 << 25):
        imm26 -= 1 << 26
    return DirectBranch("BL" if opcode == 0x94000000 else "B", pc + imm26 * 4)


@dataclass(frozen=True)
class PltEntry:
    address: int
    got_address: int
    relocation_offset: int
    symbol_index: int
    name: str


@dataclass(frozen=True)
class ResolvedPlt:
    sha256: str
    text_start: int
    text_end: int
    symbol_table_offset: int
    symbol_count: int
    string_table_offset: int
    string_table_size: int
    relocation_count: int
    entries: dict[int, PltEntry]

    @property
    def names(self) -> dict[int, str]:
        return {address: entry.name for address, entry in self.entries.items()}


def resolve_plt_imports(data: bytes, expected_sha256: str = EXPECTED_SHA256) -> ResolvedPlt:
    digest = hashlib.sha256(data).hexdigest().upper()
    if digest != expected_sha256.upper():
        raise ValueError(f"unexpected NRO SHA-256: {digest}")

    def require_file(offset: int, size: int, label: str) -> None:
        if offset < 0 or size < 0 or offset > len(data) or size > len(data) - offset:
            raise ValueError(f"{label} lies outside the NRO file")

    require_file(0, 0x80, "NRO header")
    if data[0x10:0x14] != b"NRO0":
        raise ValueError("NRO0 header magic is missing")
    segments = [struct.unpack_from("<II", data, offset) for offset in (0x20, 0x28, 0x30)]
    for index, (offset, size) in enumerate(segments):
        if not size:
            raise ValueError(f"empty NRO segment {index}")
        require_file(offset, size, f"NRO segment {index}")
        if index and offset < sum(segments[index - 1]):
            raise ValueError("overlapping or unordered NRO segments")

    def require_segment(offset: int, size: int, label: str) -> None:
        require_file(offset, size, label)
        if not any(start <= offset and offset + size <= start + length for start, length in segments):
            raise ValueError(f"{label} crosses a file-backed NRO segment")

    text_offset, text_size = segments[0]
    text_start, text_end = max(text_offset, 0x80), text_offset + text_size
    if text_start >= text_end or text_start % 4 or text_end % 4:
        raise ValueError("invalid executable text segment range")
    mod_offset = struct.unpack_from("<I", data, 4)[0]
    require_segment(mod_offset, 8, "MOD0 header")
    if data[mod_offset:mod_offset + 4] != b"MOD0":
        raise ValueError("MOD0 header magic is missing")
    dynamic_offset = mod_offset + struct.unpack_from("<i", data, mod_offset + 4)[0]
    tags: dict[int, int] = {}
    for index in range(0x1000):
        offset = dynamic_offset + index * 16
        require_segment(offset, 16, "dynamic table entry")
        tag, value = struct.unpack_from("<qQ", data, offset)
        if tag == 0:
            break
        if tag in (2, 4, 5, 6, 10, 11, 20, 23):
            if tag in tags:
                raise ValueError(f"duplicate dynamic tag {tag}")
            tags[tag] = value
    else:
        raise ValueError("unterminated dynamic table")
    if any(tag not in tags for tag in (2, 4, 5, 6, 10, 11, 20, 23)):
        raise ValueError("required PLT/symbol/hash dynamic tags are missing")
    if tags[11] != 24 or tags[20] != 7 or tags[2] % 24:
        raise ValueError("unsupported symbol or PLT RELA entry format")

    require_segment(tags[4], 8, "SysV hash header")
    bucket_count, symbol_count = struct.unpack_from("<II", data, tags[4])
    if not bucket_count or not symbol_count:
        raise ValueError("empty SysV hash table")
    require_segment(tags[4], (2 + bucket_count + symbol_count) * 4, "SysV hash table")
    require_segment(tags[6], symbol_count * 24, "dynamic symbol table")
    require_segment(tags[5], tags[10], "dynamic string table")
    require_segment(tags[23], tags[2], "PLT relocation table")
    relocation_count = tags[2] // 24
    relocations: dict[int, tuple[int, int, str]] = {}
    for index in range(relocation_count):
        offset = tags[23] + index * 24
        got_address, info, _addend = struct.unpack_from("<QQq", data, offset)
        if info & 0xFFFFFFFF != R_AARCH64_JUMP_SLOT:
            raise ValueError(f"non-JUMP_SLOT relocation in PLT table at 0x{offset:x}")
        require_segment(got_address, 8, "GOT slot")
        symbol_index = info >> 32
        if symbol_index >= symbol_count:
            raise ValueError(f"PLT relocation symbol index out of range: {symbol_index}")
        name_offset = struct.unpack_from("<I", data, tags[6] + symbol_index * 24)[0]
        if name_offset >= tags[10]:
            raise ValueError("dynamic symbol string offset is out of range")
        name_start = tags[5] + name_offset
        name_end = data.find(b"\0", name_start, tags[5] + tags[10])
        if name_end < 0:
            raise ValueError("unterminated dynamic symbol name")
        name = data[name_start:name_end].decode("utf-8", "strict")
        if not name or got_address in relocations:
            raise ValueError("empty PLT symbol name or duplicate GOT relocation")
        relocations[got_address] = (offset, symbol_index, name)

    entries: dict[int, PltEntry] = {}
    matched_got: set[int] = set()
    for pc in range(text_start, text_end - 15, 4):
        adrp = struct.unpack_from("<I", data, pc)[0]
        if adrp & 0x9F00001F != 0x90000010:  # ADRP x16, page
            continue
        ldr, add, branch = struct.unpack_from("<III", data, pc + 4)
        if (ldr & 0xFFC003FF != 0xF9400211 or  # LDR x17, [x16, #imm12 * 8]
                add & 0xFFC003FF != 0x91000210 or  # ADD x16, x16, #imm12
                branch != 0xD61F0220):  # BR x17
            continue
        load_offset, add_offset = ((ldr >> 10) & 0xFFF) * 8, (add >> 10) & 0xFFF
        if load_offset != add_offset:
            raise ValueError(f"PLT LDR/ADD GOT offsets disagree at 0x{pc:x}")
        imm21 = (((adrp >> 5) & 0x7FFFF) << 2) | ((adrp >> 29) & 3)
        if imm21 & (1 << 20):
            imm21 -= 1 << 21
        got_address = (pc & ~0xFFF) + (imm21 << 12) + load_offset
        if got_address not in relocations:
            # The PLT resolver header also ends in this instruction sequence,
            # but its reserved GOT slot has no JUMP_SLOT relocation.  It is
            # deliberately not attributed to an imported symbol.
            continue
        if got_address in matched_got:
            raise ValueError(f"multiple standard PLT stubs use GOT slot 0x{got_address:x}")
        relocation_offset, symbol_index, name = relocations[got_address]
        entries[pc] = PltEntry(pc, got_address, relocation_offset, symbol_index, name)
        matched_got.add(got_address)
    if len(entries) != relocation_count:
        raise ValueError(f"resolved {len(entries)} standard PLT stubs for {relocation_count} relocations")
    return ResolvedPlt(digest, text_start, text_end, tags[6], symbol_count,
                       tags[5], tags[10], relocation_count, entries)
