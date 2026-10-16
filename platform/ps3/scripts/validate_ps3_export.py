#!/usr/bin/env python3
"""Validate PS3 export folder artifacts before RPCS3 launch."""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

SFO_MAGIC = b"\x00PSF"
REQUIRED_FILES = ("EBOOT.pck", "PARAM.SFO")


def _read_sfo_title_id(path: Path) -> str | None:
    data = path.read_bytes()
    if len(data) < 20 or data[:4] != SFO_MAGIC:
        return None

    key_table_offset, data_table_offset, entry_count = struct.unpack_from("<III", data, 8)
    title_id = None

    for i in range(entry_count):
        entry_offset = 20 + i * 16
        if entry_offset + 16 > len(data):
            break
        key_offset, fmt, data_len, data_max_len, data_offset = struct.unpack_from("<HHIII", data, entry_offset)
        key_start = key_table_offset + key_offset
        key_end = data.find(b"\x00", key_start)
        if key_end == -1:
            continue
        key = data[key_start:key_end].decode("ascii", errors="replace")
        if key != "TITLE_ID" or fmt != 0x0204:
            continue
        value_start = data_table_offset + data_offset
        value_end = value_start + data_len
        if value_end > len(data):
            continue
        raw = data[value_start:value_end]
        title_id = raw.split(b"\x00", 1)[0].decode("utf-8", errors="replace")
        break

    return title_id


def validate_export(game_dir: Path, expected_title_id: str) -> int:
    print(f"PS3 export validation: {game_dir}")
    ok = True

    if not game_dir.is_dir():
        print(f"  FAIL: directory not found")
        return 1

    for name in REQUIRED_FILES:
        path = game_dir / name
        if path.is_file():
            print(f"  OK: {name} ({path.stat().st_size} bytes)")
        else:
            print(f"  FAIL: missing {name}")
            ok = False

    usrdir_eboot = game_dir / "USRDIR" / "EBOOT.BIN"
    if usrdir_eboot.is_file():
        print(f"  OK: USRDIR/EBOOT.BIN ({usrdir_eboot.stat().st_size} bytes)")
    else:
        print("  FAIL: missing USRDIR/EBOOT.BIN")
        ok = False

    usrdir_pack = game_dir / "USRDIR" / "game.pck"
    if usrdir_pack.is_file():
        print(f"  OK: USRDIR/game.pck ({usrdir_pack.stat().st_size} bytes)")
    else:
        print("  FAIL: missing USRDIR/game.pck")
        ok = False

    sfo_path = game_dir / "PARAM.SFO"
    if sfo_path.is_file():
        title_id = _read_sfo_title_id(sfo_path)
        if title_id is None:
            print("  FAIL: PARAM.SFO invalid (bad magic or TITLE_ID)")
            ok = False
        elif title_id != expected_title_id:
            print(f"  FAIL: PARAM.SFO TITLE_ID={title_id!r}, expected {expected_title_id!r}")
            ok = False
        else:
            print(f"  OK: PARAM.SFO TITLE_ID={title_id}")

    if ok:
        print("PASS: export artifacts look complete.")
        return 0

    print("FAIL: export validation failed.")
    return 1


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate PS3 export folder")
    parser.add_argument(
        "--game-dir",
        type=Path,
        default=Path(__file__).resolve().parents[4] / "exported_game" / "ps3_export" / "BLAZ00001",
        help="Path to TITLE_ID export folder",
    )
    parser.add_argument("--title-id", default="BLAZ00001", help="Expected TITLE_ID in PARAM.SFO")
    args = parser.parse_args()
    return validate_export(args.game_dir, args.title_id)


if __name__ == "__main__":
    raise SystemExit(main())
