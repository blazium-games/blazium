#!/usr/bin/env bash
# Recover PS3 export/deploy/launch pipeline after missing artifacts.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
BLAZIUM_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
EMULATOR_ROOT="${EMULATOR_ROOT:-$REPO_ROOT/ps3_emulator}"
TITLE_ID="${TITLE_ID:-BLAZ00001}"
SKIP_BUILD="${SKIP_BUILD:-0}"

EXPORT_CANDIDATES=(
    "$REPO_ROOT/exported_game/ps3_export"
    "$REPO_ROOT/ps3_test/export/ps3"
)

echo "== PS3 recover deploy =="
echo "Repo:      $REPO_ROOT"
echo "Blazium:   $BLAZIUM_ROOT"
echo "Emulator:  $EMULATOR_ROOT"
echo "Title ID:  $TITLE_ID"
echo ""

if [[ "$SKIP_BUILD" != "1" ]]; then
    bash "$SCRIPT_DIR/validate_toolchain.sh"
    bash "$SCRIPT_DIR/build_ps3_llvm_release.sh"
    python3 "$SCRIPT_DIR/install_templates.py" --blazium-root "$BLAZIUM_ROOT"
else
    echo "SKIP_BUILD=1 — using existing template"
fi

# Install templates for Windows editor export (APPDATA path).
python3 "$SCRIPT_DIR/install_templates.py" --blazium-root "$BLAZIUM_ROOT" 2>/dev/null || true

EXPORT_DIR=""
if [[ -f "$BLAZIUM_ROOT/bin/blazium.windows.editor.x86_64.exe" ]] || \
   [[ -f "$BLAZIUM_ROOT/bin/blazium.windows.editor.x86_64.console.exe" ]]; then
    echo "Attempting headless editor export..."
    if bash "$SCRIPT_DIR/export_ps3_headless.sh"; then
        EXPORT_DIR="$REPO_ROOT/exported_game/ps3_export"
    else
        echo "Headless export failed; falling back to assemble."
    fi
fi

if [[ -z "$EXPORT_DIR" ]]; then
    for candidate in "${EXPORT_CANDIDATES[@]}"; do
        if [[ -d "$candidate/$TITLE_ID/USRDIR" ]]; then
            EXPORT_DIR="$candidate"
            break
        fi
    done
fi

if [[ -z "$EXPORT_DIR" ]]; then
    echo "No export found; assembling from template..."
    python3 "$SCRIPT_DIR/assemble_ps3_export.py" \
        --export-dir "$REPO_ROOT/exported_game/ps3_export" \
        --title-id "$TITLE_ID" \
        --blazium-root "$BLAZIUM_ROOT"
    EXPORT_DIR="$REPO_ROOT/exported_game/ps3_export"
else
    # Headless export already has real game.pck — only refresh EBOOT from template.
    GAME_PCK="$EXPORT_DIR/$TITLE_ID/USRDIR/game.pck"
    ASSEMBLE_ARGS=(
        --export-dir "$EXPORT_DIR"
        --title-id "$TITLE_ID"
        --blazium-root "$BLAZIUM_ROOT"
    )
    if [[ -f "$GAME_PCK" ]]; then
        ASSEMBLE_ARGS+=(--game-pck "$GAME_PCK")
    fi
    echo "Refreshing EBOOT from latest template..."
    python3 "$SCRIPT_DIR/assemble_ps3_export.py" "${ASSEMBLE_ARGS[@]}"
fi

python3 "$SCRIPT_DIR/validate_ps3_export.py" --game-dir "$EXPORT_DIR/$TITLE_ID"
python3 "$SCRIPT_DIR/deploy_rpcs3.py" \
    --export-dir "$EXPORT_DIR" \
    --title-id "$TITLE_ID" \
    --emulator-root "$EMULATOR_ROOT"

echo ""
echo "Launch:"
echo "  python3 $SCRIPT_DIR/launch_rpcs3.py --emulator-root $EMULATOR_ROOT --title-id $TITLE_ID"
echo "Monitor:"
echo "  python3 $SCRIPT_DIR/monitor_rpcs3_llvm.py --interval 30"
