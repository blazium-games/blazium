#!/usr/bin/env python3
"""Install PS3 export templates from bin/ to Blazium export_templates directory."""

from __future__ import annotations

import argparse
import os
import shutil
import sys
from pathlib import Path


def _default_templates_dir(version: str) -> Path:
    appdata = os.environ.get("APPDATA")
    if appdata:
        return Path(appdata) / "blazium" / "export_templates" / version
    return Path.home() / ".local" / "share" / "blazium" / "export_templates" / version


def install_templates(
    blazium_root: Path,
    version: str,
    templates_dir: Path | None,
) -> int:
    bin_dir = blazium_root / "bin"
    dst_dir = templates_dir or _default_templates_dir(version)
    dst_dir.mkdir(parents=True, exist_ok=True)

    copied = 0
    for name in ("ps3_release.ppc64", "ps3_debug.ppc64"):
        src = bin_dir / name
        if not src.is_file():
            print(f"warning: missing template artifact: {src}", file=sys.stderr)
            continue
        shutil.copy2(src, dst_dir / name)
        print(f"Installed {src} -> {dst_dir / name}")
        copied += 1

    if copied == 0:
        print("error: no templates found in bin/. Build with scons platform=ps3 first.", file=sys.stderr)
        return 1

    print(f"Templates installed to {dst_dir}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Install PS3 export templates")
    parser.add_argument(
        "--blazium-root",
        type=Path,
        default=Path(__file__).resolve().parents[3],
        help="Blazium source root (contains bin/)",
    )
    parser.add_argument(
        "--version",
        default=os.environ.get("BLAZIUM_VERSION", "4.3.2.stable"),
        help="Export templates version folder name",
    )
    parser.add_argument(
        "--templates-dir",
        type=Path,
        default=None,
        help="Override export_templates destination directory",
    )
    args = parser.parse_args()
    return install_templates(args.blazium_root, args.version, args.templates_dir)


if __name__ == "__main__":
    raise SystemExit(main())
