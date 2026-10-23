#!/usr/bin/env python3
"""Parse RPCS3.log for PPU LLVM compile progress."""

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path

MEGA_MODULE_THRESHOLD = 100_000
PER_INSTRUCTION_THRESHOLD = 1_000_000
COMPILE_RE = re.compile(r"LLVM: Compiling module ")
COMPILED_RE = re.compile(r"LLVM: Compiled module ")
LOADED_RE = re.compile(r"LLVM: Loaded module ")
MEGA_RE = re.compile(r"LLVM: (\d+) functions generated \(code_size=0x[0-9a-f]+, num_func=(\d+)")
GHTAM_RE = re.compile(r"GHTAMzWrMaH4uqsLwAwr9h")
JUMP_TABLE_RE = re.compile(r"Jump table not found")
PER_INSTR_TOTAL_RE = re.compile(r"(\d+) instructions will be compiled on per-instruction basis in total")
PER_INSTR_BLOCK_RE = re.compile(r"compiled on per-instruction basis")
POST_LLVM_REGISTER_RE = re.compile(r'_sys_prx_register_module\(name=.*_sysProcessElf')
TIMESTAMP_RE = re.compile(r'(\d+):(\d+):(\d+)\.(\d+)')
EARLY_INIT_RE = re.compile(r'PS3:\s*early_init|(^|\n)early_init(\n|$)')
MAIN_ENTRY_RE = re.compile(r'PS3:\s*main_entry|(^|\n)main_entry(\n|$)')
POST_LLVM_WARN_SECONDS = 300
POST_LLVM_FAIL_SECONDS = 600
BOOT_SESSION_RE = re.compile(r"Booting application from command line:", re.IGNORECASE)


class HangMode:
    NONE = "none"
    LLVM_MEGA = "llvm_mega_module"
    PER_INSTRUCTION = "per_instruction_fallback"
    POST_LLVM_INIT = "post_llvm_init"


@dataclass
class LlvmProgress:
    compiling: int = 0
    compiled: int = 0
    loaded: bool = False
    mega_module_active: bool = False
    mega_module_seen: bool = False
    max_functions: int = 0
    last_compiling_line: str = ""
    jump_table_warnings: int = 0
    per_instruction_total: int = 0
    per_instruction_blocks: int = 0
    post_llvm_register_seen: bool = False
    llvm_loaded_at: float | None = None
    post_register_at: float | None = None
    early_init_seen: bool = False
    main_entry_seen: bool = False
    hang_mode: str = HangMode.NONE

    @property
    def in_progress(self) -> bool:
        return self.compiling > 0 and not self.loaded

    @property
    def per_instruction_flood(self) -> bool:
        return self.per_instruction_total >= PER_INSTRUCTION_THRESHOLD


def parse_log_timestamp(line: str) -> float | None:
    match = TIMESTAMP_RE.search(line)
    if not match:
        return None
    hours = int(match.group(1))
    minutes = int(match.group(2))
    seconds = int(match.group(3))
    fraction = int(match.group(4))
    return hours * 3600 + minutes * 60 + seconds + fraction / 1_000_000


def extract_latest_session(log_text: str) -> str:
    """Keep only the most recent RPCS3 boot session from a cumulative log."""
    if not log_text:
        return ""
    starts = [m.start() for m in BOOT_SESSION_RE.finditer(log_text)]
    if not starts:
        return log_text
    return log_text[starts[-1] :]


def parse_llvm_progress(log_text: str, boot_log_text: str = "") -> LlvmProgress:
    log_text = extract_latest_session(log_text)
    progress = LlvmProgress()
    last_compile_line = ""

    for line in log_text.splitlines():
        if COMPILE_RE.search(line):
            progress.compiling += 1
            last_compile_line = line.strip()
        if COMPILED_RE.search(line):
            progress.compiled += 1
            if progress.compiling > 0:
                progress.compiling -= 1
        if LOADED_RE.search(line):
            progress.loaded = True
            progress.compiling = 0
            ts = parse_log_timestamp(line)
            if ts is not None:
                progress.llvm_loaded_at = ts

        mega_match = MEGA_RE.search(line)
        if mega_match:
            num_func = int(mega_match.group(2))
            progress.max_functions = max(progress.max_functions, num_func)
            if num_func >= MEGA_MODULE_THRESHOLD:
                progress.mega_module_seen = True

        if GHTAM_RE.search(line) and COMPILE_RE.search(line):
            progress.mega_module_active = True

        if JUMP_TABLE_RE.search(line):
            progress.jump_table_warnings += 1

        per_total = PER_INSTR_TOTAL_RE.search(line)
        if per_total:
            progress.per_instruction_total = max(progress.per_instruction_total, int(per_total.group(1)))

        if PER_INSTR_BLOCK_RE.search(line):
            progress.per_instruction_blocks += 1

        if POST_LLVM_REGISTER_RE.search(line):
            progress.post_llvm_register_seen = True
            ts = parse_log_timestamp(line)
            if ts is not None:
                progress.post_register_at = ts

        if EARLY_INIT_RE.search(line):
            progress.early_init_seen = True
        if MAIN_ENTRY_RE.search(line):
            progress.main_entry_seen = True

    if boot_log_text:
        if "early_init" in boot_log_text:
            progress.early_init_seen = True
        if "main_entry" in boot_log_text:
            progress.main_entry_seen = True

    if last_compile_line:
        progress.last_compiling_line = last_compile_line
        if GHTAM_RE.search(last_compile_line) and progress.compiling > 0:
            progress.mega_module_active = True

    progress.hang_mode = classify_hang_mode(progress)
    return progress


