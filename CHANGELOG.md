# Changelog

## 0.5.0-beta.1

### Added

- True 360° analog movement, proportional movement speed and radial deadzone.
- Hot-plug, reconnect and neutralization on disconnect.
- Xbox/PlayStation/Generic identification, with Generic-to-Xbox glyph fallback.
- Dynamic keyboard/controller prompts and 48 project-owned glyphs.
- ConsoleDefault with RB Health Pack, LB Energy Pack and LT Call Allies.
- Advanced original navigation with South/A Accept and East/B Back.
- Guarded runtime-only hooks, safe unknown-build refusal, configuration and support logging.

### Fixed

- Original 0.75 per-component threshold dropping diagonal/intermediate directions.
- Reconnection and late-arrival input on the validated PC build.
- Glyph orientation/legibility and separate stick-direction arrows.
- Help direction examples, automap's fixed stick label and a legacy HP token.
- Incompatible writers retain local readable fallback without disabling other glyphs.
- Existing custom bindings remain protected from automatic default updates.

### Known limitations

- One supported PC EXE; Player 1 is the main validated gameplay path.
- P2–P4 and physical PlayStation hardware lack complete validation.
- PlayStation glyphs were validated through presentation override.
- Advanced intentionally uses textual fallback; edit bindings via mouse/keyboard.
- Generic HID layouts and combined trigger axes can differ.
- PreserveMagnitude=0 safely disables the movement hook and is not an alternate speed mode.

Functional/visual freeze: `511082b`; publication preparation does not change it.

## 0.1.0

Added 360-degree left-stick movement, radial deadzone, partial-speed support,
runtime-only movement hook, build fingerprinting, unique signature validation,
optional input logging, and a reversible DirectInput 7 proxy.

Known limits: one PC executable and controller index 0 were gameplay validated;
other builds fail closed and other controller indices are experimental.
The historical XML2 True Analog v0.1.0 artifact is preserved separately.
