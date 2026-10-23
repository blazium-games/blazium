#!/usr/bin/env python3
"""Generate a binary PARAM.SFO file for PS3 homebrew exports."""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

SFO_MAGIC = 0x46535000  # "\0PSF" little-endian
SFO_VERSION = 0x00000101
FMT_UTF8 = 0x0204
FMT_INT = 0x0404


def _align4(n: int) -> int:
    return (n + 3) & ~3


def _pack_utf8(value: str, max_len: int) -> bytes:
    encoded = value.encode("utf-8")
    if len(encoded) >= max_len:
        encoded = encoded[: max_len - 1]
    return encoded + b"\x00" * (max_len - len(encoded))


def _pack_int(value: int) -> bytes:
    return struct.pack("<I", value & 0xFFFFFFFF)


def build_sfo(entries: list[tuple[str, int, object]]) -> bytes:
    header_size = 20
    index_size = len(entries) * 16

    key_blob = bytearray()
    data_blob = bytearray()
    index = bytearray()

    key_offset = 0
    data_offset = 0

    for key, fmt, value in entries:
        key_bytes = key.encode("ascii") + b"\x00"

        if fmt == FMT_UTF8:
            max_len = max(len(str(value).encode("utf-8")) + 1, 16)
            payload = _pack_utf8(str(value), max_len)
            data_len = len(payload)
            data_max_len = max_len
        else:
            payload = _pack_int(int(value))
            data_len = 4
            data_max_len = 4

        index.extend(struct.pack("<H", key_offset))
        index.extend(struct.pack("<H", fmt))
        index.extend(struct.pack("<I", data_len))
        index.extend(struct.pack("<I", data_max_len))
        index.extend(struct.pack("<I", data_offset))

        key_blob.extend(key_bytes)
        key_offset += len(key_bytes)

        data_blob.extend(payload)
        data_offset += len(payload)

    key_table_offset = header_size + index_size
    aligned_key_len = _align4(len(key_blob))
    key_blob.extend(b"\x00" * (aligned_key_len - len(key_blob)))
    data_table_offset = key_table_offset + aligned_key_len

    header = struct.pack(
        "<IIIII",
        SFO_MAGIC,
        SFO_VERSION,
        key_table_offset,
        data_table_offset,
        len(entries),
    )

    result = header + bytes(index) + bytes(key_blob) + bytes(data_blob)
    assert result[:4] == b"\x00PSF"
    return result


def generate_param_sfo(
    output: Path,
    title: str,
    title_id: str,
    version: str,
    content_id: str,
) -> None:
    entries = [
        ("APP_TYPE", FMT_INT, 1),
        ("APP_VER", FMT_UTF8, version),
        ("ATTRIBUTE", FMT_INT, 0),
        ("BOOTABLE", FMT_INT, 1),
        ("CATEGORY", FMT_UTF8, "HG"),
        ("CONTENT_ID", FMT_UTF8, content_id),
        ("FORMAT", FMT_UTF8, "HD"),
        ("NP_COMMUNICATION_ID", FMT_UTF8, ""),
        ("NP_DRMS_TYPE", FMT_INT, 0),
        ("PARENTAL_LEVEL", FMT_INT, 0),
        ("PS3_SYSTEM_VER", FMT_UTF8, "3.5500"),
        ("RESOLUTION", FMT_INT, 0),
        ("SOUND_FORMAT", FMT_INT, 0),
        ("TITLE", FMT_UTF8, title),
        ("TITLE_ID", FMT_UTF8, title_id),
        ("VERSION", FMT_UTF8, version),
    ]

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(build_sfo(entries))


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate binary PARAM.SFO for PS3 export")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--title", required=True)
    parser.add_argument("--title-id", required=True)
    parser.add_argument("--version", default="01.00")
    parser.add_argument("--content-id", default="UP0001-BLAZ00001_00-BLAZIUM000")
    args = parser.parse_args()

    try:
        generate_param_sfo(
            args.output,
            args.title,
            args.title_id,
            args.version,
            args.content_id,
        )
    except OSError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    print(f"Wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
