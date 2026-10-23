# R05-A — editable font/text coverage qualification

Status: qualification, 2026-09-27, against `codex/practical-alpha@dbd171092fe6fb8fb50fafad3a59e4c0537d78a6`. This records current capability and a bounded next contract; it does not claim whole [REQ-15](https://app.notion.com/p/3e0fd279a6f381a08096ddf5f3c3b72b), [REQ-16](https://app.notion.com/p/3e0fd279a6f3810aadfaea2cab079101) or [REQ-17](https://app.notion.com/p/3e0fd279a6f3816bb9f8e00cb433216a) acceptance. [P05-A](https://app.notion.com/p/3e3fd279a6f381ee8ab0ea5ea614c006) and the open [font scope question](https://app.notion.com/p/3e0fd279a6f381908607d52af7b439f0) require a format/feature/fallback/script matrix before a broad coverage claim.

## Current code and evidence

`TextSource` retains editable Unicode content, family, locale, weight, italic, direction, alignment, layout and numeric parameters in native state. `evaluate_text` uses the installed Windows DirectWrite collection, shapes the string, reports actual used families and fallback/missing glyph warnings, and projects glyph outlines for rendering without replacing the authored source. On non-Windows hosts text projection and font discovery explicitly reject. SVG output outlines text and embeds no font. The authored family is a name, not a font binary or version identity.

| Area | Current evidence | Remaining coverage |
| --- | --- | --- |
| Installed formats | Windows font registry on this host lists Arial `.ttf`, Yu Gothic `.ttc`, Segoe UI Variable `.ttf`, Noto Sans JP VF `.ttf`, and several `.otf` entries. This proves installed candidates only. | No selected-font file/format/version readback or format-specific Nect fixture. DirectWrite family lookup is not a file identity. |
| OpenType controls | DirectWrite performs platform shaping. No authored feature tag, feature range, variation axis or typography control exists in `TextSource` or `evaluate_text`. | Explicit kerning/ligature/feature and variable-axis selection, persistence, API and UI oracles remain open. Do not infer every default feature is supported. |
| Fallback and color glyph | Missing family and per-run actual-family warnings are implemented. Monochrome outline projection for supported color-font bases is announced; color-only/no-outline glyphs reject. | Font substitution mapping and cross-machine identity need fixture readback. Color layer fidelity is unsupported by this outline projection. |
| Japanese horizontal/vertical | One editable text source selects either direction; DirectWrite vertical glyph orientation and right-to-left columns are used. Tests compare CJK upright and Latin sideways glyphs. | Additional punctuation, mixed-script, named fonts, GUI interaction and cold reopen matrix are needed for broad REQ-16 coverage. |
| RTL/complex script | Arabic `سلام` is shaped as a joined word in the focused projection test; locale is authored. | This single word is not a full bidi, diacritic, mixed RTL/LTR or script coverage suite. |
| Frame and overflow | Auto and fixed-frame layout exist. Tests confirm wrapped horizontal and vertical overflow warnings retain all glyphs. | Production frame/overflow behavior across fonts and direct GUI editing still needs qualification. |
| Text on path | No path reference or sampling contract is present in `TextSource`. | REQ-17 remains unimplemented; P05-B must define geometry, coordinate, identity and failure semantics before a feature packet. |

Focused Windows Release CTest `text_contract` and `text_authoring_contract` passed **2/2** on 2026-09-27. These are semantic API/core tests, not hands-on GUI or an installed-font-format certification. The registry observations above are host inventory, not a guarantee that another machine has the same files or that Nect selected a specific binary.

## Next eligible packet

R05-B should freeze a pure, testable path-sampling contract for editable Text-on-Path: distance versus normalized input, open/closed and multi-contour behavior, zero-length and reversed paths, transformed source space, tolerance, stable source IDs and fail-closed cases. Then implement the first real Text consumer through the shared Session, native and Desktop path. Do not add an unused generic sampler or mutate the Text native format before that contract and its actual caller are fixed. The separate broad font-format/feature matrix remains a local DESIGN_GATE until representative installed fixtures and the supported control set are selected.
