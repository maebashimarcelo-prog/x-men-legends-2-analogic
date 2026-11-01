param([Parameter(Mandatory)][string]$ZigPath)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$ZigPath = (Resolve-Path -LiteralPath $ZigPath).Path
if ((& $ZigPath version) -ne '0.16.0') { throw 'Tests require Zig 0.16.0.' }
$testDir = Join-Path $root 'build/tests/unknown-build'
New-Item -ItemType Directory -Path $testDir -Force | Out-Null
$exe = Join-Path $testDir 'XMen2.exe'
$proxy = Join-Path $testDir 'DINPUT.dll'
$log = Join-Path $testDir 'test.log'
$previousCache = [Environment]::GetEnvironmentVariable('ZIG_GLOBAL_CACHE_DIR', 'Process')
$previousLog = [Environment]::GetEnvironmentVariable('XML2_ANALOG_LOG', 'Process')
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $root 'build/zig-cache'
$env:XML2_ANALOG_LOG = $log
try {
    & $ZigPath cc -target x86-windows-gnu -std=c11 -O2 -Wall -Wextra -Werror -o $exe `
        (Join-Path $PSScriptRoot 'controller_unknown_build_test.c') -ldxguid
    if ($LASTEXITCODE -ne 0) { throw 'Unknown-build fixture compilation failed.' }
    Copy-Item -LiteralPath (Join-Path $root 'build/modern-controller/DINPUT.dll') -Destination $proxy -Force
    Copy-Item -LiteralPath (Join-Path $root 'config/modern-controller.ini') -Destination (Join-Path $testDir 'true-analog.ini') -Force
    Set-Content -LiteralPath $log -Value ''
    & $exe $proxy
    if ($LASTEXITCODE -ne 0) { throw 'Unknown-build forwarding failed.' }
    $text = Get-Content -LiteralPath $log -Raw
    if ($text -notmatch 'TRACE REFUSED unsupported build; no IAT or COM changes' -or
        $text -match 'CORE ARMED|Hook: Installed|HOTPLUG MAIN REFRESH|Prompts.*ARMED|Glyph atlas LOADED') {
        throw 'Unknown build did not fail closed.'
    }
    Write-Output 'PASS: unsupported build refuses controller IAT, COM and gameplay changes.'
} finally {
    [Environment]::SetEnvironmentVariable('ZIG_GLOBAL_CACHE_DIR', $previousCache, 'Process')
    [Environment]::SetEnvironmentVariable('XML2_ANALOG_LOG', $previousLog, 'Process')
}
