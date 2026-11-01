# Current public beta status

The final full 48-glyph artwork and help coverage at functional/visual freeze
511082b were approved by the user for publication. PlayStation style was
validated through override; physical Sony hardware is not fully validated.
Earlier pending checks below describe historical development sessions, not
remaining release gates. See BETA-VALIDATION.md for consolidated evidence.

# Dynamic prompts and original glyphs (development)

## Evidence and status

Base: Phase 1 commit `4fe8f2248c5a4877871e1faac7ba68af756f7a62`.
Hypothesis: the retail action label can select a player's live controller binding
and the existing text layout can position sprites from a separate owned atlas.
Read-only probes confirmed action, row, slot, keyboard/button names and the text
batch. The user confirmed unchanged gameplay in that diagnostic session.

Two failed renderer preparations safely produced textual names: a four-byte
font-record offset mismatch, then a request for 48 cells where retail has only
41 shared empty cells. The corrected implementation borrows 24 logical cells
and two zero-advance style prefixes. Runtime then loaded the atlas, selected
A/B/X/D-pad bindings, submitted sprites and restored D3D8 state. The user saw
images but reported upside-down art, a small/distant conversation prompt and
PC mouse prose in an early help balloon. A scoped read-only retail run then
confirmed `switching_hint`, `combat_hint` and `break_hint` select the PC override,
while the already loaded asset contains controller-authored action tokens. The
native process exited with code 0; the x86 ABI fixture also passed. This supports
enabling that scoped controller-text selection. The user then approved corrected
orientation and larger glyphs, requesting a smaller gap before conversation
“Continue”. A geometry-based adjustment now moves only the owned sprite into
same-line unused whitespace, using the following letter's native rectangle;
the original text position and wrapping are retained. Visual validation of this
gap and the enabled tutorial variant is pending. Remapping, forced PlayStation style, resolution scaling and full
input regression with glyphs also require human validation.

Earlier corrective DLL SHA-256:
`BCB77B8FBFDF0A64216D3A9054AA19DF6261E1C6822082E7C5FAF306BE71778F`.
Apply and full-tree Verify passed with exactly two Dev differences (`DINPUT.dll`
and `true-analog.ini`). Startup armed the movement and prompt hooks, validated
the active tutorial callsite and loaded the atlas. The final visual route is
pending. No Vanilla/source write or stable ZIP replacement was performed.

Follow-up: opening controller configuration disabled the first candidate's
glyph renderer (`vertex array slot`); the user also reported no gamepad navigation
in the advanced binding screen. The placement adjustment was not visibly
effective. These are unresolved acceptance items. A subsequent static check of
`005843A0` confirmed the emitter's exact two-pointer array layout, capacity and
vtable. The follow-up candidate checks that sink before a whole text run and
uses local readable-name fallback for unfamiliar sinks, leaving other glyphs
active. Owned fixtures passed the invalid-index and alternate-vtable cases.
Label substitution is now scoped to the visual token caller `004BD73E`, keeping
other label consumers unchanged. A prompts-OFF Dev comparison is being used to
determine whether advanced-menu navigation differs from the Phase 1 behavior;
the user confirmed that both navigation and button mapping still fail with all
prompt hooks and the atlas disabled. That comparison retained the Phase 1 input
core; it establishes that the issue also exists without this prompt module, but
does not establish how an entirely unmodified executable behaves. Advanced-menu
controller support remains a separate unresolved limitation. The follow-up
render correction still requires an ON runtime visit to that screen and back.

## Retail boundary

Supported PE32 x86 EXE SHA-256:
`146CD9C316EDB57A267CD73753A7CE9AF647E52AAB750D449C6B278FB4A1669B`.

