#!/usr/bin/env python3
"""Read-only NRO identity preflight for The Modding of Isaac.

This tool inspects the NRO0 header and identifies the exact uploaded reference
builds by both SHA-256 and the 20-byte build ID. It never patches or writes the
input file.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path
from typing import Any

MAGIC_OFFSET = 0x10
VERSION_OFFSET = 0x14
SIZE_OFFSET = 0x18
FLAGS_OFFSET = 0x1C
SEGMENTS_OFFSET = 0x20
BUILD_ID_OFFSET = 0x40
BUILD_ID_SIZE = 20
MIN_HEADER_SIZE = 0x60

KNOWN_TARGETS: dict[str, dict[str, str]] = {
    "afterbirth-plus": {
        "label": "Afterbirth+ reference NRO",
        "filename": "AfterbirthPlus.nro",
        "sha256": "705402b213072f67b561d011302c315448f47a16b9187b15b8823e003f6ccdad",
        "build_id": "0F65948274CE60ECA59C1E55B09C54D0607F3C57",
    },
    "repentance": {
        "label": "Repentance reference NRO",
        "filename": "Repentance.nro",
        "sha256": "cc363208c220f65547edda6d8268b3b6c0f7373b14c91ae6ffb378018de4b66a",
        "build_id": "91C73FDD575061318D68886316AFEAC72388B2AB",
    },
}


class NROFormatError(ValueError):
    """Raised when a file does not pass basic NRO0 structural checks."""


def inspect_nro(path: str | Path) -> dict[str, Any]:
    """Inspect a supplied NRO read-only, rejecting malformed headers."""
    file_path = Path(path)
    data = file_path.read_bytes()

    if len(data) < MIN_HEADER_SIZE:
        raise NROFormatError(
            f"file is too short for an NRO0 header ({len(data)} bytes)"
        )
    if data[MAGIC_OFFSET:MAGIC_OFFSET + 4] != b"NRO0":
        raise NROFormatError("NRO0 magic not found at offset 0x10")

    version = struct.unpack_from("<I", data, VERSION_OFFSET)[0]
    declared_size = struct.unpack_from("<I", data, SIZE_OFFSET)[0]
    flags = struct.unpack_from("<I", data, FLAGS_OFFSET)[0]
    if declared_size != len(data):
        raise NROFormatError(
            f"declared NRO size {declared_size} does not match file size {len(data)}"
        )

    text_offset, text_size, ro_offset, ro_size, data_offset, data_size, bss_size = (
        struct.unpack_from("<7I", data, SEGMENTS_OFFSET)
    )
    for name, offset, size in (
        ("text", text_offset, text_size),
        ("rodata", ro_offset, ro_size),
        ("data", data_offset, data_size),
    ):
        if offset + size > declared_size:
            raise NROFormatError(
                f"{name} segment end 0x{offset + size:X} exceeds declared file size"
            )

    build_id = data[BUILD_ID_OFFSET:BUILD_ID_OFFSET + BUILD_ID_SIZE].hex().upper()
    sha256 = hashlib.sha256(data).hexdigest()
    target = None
    for key, known in KNOWN_TARGETS.items():
        if (
            sha256 == known["sha256"]
            and build_id == known["build_id"]
        ):
            target = key
            break

    return {
        "path": str(file_path),
        "valid_nro0_header": True,
        "version": version,
        "declared_size": declared_size,
        "file_size": len(data),
        "flags": flags,
        "build_id": build_id,
        "sha256": sha256,
        "known_reference_target": target,
        "segments": {
            "text": {"offset": text_offset, "size": text_size},
            "rodata": {"offset": ro_offset, "size": ro_size},
            "data": {"offset": data_offset, "size": data_size},
            "bss_size": bss_size,
        },
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("nro", type=Path, help="input NRO (read-only)")
    parser.add_argument(
        "--expect",
        choices=sorted(KNOWN_TARGETS),
        help="fail unless the file exactly matches the selected reference build",
    )
    parser.add_argument("--json", action="store_true", help="print JSON output")
    args = parser.parse_args(argv)

    try:
        result = inspect_nro(args.nro)
    except (OSError, NROFormatError) as exc:
        print(f"NRO preflight failed: {exc}", file=sys.stderr)
        return 2

    if args.json:
        print(json.dumps(result, indent=2, sort_keys=True))
    else:
        target = result["known_reference_target"] or "unknown/modified build"
        print(f"File:      {result['path']}")
        print(f"NRO0:      valid")
        print(f"Size:      {result['file_size']} bytes")
        print(f"Build ID:  {result['build_id']}")
        print(f"SHA-256:   {result['sha256']}")
        print(f"Reference: {target}")

    if args.expect and result["known_reference_target"] != args.expect:
        print(
            f"Expected exact reference build {args.expect!r}; refusing to match.",
            file=sys.stderr,
        )
        return 3
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
