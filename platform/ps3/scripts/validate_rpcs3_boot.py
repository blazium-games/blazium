#!/usr/bin/env python3
"""Validate RPCS3 boot settings and log milestones for Blazium PS3 homebrew."""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

_SCRIPTS_DIR = Path(__file__).resolve().parent
if str(_SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPTS_DIR))

from rpcs3_llvm_progress import HangMode, format_hang_advice, format_llvm_status, parse_llvm_progress


def _read_bytes(path: Path) -> bytes:
    sources = [path]
    copy_path = path.with_name(path.stem + "_validate_read" + path.suffix)
    try:
        import shutil
        shutil.copy2(path, copy_path)
        sources.append(copy_path)
    except OSError:
        pass

    for source in sources:
        for _ in range(5):
            try:
                return source.read_bytes()
            except PermissionError:
                time.sleep(0.5)
    return b""


def _read_head(path: Path, max_bytes: int = 512_000) -> str:
    if not path.is_file():
        return ""
    data = _read_bytes(path)
    if not data:
        return ""
    if len(data) > max_bytes:
        data = data[:max_bytes]
    return data.decode("utf-8", errors="replace")


def _read_tail(path: Path, max_bytes: int = 512_000) -> str:
    if not path.is_file():
        return ""
    data = _read_bytes(path)
    if not data:
        return ""
    if len(data) > max_bytes:
        data = data[-max_bytes:]
    return data.decode("utf-8", errors="replace")


