# XML2 Modern Controller

**v0.5.0-beta.1 — Public Beta**

Modern controller support for X-Men Legends II on PC, including true 360°
analog movement, radial deadzone, hot-plug, reconnect and dynamic
Xbox/PlayStation button prompts. The executable on disk is never patched.

## Features

- True 360° analog movement, proportional partial-stick speed and radial deadzone.
- Controller hot-plug, automatic reconnect and neutral input on disconnect.
- Conservative controller identification: Xbox, PlayStation and Generic.
- Dynamic prompts with project-owned Xbox and PlayStation glyphs; Generic defaults to Xbox style.
- Automatic keyboard/controller presentation based on LastInputSource.
  Keyboard, mouse and controller remain available simultaneously.
- Console-style defaults: **RB Health Pack, LB Energy Pack, LT Call Allies**.
  Existing controller customizations are preserved unless restore is explicitly requested.
- Advanced keeps original navigation, with South/A Accept and East/B Back.
  Select/edit bindings using mouse/keyboard; the original capture behavior is retained.
- Help/tutorial labels use current bindings. Incompatible text writers keep safe readable labels.
- Build fingerprinting and guarded runtime hooks; unknown builds refuse incompatible patches.

## Why this exists

The PC port receives continuous analog axes, but applies a 0.75 threshold to
each component. This removes diagonals and intermediate directions. The mod
corrects that movement logic in memory, retaining the stick's angle and magnitude.

## Installation

1. Start with a working X-Men Legends II PC installation and close the game.
2. Check for another `DINPUT.dll`. Do not overwrite another mod's proxy blindly;
   this beta does not provide a proxy-chaining installer.
3. Extract `DINPUT.dll` and `true-analog.ini` beside `XMen2.exe`.
4. Run the game normally. No installer, build tools or source checkout are needed.

Windows 10 or later is targeted; Windows 10 was tested. The DLL is **PE32 x86**
and uses system DirectInput and the Universal C Runtime.

## Uninstallation

Close the game and remove this mod's `DINPUT.dll`, `true-analog.ini`, and
`true-analog.log` if present. **XMen2.exe is not altered**, and game assets are not replaced.

## Configuration

`true-analog.ini` is the XML2 Modern Controller configuration file. Its legacy
filename is retained for compatibility. Edit it with the game closed.

| Section / option | Default | Purpose |
| --- | --- | --- |
| Controller / TrueAnalogMovement | 1 | Enable the P1 movement correction |
| Controller / Deadzone | 0.15 | Radial deadzone, range 0–0.5 |
| Controller / PreserveMagnitude | 1 | Proportional stick speed; keep 1 (0 safely disables this beta's hook) |
| Controller / ControllerIndex | 0 | Preferred DirectInput pad; other indices are experimental |
| General / HotPlug | 1 | Late connection and automatic reconnect; no separate Reconnect option |
| General / DynamicPrompts | 1 | Switch prompt presentation with meaningful input |
| General / ControllerGlyphs | 1 | Inline images on compatible text paths |
| General / TutorialControllerText | 1 | Controller-oriented help with dynamic binding tokens |
| General / AdvancedController | 1 | Contextual Accept/Back in Advanced |
| Player1 / ControllerType | Auto | Auto, Xbox, PlayStation or Generic; physical interpretation |
| Player1 / GlyphStyle | Auto | Auto, Xbox or PlayStation; presentation only |
| Player1 / Preset | ConsoleDefault | Defaults for an unconfigured pad; None disables publication |
| Player1 / RestoreConsoleDefaults | 0 | Set 1 explicitly to replace pad mappings on next startup, then return to 0 |

DebugInput, ControllerTrace, PromptTrace and AdvancedTrace default to 0.
The normal support log remains available. Optional Player1 analog overrides take
precedence over legacy Controller options. There is no exclusive input-device mode.

## Compatibility

Officially validated: `XMen2.exe`, **PE32 x86**, SHA-256:

```text
146CD9C316EDB57A267CD73753A7CE9AF647E52AAB750D449C6B278FB4A1669B
```

**SUPPORTED:** recognized build with validated hook signatures/layouts.
**UNKNOWN:** incompatible game patches are not installed; system DirectInput
forwarding remains available. A failed guard also refuses its affected module.
Not all PC releases are supported. See [compatibility](docs/COMPATIBILITY.md).

## Known limitations

- Only one PC executable build is officially validated.
- Player 1 is the main validated gameplay path; P2–P4 lack complete validation.
- Physical PlayStation hardware/native Sony HID mapping lacks complete validation.
  PlayStation GlyphStyle was validated by override on the available controller.
- Generic uses Xbox-compatible DirectInput indices; other HID layouts may differ.
- Advanced uses intentional textual fallback where inline glyphs are incompatible.
  Binding editing remains mouse/keyboard based; no controller binding editor is added.
- Combined DirectInput trigger axes retain the original game's limitation.
- Additional builds/controllers require community evidence and guarded profiles.

## Troubleshooting

The adjacent `true-analog.log` reports version, compatibility and hook status.
If startup fails, uninstall the two mod files and check the original game.
Report the EXE SHA-256, mod version, controller model, USB/Bluetooth connection,
player slot, GlyphStyle, ControllerType, reproduction steps and relevant log lines.
Use the bug report template; do not upload game files or personal logs in full.

This is an x86 DirectInput proxy with hooks applied only in memory. Source is
included in the source package and official hashes accompany the release.
Modding/hooking tools can receive heuristic security detections; no absolute
antivirus guarantee is made. Check provenance/checksums and report the details.

## Building from source

Use Windows, PowerShell 7+, **Zig 0.16.0**, **resvg 0.47.0**, and Python/Pillow.
The tested asset toolchain is **Python 3.12.14 / Pillow 12.3.0**. Put the three
executables at the example paths below or substitute your installed paths.

```powershell
./build-controller.ps1 -ZigPath ./tools/zig.exe -PythonPath ./tools/python.exe -ResvgPath ./tools/resvg.exe
./tests/Test-Controller.ps1 -ZigPath ./tools/zig.exe
./tests/Test-Prompts.ps1 -ZigPath ./tools/zig.exe
./tests/Test-Advanced.ps1 -ZigPath ./tools/zig.exe
./tests/Test-UnknownBuild.ps1 -ZigPath ./tools/zig.exe
./tests/Test-Release.ps1 -ZigPath ./tools/zig.exe
./scripts/Package.ps1
./scripts/Package.ps1 -Source
```

The target is Windows x86. Build regenerates PNGs, the 512×512 atlas and embedded
header from the 48 editable SVGs and writes `build/modern-controller/DINPUT.dll`.
Packaging produces the binary/source ZIPs in `dist/`. Generate final checksums
after packaging as described in [GitHub setup](docs/GITHUB_SETUP.md).
No game installation, proprietary files or parent workspace are needed.
`VERSION` controls the modern identity; `build.ps1` preserves the historical build.

## Credits

[SomeoneIsWorking/xmen2](https://github.com/SomeoneIsWorking/xmen2/tree/cf0def5efd076e18f6333db67916bfdee336729b)
was an important MIT-licensed reverse-engineering/documentation reference.
XML2 Modern Controller is an independent implementation, not their codebase.
No upstream game assets are included. Zig, resvg and Python/Pillow are used for
building; they are not bundled in the release. See [licenses](docs/LICENSES.md).

XML2 True Analog v0.1.0 remains a preserved historical artifact. The new public
identity and configuration documentation use XML2 Modern Controller.

## License

MIT covers original project code, documentation and glyph assets. See [LICENSE](LICENSE).
No license to X-Men Legends II or its proprietary assets is included.
