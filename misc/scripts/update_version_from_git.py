#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Update version.py external_* fields from local git tags for local nightly builds.

Computes Blazium version as: nearest ``v*`` tag's major.minor.patch, plus the number
of commits after that tag on HEAD (same idea as CI nightlies / issue #393).

Usage (from repository root)::

    python misc/scripts/update_version_from_git.py
    # then: scons ...

    python misc/scripts/update_version_from_git.py --dry-run
    python misc/scripts/update_version_from_git.py --status nightly

Leaves version.py uncommitted on purpose; do not check the modified file in.
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys


def find_repo_root(start: str) -> str:
    path = os.path.abspath(start)
    while True:
        if os.path.isfile(os.path.join(path, "version.py")) and (
            os.path.isdir(os.path.join(path, ".git")) or os.path.isfile(os.path.join(path, ".git"))
        ):
            return path
        parent = os.path.dirname(path)
        if parent == path:
            raise SystemExit("Error: could not find repository root (version.py + .git).")
        path = parent


def git(repo: str, *args: str) -> str:
    try:
        out = subprocess.check_output(
            ["git", *args],
            cwd=repo,
            stderr=subprocess.STDOUT,
            text=True,
        )
    except FileNotFoundError as exc:
        raise SystemExit("Error: git is not available on PATH.") from exc
    except subprocess.CalledProcessError as exc:
        detail = (exc.output or "").strip()
        raise SystemExit(f"Error: git {' '.join(args)} failed: {detail or exc}") from exc
    return out.strip()


def compute_version(repo: str, status_override: str | None) -> dict[str, str | int]:
    tag = git(repo, "describe", "--tags", "--match", "v[0-9]*", "--abbrev=0", "HEAD")
    commits_s = git(repo, "rev-list", "--count", f"{tag}..HEAD")
    sha = git(repo, "rev-parse", "HEAD")

    try:
        commits = int(commits_s)
    except ValueError as exc:
        raise SystemExit(f"Error: invalid commit count from git: {commits_s!r}") from exc

    # v0.6.737-nightly -> base 0.6.737, status nightly
    m = re.fullmatch(r"v(\d+)\.(\d+)\.(\d+)(?:-(.+))?", tag)
    if not m:
        raise SystemExit(f"Error: unsupported tag format: {tag!r} (expected vMAJOR.MINOR.PATCH[-status])")

    major = int(m.group(1))
    minor = int(m.group(2))
    patch = int(m.group(3)) + commits
    status = status_override if status_override is not None else (m.group(4) or "nightly")

    return {
        "tag": tag,
        "commits_after_tag": commits,
        "external_major": major,
        "external_minor": minor,
        "external_patch": patch,
        "external_status": status,
        "external_sha": sha,
    }


def update_version_py(path: str, version: dict[str, str | int]) -> None:
    with open(path, "r", encoding="utf-8", newline="") as f:
        text = f.read()

    replacements = {
        "external_major": f"external_major = {version['external_major']}",
        "external_minor": f"external_minor = {version['external_minor']}",
        "external_patch": f"external_patch = {version['external_patch']}",
        "external_status": f'external_status = "{version["external_status"]}"',
        "external_sha": f'external_sha = "{version["external_sha"]}"',
    }

    lines = text.splitlines(keepends=True)
    seen = set()
    out = []
    for line in lines:
        stripped = line.lstrip()
        replaced = False
        for key, new_line in replacements.items():
            if stripped.startswith(f"{key} ="):
                eol = "\r\n" if line.endswith("\r\n") else "\n" if line.endswith("\n") else ""
                out.append(new_line + eol)
                seen.add(key)
                replaced = True
                break
        if not replaced:
            out.append(line)

    missing = [k for k in replacements if k not in seen]
    if missing:
        raise SystemExit(f"Error: version.py missing fields: {', '.join(missing)}")

    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write("".join(out))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Update version.py external_* from local git tags.")
    parser.add_argument(
        "--dry-run",
        "--print-only",
        action="store_true",
        dest="dry_run",
        help="Print computed values without writing version.py",
    )
    parser.add_argument(
        "--status",
        default=None,
        help="Override external_status (default: tag suffix, or 'nightly')",
    )
    args = parser.parse_args(argv)

    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo = find_repo_root(os.path.join(script_dir, "..", ".."))
    version_path = os.path.join(repo, "version.py")

    version = compute_version(repo, args.status)
    print(f"Tag: {version['tag']} (+{version['commits_after_tag']} commits)")
    print(
        f"Version: {version['external_major']}.{version['external_minor']}.{version['external_patch']}"
        f"-{version['external_status']}"
    )
    print(f"SHA: {version['external_sha']}")

    if args.dry_run:
        print("Dry run: version.py not modified.")
        return 0

    update_version_py(version_path, version)
    print(f"Updated {version_path}")
    print("Leave version.py uncommitted for local builds; restore with: git checkout -- version.py")
    return 0


if __name__ == "__main__":
    sys.exit(main())
