#!/usr/bin/env bash
# Build PS3 template with gc-sections enabled (rpcs3_compat=no) for RPCS3 LLVM jump tables.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BLAZIUM_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

echo "== PS3 LLVM release build (rpcs3_compat=no) =="
cd "$BLAZIUM_ROOT"
scons platform=ps3 target=template_release arch=ppc64 rpcs3_compat=no -j"$(nproc)"

echo "== Installing export templates =="
python3 "$SCRIPT_DIR/install_templates.py" --blazium-root "$BLAZIUM_ROOT"

EBOOT="$BLAZIUM_ROOT/bin/ps3_release.ppc64"
if [[ -f "$EBOOT" ]]; then
    SIZE=$(stat -c%s "$EBOOT" 2>/dev/null || stat -f%z "$EBOOT")
    echo "EBOOT size: $SIZE bytes"
    if [[ "$SIZE" -gt 15728640 ]]; then
        echo "WARN: EBOOT still > 15 MB — jump-table fallback may persist. Audit enabled modules."
    fi
else
    echo "error: $EBOOT not found" >&2
    exit 1
fi

echo "PASS: llvm_release template ready."
