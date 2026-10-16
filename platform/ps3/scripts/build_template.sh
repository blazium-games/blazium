#!/usr/bin/env bash
set -euo pipefail
export PS3DEV="${PS3DEV:-/usr/local/ps3dev}"
export PSL1GHT="${PSL1GHT:-$PS3DEV}"
export PATH="$PS3DEV/ppu/bin:$PS3DEV/spu/bin:$PS3DEV/bin:$PATH"
cd /mnt/d/projects/console_support/blazium
TARGET="${1:-template_release}"
scons platform=ps3 target="$TARGET" arch=ppc64 module_ps3_module_enabled=yes -j"$(nproc --all 2>/dev/null || echo 4)"
