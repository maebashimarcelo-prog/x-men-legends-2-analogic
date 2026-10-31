param([string]$ZigPath,[string]$PythonPath,[string]$ResvgPath)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$version = (Get-Content -LiteralPath (Join-Path $root 'VERSION') -Raw).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+-beta\.\d+$') { throw 'Invalid beta VERSION.' }
New-Item -ItemType Directory -Path (Join-Path $root 'build/modern-controller') -Force | Out-Null
Set-Content -LiteralPath (Join-Path $root 'build/modern-controller/project_version.h') `
    -Value ('#define XMC_VERSION "' + $version + '"') -Encoding ascii
if (-not $PythonPath) { $PythonPath = (Get-Command python -ErrorAction Stop).Source }
if (-not $ResvgPath) { $ResvgPath = (Get-Command resvg -ErrorAction Stop).Source }
& $PythonPath (Join-Path $root 'scripts/build-glyphs.py') --resvg $ResvgPath
if ($LASTEXITCODE -ne 0) { throw 'Owned glyph atlas build failed.' }
if (-not $ZigPath) {
    if ($env:ZIG_EXE) { $ZigPath = $env:ZIG_EXE }
    else {
        $command = Get-Command zig -ErrorAction SilentlyContinue
        if (-not $command) { throw 'Install Zig 0.16.0 and put zig on PATH, or pass -ZigPath.' }
        $ZigPath = $command.Source
    }
}
$ZigPath = (Resolve-Path -LiteralPath $ZigPath).Path
if ((& $ZigPath version) -ne '0.16.0') { throw 'This build requires Zig 0.16.0.' }
$output = Join-Path $root 'build/modern-controller/DINPUT.dll'
New-Item -ItemType Directory -Path (Split-Path $output) -Force | Out-Null
$previousCache = [Environment]::GetEnvironmentVariable('ZIG_GLOBAL_CACHE_DIR', 'Process')
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $root 'build/zig-cache'
try {
    & $ZigPath cc -target x86-windows-gnu -std=c11 -O2 -Wall -Wextra -Werror -DXML2_MODERN_CONTROLLER `
        -s `
        -I (Join-Path $root 'build/modern-controller') `
        -I (Join-Path $root 'include') -shared -o $output `
        -I (Join-Path $root 'build/modern-controller/glyphs') `
        (Join-Path $root 'src/controller_proxy.c') `
        (Join-Path $root 'src/controller_trace.c') `
        (Join-Path $root 'src/controller_model.c') `
        (Join-Path $root 'src/controller_prompts.c') `
        (Join-Path $root 'src/advanced_menu.c') `
        (Join-Path $root 'src/prompt_model.c') `
        (Join-Path $root 'src/prompt_render.c') `
        (Join-Path $root 'src/controller_forward.S') `
        (Join-Path $root 'src/compat.c') `
        (Join-Path $root 'src/controller_analog.c') `
        (Join-Path $root 'src/analog_hook.c') `
        (Join-Path $root 'src/analog_gate.S') `
        (Join-Path $root 'src/dinput_proxy.def') -ladvapi32 -ldxguid
    if ($LASTEXITCODE -ne 0) { throw 'Zig compilation failed.' }
} finally {
    [Environment]::SetEnvironmentVariable('ZIG_GLOBAL_CACHE_DIR', $previousCache, 'Process')
}
& (Join-Path $root 'scripts/Inspect-Proxy.ps1') -Path $output
if (-not $?) { throw 'PE inspection failed.' }
Write-Output "PASS: modern controller DLL SHA-256 $((Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash)"
$atlas = Get-Content -LiteralPath (Join-Path $root 'build/modern-controller/glyphs/atlas.json') -Raw | ConvertFrom-Json
[ordered]@{name='XML2 Modern Controller'; version=$version; format='PE32 x86'; toolchain='Zig 0.16.0';
    resvg='0.47.0'; dll_sha256=(Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash;
    atlas_bgra_sha256=$atlas.raw_bgra_sha256} | ConvertTo-Json | Set-Content `
    -LiteralPath (Join-Path $root 'build/modern-controller/build-info.json') -Encoding utf8
