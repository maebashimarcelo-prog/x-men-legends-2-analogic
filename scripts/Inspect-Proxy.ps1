param([Parameter(Mandatory)][string]$Path)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$bytes = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Path).Path)
function U16([int]$offset) {
    if ($offset -lt 0 -or $offset + 2 -gt $bytes.Length) { throw 'Truncated PE field.' }
    return [BitConverter]::ToUInt16($bytes, $offset)
}
function U32([int]$offset) {
    if ($offset -lt 0 -or $offset + 4 -gt $bytes.Length) { throw 'Truncated PE field.' }
    return [BitConverter]::ToUInt32($bytes, $offset)
}
function CStr([int]$offset) {
    if ($offset -lt 0 -or $offset -ge $bytes.Length) { throw 'Invalid PE string.' }
    $end = $offset
    while ($end -lt $bytes.Length -and $bytes[$end] -ne 0) { $end++ }
    if ($end -eq $bytes.Length) { throw 'Unterminated PE string.' }
    return [Text.Encoding]::ASCII.GetString($bytes, $offset, $end - $offset)
}
if ($bytes.Length -lt 512 -or (U16 0) -ne 0x5A4D) { throw 'Not an MZ image.' }
$pe = [int](U32 0x3C)
if ((U32 $pe) -ne 0x4550 -or (U16 ($pe + 4)) -ne 0x14C) { throw 'Not a PE32 x86 image.' }
$sectionCount = U16 ($pe + 6)
$optional = $pe + 24
if ((U16 $optional) -ne 0x10B -or $sectionCount -lt 1 -or $sectionCount -gt 32) {
    throw 'Invalid PE32 header.'
}
$sectionTable = $optional + (U16 ($pe + 20))
$sections = @()
for ($i=0; $i -lt $sectionCount; $i++) {
    $at = $sectionTable + 40*$i
    $sections += [pscustomobject]@{
        rva = U32 ($at + 12)
        span = [Math]::Max((U32 ($at + 8)), (U32 ($at + 16)))
        raw = U32 ($at + 20)
    }
}
function RvaOffset([uint32]$rva) {
    foreach ($section in $sections) {
        if ($rva -ge $section.rva -and $rva -lt $section.rva + $section.span) {
            $offset = [int]($section.raw + $rva - $section.rva)
            if ($offset -ge $bytes.Length) { throw 'PE RVA outside file.' }
            return $offset
        }
    }
    throw ('Unmapped PE RVA 0x{0:X8}.' -f $rva)
}
$exportRva = U32 ($optional + 96)
$exportAt = RvaOffset $exportRva
$ordinalBase = U32 ($exportAt + 16)
$functionCount = U32 ($exportAt + 20)
$nameCount = U32 ($exportAt + 24)
$namesAt = RvaOffset (U32 ($exportAt + 32))
$ordinalsAt = RvaOffset (U32 ($exportAt + 36))
$expected = @('DirectInputCreateA','DirectInputCreateEx','DirectInputCreateW',
              'DllCanUnloadNow','DllGetClassObject','DllRegisterServer','DllUnregisterServer')
if ($ordinalBase -ne 1 -or $functionCount -ne 7 -or $nameCount -ne 7) {
    throw 'DINPUT export count or ordinal base differs.'
}
$actual = @{}
for ($i=0; $i -lt $nameCount; $i++) {
    $name = CStr (RvaOffset (U32 ($namesAt + 4*$i)))
    $ordinal = $ordinalBase + (U16 ($ordinalsAt + 2*$i))
    $actual[[int]$ordinal] = $name
}
for ($i=0; $i -lt 7; $i++) {
    if ($actual[$i+1] -cne $expected[$i]) {
        throw "Export ordinal $($i+1) differs from DINPUT.dll."
    }
}
$imports = @()
$importRva = U32 ($optional + 104)
if ($importRva) {
    $importAt = RvaOffset $importRva
    for ($i=0; $i -lt 128; $i++) {
        $nameRva = U32 ($importAt + 20*$i + 12)
        if (-not $nameRva) { break }
        $imports += (CStr (RvaOffset $nameRva))
    }
}
$allowed = @('KERNEL32.dll','ADVAPI32.dll','msvcrt.dll')
foreach ($name in $imports) {
    if ($name -notin $allowed -and $name -notlike 'api-ms-win-crt-*.dll' -and
        $name -ine 'ucrtbase.dll') {
        throw "Unexpected runtime DLL dependency: $name"
    }
}
[pscustomobject]@{
    status='PASS'; format='PE32 x86'; exports=$expected.Count; imports=$imports
} | ConvertTo-Json -Compress
