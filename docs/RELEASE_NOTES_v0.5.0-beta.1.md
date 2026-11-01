# XML2 Modern Controller v0.5.0-beta.1

**Public Beta** — modern controller support for X-Men Legends II PC.

- True 360° analog movement, partial-stick speed and radial deadzone.
- Hot-plug, reconnect and neutral input on disconnect.
- Dynamic keyboard/controller prompts with Xbox/PlayStation glyphs and Generic fallback.
- ConsoleDefault: RB Health Pack, LB Energy Pack, LT Call Allies.
- Advanced native navigation and A/South Accept, B/East Back; mouse/keyboard binding edits.
- Help/tutorial binding coverage and safe textual fallback.

Install `DINPUT.dll` and `true-analog.ini` beside `XMen2.exe`, then launch normally.
Check for another DINPUT proxy first. Uninstall with the game closed by removing
the two files and optional `true-analog.log`. The EXE is never patched on disk.

Supported EXE: PE32 x86, SHA-256
`146CD9C316EDB57A267CD73753A7CE9AF647E52AAB750D449C6B278FB4A1669B`.
Unknown builds refuse incompatible patches. P1 is the main validated path;
P2–P4 and physical PlayStation hardware lack complete validation. PlayStation
presentation was checked by override. Advanced textual fallback is intentional.

Verify downloads against the accompanying **SHA256SUMS.txt**. The source ZIP
includes editable SVGs, build scripts and tests; no game content is distributed.
Report bugs with mod version, EXE hash, controller/connection/player, styles,
steps and relevant log lines. Never upload game executables/assets.
Real publication screenshots will be supplied separately by the user.
