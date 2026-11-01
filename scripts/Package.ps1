[CmdletBinding()]
param([switch]$Source)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction SilentlyContinue
$root = Split-Path -Parent $PSScriptRoot
$version = (Get-Content -LiteralPath (Join-Path $root 'VERSION') -Raw).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+-beta\.\d+$') { throw 'Invalid beta VERSION.' }
$name = "XML2-Modern-Controller-v$version"
if ($Source) { $name += '-source' }
$zipPath = Join-Path $root "dist/$name.zip"
$files = [ordered]@{}
if ($Source) {
    foreach ($relative in @('VERSION','README.md','CHANGELOG.md','CONTRIBUTING.md','LICENSE','.gitignore','build-controller.ps1','build.ps1')) {
        $files[$relative] = Join-Path $root $relative
    }
    $allow = @{src=@('.c','.S','.def'); include=@('.h'); assets=@('.svg','.json'); scripts=@('.ps1','.py'); tests=@('.c','.S','.ps1'); docs=@('.md'); '.github'=@('.md')}
    foreach ($directory in $allow.Keys | Sort-Object) {
        foreach ($file in Get-ChildItem -LiteralPath (Join-Path $root $directory) -File -Recurse | Sort-Object FullName) {
            if ($file.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Symlink/reparse source is forbidden.' }
            if ($allow[$directory] -cnotcontains $file.Extension) { continue }
            $relative = [IO.Path]::GetRelativePath($root,$file.FullName).Replace('\','/')
            $files[$relative] = $file.FullName
        }
    }
    foreach ($file in Get-ChildItem -LiteralPath (Join-Path $root 'config') -File -Filter '*.ini' | Sort-Object Name) {
        $files["config/$($file.Name)"] = $file.FullName
    }
} else {
    $files['DINPUT.dll'] = Join-Path $root 'build/modern-controller/DINPUT.dll'
    $files['true-analog.ini'] = Join-Path $root 'config/modern-controller.ini'
    $files['README.md'] = Join-Path $root 'README.md'
    $files['LICENSE'] = Join-Path $root 'LICENSE'
    $metadata = Get-Content -LiteralPath (Join-Path $root 'build/modern-controller/build-info.json') -Raw | ConvertFrom-Json
    if ($metadata.version -cne $version -or $metadata.name -cne 'XML2 Modern Controller' -or
        (Get-FileHash -LiteralPath $files['DINPUT.dll'] -Algorithm SHA256).Hash -ine $metadata.dll_sha256 -or
        $metadata.atlas_bgra_sha256 -cne '6a11ef30dc9717b06d23ced009434ba4ee74ad35afc5a35ef37dc0e318d78824') {
        throw 'Build metadata, version, DLL or approved atlas mismatch.'
    }
    & (Join-Path $root 'scripts/Inspect-Proxy.ps1') -Path $files['DINPUT.dll'] | Out-Null
    if (-not $?) { throw 'DLL inspection failed.' }
    $binaryText = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($files['DINPUT.dll']))
    if ($binaryText -match '(?i)[A-Za-z]:[\\/]Users[\\/]|/home/|/Users/') { throw 'Personal build path in DLL.' }
}
foreach ($relative in $files.Keys) {
    $path = $files[$relative]
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing release file: $relative" }
    if ($relative -match '(?i)(^|/)(XMen2\.exe|work|dumps|saves|extracted|logs)(/|$)|\.(igb|engb|xmlb|dds|png|save|dat|bin|log)$') { throw 'Forbidden package content.' }
    if ($relative -cne 'DINPUT.dll') {
        $text = Get-Content -LiteralPath $path -Raw
        if ($text -match '(?i)C:[\\/]Users[\\/]|(?<![A-Za-z])[A-Za-z]:[\\/]') { throw "Machine path in $relative" }
    }
}
New-Item -ItemType Directory -Path (Split-Path $zipPath) -Force | Out-Null
$stream = [IO.File]::Open($zipPath,[IO.FileMode]::Create,[IO.FileAccess]::Write)
try {
    $archive = [IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Create,$false)
    try {
        foreach ($entryName in $files.Keys | Sort-Object -CaseSensitive) {
            $entry = $archive.CreateEntry("$name/$entryName",[IO.Compression.CompressionLevel]::Optimal)
            $entry.LastWriteTime = [DateTimeOffset]::new(2026,9,28,0,0,0,[TimeSpan]::Zero)
            $input = [IO.File]::OpenRead($files[$entryName]); $destination = $entry.Open()
            try { $input.CopyTo($destination) } finally { $destination.Dispose(); $input.Dispose() }
        }
    } finally { $archive.Dispose() }
} finally { $stream.Dispose() }
$check = [IO.Compression.ZipFile]::OpenRead($zipPath)
try {
    if ($check.Entries.Count -ne $files.Count) { throw 'Unexpected ZIP entry count.' }
    foreach ($entry in $check.Entries) {
        if (-not $entry.FullName.StartsWith("$name/",[StringComparison]::Ordinal)) { throw 'Unsafe ZIP entry.' }
        $relative = $entry.FullName.Substring($name.Length+1)
        if (-not $files.Contains($relative)) { throw "Unexpected ZIP entry: $relative" }
        $sha = [Security.Cryptography.SHA256]::Create(); $entryStream = $entry.Open()
        try { $entryHash = [BitConverter]::ToString($sha.ComputeHash($entryStream)).Replace('-','') }
        finally { $entryStream.Dispose(); $sha.Dispose() }
        if ($entryHash -ine (Get-FileHash -LiteralPath $files[$relative] -Algorithm SHA256).Hash) { throw "ZIP content differs: $relative" }
    }
} finally { $check.Dispose() }
Write-Output "PASS: owned-content audit and ZIP round-trip; entries=$($files.Count); $zipPath"
Write-Output "SHA-256: $((Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash)"
