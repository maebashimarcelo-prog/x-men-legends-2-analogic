# XML2 True Analog

True analog movement fix for **X-Men Legends II on PC**. The PC port applies a
0.75 threshold to each movement component separately, losing diagonals and
intermediate directions. This mod keeps both movement components after a
configurable radial deadzone, including partial stick speed. It leaves the
camera, buttons, triggers, keyboard and game executable unchanged.

## Requirements and installation

- A working, legally obtained PC installation with `XMen2.exe`.
- Windows 10 or later. The release DLL is x86 and uses Windows system DirectInput
  and Universal C Runtime components; no separate mod runtime is bundled.
- The tested EXE has SHA-256
  `146CD9C316EDB57A267CD73753A7CE9AF647E52AAB750D449C6B278FB4A1669B`.
  Other builds are detected but do not receive the hook.

1. Check that the unmodified game starts and that no other `DINPUT.dll` is
   already in the folder containing `XMen2.exe`.
2. Extract the release folder's `DINPUT.dll` and `true-analog.ini` beside
   `XMen2.exe`.
3. Start the game normally. `true-analog.log` in that folder reports the build,
   signature result and hook status.

To uninstall, close the game and remove this mod's `DINPUT.dll`,
`true-analog.ini` and optional `true-analog.log`. `XMen2.exe` is never edited.
Do not remove a `DINPUT.dll` belonging to another mod.

## Configuration

Edit `true-analog.ini` before launching the game:

```ini
[Controller]
TrueAnalogMovement=1
Deadzone=0.15
PreserveMagnitude=1
ControllerIndex=0
DebugInput=0
```

`TrueAnalogMovement=0` forwards DirectInput without installing the movement
hook. `Deadzone` accepts 0 through 0.5 and applies to the left stick's vector.
`PreserveMagnitude=1` retains partial speed; `0` is unsupported and safely
disables the hook. Controller index 0 was tested. Indices 1–9 select another
DirectInput pad slot but are experimental. `DebugInput=1` logs sampled axes,
angles and resolved movement slots; use it briefly because the log grows.

## Compatibility and troubleshooting

The DLL forwards the seven DirectInput 7 exports to the real Windows DLL. It
hashes the EXE and scans executable code for one specific movement signature.
The hook is installed only for the validated build after a unique match and
instruction/layout checks. An unknown or incompatible build runs without this
movement patch. If the log says `Hook: NOT installed`, check the EXE hash,
configuration and `Pattern` line. The source project includes additional
compatibility notes in `docs/COMPATIBILITY.md`.

If the game does not start, remove the two mod files and verify the original
installation. To report a bug, include Windows version, controller model,
`XMen2.exe` SHA-256, the relevant `true-analog.log` lines and reproduction
steps. Do not attach game files, saves or proprietary assets.

## Build from source

Install [Zig 0.16.0](https://ziglang.org/download/) and run PowerShell 5.1 or
newer on Windows:

```powershell
./build.ps1                 # zig must be on PATH
./scripts/Package.ps1       # creates dist/XML2-True-Analog-v0.1.0.zip
```

Alternatively pass `-ZigPath` to `build.ps1`. The output is a PE32 x86
`build/DINPUT.dll`; the build script checks its exports and imported runtimes.
The compiler is a build tool and is not included in the release. Source code
is MIT licensed; the release contains no game assets.

Reverse-engineering reference and technical credit:
[`SomeoneIsWorking/xmen2` at cf0def5](https://github.com/SomeoneIsWorking/xmen2/tree/cf0def5efd076e18f6333db67916bfdee336729b).
Its code is MIT licensed. XML2 True Analog uses an independent implementation;
no xmen2 source or game data was copied into this mod.

## Optional controller core in development

`build-controller.ps1` builds XML2 Modern Controller separately, including
runtime hot-plug, device identity, player reservations and logical console
presets, dynamic binding labels and an additional atlas of original controller
glyphs. Use `config/modern-controller.ini` for that build. The optional build
also needs Python with Pillow and the official resvg 0.47.0 CLI:

```powershell
./build-controller.ps1 -ZigPath ./tools/zig.exe -PythonPath ./tools/python.exe -ResvgPath ./tools/resvg.exe
./tests/Test-Prompts.ps1 -ZigPath ./tools/zig.exe
./scripts/Export-GlyphArt.ps1
```

These tools are build dependencies; they are not needed by the runtime DLL.
The atlas is embedded in the DLL. See [glyph notes](CONTROLLER-GLYPHS.md)
for the retail pipeline, configuration, visual validation and known limits. See
[controller core notes](CONTROLLER-CORE.md) for current validation and
limits. The v0.1.0 build and release package remain separate.

The optional build also includes a contextual Advanced editor adapter, enabled
with `General/AdvancedController=1`. See [Advanced controls](ADVANCED-CONTROLLER.md)
for native navigation, Accept/Back, mouse-selected binding edits, local text
fallback and validation.
