# F1: static family/style selection

Implemented 2026-10-17. This is a bounded static-face selection UI, not full font support.

## User flow and identity

Open the existing Text font control, or the selected-character font control. The searchable Font picker now offers:

- Font family, populated only from parsed embedded/bundled metadata and optional native catalog descriptors.
- Available weight, containing the recorded OS/2 weights of faces in that family. It is a filter, not an axis or a synthesized weight control.
- A keyboard-accessible face/style list showing the actual subfamily and weight. Pick a real row, preview it, then Apply font or Cancel.
- All families / flat list and explicit unknown family/style/weight labels. Legacy three-field native catalogs remain usable. Browser sessions use bundled and embedded faces; import TTF/OTF first to add other available faces.

Unsupported entries retain their rejection reason and cannot be selected. Search includes family, style, weight and rejection reason. Favorites remain available; previous name-based favorites are recognized and migrated to the currently visible ID keys on the next favorite edit. Duplicate display names stay distinct and show face IDs. Installed favorites follow opaque catalog IDs and may need reselection when those IDs change across catalogs/sessions; no cross-session installed-face identity is invented.

Metadata is display/filter data only. A selection continues through the existing Session text-font gesture and adoption machinery. Installed bytes are read only for a selected preview, never to classify the whole catalog. Embedded metadata comes from already parsed project faces. Exact bytes, asset references, outline geometry and export remain authoritative. No CSS weight, synthetic stroke, scaling, schema change or geometry-engine changes were added.

Family/weight changes can select the first matching available face for preview, just as existing search filtering does; nothing is committed until Apply. Escape/Cancel restores the committed document. Whole-Text and selected-range controls share the same picker and transaction.

## Evidence

Focused command:

```
node --test tests/font-style-dom.test.js tests/font-dom.test.js tests/system-font-dom.test.js tests/font-preview-session.test.js tests/font-metadata.test.js tests/system-font-metadata.test.js tests/fonts.test.js tests/rich-text-dom.test.js tests/rich-text.test.js
```

Result: **45 passed, 0 failed, 0 skipped** on this workspace. One production Web build (`npm run build`) passed; Vite retained its >500 kB chunk advisory (main JS approximately 1,014 kB before gzip).

New `tests/font-style-dom.test.js` is happy-dom integration, not visual browser acceptance. It uses already installed licensed DejaVu Sans Bold and Oblique 2.37 files, with provenance/hashes in `font-metadata-contract.md`; no downloads, installation or font copying. It explicitly skips if either local face is unavailable. It verifies:

- Lazy catalog grouping; family and weight filtering; actual Bold/Oblique labels; unknown metadata; disabled reasons; same-name opaque-ID distinction and independent favorites.
- Whole-text Bold and selected-range Oblique adoption, exact embedded bytes, real changed SVG outlines, stable logical character IDs, unaffected neighboring range styles, one-step undo/redo, save/reopen equality.
- Mixed actual-style project exported through the headless CLI matches the UI SVG byte-for-byte.
- Private preview does not change saved JSON or export; late responses cannot replace a newer preview; cancellation while reading does not commit.
- Flat legacy entries remain selectable and old-project SVG is unchanged after reopening.

Existing focused checks additionally retain malformed-byte, embedding, variable-font and collection rejection, range editing, metadata validation, missing-glyph and stale-document protections. These checks do not establish general shaping, RTL, arbitrary fonts, variable instances, fallback or collection import support.

## Remaining acceptance boundary

No native Rust compilation, Windows host enumeration/IME, actual browser visual acceptance, user-computer check, or full test suite was run for this UI change. Native metadata availability still depends on the separately implemented catalog path and its own host acceptance. The UI is backward-compatible when descriptors are absent. No new font fixture binaries were distributed.