| Boundary | Contract / observation |
| --- | --- |
| `004BD720` | Resolves action tokens (`F000` high bits, low-byte action). |
| `00619E30` | cdecl action -> `const char*` bracketed label; calls binding reader and name formatter. |
| `00619C40` | Original action-to-row/context mapping, retained. |
| `006294B0` | thiscall binding owner, row, slot, kind/code out; `RET 16`. |
| `00A68F40` | 16 native binding owners; owner+18 is 42 rows x four 12-byte slots. |
| `006281F0` | thiscall input manager, kind, code -> localized name; `RET 8`. |
| `00596DF0` / `005EF2E0` | Measurement / markup-to-wide-line consumers of action labels. |
| `005EE780` | thiscall text owner, wide string as first stack argument, writer, x/y, style, pen, scales; `RET 28`. |
| `005EE400` | thiscall writer, eight floats; emits six text-plane vertices; `RET 32`. |
| `libIGGfx +352D0` | Non-indexed text draw; type 4 strip, start and count. |
| `libIGGfx +34E60` | Original state preparation; submit sprites after its return. |
| `005EC061` | One PC tutorial localization call in popup creation. Scoped observation reads the already loaded dialog's `text`; original keyboard prose is retained. |

The core validates the known EXE first. Prompt hooks check unique executable
patterns, instruction boundaries, fixed image base and Gfx call/layout bytes
before a six-entry transaction. Five-byte jumps target owned x86 trampolines.
The tutorial call interception separately validates its unique caller, field
reader and localization function. No on-disk executable or game asset changes.
Disable glyphs/prompts independently if validation or rendering fails.

## Binding, player and input source

An original binding-owner pointer identifies bank and player (bank index modulo
four). Unknown owners keep their original label. Global menu actions retain
the original game's P1 context. The current action's four live slots (48 bytes)
are inspected for the connected device assigned to that player; ConsoleDefault
is not used to infer glyphs. Custom binding changes are seen at the next action
resolution. Unmapped pad codes retain the original localized textual name.

Four independent source/style states reuse Phase 1 GUID ownership and connection
state. Successful original DirectInput state reads are observed without changing
buffers. Bound keyboard press edges update their owners; shared mouse UI uses
P1. Pad button/POV edges or meaningful stick changes select Controller.
Source changes use 250ms debounce; stick hysteresis enters above
`max(0.20, Deadzone+0.05)`, leaves below `max(0.15, Deadzone)`. Held sticks do not
steal keyboard ownership. Mouse motion needs eight counts within 150ms; one-count
jitter never accumulates. Connect alone does not select Controller; reconnect
retains the core's device identity. No keyboard/mouse/button input is blocked.

`GlyphStyle=Auto|Xbox|PlayStation` is per player, loaded at startup. Manual style
has priority. Auto selects PlayStation for classified Sony devices, Xbox for
Xbox/Generic/Unknown. Changing presentation style does not change the physical
button adapter. Native Sony HID button ordering is provisional; Sony axes keep
text because no physical Sony controller has been validated.

## Owned art and rendering

48 project SVGs (24 per style), with geometric shapes and our own
monoline letter paths; no copied console image or logo.
Xbox: A/B/X/Y, LB/RB, LT/RT, LS/RS, four D-pad arrows, Menu/View, eight stick
directions. PlayStation: Cross/Circle/Square/Triangle, L1/R1, L2/R2, L3/R3,
D-pad arrows, Options/Create and eight stick directions. All are MIT licensed.

Official resvg **0.47.0** rasterizes 64-unit SVGs to 56x56 RGBA; Pillow packs an
8-column **512x512** lossless atlas, 64px cells, **4px padding**. `atlas.json`
records names, cell positions and UVs. BGRA RLE is embedded in the DLL. Repeated
builds of the refined art gave identical PNG, metadata and embedded header, with decoded hash
`ae22bd3b54b7ed525c9f65739025a72a6696c2ddfb952a9129fa2be88b6ef014`.
The runtime creates one managed A8R8G8B8 texture; no SVG/INI/file reads per frame.

Art integration: the user's refined input package replaces Start/Back, four
D-pad directions and eight stick directions in each family (28 SVGs).
All 48 names, IDs, source paths and order remain unchanged. Sixteen L/R text
labels were converted to our own vector strokes at the supplied position,
removing the package's Arial dependency. PNGs and atlas are regenerated from
these editable sources; input rasters are not used. Every cell's four-pixel
gutters and all unused cells are checked for transparency. The initial sketch
generator now refuses to overwrite edited SVGs. No runtime or input changes
are part of this art integration. New art still requires in-game visual review.

