# Recover PS3 export/deploy/launch pipeline after missing artifacts.
$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ScriptDir "..\..\..\..")
$EmulatorRoot = if ($env:EMULATOR_ROOT) { $env:EMULATOR_ROOT } else { Join-Path $RepoRoot "ps3_emulator" }
$TitleId = if ($env:TITLE_ID) { $env:TITLE_ID } else { "BLAZ00001" }
$SkipBuild = if ($env:SKIP_BUILD) { $env:SKIP_BUILD } else { "0" }

Write-Host "== PS3 recover deploy =="
Write-Host "Repo:     $RepoRoot"
Write-Host "Emulator: $EmulatorRoot"
Write-Host "Title ID: $TitleId"
Write-Host ""

if ($SkipBuild -ne "1") {
    wsl -e bash -lc "cd /mnt/d/projects/console_support/blazium/platform/ps3/scripts && bash recover_ps3_deploy.sh"
    exit $LASTEXITCODE
}

$exportDir = $null
$editorCandidates = @(
    (Join-Path $RepoRoot "blazium\bin\blazium.windows.editor.x86_64.exe"),
    (Join-Path $RepoRoot "blazium\bin\blazium.windows.editor.x86_64.console.exe")
)
$hasEditor = $false
foreach ($c in $editorCandidates) { if (Test-Path $c) { $hasEditor = $true; break } }

if ($hasEditor) {
    & (Join-Path $ScriptDir "export_ps3_headless.ps1")
    if ($LASTEXITCODE -eq 0) {
        $exportDir = Join-Path $RepoRoot "exported_game\ps3_export"
    }
}

if (-not $exportDir) {
    $exportCandidates = @(
        (Join-Path $RepoRoot "exported_game\ps3_export"),
        (Join-Path $RepoRoot "ps3_test\export\ps3")
    )
    foreach ($candidate in $exportCandidates) {
        $gameDir = Join-Path $candidate "$TitleId\USRDIR"
        if (Test-Path $gameDir) {
            $exportDir = $candidate
            break
        }
    }
}

if (-not $exportDir) {
    python (Join-Path $ScriptDir "assemble_ps3_export.py") `
        --export-dir (Join-Path $RepoRoot "exported_game\ps3_export") `
        --title-id $TitleId `
        --blazium-root (Join-Path $RepoRoot "blazium")
    $exportDir = Join-Path $RepoRoot "exported_game\ps3_export"
}

python (Join-Path $ScriptDir "validate_ps3_export.py") --game-dir (Join-Path $exportDir "$TitleId")
python (Join-Path $ScriptDir "deploy_rpcs3.py") `
    --export-dir $exportDir `
    --title-id $TitleId `
    --emulator-root $EmulatorRoot

Write-Host ""
Write-Host "Launch:"
Write-Host "  python $(Join-Path $ScriptDir 'launch_rpcs3.py') --emulator-root $EmulatorRoot --title-id $TitleId"
