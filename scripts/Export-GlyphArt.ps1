param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$assets = Join-Path $root 'assets/glyphs'
$built = Join-Path $root 'build/modern-controller/glyphs'
$review = Join-Path $root 'build/glyph-art-review'
$manifest = Get-Content -LiteralPath (Join-Path $assets 'manifest.json') -Raw | ConvertFrom-Json
if (-not (Test-Path -LiteralPath (Join-Path $built 'atlas.json'))) { throw 'Build controller glyphs first.' }
New-Item -ItemType Directory -Path $review -Force | Out-Null
$index = 0
foreach ($entry in $manifest.entries) {
    $folder = Join-Path $review $entry.style
    New-Item -ItemType Directory -Path $folder -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $assets $entry.source) -Destination $folder -Force
    $png = Join-Path $built ('{0:D2}.png' -f $index)
    Copy-Item -LiteralPath $png -Destination (Join-Path $folder ($entry.name + '.png')) -Force
    $index++
}
foreach ($name in 'atlas.png','atlas.json') {
    Copy-Item -LiteralPath (Join-Path $built $name) -Destination $review -Force
}
Copy-Item -LiteralPath (Join-Path $assets 'manifest.json') -Destination $review -Force
Copy-Item -LiteralPath (Join-Path $root 'LICENSE') -Destination $review -Force
@'
XML2 Modern Controller - original glyph art for review

48 original SVGs, 48 transparent PNG previews and the current 512x512 atlas.
Xbox and PlayStation are presentation styles; no manufacturer logos or copied
manufacturer artwork are included. The project art is MIT licensed.

For revisions, preserve each filename, the control association in manifest.json,
the SVG viewBox 0 0 64 64 and transparency. Prefer vector geometry; do not add
external fonts or linked assets. Keep an asymmetric L, R, arrow or triangle
upright. PNG previews are 56x56; the atlas uses 64px cells and 4px padding.
The editable source of truth is the SVG, not the packed atlas.

Runtime size, position and orientation are controlled separately by the code.
The glyph atlas does not contain any game asset.
'@ | Set-Content -LiteralPath (Join-Path $review 'README.txt') -Encoding utf8
$zip = Join-Path $root 'build/XML2-Glyph-Art-Review.zip'
Compress-Archive -LiteralPath (Join-Path $review 'xbox'),(Join-Path $review 'playstation'),
    (Join-Path $review 'atlas.png'),(Join-Path $review 'atlas.json'),(Join-Path $review 'manifest.json'),
    (Join-Path $review 'LICENSE'),(Join-Path $review 'README.txt') -DestinationPath $zip -Force
Write-Output "PASS: original glyph review package: $zip"
