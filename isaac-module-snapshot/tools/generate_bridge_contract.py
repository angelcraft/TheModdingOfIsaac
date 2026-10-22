"""Generate hashes/offsets only; never copy game code into the public reader."""
import hashlib
import json
import struct
import sys
from pathlib import Path

from nro_plt import resolve_plt_imports, decode_direct_branch

ROUTE = [
    ("run_repentance", "_Z14run_repentancev", 0x4C0840, 0x228),
    ("IsaacUpdate", "_ZN15IsaacRepentance11IsaacUpdateEv", 0x3B37F0, 0x24),
    ("Manager::Update", "_ZN15IsaacRepentance7Manager6UpdateEv", 0x3F8DB8, 0x368),
    ("Game::Update", "_ZN15IsaacRepentance4Game6UpdateEv", 0x351884, 0x97C),
    ("Level::Update", "_ZN15IsaacRepentance5Level6UpdateEv", 0x3D08A4, 0x10),
    ("Room::Update", "_ZN15IsaacRepentance4Room6UpdateEv", 0x46D788, 0x3384),
    ("EntityList::Update", "_ZN15IsaacRepentance10EntityList6UpdateEb", 0x767C0, 0xC28),
]
SITES = [0x4C0A0C, 0x3B3810, 0x3F905C, 0x352154, 0x3D08B0, 0x46E6C8]


def main():
    source, destination = map(Path, sys.argv[1:3])
    data = source.read_bytes()
    resolved = resolve_plt_imports(data)  # Requires the exact original SHA256.
    symbols = {}
    for index in range(resolved.symbol_count):
        entry = resolved.symbol_table_offset + index * 24
        noff, info, other, section, address, size = struct.unpack_from("<IBBHQQ", data, entry)
        start = resolved.string_table_offset + noff
        end = data.index(b"\0", start)
        symbols[data[start:end].decode()] = (section, address, size)
    windows = []
    bindings = []
    for label, name, address, size in ROUTE:
        if symbols[name][0] == 0 or symbols[name][1:] != (address, size):
            raise ValueError("Symbol extent differs: " + name)
        windows.append(dict(name=label, offset=address, size=size,
                            sha256=hashlib.sha256(data[address:address+size]).hexdigest()))
    for site, (label, name, address, _) in zip(SITES, ROUTE[1:]):
        entry, = [value for value in resolved.entries.values() if value.name == name]
        branch = decode_direct_branch(struct.unpack_from("<I", data, site)[0], site)
        if branch is None or branch.target != entry.address:
            raise ValueError("Call route changed")
        bindings.append(dict(name=label, got=entry.got_address, target=address,
                             callsite=site, branch=branch.kind, plt=entry.address))
        windows.append(dict(name=label + " PLT", offset=entry.address, size=16,
                            sha256=hashlib.sha256(data[entry.address:entry.address+16]).hexdigest()))
    # All dynamic RELA writes must lie outside the hashed instruction ranges.
    mod = struct.unpack_from("<I", data, 4)[0]
    dyn = mod + struct.unpack_from("<i", data, mod+4)[0]
    tags = {}
    for pos in range(dyn, dyn+0x10000, 16):
        tag, value = struct.unpack_from("<qQ", data, pos)
        if tag == 0: break
        tags[tag] = value
    if tags.get(36, 0):
        raise ValueError("RELR relocations require explicit decoding")
    for address_tag, size_tag in [(7, 8), (23, 2)]:
        for pos in range(tags[address_tag], tags[address_tag] + tags[size_tag], 24):
            target, info, addend = struct.unpack_from("<QQq", data, pos)
            if any(w['offset'] <= target < w['offset'] + w['size'] for w in windows):
                raise ValueError("Code range contains runtime relocation")
    manifest = dict(sha256=resolved.sha256, windows=windows, bindings=bindings,
                    observation="static code hashes and runtime binding expectations; no hook")
    lines = ["/* Generated from exact-hash local NRO; hashes/offsets only. */",
             "#pragma once",
             "static const struct { const char *name; u64 offset; size_t size; u8 hash[32]; } k_contract_code[] = {"]
    for w in windows:
        values = ','.join('0x'+w['sha256'][i:i+2] for i in range(0,64,2))
        lines.append('    {"%s",0x%X,0x%X,{%s}},' % (w['name'],w['offset'],w['size'],values))
    lines += ["};", "static const struct { const char *name; u64 got, target; } k_contract_bindings[] = {"]
    for b in bindings:
        lines.append('    {"%s",0x%X,0x%X},' % (b['name'],b['got'],b['target']))
    lines += ["};", ""]
    destination.write_text('\n'.join(lines))
    destination.with_suffix('.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Generated %d code hashes and %d GOT expectations; no instruction relocations.' % (len(windows),len(bindings)))


if __name__ == '__main__':
    main()
