#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Post-build Hub registration for `scons hub_register=yes` editor builds.

Invokes `blazium-cli handle-uri blazium://register?...` for the linked editor
binary. Missing CLI or non-zero exit only warns (build still succeeds).
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from typing import Iterable, List, Optional
from urllib.parse import quote, urlencode


def build_register_uri(
    path: str,
    version: str,
    channel: str,
    platform: str,
    arch: str,
    mono: bool,
) -> str:
    query = urlencode(
        {
            "path": path,
            "version": version,
            "channel": channel,
            "platform": platform,
            "arch": arch,
            "mono": "true" if mono else "false",
        },
        quote_via=quote,
    )
    return f"blazium://register?{query}"


def _cli_name() -> str:
    return "blazium-cli.exe" if os.name == "nt" else "blazium-cli"


def _existing(path: str) -> Optional[str]:
    if path and os.path.isfile(path):
        return os.path.abspath(path)
    return None


def find_blazium_cli() -> Optional[str]:
    env_cli = os.environ.get("BLAZIUM_CLI", "").strip()
    found = _existing(env_cli)
    if found:
        return found

    name = _cli_name()
    candidates: List[str] = []

    blazium_root = os.environ.get("BLAZIUM", "").strip()
    if blazium_root:
        candidates.append(os.path.join(blazium_root, "bin", name))
        candidates.append(os.path.join(blazium_root, name))

    if os.name == "nt":
        pf = os.environ.get("ProgramFiles", "")
        local = os.environ.get("LOCALAPPDATA", "")
        if pf:
            candidates.append(os.path.join(pf, "Blazium", name))
        if local:
            candidates.append(os.path.join(local, "Blazium", name))
    else:
        candidates.append("/opt/blazium/bin/blazium-cli")
        candidates.append("/opt/blazium/blazium-cli")
        candidates.append("/usr/local/bin/blazium-cli")
        candidates.append("/usr/bin/blazium-cli")

    for c in candidates:
        found = _existing(c)
        if found:
            return found

    which = shutil.which("blazium-cli") or shutil.which("blazium-cli.exe")
    return _existing(which) if which else None


def derive_channel(build_name: str, external_status: str = "") -> str:
    blob = f"{build_name} {external_status}".lower()
    if "custom" in blob or "dev" in blob:
        return "custom"
    return "release"


def map_platform(platform: str) -> str:
    if platform == "linuxbsd":
        return "linux"
    return platform


def register_editor(
    editor_path: str,
    platform: str,
    arch: str,
    mono: bool,
    version: str,
    channel: str,
) -> int:
    editor_path = os.path.abspath(editor_path)
    if not os.path.isfile(editor_path):
        print(f'hub_register: editor binary not found: "{editor_path}" (skipping)', file=sys.stderr)
        return 0

    cli = find_blazium_cli()
    if not cli:
        print(
            "hub_register: blazium-cli not found (set BLAZIUM_CLI, BLAZIUM, or install Hub); skipping registration.",
            file=sys.stderr,
        )
        return 0

    uri = build_register_uri(
        path=editor_path,
        version=version,
        channel=channel,
        platform=map_platform(platform),
        arch=arch,
        mono=mono,
    )
    print(f'hub_register: registering via "{cli}" handle-uri')
    print(f"hub_register: {uri}")
    try:
        completed = subprocess.run([cli, "handle-uri", uri], check=False)
    except OSError as exc:
        print(f"hub_register: failed to run blazium-cli: {exc}", file=sys.stderr)
        return 0

    if completed.returncode != 0:
        print(
            f"hub_register: blazium-cli exited {completed.returncode} (build continues)",
            file=sys.stderr,
        )
        return 0

    print("hub_register: registration succeeded")
    return 0


def scons_post_action(target, source, env) -> None:
    """SCons AddPostAction callback (warn-only; never fails the build)."""
    editor = os.path.abspath(str(target[0]))
    vi = getattr(env, "version_info", None) or {}
    version = "{}.{}.{}".format(
        vi.get("external_major", 0),
        vi.get("external_minor", 0),
        vi.get("external_patch", 0),
    )
    channel = derive_channel(str(vi.get("build", "")), str(vi.get("external_status", "")))
    platform = str(env.get("platform", ""))
    arch = str(env.get("arch", ""))
    mono = bool(env.get("module_mono_enabled", False))
    register_editor(editor, platform, arch, mono, version, channel)


def main(argv: Optional[Iterable[str]] = None) -> int:
    args = list(sys.argv[1:] if argv is None else argv)
    if len(args) < 1:
        print(
            "Usage: hub_register_editor.py <editor-path> [platform] [arch] [mono] [version] [channel]",
            file=sys.stderr,
        )
        return 1
    editor = args[0]
    platform = args[1] if len(args) > 1 else ("windows" if os.name == "nt" else "linux")
    arch = args[2] if len(args) > 2 else ("x86_64" if sys.maxsize > 2**32 else "x86_32")
    mono = (args[3].lower() in ("1", "true", "yes")) if len(args) > 3 else False
    version = args[4] if len(args) > 4 else "0.0.0"
    channel = args[5] if len(args) > 5 else "release"
    return register_editor(editor, platform, arch, mono, version, channel)


if __name__ == "__main__":
    sys.exit(main())
