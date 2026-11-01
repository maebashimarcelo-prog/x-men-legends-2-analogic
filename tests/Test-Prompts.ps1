param([Parameter(Mandatory)][string]$ZigPath)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$ZigPath = (Resolve-Path -LiteralPath $ZigPath).Path
$output = Join-Path $root 'build/tests/prompt-model-test.exe'
New-Item -ItemType Directory -Path (Split-Path $output) -Force | Out-Null
$previousCache = [Environment]::GetEnvironmentVariable('ZIG_GLOBAL_CACHE_DIR', 'Process')
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $root 'build/zig-cache'
try {
    & $ZigPath cc -target x86-windows-gnu -std=c11 -O2 -Wall -Wextra -Werror `
        -I (Join-Path $root 'include') -o $output `
        (Join-Path $PSScriptRoot 'prompt_model_test.c') (Join-Path $root 'src/prompt_model.c')
    if ($LASTEXITCODE -ne 0) { throw 'Prompt model test build failed.' }
    $renderOutput = Join-Path $root 'build/tests/prompt-render-test.exe'
    & $ZigPath cc -target x86-windows-gnu -std=c11 -O2 -Wall -Wextra -Werror `
        -I (Join-Path $root 'include') -I (Join-Path $root 'build/modern-controller/glyphs') -o $renderOutput `
        (Join-Path $PSScriptRoot 'prompt_render_test.c') (Join-Path $root 'src/prompt_model.c')
    if ($LASTEXITCODE -ne 0) { throw 'Prompt render test build failed.' }
    $tutorialOutput = Join-Path $root 'build/tests/tutorial-gate-test.exe'
    & $ZigPath cc -target x86-windows-gnu -std=c11 -O2 -Wall -Wextra -Werror `
        -I (Join-Path $root 'include') -I (Join-Path $root 'build/modern-controller/glyphs') -o $tutorialOutput `
        (Join-Path $PSScriptRoot 'tutorial_gate_test.c') (Join-Path $root 'src/prompt_model.c') `
        (Join-Path $root 'src/prompt_render.c')
    if ($LASTEXITCODE -ne 0) { throw 'Tutorial gate ABI test build failed.' }
} finally { [Environment]::SetEnvironmentVariable('ZIG_GLOBAL_CACHE_DIR', $previousCache, 'Process') }
& $output
if ($LASTEXITCODE -ne 0) { throw 'Prompt model test failed.' }
& $renderOutput
if ($LASTEXITCODE -ne 0) { throw 'Prompt render test failed.' }
& $tutorialOutput
if ($LASTEXITCODE -ne 0) { throw 'Tutorial gate ABI test failed.' }
