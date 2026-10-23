#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BLAZIUM_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
# shellcheck source=setup_ps3dev_env.sh
source "$SCRIPT_DIR/setup_ps3dev_env.sh"

echo "== PS3 smoke test =="
bash "$SCRIPT_DIR/validate_toolchain.sh"

cd "$BLAZIUM_ROOT"
if [[ "${SKIP_BUILD:-0}" != "1" ]]; then
    echo "== Building PS3 LLVM release template (rpcs3_compat=no) =="
    bash "$SCRIPT_DIR/build_ps3_llvm_release.sh"

    echo "== Installing export templates =="
    python3 "$SCRIPT_DIR/install_templates.py" --blazium-root "$BLAZIUM_ROOT"
else
    echo "SKIP_BUILD=1 — using existing template"
fi

TEST_ROOT="/mnt/d/projects/console_support/ps3_test"
EXPORT_CANDIDATES=(
    "/mnt/d/projects/console_support/exported_game/ps3_export"
    "$TEST_ROOT/export/ps3"
)

EXPORT_DIR=""
for candidate in "${EXPORT_CANDIDATES[@]}"; do
    if [ -d "$candidate/BLAZ00001" ]; then
        EXPORT_DIR="$candidate"
        break
    fi
done

if [ -n "$EXPORT_DIR" ]; then
    echo "== Deploying test export to RPCS3 from $EXPORT_DIR =="
    python3 "$SCRIPT_DIR/deploy_rpcs3.py" \
        --export-dir "$EXPORT_DIR" \
        --title-id BLAZ00001 \
        --emulator-root "/mnt/d/projects/console_support/ps3_emulator"
else
    echo "note: export ps3_test first (Project -> Export -> PS3) or run recover_ps3_deploy.sh"
fi

echo ""
echo "RPCS3 validation:"
echo "  1. Enable Debug Console Mode in RPCS3"
echo "  2. Launch dev_hdd0/game/BLAZ00001/USRDIR/EBOOT.BIN"
echo "  3. Confirm TTY shows PS3 boot logs and pack loading"
