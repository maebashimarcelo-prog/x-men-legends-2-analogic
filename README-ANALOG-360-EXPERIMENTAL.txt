X-MEN LEGENDS II - EXPERIMENTAL 360 ANALOG PATCH SOURCE

Exact supported XMen2.exe SHA256: 8AE56DB5E3739E05977991380A6CF14165A4CF4D993D9A968369E332AFD7638C

This replaces the virtual input getter (vtable 0x69A104, original 0x550B10)
ONLY when called from 0x42DC51 / 0x42DC5F in the character state routine.
Other input calls fall through to the original getter.
Raw left stick X/Y is remapped radially with deadzone and magnitude preservation.

WARNING: NOT RUNTIME TESTED. This is a source-level experimental implementation,
not proof of working 360-degree movement in-game. Back up the existing DINPUT.dll.
The existing gate hook is disabled in favor of the new getter hook.

Build: copy all source files into repository root, run GitHub Actions build-gcc.yml,
then copy artifact DINPUT.dll into the game directory.
If movement is incorrect, remove the experimental DLL and restore backup.

Known limitations: other game paths may quantize movement after the getter;
walk/run threshold 40% is not yet implemented in character animation/speed.
