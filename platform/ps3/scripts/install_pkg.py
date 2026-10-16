#!/usr/bin/env python3
"""Install a PS3 PKG or game folder to emulator/real hardware paths."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path


def install_pkg(pkg_path: Path, emulator_root: Path) -> int:
    if not pkg_path.is_file():
        print(f"error: PKG not found: {pkg_path}", file=sys.stderr)
        return 1

    pkg_dir = emulator_root / "dev_hdd0" / "game" / "PKG"
    pkg_dir.mkdir(parents=True, exist_ok=True)
    dst = pkg_dir / pkg_path.name
    shutil.copy2(pkg_path, dst)
    print(f"Copied PKG to {dst}")
    print("Install via RPCS3: File -> Install PKG")
    return 0


def build_pkg_from_game(game_dir: Path, content_id: str, output: Path | None) -> int:
    if not game_dir.is_dir():
        print(f"error: game directory not found: {game_dir}", file=sys.stderr)
        return 1

    pkg_path = output or game_dir.parent / f"{game_dir.name}.pkg"
    cmd = [
        "bash",
        "-lc",
        (
            "export PS3DEV=${PS3DEV:-/usr/local/ps3dev}; "
            "export PATH=$PATH:$PS3DEV/bin; "
            f"make_pkg --contentid '{content_id}' --pkgdir '{pkg_path}' '{game_dir}'"
        ),
    ]
    result = subprocess.run(cmd, check=False)
    if result.returncode != 0:
        print("error: make_pkg failed", file=sys.stderr)
        return result.returncode

    print(f"Wrote PKG to {pkg_path}")
    return 0


def deploy_game(game_dir: Path, title_id: str, emulator_root: Path) -> int:
    validate_script = Path(__file__).resolve().parent / "validate_ps3_export.py"
    if validate_script.is_file():
        result = subprocess.run(
            [sys.executable, str(validate_script), "--game-dir", str(game_dir)],
            check=False,
        )
        if result.returncode != 0:
            return result.returncode

    dst = emulator_root / "dev_hdd0" / "game" / title_id
    dst.parent.mkdir(parents=True, exist_ok=True)
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(game_dir, dst)
    print(f"Deployed game folder to {dst}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Install PS3 PKG or game folder")
    parser.add_argument("pkg", nargs="?", type=Path, help="Path to .pkg file")
    parser.add_argument("--game-dir", type=Path, help="Exported game directory")
    parser.add_argument("--title-id", default="BLAZ00001", help="TITLE_ID for game folder deploy")
    parser.add_argument("--content-id", default="UP0001-BLAZ00001_00-BLAZIUM000", help="CONTENT_ID for PKG build")
    parser.add_argument("--output", type=Path, help="Output PKG path")
    parser.add_argument(
        "--emulator-root",
        type=Path,
        default=Path(__file__).resolve().parents[4] / "ps3_emulator",
        help="RPCS3 user data root",
    )
    args = parser.parse_args()

    if args.game_dir:
        if args.pkg:
            print("error: pass either pkg or --game-dir, not both", file=sys.stderr)
            return 1
        if args.output or args.content_id:
            return build_pkg_from_game(args.game_dir, args.content_id, args.output)
        return deploy_game(args.game_dir, args.title_id, args.emulator_root)

    if args.pkg:
        return install_pkg(args.pkg, args.emulator_root)

    parser.print_help()
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
