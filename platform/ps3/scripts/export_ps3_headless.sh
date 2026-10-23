#!/usr/bin/env bash
# Headless PS3 export for ps3_test (Windows editor via WSL path or native).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
BLAZIUM_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
PROJECT_DIR="${PROJECT_DIR:-$REPO_ROOT/ps3_test}"
EXPORT_PATH="${EXPORT_PATH:-$REPO_ROOT/exported_game/ps3_export.bin}"
PRESET="${PS3_EXPORT_PRESET:-PS3}"

EDITOR_CANDIDATES=(
    "$BLAZIUM_ROOT/bin/blazium.windows.editor.x86_64.exe"
    "$BLAZIUM_ROOT/bin/blazium.windows.editor.x86_64.console.exe"
)

EDITOR=""
for candidate in "${EDITOR_CANDIDATES[@]}"; do
    if [[ -f "$candidate" ]]; then
        EDITOR="$candidate"
        break
    fi
done

if [[ -z "$EDITOR" ]]; then
    echo "error: Windows editor not found in blazium/bin/" >&2
    echo "Build with: py -m SCons platform=windows target=editor -j8" >&2
    exit 1
fi

if [[ ! -d "$PROJECT_DIR" ]]; then
    echo "error: project not found: $PROJECT_DIR" >&2
    exit 1
fi

mkdir -p "$(dirname "$EXPORT_PATH")"

echo "== PS3 headless export =="
echo "Editor:  $EDITOR"
echo "Project: $PROJECT_DIR"
echo "Preset:  $PRESET"
echo "Output:  $EXPORT_PATH"
echo ""

if command -v cmd.exe >/dev/null 2>&1; then
    WIN_EDITOR=$(wslpath -w "$EDITOR")
    WIN_PROJECT=$(wslpath -w "$PROJECT_DIR")
    WIN_EXPORT=$(wslpath -w "$EXPORT_PATH")
    cmd.exe /c "\"$WIN_EDITOR\" --headless --path \"$WIN_PROJECT\" --export-release $PRESET \"$WIN_EXPORT\""
else
    "$EDITOR" --headless --path "$PROJECT_DIR" --export-release "$PRESET" "$EXPORT_PATH"
fi

GAME_DIR="$REPO_ROOT/exported_game/ps3_export/BLAZ00001"
if [[ ! -f "$GAME_DIR/USRDIR/EBOOT.BIN" ]]; then
    echo "error: export did not produce $GAME_DIR/USRDIR/EBOOT.BIN" >&2
    exit 1
fi

echo "PASS: exported to $GAME_DIR"
