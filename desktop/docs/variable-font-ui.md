# Experimental variable font UI

## Explicit opt-in

The Text inspector provides **Load variable TTF (experimental)…** separately from the unchanged static TTF/OTF import. Installed/native variable font rows remain rejected; this work does not establish native font parity. The regular static parser retains its rejection of variable fonts.

This first path accepts supported monochrome TrueType `glyf`/`gvar` files. It is deliberately limited to whole, horizontal, single-line simple Latin Text. It rejects CFF2/color fonts, unsupported variation tables, complex script/mapping, combining marks, emoji, line breaks, wrapping, text paths, vertical writing, maintained character arrangements and character overrides. It is not a claim of full variable-font or shaping support. Files still require appropriate editable-embedding licenses.

Axes come exclusively from validated font metadata. Each numeric field advertises the real minimum, maximum and default; all finite values inside that range are accepted (no invented weight presets or synthetic styles). The Named instance selector lists actual validated `fvar` records and applies every advertised axis coordinate. It shows a preset only on exact coordinate equality (including default axes); arbitrary edits remain Custom axes. Instance names are labels, never persisted identity.

## Transactions and cancellation

- Startup/static documents create no Worker. Import is explicit and preparation is lazy.
- Font bytes are validated and embedded once per byte-identical font. Axis changes serialize only sparse coordinates; no per-axis font-byte copies or generated outline blobs enter JSON.
- Axis or named-instance input prepares a detached candidate and shows its ready outlines without changing history. The field's change event prepares/commits the chosen value as one authored action. Escape cancels pending preparation or the ready preview.
- Text content uses a direct prepared command on change / Ctrl+Enter. It does not use the synchronous static text-draft path. IME composition defers commits; unsupported final strings fail closed. Escape restores committed text. Variable text has no per-keystroke canvas preview in this first pass.
- Variable Text uses a small whole-text picker and an async prepared command. It offers embedded fonts and the default static font; variable rich-range selection is rejected. To load another static file onto variable Text, first choose a static/default font. Static Text retains the full family/style/favorites/installed-font and range picker even in mixed documents. Embedded variable faces can be adopted for whole Text only through prepared Apply, without a synchronous variable preview; range and native-variable rows remain disabled. Static selection, import, cancellation and history preserve the other live variable nodes’ prepared bank.
- Position and size edits remain synchronous and reuse active prepared design units. Ordinary synchronous text/axis mutations cannot commit an unprepared candidate.
- Undo/Redo prepares the destination before publishing it. JSON Open prepares a separate bridge before replacing the current document; the prior active font bank survives failure or cancellation. Successful replacement disposes the old bank.
- Each new prepared action supersedes the previous candidate. A static edit or document replacement makes outstanding candidates stale. No global asynchronous command queue blocks ordinary static editing.

The Worker contract currently bounds files to 20 MB each / 32 MB total, 8 assets, 16 axes, 4096 glyphs, 1024 characters per run, 8 runs, 50,000 commands per result, 100,000 cached commands, 4 MB per result and a 5-second timeout. Unsupported/over-limit requests preserve the committed document and display a reason.

## Focused validation

`node --test tests/variable-font-ui.test.js tests/font-dom.test.js tests/font-style-dom.test.js tests/system-font-dom.test.js`

The new happy-dom test uses the licensed real Roboto variable fixture and a Node Worker transport adapter, not a main-thread fontkit fallback. It checks no eager Worker, import/cancel, metadata bounds, preview/escape, one Undo/Redo, reuse of identical bytes, JSON reopen/cancel, ordinary unprepared command rejection, vertical/complex-mapping rejection, size reuse, delayed stale candidates, text commit and malformed import preservation. The three existing static font DOM tests retain their assertions; only their missing character-snapping import rewrite was repaired.

These tests are synthetic DOM + real font/Worker evidence. They do not establish real browser CSP, native/Tauri font parity, screen rendering quality, or end-to-end production behavior. No CSP/package/Rust changes are part of this UI work.

## Named-instance metadata bounds

The existing limit of 256 `fvar` records and 16 axes is unchanged. Coordinates,
record size, zero instance flags, subfamily name IDs and optional PostScript name IDs
are validated before preparation. Name-table versions 0/1 have bounds-checked records
and language-tag storage, capped at 4,096 records. Display names decode supported
Unicode/Windows UTF-16BE records (at most 1,024 bytes per referenced name); malformed
Unicode/control/bidi-control labels reject. English Windows names are preferred,
then Unicode platform names, then another supported Windows name. Unsupported legacy
encodings or unavailable names get an explicit “Instance N (name ID …)” label for
the actual record, never a guessed style. Unknown version-1 language references are
not used. Labels are HTML-escaped. Duplicate labels remain distinct by record index.

No preset ID or name is added to JSON: only the existing sparse coordinate contract
is authored, so reopening and Undo/Redo derive the selected instance from real axes.
The original font bytes are retained once. Zero-record fonts keep numeric axis editing
without inventing named presets. Existing format/script restrictions remain unchanged.

References: [OpenType fvar](https://learn.microsoft.com/en-us/typography/opentype/spec/fvar)
and [OpenType name](https://learn.microsoft.com/en-us/typography/opentype/spec/name).

Focused coverage: `variable-font-named-instances.test.js` checks the real fixture,
exact matching, malformed records/names and bounded Unicode decoding;
`variable-font-ui.test.js` runs actual worker preview/cancel/commit, full-axis preset
selection, custom edits, repeated/cancelled choices, Undo/Redo, JSON reopen and byte
reuse alongside existing mixed-font picker regressions. This DOM/worker harness is
not a Windows or real-browser visual acceptance claim.
