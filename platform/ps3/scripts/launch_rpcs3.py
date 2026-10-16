#!/usr/bin/env python3
"""Launch a deployed PS3 homebrew from the RPCS3 dev_hdd0 game tree."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


def _find_rpcs3_exe(explicit: str | None) -> Path | None:
    if explicit:
        path = Path(explicit)
        if path.is_file():
            return path
        print(f"error: RPCS3 executable not found: {path}", file=sys.stderr)
        return None

    candidates = [
        Path(os.environ.get("RPCS3_EXE", "")),
        Path(__file__).resolve().parents[4] / "ps3_emulator" / "rpcs3.exe",
        Path(r"C:\Program Files\RPCS3\rpcs3.exe"),
        Path(r"C:\Program Files (x86)\RPCS3\rpcs3.exe"),
        Path.home() / "AppData" / "Local" / "RPCS3" / "rpcs3.exe",
        shutil.which("rpcs3") or "",
    ]
    for candidate in candidates:
        if not candidate:
            continue
        path = Path(candidate)
        if path.is_file():
            return path
    return None


def launch(emulator_root: Path, title_id: str, rpcs3_exe: str | None) -> int:
    game_dir = emulator_root / "dev_hdd0" / "game" / title_id
    eboot = game_dir / "USRDIR" / "EBOOT.BIN"

    print("RPCS3 launch helper")
    print(f"  Emulator root: {emulator_root}")
    print(f"  Game path:     {game_dir}")
    print("")

    if not eboot.is_file():
        print(f"error: {eboot} not found.", file=sys.stderr)
        print("Run deploy_rpcs3.py first, then launch from the installed game path.", file=sys.stderr)
        return 1

    exe = _find_rpcs3_exe(rpcs3_exe)
    if exe is None:
        print("RPCS3 executable not found.")
        print(f"Launch manually: RPCS3 game list -> {title_id}")
        print(f"Or set RPCS3_EXE, or pass --rpcs3-exe <path>")
        print("")
        print("First LLVM boot on a 24 MB template can take 30-40 minutes.")
        print("The last mega-module (GHTAMzWrMaH4uqsLwAwr9h) may sit at low CPU for 5-10 min — do not close RPCS3.")
        print("Cached second boot should reach the game within a few minutes.")
        print("Watch the TTY tab for PS3 milestones and the dark-blue boot splash.")
        print("Interpreter fallback: deploy rpcs3_game_config_interpreter.yml (10+ min boot).")
        return 2

    print(f"Launching: {exe}")
    print(f"Boot path: {game_dir}")
    print("")
    print("LLVM boot timing:")
    print("  First boot: up to 30-40 min (last mega-module looks idle for 5-10 min)")
    print("  Cached boot: a few minutes — watch for LLVM: Loaded module in RPCS3.log")
    print("Watch RPCS3 TTY tab for:")
    print("  PS3: early_init")
    print("  PS3: modules_loaded")
    print("  PS3: boot_splash_presented")
    print("  PS3: Main::setup end")
    print("  PS3: first_main_loop_iterate")
    print("")

    try:
        subprocess.Popen([str(exe), str(game_dir)])
    except OSError as exc:
        print(f"error: failed to start RPCS3: {exc}", file=sys.stderr)
        return 1

    print("RPCS3 started.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Launch deployed PS3 homebrew in RPCS3")
    parser.add_argument("--title-id", default="BLAZ00001", help="PS3 TITLE_ID")
    parser.add_argument(
        "--emulator-root",
        type=Path,
        default=Path(__file__).resolve().parents[4] / "ps3_emulator",
        help="RPCS3 user data root (contains dev_hdd0/)",
    )
    parser.add_argument("--rpcs3-exe", default=None, help="Path to rpcs3.exe")
    args = parser.parse_args()
    return launch(args.emulator_root, args.title_id, args.rpcs3_exe)


if __name__ == "__main__":
    raise SystemExit(main())