def validate(log_path: Path, wait_seconds: int, min_wait: int, boot_log_path: Path | None) -> int:
    print("RPCS3 boot validation")
    print(f"Log: {log_path}")
    print(f"Wait: {wait_seconds}s (recommended first-run min-wait: {min_wait}s)")
    print("")
    print("TTY checklist (RPCS3 Debug Console / TTY tab — not RPCS3.log):")
    print("  [ ] PS3: modules loaded")
    print("  [ ] PS3: boot splash presented")
    print("  [ ] PS3: NoRSX initialized.")
    print("  [ ] PS3: NoRSX first frame presented.")
    print("  [ ] PS3: Main::setup begin/end")
    print("  [ ] PS3: Main::start")
    print("  [ ] PS3: first main_loop_iterate")
    print("  [ ] PS3 Test: main scene loaded")
    print("")
    print("Launch via: python launch_rpcs3.py  OR  RPCS3 game list -> BLAZ00001")
    print("Do NOT use Boot Game on exported_game/ps3_export/BLAZ00001")
    print("")

    deadline = time.time() + wait_seconds
    boot_ok = False

    while time.time() < deadline:
        text = _read_tail(log_path)
        boot_ok = "GUI: Boot successful" in text
        if boot_ok:
            break
        time.sleep(2)

    text_tail = _read_tail(log_path)
    text_head = _read_head(log_path)
    text = text_head + "\n" + text_tail
    boot_ok = "GUI: Boot successful" in text_tail
    llvm_recompiler = "PPU Decoder: Recompiler (LLVM)" in text_head
    llvm_compiling = "PPU: LLVM: Compiling" in text_tail
    invalid_ppu_enum = "try_to_enum_value('LLVM Recompiler', 'PPU Decoder'): invalid enum" in text
    invalid_enum_any = "invalid enum or integer" in text and "PPU Decoder" in text
    llvm_precompile = "LLVM Precompilation: true" in text_head
    interpreter = "PPU Decoder: Interpreter (static)" in text_head
    jump_warnings = text_tail.count("Jump table not found")
    boot_from_export = "exported_game/ps3_export" in text or "ps3_export/BLAZ00001" in text
    boot_game_line = next((line for line in text_head.splitlines() if "Emulator::BootGame:" in line), "")
    empty_title_id = "title_id=''" in boot_game_line or "title_id=\"\"" in boot_game_line
    missing_db_config = "Cannot retrieve database config for empty title_id" in text_head
    title_id_ok = "title_id='BLAZ00001'" in text or 'title_id="BLAZ00001"' in text or "Serial: BLAZ00001" in text
    boot_from_hdd0 = "dev_hdd0/game/BLAZ00001" in boot_game_line
    paramsfo_enoent = "CELL_ENOENT" in text and "_sys_process_get_paramsfo" in text
    boot_log_text = ""
    if boot_log_path and boot_log_path.is_file():
        boot_log_text = boot_log_path.read_text(encoding="utf-8", errors="replace")
        print(f"boot.log ({boot_log_path}):")
        for line in boot_log_text.strip().splitlines()[-12:]:
            print(f"  {line}")
        print("")
        for milestone in (
            "early_init",
            "main_entry",
            "modules_loaded",
            "boot_splash_presented",
            "norsx_first_frame_presented",
            "main_setup_end",
            "first_main_loop_iterate",
        ):
            print(f"  boot.log {milestone}: {'yes' if milestone in boot_log_text else 'no'}")
        print("")

    llvm_progress = parse_llvm_progress(text, boot_log_text)

    print("RPCS3.log results:")
    print(f"  Boot successful: {'yes' if boot_ok else 'no'}")
    print(f"  PPU Recompiler (LLVM): {'yes' if llvm_recompiler else 'no'}")
    print(f"  LLVM compile in progress: {'yes' if llvm_compiling else 'no'}")
    print(f"  Interpreter (static): {'yes (slow fallback)' if interpreter else 'no'}")
    print(f"  Invalid PPU Decoder enum: {'yes (bad)' if invalid_ppu_enum or invalid_enum_any else 'no (good)'}")
    print(f"  LLVM precompilation enabled: {'yes' if llvm_precompile else 'no'}")
    print(f"  Jump table warnings: {jump_warnings}")
    print(f"  TITLE_ID BLAZ00001 in log: {'yes' if title_id_ok else 'no'}")
    print(f"  Empty title_id detected: {'yes (bad)' if empty_title_id else 'no (good)'}")
    print(f"  Missing database config: {'yes (bad)' if missing_db_config else 'no (good)'}")
    print(f"  Booted from export folder: {'yes (bad)' if boot_from_export else 'no (good)'}")
    print(f"  Booted from dev_hdd0 game path: {'yes' if boot_from_hdd0 else 'no'}")
    print(f"  PARAM.SFO ENOENT: {'yes (bad)' if paramsfo_enoent else 'no (good)'}")
    print("")
    print("LLVM progress:")
    for line in format_llvm_status(llvm_progress):
        print(line)
    print("")
    for line in format_hang_advice(llvm_progress, llvm_recompiler):
        print(line)
    if format_hang_advice(llvm_progress, llvm_recompiler):
        print("")

    ok = llvm_recompiler and not invalid_ppu_enum and not invalid_enum_any and not boot_from_export
    if llvm_progress.per_instruction_flood and llvm_recompiler and llvm_progress.compiled == 0 and llvm_progress.compiling == 0:
        ok = False
    if boot_ok:
        ok = ok and not missing_db_config
    elif llvm_compiling and llvm_recompiler:
        pass  # advice printed above via format_hang_advice
    if jump_warnings > 50 and llvm_recompiler and llvm_progress.per_instruction_total > 0:
        print(f"WARN: {jump_warnings} jump-table warnings with per-instruction LLVM fallback.")
        ok = False
    if invalid_ppu_enum or invalid_enum_any:
        print("FAIL: PPU Decoder config rejected by RPCS3 (use Recompiler (LLVM), not LLVM Recompiler).")
        print("      Redeploy via deploy_rpcs3.py.")
        ok = False
    if interpreter and not llvm_recompiler and title_id_ok:
        print("FAIL: Interpreter (static) active for game-list launch — expect 8+ minute init.")
        print("      Redeploy rpcs3_game_config.yml (Recompiler (LLVM)) via deploy_rpcs3.py.")
        ok = False
    elif interpreter and not llvm_recompiler:
        print("WARN: Interpreter (static) is active — expect 8+ minute init on large templates.")
        print("      Redeploy rpcs3_game_config.yml (Recompiler (LLVM)) via deploy_rpcs3.py.")
    if jump_warnings > 50 and interpreter:
        print(f"WARN: {jump_warnings} jump-table warnings under interpreter mode.")
    if boot_from_export:
        print("FAIL: Booted export folder. Use launch_rpcs3.py or RPCS3 game list.")
        ok = False
    if empty_title_id and boot_from_hdd0 and not title_id_ok:
        print("WARN: TITLE_ID empty in BootGame line — PARAM.SFO may be unread; per-folder config.yml still applies.")
        if paramsfo_enoent:
            print("      PARAM.SFO ENOENT detected — redeploy with validate_ps3_export.py passing first.")
    elif empty_title_id and not title_id_ok:
        print("WARN: TITLE_ID not applied — launch dev_hdd0/game/BLAZ00001, not export folder.")
    if wait_seconds < min_wait:
        print(f"WARN: --wait {wait_seconds}s is below recommended --min-wait {min_wait}s for first LLVM boot.")

    if ok:
        if boot_ok:
            print("PASS: RPCS3 boot looks healthy. Check TTY tab for PS3 milestones and visible UI.")
        else:
            print("PASS: RPCS3 config and LLVM path look correct. Wait for LLVM compile to finish, then check TTY.")
        return 0

    print("FAIL: RPCS3 boot validation failed.")
    if not boot_ok and not llvm_compiling:
        print("  - Wait longer or confirm RPCS3 started the game.")
    if llvm_progress.hang_mode == HangMode.PER_INSTRUCTION:
        print("  - Rebuild with build_ps3_llvm_release.sh (rpcs3_compat=no) and redeploy.")
    if missing_db_config or boot_from_export:
        print("  - Deploy and launch via launch_rpcs3.py / game list so TITLE_ID config applies.")
    if invalid_ppu_enum or invalid_enum_any:
        print("  - Fix PPU Decoder to Recompiler (LLVM) in rpcs3_game_config.yml and redeploy.")
    return 1


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate RPCS3 boot log for Blazium PS3")
    parser.add_argument(
        "--log",
        type=Path,
        default=Path(__file__).resolve().parents[4] / "ps3_emulator" / "log" / "RPCS3.log",
        help="Path to RPCS3.log",
    )
    parser.add_argument("--wait", type=int, default=30, help="Seconds to wait for Boot successful")
    parser.add_argument("--min-wait", type=int, default=180, help="Recommended minimum session length for first run")
    parser.add_argument(
        "--boot-log",
        type=Path,
        default=Path(__file__).resolve().parents[4] / "ps3_emulator" / "dev_hdd0" / "game" / "BLAZ00001" / "USRDIR" / "boot.log",
        help="Path to USRDIR/boot.log written by the game",
    )
    args = parser.parse_args()
    return validate(args.log, args.wait, args.min_wait, args.boot_log)


if __name__ == "__main__":
    raise SystemExit(main())