def classify_hang_mode(progress: LlvmProgress) -> str:
    if progress.loaded and progress.post_llvm_register_seen and not progress.early_init_seen:
        return HangMode.POST_LLVM_INIT
    if progress.compiling > 0 or (progress.compiled > 0 and not progress.loaded):
        return HangMode.LLVM_MEGA
    if progress.per_instruction_flood:
        return HangMode.PER_INSTRUCTION
    if progress.per_instruction_total > 0 and progress.jump_table_warnings > 50 and not progress.loaded:
        return HangMode.PER_INSTRUCTION
    return HangMode.NONE


def format_llvm_status(progress: LlvmProgress) -> list[str]:
    lines = [
        f"  LLVM modules compiled: {progress.compiled}",
        f"  LLVM modules in progress: {progress.compiling}",
        f"  LLVM loaded into main_thread: {'yes' if progress.loaded else 'no'}",
        f"  Post-LLVM _sysProcessElf register: {'yes' if progress.post_llvm_register_seen else 'no'}",
        f"  early_init seen: {'yes' if progress.early_init_seen else 'no'}",
        f"  main_entry seen: {'yes' if progress.main_entry_seen else 'no'}",
        f"  Jump table warnings: {progress.jump_table_warnings}",
        f"  Per-instruction blocks: {progress.per_instruction_blocks}",
    ]
    if progress.per_instruction_total > 0:
        lines.append(f"  Per-instruction total: {progress.per_instruction_total:,}")
    if progress.max_functions > 0:
        lines.append(f"  LLVM max functions in one module: {progress.max_functions:,}")
    if progress.mega_module_active:
        lines.append("  LLVM mega-module active: yes (low CPU for 5-10 min is normal — do not close RPCS3)")
    elif progress.mega_module_seen:
        lines.append("  LLVM mega-module seen: yes (already compiled or loading)")
    if progress.hang_mode == HangMode.PER_INSTRUCTION:
        lines.append("  Hang mode: per-instruction fallback (rebuild with rpcs3_compat=no)")
    elif progress.hang_mode == HangMode.LLVM_MEGA:
        lines.append("  Hang mode: LLVM mega-module compile (wait 5-10 min)")
    elif progress.hang_mode == HangMode.POST_LLVM_INIT:
        lines.append("  Hang mode: post-LLVM CRT init stall (NOT LLVM compile — check TTY/boot.log for early_init)")
    if progress.last_compiling_line:
        short = progress.last_compiling_line
        if len(short) > 120:
            short = short[:117] + "..."
        lines.append(f"  LLVM last compile line: {short}")
    return lines


def format_hang_advice(progress: LlvmProgress, llvm_recompiler: bool = True) -> list[str]:
    advice: list[str] = []
    if progress.hang_mode == HangMode.PER_INSTRUCTION:
        advice.append(
            "FAIL: Per-instruction LLVM fallback detected — RPCS3 appears hung but is analyzing millions of blocks."
        )
        advice.append("      Rebuild template: bash platform/ps3/scripts/build_ps3_llvm_release.sh (rpcs3_compat=no).")
    elif progress.hang_mode == HangMode.LLVM_MEGA:
        if progress.mega_module_active:
            advice.append("INFO: LLVM mega-module compiling — wait 5-10 min (low CPU is normal). Do not close RPCS3.")
        else:
            advice.append("INFO: PPU LLVM cache still building — first boot can take 30-40 min total.")
    elif progress.hang_mode == HangMode.POST_LLVM_INIT:
        advice.append(
            "WARN: Post-LLVM CRT init stall — LLVM compile is DONE. "
            "Wait 10+ min or check TTY/boot.log for early_init / main_entry."
        )
        advice.append("      Do NOT close RPCS3 thinking LLVM is stuck on the last PPU file.")
        advice.append(
            "      Stuck PC 0x033c526c (prior session) = .text padding after a branch — CRT/static init before main()."
        )
    if progress.per_instruction_flood and llvm_recompiler and progress.compiled == 0 and progress.compiling == 0:
        advice.append(
            f"FAIL: {progress.per_instruction_total:,} instructions queued for per-instruction compile with LLVM config."
        )
    elif progress.per_instruction_total > 1_000_000 and (progress.compiling > 0 or (progress.compiled > 0 and not progress.loaded)):
        advice.append(
            f"INFO: Per-instruction analysis reported {progress.per_instruction_total:,} blocks; LLVM module compile is in progress."
        )
    return advice


def read_log_text(log_path: Path, max_bytes: int = 2_000_000) -> str:
    if not log_path.is_file():
        return ""
    copy_path = log_path.with_name(log_path.stem + "_llvm_read" + log_path.suffix)
    sources = [log_path]
    try:
        import shutil
        shutil.copy2(log_path, copy_path)
        sources.append(copy_path)
    except OSError:
        pass

    for source in sources:
        for _ in range(5):
            try:
                data = source.read_bytes()
                if len(data) > max_bytes:
                    data = data[-max_bytes:]
                return data.decode("utf-8", errors="replace")
            except PermissionError:
                import time
                time.sleep(0.5)
    return ""
