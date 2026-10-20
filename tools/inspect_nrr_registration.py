"""Read-only NRR0 parser that checks whether an NRO hash is registered."""
from __future__ import annotations

import hashlib
import struct
import sys
from pathlib import Path

HEADER_MIN_SIZE = 0x350
HASH_LIST_OFFSET_FIELD = 0x340
HASH_COUNT_FIELD = 0x344
PROGRAM_ID_FIELD = 0x330
DECLARED_SIZE_FIELD = 0x338


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def u64(data: bytes, offset: int) -> int:
    return struct.unpack_from("<Q", data, offset)[0]


def main() -> None:
    if len(sys.argv) != 4:
        raise SystemExit(
            "usage: inspect_nrr_registration.py NRO NRR EXPECTED_PROGRAM_ID_HEX"
        )

    nro_path, nrr_path = map(Path, sys.argv[1:3])
    try:
        expected_program_id = int(sys.argv[3], 16)
    except ValueError as error:
        raise SystemExit("expected program ID must be hexadecimal") from error

    nro = nro_path.read_bytes()
    nrr = nrr_path.read_bytes()
    nro_sha256 = hashlib.sha256(nro).hexdigest().upper()
    nrr_sha256 = hashlib.sha256(nrr).hexdigest().upper()

    if len(nrr) < HEADER_MIN_SIZE or nrr[:4] != b"NRR0":
        raise SystemExit("invalid or truncated NRR0 header")

    program_id = u64(nrr, PROGRAM_ID_FIELD)
    declared_size = u32(nrr, DECLARED_SIZE_FIELD)
    hash_offset = u32(nrr, HASH_LIST_OFFSET_FIELD)
    hash_count = u32(nrr, HASH_COUNT_FIELD)
    hash_end = hash_offset + hash_count * 32

    if declared_size != len(nrr):
        raise SystemExit(
            f"declared NRR size {declared_size} does not match file size {len(nrr)}"
        )
    if hash_offset < HEADER_MIN_SIZE or hash_end > len(nrr):
        raise SystemExit("NRR hash-list range falls outside the file")

    hashes = [
        nrr[offset : offset + 32].hex().upper()
        for offset in range(hash_offset, hash_end, 32)
    ]
    registered = nro_sha256 in hashes
    program_matches = program_id == expected_program_id

    print(f"nro_sha256={nro_sha256}")
    print(f"nrr_sha256={nrr_sha256}")
    print(f"nrr_program_id=0x{program_id:016X}")
    print(f"expected_program_id=0x{expected_program_id:016X}")
    print(f"nrr_declared_size={declared_size}")
    print(f"nrr_hash_list_offset=0x{hash_offset:X}")
    print(f"nrr_hash_count={hash_count}")
    print(f"nro_hash_registered={str(registered).lower()}")
    print(f"program_id_matches={str(program_matches).lower()}")
    if not registered or not program_matches:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
