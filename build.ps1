param([string]$ZigPath)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
if (-not $ZigPath) {
    if ($env:ZIG_EXE) { $ZigPath = $env:ZIG_EXE }
    else {
        $command = Get-Command zig -ErrorAction SilentlyContinue
        if (-not $command) { throw 'Install Zig 0.16.0 and put zig on PATH, or pass -ZigPath.' }
        $ZigPath = $command.Source
    }
}
$ZigPath = (Resolve-Path -LiteralPath $ZigPath).Path
if ((& $ZigPath version) -ne '0.16.0') { throw 'This release requires Zig 0.16.0.' }
$output = Join-Path $root 'build/DINPUT.dll'
New-Item -ItemType Directory -Path (Split-Path $output) -Force | Out-Null

if (-not $env:ZIG_GLOBAL_CACHE_DIR) {
    $env:ZIG_GLOBAL_CACHE_DIR = Join-Path $root 'build/zig-cache'
}
New-Item -ItemType Directory -Path $env:ZIG_GLOBAL_CACHE_DIR -Force | Out-Null
Write-Output "Zig cache: $env:ZIG_GLOBAL_CACHE_DIR"

try {
    & $ZigPath cc -target x86-windows-gnu -std=c11 -O2 -Wall -Wextra -Werror `
        -I (Join-Path $root 'include') -shared -o $output `
        (Join-Path $root 'src/dinput_proxy.c') `
        (Join-Path $root 'src/compat.c') `
        (Join-Path $root 'src/analog_probe.c') `
        (Join-Path $root 'src/analog_hook.c') `
        (Join-Path $root 'src/analog_gate.S') `
        (Join-Path $root 'src/dinput_proxy.def') -ladvapi32
    if ($LASTEXITCODE -ne 0) { throw 'Zig compilation failed.' }

} finally {
    if (Get-Variable -Name previousCache -ErrorAction SilentlyContinue) {
        [Environment]::SetEnvironmentVariable(
            'ZIG_GLOBAL_CACHE_DIR',
            $previousCache,
            'Process'
        )
    }
}

& (Join-Path $root 'scripts/Inspect-Proxy.ps1') -Path $output
if (-not $?) { throw 'PE inspection failed.' }
$hash = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash
Write-Output "PASS: PE32 x86 DINPUT.dll SHA-256 $hash"
