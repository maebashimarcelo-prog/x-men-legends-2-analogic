# Compatibility

| State | Meaning | Hook |
| --- | --- | --- |
| Supported | Known SHA-256, expected PE metadata, one signature match, validated instructions and controller layout | Eligible |
| Unknown | Valid x86 PC executable with an unprofiled SHA-256 | Never installed |
| Incompatible | Invalid architecture, unreadable EXE, or a known hash with failed safety checks | Never installed |

Validated build: SHA-256
`146CD9C316EDB57A267CD73753A7CE9AF647E52AAB750D449C6B278FB4A1669B`,
3,129,344 bytes, PE32 x86, timestamp `0x43203BAF`, preferred image base
`0x00400000`, image size `0x006744C6`. Its resolver's 30-byte instruction
signature matched **once** in an executable section; four bytes encoding the
threshold address are wildcards. The loaded instructions, the threshold address
and value (0.75), and the controller-state locations are checked before the
six-byte in-memory hook is installed. Fixed layout offsets are used only with
this known SHA-256 profile. The EXE on disk is never patched.

An unknown build is reported even if it contains a similar pattern. Its
controller memory layout has not been validated, so pattern similarity alone
does not authorize a hook. Additional builds require a separately tested
profile and gameplay verification.

Controller index 0 and Windows 10 (10.0.19045) were tested. Indices 1–9 and
other Windows versions were not gameplay tested. `PreserveMagnitude=0` is not
implemented. The default configuration preserves the original keyboard,
camera and button paths.
