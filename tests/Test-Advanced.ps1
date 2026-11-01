param([Parameter(Mandatory)][string]$ZigPath)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$ZigPath = (Resolve-Path -LiteralPath $ZigPath).Path
if ((& $ZigPath version) -ne '0.16.0') { throw 'Tests require Zig 0.16.0.' }
$output = Join-Path $root 'build/tests/advanced-menu-test.exe'
New-Item -ItemType Directory -Path (Split-Path $output) -Force | Out-Null
$previousCache = [Environment]::GetEnvironmentVariable('ZIG_GLOBAL_CACHE_DIR','Process')
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $root 'build/zig-cache'
try {
    & $ZigPath cc -target x86-windows-gnu -std=c11 -O2 -Wall -Wextra -Werror `
        -I (Join-Path $root 'include') -o $output `
        (Join-Path $PSScriptRoot 'advanced_menu_test.c') (Join-Path $root 'src/controller_model.c')
    if ($LASTEXITCODE -ne 0) { throw 'Advanced fixture build failed.' }
} finally { [Environment]::SetEnvironmentVariable('ZIG_GLOBAL_CACHE_DIR',$previousCache,'Process') }
& $output
if ($LASTEXITCODE -ne 0) { throw 'Advanced fixture failed.' }