Refined build DLL SHA-256:
`C7B5FF7964673A74B1F5911EABE7B7E517C64D10657328B955534D4CB0BEA2AD`.
Prompt/controller/unsupported-build fixtures passed. The new DLL ran in Dev
with DynamicPrompts, ControllerGlyphs and PromptTrace OFF, retaining the input
core. The user confirmed no regression in Advanced mouse/keyboard behavior,
other controls, diagonal/circle/partial movement and unplug/reconnect, then
closed the game. Advanced controller navigation/remapping remains unavailable,
as in the preceding OFF comparison. The ON preparation passed full-tree Verify
with exactly two Dev additions, armed the prompt/movement hooks and loaded the
512x512 managed atlas. It does not constitute visual approval. Vanilla's 12,529
files and source's 12,527 files matched baseline hashes; all three EXE hashes
and the stable v0.1.0 ZIP remained unchanged.

Retail metrics at `008161F8 + font*1C18 + 18 + code*1C` provide cap height and
baseline. Only otherwise empty cells get runtime metrics; original font pixels
and UVs are unchanged. Original text measurement, layout, transforms and viewport
remain in use. Collapsed native quads retain original batching counts; new quads
are keyed by vertex array and range. Six vertices use the original `(x,0,y)`
plane, inline color, linear sampling and alpha blending. A retained D3DSBT_ALL
state block restores the complete original device state. Stable queue order
preserves text order. Capacity, invalid font, device change or GPU failures fall
back to readable names; input and hot-plug remain independent.

The queue holds 256 glyphs; strings are bounded to 512 wide units. No frame
allocation, asset rasterization or full binding-table scan. A real keyboard edge
may search native ownership. No FPS benchmark or simultaneous P1-P4 gameplay
validation has been performed. Resolution/device reset may require a restart
after conservative renderer fallback.

## Build, configuration and validation

Use `build-controller.ps1` with Zig 0.16.0, Python/Pillow and official resvg
0.47.0. `scripts/Export-GlyphArt.ps1` exports only our SVGs, transparent previews,
atlas, UV manifest and license for art review. Legacy `build.ps1`, the old INI,
and the v0.1.0 ZIP remain separate and unchanged.

New `[General]` keys: `DynamicPrompts`, `ControllerGlyphs`, `PromptTrace` and
`TutorialControllerText`. `DynamicPrompts=0` retains original labels;
`ControllerGlyphs=0` uses readable controller names. `TutorialControllerText=0`
observes PC/tutorial text without changing it. The prepared Dev enables the
scoped selection after the retail observation and native ABI checks passed.
Help prose is selected when the popup is created; reopen it after switching
source to obtain the other prose variant. Action labels within it remain dynamic.

### Help coverage sweep

The controller-authored `switching_hint` field contains literal Left, Right,
Down and Up example lines, unlike the PC localization's action tokens.
At the existing `005EC061` popup callsite, those exact escaped-newline example
labels become `$DPAD_LF`, `$DPAD_RT`, `$DPAD_DN`, `$DPAD_UP`.
`automap_hint` normalizes its right analog stick/thumbstick click instruction
to `$MAPTOGGLE`, so a remapped Map Toggle action displays its current binding.
One legacy `potion_hint` variant instructs health recovery using `$ALLY` (PC
Energy Pack). Only that complete health-pack instruction is normalized to
`$TARGET_LOCK` (PC Health Pack); already correct health/energy tokens are kept.
Other prose, help fields and keyboard/mouse selection remain unchanged;
unknown, incomplete, repeated or oversized text retains its original field.
No game assets or additional hooks are installed.

The native token resolver `004BD720` calls the existing label boundary
`00619E30`: action 17 maps to row 14 DecreaseHeroAggr; action 16 maps to row 15
IncreaseHeroAggr; actions 11/12 map Up/Down to rows 12/13, action 15 maps
Map Toggle to row 16 and action 13 maps Health Pack to row 10.
The assigned controller's current live row supplies its
logical control, then the existing Xbox/PlayStation lookup supplies the glyph.
ConsoleDefault resolves all four D-pad directions, but remapped sticks/buttons resolve
their own glyphs. Unknown bindings and incompatible writers keep the existing
readable textual fallback; glyph rendering is never forced on another writer.
Owned tests cover scoped normalization, native tutorial ABI, live row selection,
both styles and incompatible-writer fallback. In-game appearance awaits the
short user check of these help screens.

