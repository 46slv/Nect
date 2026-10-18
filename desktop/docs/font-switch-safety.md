# Explicit OS Text replacement

The OS font inspector names the stored source face. **Use DejaVu Sans** is an
explicit replacement with the bundled default face; it does not recover or
reactivate the missing original font. The resulting face is also named in the
success status.

The existing common `clear-os-font-text` command now evaluates the complete
replacement Text before committing. Missing replacement fonts, missing glyphs
or unsupported replacement layout refuse atomically: original text, OS face,
saved outlines, revision and history remain intact. Supported replacement is one
Undo. A missing original OS font is not required to display/export its saved
outlines or to perform a supported explicit replacement.

This closes a separate path from DirectWrite's existing exact-face/fallback
guard and normal font-picker preview validation; neither was reimplemented.
No schema/native-font changes, fallback chain or universal font guarantee is
introduced. The temporary browser/IME input font is not an export authority.

Focused evidence: `tests/os-font-text.test.js`, `tests/os-font-outline-dom.test.js`,
`tests/desktop-font-text.test.js` (12 passing checks), and production bundle/CSP
`tests/font-switch.e2e.js` (synthetic stored outline, real bundled DejaVu). The
Japanese refusal and supported Latin replacement both use actual UI controls;
Undo/Redo and JSON reopen retain exact SVG. This is not manual IME/DPI acceptance.
