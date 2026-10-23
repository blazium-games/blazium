#!/usr/bin/env python3
"""Assemble a PS3 export folder from template + PARAM.SFO when editor export is unavailable."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

_SCRIPTS_DIR = Path(__file__).resolve().parent
if str(_SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPTS_DIR))

from validate_ps3_export import validate_export


def _find_template(blazium_root: Path) -> Path | None:
    candidates = [
        blazium_root / "bin" / "ps3_release.ppc64",
        Path.home() / ".local" / "share" / "blazium" / "export_templates" / "4.3.2.stable" / "ps3_release.ppc64",
    ]
    for path in candidates:
        if path.is_file():
            return path
    return None


def assemble(
    export_dir: Path,
    title_id: str,
    blazium_root: Path,
    template: Path | None,
    game_pck: Path | None,
    title: str,
    content_id: str,
    version: str,
) -> int:
    game_dir = export_dir / title_id
    usrdir = game_dir / "USRDIR"
    usrdir.mkdir(parents=True, exist_ok=True)

    template_path = template or _find_template(blazium_root)
    if not template_path or not template_path.is_file():
        print("error: ps3_release.ppc64 not found. Run build_ps3_llvm_release.sh first.", file=sys.stderr)
        return 1

    shutil.copy2(template_path, usrdir / "EBOOT.BIN")

    pck_dst = usrdir / "game.pck"
    if game_pck and game_pck.is_file():
        if game_pck.resolve() != pck_dst.resolve():
            shutil.copy2(game_pck, pck_dst)
        else:
            print(f"Keeping existing game.pck at {pck_dst}")
    else:
        pck_dst.write_bytes(b"GDPC\x03\x00\x00\x00\x00\x00\x00\x00")
        print("warning: no game.pck supplied; wrote minimal placeholder pack")

    shutil.copy2(pck_dst, game_dir / "EBOOT.pck")

    sfo_script = _SCRIPTS_DIR / "generate_param_sfo.py"
    subprocess.run(
        [
            sys.executable,
            str(sfo_script),
            "--output",
            str(game_dir / "PARAM.SFO"),
            "--title",
            title,
            "--title-id",
            title_id,
            "--version",
            version,
            "--content-id",
            content_id,
        ],
        check=True,
    )

    print(f"Assembled export: {game_dir}")
    return validate_export(game_dir, title_id)


def main() -> int:
    repo_root = Path(__file__).resolve().parents[4]
    parser = argparse.ArgumentParser(description="Assemble PS3 export folder from template")
    parser.add_argument("--export-dir", type=Path, default=repo_root / "exported_game" / "ps3_export")
    parser.add_argument("--title-id", default="BLAZ00001")
    parser.add_argument("--blazium-root", type=Path, default=repo_root / "blazium")
    parser.add_argument("--template", type=Path, default=None)
    parser.add_argument("--game-pck", type=Path, default=None)
    parser.add_argument("--title", default="PS3 Test")
    parser.add_argument("--content-id", default="UP0001-BLAZ00001_00-BLAZIUM000")
    parser.add_argument("--version", default="01.00")
    args = parser.parse_args()
    return assemble(
        args.export_dir,
        args.title_id,
        args.blazium_root,
        args.template,
        args.game_pck,
        args.title,
        args.content_id,
        args.version,
    )


if __name__ == "__main__":
    raise SystemExit(main())
