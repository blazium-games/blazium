#!/usr/bin/env python3
"""Monitor RPCS3 PPU LLVM compile progress from RPCS3.log."""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

_SCRIPTS_DIR = Path(__file__).resolve().parent
if str(_SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPTS_DIR))

from rpcs3_llvm_progress import (
    POST_LLVM_FAIL_SECONDS,
    POST_LLVM_WARN_SECONDS,
    HangMode,
    format_hang_advice,
    format_llvm_status,
    parse_llvm_progress,
    read_log_text,
)


def _read_boot_log(path: Path) -> str:
    if not path.is_file():
        return ""
    try:
        return path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


def monitor(log_path: Path, boot_log_path: Path, interval: int, once: bool) -> int:
    print(f"Monitoring LLVM progress: {log_path}")
    if boot_log_path:
        print(f"Boot log: {boot_log_path}")
    print(f"Poll interval: {interval}s")
    print("")
    print("Phases:")
    print("  1) Until LLVM loaded: first boot ~25 min; cached boot ~1-2 min")
    print("  2) After LLVM loaded: wait 10+ min for post-LLVM CRT init (low CPU is normal)")
    print("")

    post_llvm_wall_start: float | None = None

    while True:
        text = read_log_text(log_path)
        boot_log_text = _read_boot_log(boot_log_path)
        progress = parse_llvm_progress(text, boot_log_text)

        print(f"--- {time.strftime('%H:%M:%S')} ---", flush=True)
        for line in format_llvm_status(progress):
            print(line, flush=True)
        for line in format_hang_advice(progress):
            print(line, flush=True)

        if progress.loaded and progress.post_llvm_register_seen:
            if post_llvm_wall_start is None:
                post_llvm_wall_start = time.time()
                print("")
                print("LLVM loaded and _sysProcessElf registered — post-LLVM CRT init started.")
                print("This is NOT LLVM compiling the last PPU file. Wait for early_init / main_entry.")
            elapsed = int(time.time() - post_llvm_wall_start)
            print(f"  Post-LLVM wall timer: {elapsed}s")

            if progress.main_entry_seen:
                print("")
                print("PASS: main_entry reached — CRT init completed and main() is running.")
                return 0
            if progress.early_init_seen:
                print("")
                print("PASS: early_init seen — global constructor ran; waiting for main_entry.")
            elif elapsed >= POST_LLVM_WARN_SECONDS:
                print("")
                print(
                    f"WARN: No early_init after {elapsed}s post-LLVM — CRT/static init stall at low CPU. "
                    "Check RPCS3 TTY tab and USRDIR/boot.log; do not close yet."
                )
            if elapsed >= POST_LLVM_FAIL_SECONDS and not progress.early_init_seen:
                print("")
                print(
                    f"FAIL: No early_init after {elapsed}s post-LLVM — investigate CRT/static init (PC 0x033c526c)."
                )
                if once:
                    return 1
        elif progress.loaded:
            print("")
            print("LLVM loaded — waiting for _sysProcessElf register and game CRT init.")
        elif progress.hang_mode == HangMode.PER_INSTRUCTION:
            print("")
            print("Per-instruction fallback — rebuild with build_ps3_llvm_release.sh and redeploy.")
            if once:
                return 1
        elif progress.mega_module_active:
            print("")
            print("Mega-module active — wait 5-10 min even if CPU looks idle.")
        elif progress.in_progress or progress.compiled > 0:
            print("")
            print("LLVM compile in progress — first boot can take 30-40 min total.")

        print("")

        if once:
            if progress.per_instruction_flood and progress.compiling == 0 and progress.compiled == 0:
                return 1
            if (
                progress.loaded
                and progress.post_llvm_register_seen
                and post_llvm_wall_start is not None
                and time.time() - post_llvm_wall_start >= POST_LLVM_FAIL_SECONDS
                and not progress.early_init_seen
            ):
                return 1
            return 0

        if (
            progress.loaded
            and progress.post_llvm_register_seen
            and progress.main_entry_seen
        ):
            return 0

        time.sleep(interval)


def main() -> int:
    repo_root = Path(__file__).resolve().parents[4]
    parser = argparse.ArgumentParser(description="Monitor RPCS3 LLVM compile progress")
    parser.add_argument(
        "--log",
        type=Path,
        default=repo_root / "ps3_emulator" / "log" / "RPCS3.log",
        help="Path to RPCS3.log",
    )
    parser.add_argument(
        "--boot-log",
        type=Path,
        default=repo_root / "ps3_emulator" / "dev_hdd0" / "game" / "BLAZ00001" / "USRDIR" / "boot.log",
        help="Path to USRDIR/boot.log written by the game",
    )
    parser.add_argument("--interval", type=int, default=30, help="Poll interval in seconds")
    parser.add_argument("--once", action="store_true", help="Print status once and exit")
    args = parser.parse_args()
    return monitor(args.log, args.boot_log, args.interval, args.once)


if __name__ == "__main__":
    raise SystemExit(main())
