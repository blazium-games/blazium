#!/usr/bin/env python3
"""Deploy a PS3 export folder to the RPCS3 emulator data tree."""

from __future__ import annotations

import argparse
import json
import shutil
import sys
import time
from pathlib import Path

_SCRIPTS_DIR = Path(__file__).resolve().parent
if str(_SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPTS_DIR))

from validate_ps3_export import validate_export


VALID_PPU_DECODERS = frozenset({"Recompiler (LLVM)", "Interpreter (static)"})


def _load_game_config(config_src: Path) -> str:
    lines = []
    for line in config_src.read_text(encoding="utf-8").splitlines():
        if line.startswith("#"):
            continue
        lines.append(line)
    return "\n".join(lines).strip()


def _validate_ppu_decoder(config_src: Path) -> str | None:
    for line in config_src.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if not stripped.startswith("PPU Decoder:"):
            continue
        value = stripped.split(":", 1)[1].strip()
        if value not in VALID_PPU_DECODERS:
            print(
                f"error: invalid PPU Decoder value {value!r}; "
                f"expected one of {sorted(VALID_PPU_DECODERS)}",
                file=sys.stderr,
            )
            return None
        return value
    print("error: rpcs3_game_config.yml missing PPU Decoder setting", file=sys.stderr)
    return None


def _update_config_database(emulator_root: Path, title_id: str, config_yaml: str) -> None:
    db_path = emulator_root / "GuiConfigs" / "config_database.dat"
    if not db_path.is_file():
        print(f"warning: config database not found: {db_path}", file=sys.stderr)
        return

    try:
        data = json.loads(db_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        print(f"warning: could not parse config database: {exc}", file=sys.stderr)
        return

    games = data.setdefault("games", {})
    games[title_id] = {"config": config_yaml}
    data["timestamp"] = int(time.time())
    db_path.write_text(json.dumps(data, indent=4), encoding="utf-8")
    print(f"Registered {title_id} in {db_path}")


def deploy(export_dir: Path, title_id: str, emulator_root: Path) -> int:
    src = export_dir / title_id
    if not src.is_dir():
        print(f"error: export directory not found: {src}", file=sys.stderr)
        return 1

    # Preflight: require EBOOT, PARAM.SFO, and USRDIR/game.pck before copying.
    if validate_export(src, title_id) != 0:
        print("error: export validation failed; fix export before deploy.", file=sys.stderr)
        return 1

    dst = emulator_root / "dev_hdd0" / "game" / title_id
    dst.parent.mkdir(parents=True, exist_ok=True)

    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(src, dst)

    config_src = Path(__file__).resolve().parent / "rpcs3_game_config.yml"
    ppu_decoder = None
    if config_src.is_file():
        ppu_decoder = _validate_ppu_decoder(config_src)
        if ppu_decoder is None:
            return 1

        config_dst = emulator_root / "config" / f"{title_id}.yml"
        config_dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(config_src, config_dst)
        print(f"Installed RPCS3 config {config_dst}")
        game_config_dst = dst / "config.yml"
        shutil.copy2(config_src, game_config_dst)
        print(f"Installed per-game config fallback {game_config_dst}")
        _update_config_database(emulator_root, title_id, _load_game_config(config_src))

    print(f"Deployed {src} -> {dst}")
    print(f"Launch {title_id} from the RPCS3 game list (not Boot Game on the export folder).")
    print("Or run: python launch_rpcs3.py --emulator-root ... --title-id ...")
    if ppu_decoder:
        print(f"Expected log line: PPU Decoder: {ppu_decoder}")
    print("Use Debug Console Mode for TTY output (PS3 milestones).")
    print(f"Validate: python validate_rpcs3_boot.py (see also validate_ps3_export.py)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Deploy PS3 export to RPCS3 dev_hdd0")
    parser.add_argument("--export-dir", required=True, type=Path, help="Export output directory containing TITLE_ID folder")
    parser.add_argument("--title-id", default="BLAZ00001", help="PS3 TITLE_ID")
    parser.add_argument(
        "--emulator-root",
        type=Path,
        default=Path(__file__).resolve().parents[4] / "ps3_emulator",
        help="RPCS3 user data root (contains dev_hdd0/)",
    )
    args = parser.parse_args()
    return deploy(args.export_dir, args.title_id, args.emulator_root)


if __name__ == "__main__":
    raise SystemExit(main())
