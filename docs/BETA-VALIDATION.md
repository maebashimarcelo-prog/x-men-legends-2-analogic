# Public beta validation: 0.5.0-beta.1

Functional/visual freeze: **511082b**. The user approved the final full glyph
set and help coverage for publication. Release preparation changes documentation,
explicit distribution defaults and packaging/audit gates only; runtime source
and all 48 approved SVGs remain unchanged.

## Gameplay and visual evidence

- User-confirmed P1 360° directions/circle, partial-stick speed and radial deadzone.
- Keyboard/mouse/controller input, camera, combat/powers, character changes,
  menus and save/load passed the preceding regression sessions.
- Startup connected, startup without a pad, late connection, disconnect/reconnect
  and no stuck input were confirmed by the user.
- Advanced directions, South/A Accept, East/B Back and original mouse/keyboard
  binding editing passed. Advanced readable fallback is intentional and local.
- Xbox/Generic glyph presentation and PlayStation style override passed; the final
  art/help revision was subsequently accepted by the user at 511082b.
- No physical Sony or complete P2–P4 support claim is made.

## Technical release gates

The existing suite covers COM/x86 calling conventions, core identity/ownership,
custom-binding protection, fresh/explicit presets, actual analog config reader,
48 glyph lookups/styles, help normalization, tutorial ABI, rendering/state restore,
Advanced adapter, unknown-build refusal and quiet diagnostic defaults. It does
not substitute for the user-confirmed gameplay tests above.

Release preparation passed the five existing suites: Test-Controller,
Test-Prompts, Test-Advanced, Test-UnknownBuild and Test-Release. The actual config
reader accepts the public INI; 10,000 diagnostic events stay quiet with traces
OFF. Runtime/header/SVG/build sources match the freeze commit.

Standalone clean builds, including fresh source-ZIP extractions, produced the
same stripped PE32 x86 DLL. All 51 generated PNG/atlas/metadata/header files
repeated exactly, and both ZIPs reproduced byte for byte.
The approved decoded BGRA atlas SHA-256 is
`6a11ef30dc9717b06d23ced009434ba4ee74ad35afc5a35ef37dc0e318d78824`.
Published binary/archive checksums are generated after final packaging in
SHA256SUMS.txt; earlier candidate hashes are obsolete.

Content audits reject personal paths, proprietary files, caches and generated
outputs from the source package. Binary ZIP has exactly DINPUT.dll,
true-analog.ini, README.md and LICENSE under its version directory.

## Standalone initialization and rollback

Only the release DLL/INI were installed into the marked development copy after
it matched baseline. XMen2.exe launched directly without a project log override.
The adjacent log confirmed the beta identity, supported EXE, movement hook at
0061A4FE, prompt/Advanced/core hooks and the 512x512 managed atlas with 48 cells.
The game responded and closed normally with exit code 0. Both mod files and the
generated log were removed; the entire development copy returned to baseline.
Read-only comparisons also confirmed the original and baseline installations
unchanged. This startup check did not reopen visual/gameplay regression.

Historical XML2 True Analog v0.1.0 ZIP remains unchanged. No remote, push,
publication, fabricated screenshot or new functional feature is part of preparation.
Real publication screenshots remain for the user to supply separately.
