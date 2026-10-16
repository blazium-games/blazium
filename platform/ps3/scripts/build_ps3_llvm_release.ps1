# Build PS3 LLVM-optimized template (rpcs3_compat=no) via WSL.
$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$BlaziumRoot = Resolve-Path (Join-Path $ScriptDir "..\..\..")
wsl -e bash -lc "cd /mnt/d/projects/console_support/blazium/platform/ps3/scripts && bash build_ps3_llvm_release.sh"
