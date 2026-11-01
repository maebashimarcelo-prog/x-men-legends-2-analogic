param([Parameter(Mandatory)][string]$ZigPath)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$ZigPath = (Resolve-Path -LiteralPath $ZigPath).Path
if ((& $ZigPath version) -ne '0.16.0') { throw 'Tests require Zig 0.16.0.' }
$output = Join-Path $root 'build/tests/controller-log-test.exe'
New-Item -ItemType Directory -Path (Split-Path $output) -Force | Out-Null
$previousCache = [Environment]::GetEnvironmentVariable('ZIG_GLOBAL_CACHE_DIR','Process')
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $root 'build/zig-cache'
try {
    & $ZigPath cc -target x86-windows-gnu -std=c11 -O2 -Wall -Wextra -Werror `
        -I (Join-Path $root 'include') -I (Join-Path $root 'build/modern-controller/glyphs') `
        -o $output (Join-Path $PSScriptRoot 'controller_log_test.c') `
        (Join-Path $root 'src/controller_model.c') (Join-Path $root 'src/controller_prompts.c') `
        (Join-Path $root 'src/prompt_model.c') (Join-Path $root 'src/prompt_render.c') `
        (Join-Path $root 'src/advanced_menu.c') (Join-Path $root 'src/controller_forward.S') `
        (Join-Path $root 'src/compat.c') (Join-Path $root 'src/controller_analog.c') `
        (Join-Path $root 'src/analog_hook.c') (Join-Path $root 'src/analog_gate.S') -ladvapi32 -ldxguid
    if ($LASTEXITCODE -ne 0) { throw 'Release log fixture build failed.' }
} finally { [Environment]::SetEnvironmentVariable('ZIG_GLOBAL_CACHE_DIR',$previousCache,'Process') }
& $output
if ($LASTEXITCODE -ne 0) { throw 'Release log fixture failed.' }
$version = (Get-Content -LiteralPath (Join-Path $root 'VERSION') -Raw).Trim()
$dll = Join-Path $root 'build/modern-controller/DINPUT.dll'
$metadata = Get-Content -LiteralPath (Join-Path $root 'build/modern-controller/build-info.json') -Raw | ConvertFrom-Json
$binary = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($dll))
if ($metadata.version -cne $version -or $metadata.name -cne 'XML2 Modern Controller' -or
    (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash -ine $metadata.dll_sha256 -or
    -not $binary.Contains("XML2 Modern Controller v$version")) { throw 'Release identity mismatch.' }
$ini = Get-Content -LiteralPath (Join-Path $root 'config/modern-controller.ini') -Raw
foreach ($line in @('TrueAnalogMovement=1','Deadzone=0.15','PreserveMagnitude=1','DebugInput=0','HotPlug=1','DynamicPrompts=1','ControllerGlyphs=1','AdvancedController=1','ControllerType=Auto','GlyphStyle=Auto','AdvancedTrace=0','PromptTrace=0','ControllerTrace=0','RestoreConsoleDefaults=0')) {
    if ($ini -notmatch ('(?m)^'+[regex]::Escape($line)+'\s*$')) { throw "Release default missing: $line" }
}
if ($ini -match '(?m)^(AdvancedTrace|PromptTrace|ControllerTrace|DebugInput)=1\s*$') { throw 'Release diagnostics enabled.' }
$configOutput = Join-Path $root 'build/tests/config/controller-config-test.exe'
if (-not (Test-Path -LiteralPath $configOutput)) { throw 'Run Test-Controller first.' }
Copy-Item -LiteralPath (Join-Path $root 'config/modern-controller.ini') -Destination (Join-Path (Split-Path $configOutput) 'true-analog.ini') -Force
& $configOutput 0
if ($LASTEXITCODE -ne 0) { throw 'Actual analog reader rejected public defaults.' }
Write-Output 'PASS: release version/DLL metadata, public defaults and actual reader compatibility.'
