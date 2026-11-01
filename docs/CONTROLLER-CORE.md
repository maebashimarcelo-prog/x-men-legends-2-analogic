# XML2 Modern Controller — development core

This optional expansion builds separately from the validated v0.1.0 package.
The original `build.ps1`, analog sources, legacy configuration and package
remain usable. No release of this expansion has been published.

## Diagnosis and method

Hypothesis: the gameplay device list does not follow Windows device arrivals.
Read-only runtime observation confirmed a distinction: the auxiliary controls
manager reenumerates in menus, while the main gameplay manager keeps its old
slots. A late connection was visible to Windows and the auxiliary manager,
with no corresponding main device or input read.

Starting connected produced successful main reads. Unplugging stopped movement
and made the principal device's Acquire return `DIERR_UNPLUGGED`. On reconnect
Windows reported the same instance GUID, but the main principal device did not
resume successful reads. The user confirmed that input did not recover. Two
supplemental endpoints resumed reads; they are not additional physical players.

Validated retail addresses (relocated from image base):

| Role | Address |
| --- | --- |
| DirectInput8 factory lookup | `00626CAF` |
| Original refresh / callback | `00628E20` / `00628E00` |
| Main keyboard sample, before joystick iteration | `0062861E`, return `00628621` |
| Binding setter | `006297A0` |
| Main manager pointer | `00A6ABFC` |
| Player binding table | `00A68F40` |

The proxy observes the game's dynamic system DirectInput8 factory through its
GetProcAddress IAT cell. Front COM objects forward untouched methods with the
original `this`; no Windows COM object/vtable is modified. Unknown builds refuse
IAT, COM and gameplay changes. Refresh also requires a unique entry signature,
verified call sites, layout checks and observation of the original main call.

Windows PnP notifications wake a separate DirectInput observer. A five-second
fallback handles missed notifications. Enumeration and identity queries stay
off the game thread. Only a changed snapshot or confirmed unplug triggers the
original main refresh, before its joystick loop. That call consumes the cached
records instead of running another Windows enumeration on a frame.

The original callback creates devices and sets their data format, ranges and
cooperative level; the original poll loop performs Acquire. Removed devices
are released by the original refresh. Current/previous analog, button and
trigger samples are cleared, with all POVs neutral. Session GUID reservations
restore the same slot on reconnect. Another pad is available in a free slot;
it does not silently take a reserved player's device.

## Configuration

Build with `./build-controller.ps1 -ZigPath <zig>` and use
`config/modern-controller.ini` as the adjacent `true-analog.ini`. Legacy
`[Controller]` settings still work. Player1 analog settings take precedence;
`[General] DebugInput` overrides the legacy diagnostic setting.

`HotPlug=0` disables the refresh. With `ControllerTrace=0` as well, the new
DirectInput observer is not installed. Changes take effect at the next launch.
Closing the game and removing this proxy/INI restores the original executable
behavior. Remove its generated support log as well.

Player1–Player4 each have ControllerIndex, ControllerType, Preset and
RestoreConsoleDefaults. Index `-1` disables automatic assignment. The default
enables only P1. Overrides change effective classification, never identity.
Xbox requires a recognized Microsoft gamepad VID/PID or XInput interface-path
evidence. Known Sony VID/PIDs classify as PlayStation. Everything else is
Generic; the textual product name alone is insufficient.

ConsoleDefault contains 23 logical action assignments: sticks for movement and
camera; South punch, East smash, West use/guard, North jump; LT Call Allies, RT powers;
LB Recover Energy (row 9 Ally), RB Recover HP (row 10 TargetLock);
D-pad next/previous/increase/decrease; stick
click map; Start pause; Back team information. It is published to master,
working and menu binding sets using the original setter. Existing valid pad
customizations and alternate keyboard/mouse bindings are preserved. Setting
RestoreConsoleDefaults=1 explicitly requests the preset at the next launch.

Focused manual check: close the game and explicitly set Player1
`RestoreConsoleDefaults=1` in its adjacent `true-analog.ini` to opt into replacing
pad bindings with ConsoleDefault. Launch the game; check RB health, LB energy and LT
Call Allies. Close the game and remove that key (or set it to 0) before making custom
bindings. Leave the key disabled to test the help fix with existing bindings.

## Evidence and limits

- Automated: all 32 forwarding slots, five method arities, original `this`,
  arguments, return and stack; original COM memory unchanged.
- Automated: conservative classifications, stable identities, reconnect
  reservations, exclusive logical assignment and custom-binding protection.
- Automated: unsupported executable fixture forwards Windows DirectInput and
  keeps its original IAT; the controller and analog patches are refused.
- Automated: legacy INI compatibility, Player1 overrides, invalid-value refusal,
  fresh preset plans and explicit restore without duplicate pad actions.
- Runtime: the supported executable hash and refresh signatures matched.
- Runtime: original setter passed fresh/restore checks on owned buffers,
  including canaries and preservation of synthetic keyboard mappings. No
  player's custom configuration was changed by this test.
- Physical: late connection, disconnect, reconnect and startup connected passed
  on the available controller. The same GUID recovered slot 0.
- Physical: all requested angles, partial stick, deadzone, keyboard, camera,
  combat/powers, character changes, menus and save/load passed after integration.
- A different physical pad and first-configuration gameplay with ConsoleDefault
  remain pending. Existing custom configuration was preserved in the live game.
- P1 True Analog retains the v0.1.0 hook. P2–P4 resolver mapping was inspected
  but has no physical multiplayer validation; per-player correction is deferred.
- Sony native HID button/axis layouts need an adapter and hardware capture;
  classification is implemented, automatic Sony preset publication is deferred.
- Generic uses the Xbox-compatible DirectInput index convention; arbitrary HID
  layouts may need a later adapter. Custom mappings remain intact.
- Retail `0061B030` and `igct.bnx` confirm row 9 `Ally` = Energy Pack,
  row 10 `TargetLock` = Health Pack, row 11 `Solo` = Call Allies.
  The former preset incorrectly used row 9 for LT allies and omitted row 11.
  LT now publishes row 11, retaining the console Call Allies position documented
  by the Xbox mapping reference below. No other controls were reorganized.
  The change applies only to fresh configuration or explicit
  `RestoreConsoleDefaults=1`; existing pad mappings, including the old LT
  recovery binding, remain untouched. Set restore back to 0 before subsequent
  launches to preserve later edits. DirectInput's combined Xbox trigger axis
  retains the original game's limitation.
- Ten original game slots, 32 session identities. Disconnected reservations
  survive within the process; slot exhaustion requires a restart/reconfiguration.
- No EnumDevices, allocation or logging per gameplay frame. The main refresh
  measured 0–16 ms; Windows enumeration measured roughly 300–422 ms in the
  worker. These wall times do not establish a frame-rate or CPU benchmark.

References: [xmen2 controller mapping research](https://github.com/SomeoneIsWorking/xmen2/blob/cf0def5efd076e18f6333db67916bfdee336729b/docs/features/controller-mapping-defaults.md)
(MIT; independent implementation), [Xbox manual](https://manuals.plus/activision/x-men-legends-ii-rise-of-apocalypse-manual),
[Windows PnP callback](https://learn.microsoft.com/en-us/windows/win32/api/cfgmgr32/nf-cfgmgr32-cm_register_notification),
[DirectInput identity properties](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee417908(v=vs.85)).
