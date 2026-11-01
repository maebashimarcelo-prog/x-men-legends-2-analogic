param([Parameter(Mandatory)][string]$ZigPath)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$ZigPath = (Resolve-Path -LiteralPath $ZigPath).Path
if ((& $ZigPath version) -ne '0.16.0') { throw 'Tests require Zig 0.16.0.' }
$output = Join-Path $root 'build/tests/controller-forward-test.exe'
New-Item -ItemType Directory -Path (Split-Path $output) -Force | Out-Null
$previousCache = [Environment]::GetEnvironmentVariable('ZIG_GLOBAL_CACHE_DIR', 'Process')
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $root 'build/zig-cache'
try {
    & $ZigPath cc -target x86-windows-gnu -std=c11 -O2 -Wall -Wextra -Werror -o $output `
        (Join-Path $PSScriptRoot 'controller_forward_test.c') `
        (Join-Path $PSScriptRoot 'controller_invoke.S') `
        (Join-Path $root 'src/controller_forward.S')
    if ($LASTEXITCODE -ne 0) { throw 'Forwarding test build failed.' }
    $modelOutput = Join-Path $root 'build/tests/controller-model-test.exe'
    & $ZigPath cc -target x86-windows-gnu -std=c11 -O2 -Wall -Wextra -Werror `
        -I (Join-Path $root 'include') -o $modelOutput `
        (Join-Path $PSScriptRoot 'controller_model_test.c') `
        (Join-Path $root 'src/controller_model.c')
    if ($LASTEXITCODE -ne 0) { throw 'Model test build failed.' }
    $configDir = Join-Path $root 'build/tests/config'
    New-Item -ItemType Directory -Path $configDir -Force | Out-Null
    $configOutput = Join-Path $configDir 'controller-config-test.exe'
    & $ZigPath cc -target x86-windows-gnu -std=c11 -O2 -Wall -Wextra -Werror `
        -I (Join-Path $root 'include') -o $configOutput `
        (Join-Path $PSScriptRoot 'controller_config_test.c')
    if ($LASTEXITCODE -ne 0) { throw 'Configuration test build failed.' }
} finally {
    [Environment]::SetEnvironmentVariable('ZIG_GLOBAL_CACHE_DIR', $previousCache, 'Process')
}
& $output
if ($LASTEXITCODE -ne 0) { throw 'Forwarding test failed.' }
& $modelOutput
if ($LASTEXITCODE -ne 0) { throw 'Model test failed.' }
$configIni = Join-Path $configDir 'true-analog.ini'
Copy-Item -LiteralPath (Join-Path $root 'config/true-analog.ini') -Destination $configIni -Force
& $configOutput 0
if ($LASTEXITCODE -ne 0) { throw 'Legacy configuration compatibility failed.' }
Add-Content -LiteralPath $configIni -Value @'

[General]
DebugInput=1
[Player1]
TrueAnalogMovement=0
ControllerIndex=3
Deadzone=0.25
'@
& $configOutput 1
if ($LASTEXITCODE -ne 0) { throw 'Player1 configuration overrides failed.' }
$invalid = (Get-Content -LiteralPath $configIni -Raw).Replace('ControllerIndex=3', 'ControllerIndex=10')
Set-Content -LiteralPath $configIni -Value $invalid
& $configOutput 2
if ($LASTEXITCODE -ne 0) { throw 'Invalid configuration refusal failed.' }
