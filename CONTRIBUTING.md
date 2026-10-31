# Contributing

Use the pinned Windows toolchain and exact build/test commands in [README](README.md).
The runtime target is PE32 **x86**, even on a 64-bit Windows host. Run the existing
Controller, Prompts, Advanced, UnknownBuild and Release test scripts before a patch.
No proprietary game installation is required for those fixtures.

Keep changes small and verifiable. The beta freezes functional/visual behavior
at `511082b`; publication edits must not change gameplay or redraw glyphs.
Preserve hash/signature/layout guards, unknown-build refusal, keyboard/mouse
behavior, custom bindings and local textual fallback. Additional executable
support needs a verified profile and actual gameplay evidence.

Never submit game executables/DLLs, dumps, extracted assets, saves, local machine
configuration or personal logs. Original project source/SVGs are MIT licensed;
the game is not included. Build output, caches and installed tools stay ignored.
