# Advanced controller adapter

Branch: `feature/modern-controller-core`, base `396484b`.

## Scope and diagnosis

The user narrowed this task after the initial prototype: keep controller navigation/Accept/Back and glyphs; select bindings with the mouse. The controller list cursor, spatial focus replacement, controller-initiated edit and capture hook were removed. No binding capture fields are written by the final adapter. Original directional focus, mouse selection, keyboard capture and controller input after mouse selection are retained.

Read-only runtime collection confirmed pad directions arrive as native `WM_KEYUP` arrow commands at `00621EA0`, caller `006192CC`. South and East were read successfully but produced no confirmation/back command. Static inspection of `00619080` explains the difference: its direction path includes Advanced, while its pad Accept/Back branches check other modal globals and omit the Advanced root at `00A6AD34`. The user confirmed keyboard arrows worked, mouse selection could capture a controller button, and the controller could navigate but could not leave the screen.

## Implementation

`General/AdvancedController=1` maps logical South/East from the assigned P1 pad to contextual Accept/Back through the original Advanced dispatcher. No global keyboard injection is used. Entry/reconnect requires neutral input; presses are edge-triggered, held copies are suppressed, and existing secondary modal dialogs keep their original controller path. While a mouse-selected binding awaits input, the adapter emits neither Accept nor Back and leaves capture to the original game. Cancel that edit with Esc as before.

Six x86 runtime hooks validate unique signatures and original list vtable slots before a reversible transaction: update `00626A20`, dispatch `00621EA0`, selection observer `006265A0`, notification observer `0061FD10`, draw `00625840`, binding name `006281F0`. Capture `00626620` is not hooked. `AdvancedTrace=1` logs at most 512 transitions/events; it defaults to 0. Failure refuses this module independently of analog/prompts/hot-plug.

The Advanced auxiliary DirectInput manager is refreshed on entry/inventory changes through the validated native refresh routine, using the existing worker snapshot. No new frame enumeration or scan is introduced.

## Glyphs

Runtime writer `006A4C4C` uses a different text layout from the inline renderer. Incompatible text runs use local readable fallback; renderer stayed active (`glyph_active=1`) and the user confirmed glyphs returned in gameplay. Advanced binding cells derive controller names from real binding codes and the configured Auto/Xbox/PlayStation style; keyboard labels remain original. No new sprites or UI were added. Entering Advanced does not reset LastInputSource. The final approved atlas is recorded in BETA-VALIDATION.md; the adapter behavior is unchanged.

## Historical adapter validation and limits

Later beta sessions confirmed the PlayStation style override; physical Sony
hardware remains unvalidated. See BETA-VALIDATION.md for current release evidence.

Owned fixtures verify all six hook calling conventions, object/arguments/returns/ESP, Accept/Back edges, neutral entry, native directions/modal retention, foreign/disconnected pad refusal, and absence of capture mutation. Controller, prompt/render and unsupported-build fixtures pass. Final runtime confirms all six hooks armed, native direction focus, South reaching the original Exit callback, auxiliary refresh, preserved controller source and active glyph renderer; Dev exited with code 0. Only DLL/INI differ from baseline and EXE is unchanged. The user reported the simplified direction/A/B, mouse/Esc, reconnect and diagonal route worked. A follow-up explicitly confirmed images still appear in dialogues and text appears only in Advanced, consistent with local fallback. PlayStation override loaded in this session and dialogue images were confirmed, but exact Cross/Circle appearance and PlayStation hardware were not independently verified. Final local configuration restores Auto and disables AdvancedTrace/PromptTrace. Earlier gameplay/save-load regression results remain from the preceding validated core, rather than a new exhaustive test in this session.

Only the supported PC build and assigned P1 controller are targeted. Navigation follows the original menu graph; bindings are selected with the mouse. Advanced currently uses text fallback, while compatible dialogue/gameplay paths retain inline glyphs.

Disable only the adapter with `General/AdvancedController=0`. Full standalone rollback, game closed: remove the mod DLL, INI and generated support log. The EXE and game assets remain unchanged.
