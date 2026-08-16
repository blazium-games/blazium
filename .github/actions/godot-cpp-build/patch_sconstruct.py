#!/usr/bin/env python3
"""Patch a checked-out godot-cpp SConstruct to use SCons TEMPFILE.

godot-cpp ignores unknown command-line variables such as ARCOM, so the
archive/link commands must be wrapped in the SConstruct itself. Blazium's
extra ClassDB types generate enough objects that `ar`/`ld` otherwise fail
with "Argument list too long".
"""

import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: patch_sconstruct.py <godot-cpp-dir>", file=sys.stderr)
        return 2

    path = Path(sys.argv[1]) / "SConstruct"
    text = path.read_text(encoding="utf-8")
    marker = 'Return("env")'
    insert = """
def _blazium_tempfile(env, key, fallback):
    cmd = str(env.get(key, fallback))
    if "TEMPFILE" not in cmd:
        env[key] = "${TEMPFILE(%r)}" % cmd
_blazium_tempfile(env, "ARCOM", "$AR $ARFLAGS $TARGET $SOURCES")
_blazium_tempfile(env, "SHLINKCOM", "$SHLINK -o $TARGET $SHLINKFLAGS $SOURCES $_LIBDIRFLAGS $_LIBFLAGS")
_blazium_tempfile(env, "LINKCOM", "$LINK -o $TARGET $LINKFLAGS $SOURCES $_LIBDIRFLAGS $_LIBFLAGS")
Return("env")
"""
    if "def _blazium_tempfile" in text:
        print("already patched", path)
        return 0
    if marker not in text:
        print("Return(env) not found in", path, file=sys.stderr)
        return 1
    path.write_text(text.replace(marker, insert, 1), encoding="utf-8")
    print("patched", path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
