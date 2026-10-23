# Headless PS3 export for ps3_test via Blazium editor CLI.
$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ScriptDir "..\..\..\..")
$BlaziumRoot = Join-Path $RepoRoot "blazium"
$ProjectDir = Join-Path $RepoRoot "ps3_test"
$ExportPath = Join-Path $RepoRoot "exported_game\ps3_export.bin"
$Preset = if ($env:PS3_EXPORT_PRESET) { $env:PS3_EXPORT_PRESET } else { "PS3" }

$editorCandidates = @(
    (Join-Path $BlaziumRoot "bin\blazium.windows.editor.x86_64.exe"),
    (Join-Path $BlaziumRoot "bin\blazium.windows.editor.x86_64.console.exe")
)

$Editor = $null
foreach ($candidate in $editorCandidates) {
    if (Test-Path $candidate) {
        $Editor = $candidate
        break
    }
}

if (-not $Editor) {
    Write-Host "error: Windows editor not found in blazium/bin/" -ForegroundColor Red
    Write-Host "Build with: py -m SCons platform=windows target=editor -j8"
    exit 1
}

if (-not (Test-Path $ProjectDir)) {
    Write-Host "error: project not found: $ProjectDir" -ForegroundColor Red
    exit 1
}

$exportParent = Split-Path $ExportPath -Parent
if (-not (Test-Path $exportParent)) {
    New-Item -ItemType Directory -Path $exportParent -Force | Out-Null
}

Write-Host "== PS3 headless export =="
Write-Host "Editor:  $Editor"
Write-Host "Project: $ProjectDir"
Write-Host "Preset:  $Preset"
Write-Host "Output:  $ExportPath"
Write-Host ""

& $Editor --headless --path $ProjectDir --export-release $Preset $ExportPath | Out-Host
$gameDir = Join-Path $RepoRoot "exported_game\ps3_export\BLAZ00001"
if (-not (Test-Path (Join-Path $gameDir "USRDIR\EBOOT.BIN"))) {
    Write-Host "error: export did not produce $gameDir\USRDIR\EBOOT.BIN" -ForegroundColor Red
    exit 1
}

Write-Host "PASS: exported to $gameDir"
exit 0
