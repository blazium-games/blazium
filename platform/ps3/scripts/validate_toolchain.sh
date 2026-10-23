#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=setup_ps3dev_env.sh
source "$SCRIPT_DIR/setup_ps3dev_env.sh"

MIN_GCC_MAJOR=9

GCC_VERSION_LINE="$(ppu-gcc --version | head -1)"
echo "$GCC_VERSION_LINE"

GCC_MAJOR="$(echo "$GCC_VERSION_LINE" | sed -nE 's/.*GCC\) ([0-9]+)\.[0-9]+.*/\1/p')"
if [ -z "$GCC_MAJOR" ] || [ "$GCC_MAJOR" -lt "$MIN_GCC_MAJOR" ]; then
    echo "error: ppu-gcc must be GCC ${MIN_GCC_MAJOR}+ (found: ${GCC_VERSION_LINE})" >&2
    echo "hint: build ps3toolchain fork (GCC 9+) — see ps3toolchain/build-ppu-blazium.sh" >&2
    exit 1
fi

LV2_LD=""
for candidate in \
    "$PSL1GHT/ppu/lib/lv2.ld" \
    "$PSL1GHT/ppu/powerpc64-ps3-elf/lib/lv2.ld"; do
    if [ -f "$candidate" ]; then
        LV2_LD="$candidate"
        break
    fi
done

if [ -z "$LV2_LD" ]; then
    echo "error: lv2.ld not found under $PSL1GHT" >&2
    exit 1
fi

echo "lv2.ld OK ($LV2_LD)"
echo "toolchain OK (GCC ${GCC_MAJOR}+)"