Sweep scope: all 4,408 Dev `.engb`, `.xmlb`, `.bnx` and script files (159,768
printable spans), plus standalone prompt adapters, renderer, tests and the
existing retail-boundary documentation. Reviewed the eight PC tutorial overrides,
other dialog/hint/help fields, conversations, menus and PC localization. The
additional literal-label hit in an X-jet script is a comment about using a vehicle,
not a control prompt. Automap's physical click label is a real bypass; continuous
left-stick throwing prose, flight direction prose, and ordinary "up/down/left"
words do not identify a single bindable action and are retained. Existing action
tokens continue through `004BD720`/`00619E30`; the supported executable-section
call census confirms `004BD739` as the sole direct action-label caller.
Advanced's incompatible writer
keeps its established local text fallback. No new reverse-engineering boundary
or writer hook was added. This is static/fixture coverage, not proof that every
screen and localized variant has been visited during gameplay.

### Full refined art integration

Imported all 48 SVGs from `XML2-Glyph-Assets-Refined-FullPass.zip`
(SHA-256 `f72e18000ec04d7b8d9d0cbe0599f3cc44e48d7e1d9a5069d1e15317b6aaa894`).
Its MIT manifest is semantically identical; the existing manifest file, names,
IDs and UV mapping are retained. Forty font labels are converted to our own
vector lettering so no Arial/Helvetica installation affects the build.
Sixteen stick-direction glyphs need an integration adjustment: the supplied
outlined arrow still overlaps its cap/badge. A smaller cap/badge at the left
and external arrow at the right preserve the new palette/frame and separate
the two references in both styles. The supplied eight D-pad arrows are retained.

resvg 0.47.0 regenerates all PNGs and the 512x512 RGBA atlas with 4px gutters;
the metadata/header are regenerated without logical changes. Decoded BGRA
SHA-256 is `6a11ef30dc9717b06d23ced009434ba4ee74ad35afc5a35ef37dc0e318d78824`.
Validation checks transparent gutters, unused cells, SVG/PNG correspondence,
connected D-pad arrow masks, and repeatable derived-file hashes. Renderer
fixtures verify exact shared position/UV/color edges of the two sprite triangles;
no renderer change is required by the observed geometry. Visual review at 56px
and 28px shows separated stick arrows and continuous D-pad arrows. In-game
visual legibility, orientation and positioning were accepted at the final freeze.

Automated gates: `Test-Prompts.ps1` covers logical mappings, styles, custom live
rows, unknown bindings, player isolation, noise/debounce, unplug/reconnect,
invalid config, actual font-header offset, render range matching, UV/color,
alpha/state restore and missing-atlas fallback. Its owned x86 tutorial fixture
also verifies caller stack, reader offset, cdecl/thiscall and parser restoration.
`Test-Controller.ps1` keeps COM ABI/core/preset/legacy tests; unknown EXE fixtures
must leave IAT, gameplay, prompts and atlas untouched.

Human route: main menu Select/Back; new campaign's first Cyclops/Nightcrawler
conversation and first help balloons; keyboard -> controller -> keyboard;
remap South to North and restore; force PlayStation style and restart. Check
orientation, baseline, gap, clipping, transparency and low/720p/1080p modes if
available. Retest all ten requested stick angles, circle, partial speed,
deadzone, camera, attacks/powers, switching, menus, save/load and hot-plug/reconnect.

Standalone install/rollback: with the game closed, install the release DLL
and INI beside XMen2.exe. To uninstall, remove those files and the generated
true-analog.log. No parent workspace or managed installation scripts are needed.

References: independent implementation after reading the MIT
[xmen2 text investigation](https://github.com/SomeoneIsWorking/xmen2/blob/cf0def5efd076e18f6333db67916bfdee336729b/docs/RE/text.md)
and tutorial routing notes. No upstream code or game assets copied.
[resvg release](https://github.com/linebender/resvg/releases/tag/v0.47.0),
[MIT license](https://github.com/linebender/resvg/blob/v0.47.0/LICENSE-MIT).
