# Direct anchor marquee verification

Base: PR64 `3b56815f39b90147b1e5b8ad648891eb826b4cba`, branch
`codex/angled-editor-grid`. Product/build source:
`92dea04f6242f2e5b259f2e786b839aa8c1218b6`.
Three product files add 224 lines and remove five: the anchor-center query,
read-only resolved route IDs and main.js interaction/identity handling.
No schema or authored geometry changes. This is the new parent-selected slice
described in [direct-anchor-marquee.md](direct-anchor-marquee.md), not a claim of
implementing an older adopted requirement.

## Results

37 focused Node checks passed, zero failures/skips. The final combined command:

```text
node --test tests/direct-anchor-marquee.test.js tests/direct-selection.test.js tests/point-controls.test.js tests/direct-anchor-marquee-dom.test.js tests/canvas-artwork-marquee-dom.test.js tests/scoped-point-selection-dom.test.js tests/guides-dom.test.js
```

Coverage includes inclusive anchor centers, reverse/empty rectangles, handle and
segment exclusion, stable source order, compound holes, rotation plus nonuniform
scale, two zoom/pan mappings, replace and starting-snapshot Shift union, focus and
panel synchronization, four-CSS-pixel threshold, locked/driven selection with
atomic edit rejection, Escape/pointercancel/capture-loss/blur and late events.
Stale artboard, Session and owner changes discard previews. A real Multi Path
input to Path Remap is changed while keeping ordered point IDs; its changed
fingerprint/revision rejects the captured set without creating/rebinding edits.
The GUI busy guard rejects interleaved authoring; a test-only independent-owner
adapter creates the stale revision through the real Session command for that
defensive test. Existing whole-artwork marquee, scoped selection and Guides pass.
Exact JSON/SVG, dirty/file state and Undo/Redo stack snapshots remain unchanged
after selection or cancellation.

Frontend build passed (4.59s, existing Vite chunk-size warning). Locked offline
native custom-protocol build passed (41.39s). Changed-file formatting, syntax,
diff whitespace and native-build configuration checks passed.

Native Tauri/WebView2 CDP synthetic interaction on SHIRO-WS passed two distinct
configurations: the same 35-degree rotation with width scale 1.5/height scale 0.7 at
100% with pan [20,-12], and 120% with pan [-24,16]. The fixture is 600 x 500, two
layers, six nodes/four connections, with no images or dither. Anchors selected in
the actual rendered positions match the point-panel rows and stable active ID.
The larger-then-smaller preview drops intermediate hits; Shift retains only its
starting set. Escape and DOM pointercancel with late CDP mouseup preserve exact
selection/document/history/file/SVG. Threshold click, reverse drag and exact JSON
reload also pass. No page errors.

A screenshot readout concern prompted one bounded recheck of those same two
configurations. DOM text is exactly 100%/120%. The extra harness initially compared
the native CTM to an ideal scale at 1e-6 and failed: at 120%, inline width 720px has
measured width 719.9971313476562px and CTM scale 1.1999952334868575. The final check
compares the actual before/after CTM exactly, plus exact inline dimensions and
readout, and passes. No product fix or tolerance widening was needed. The final
native receipt contains the observed matrices. Preview and selected screenshots
were captured at the configured zooms, before reload, and visually inspected.
Repeated configurations are not counted as new acceptance cases.

Initial DOM preparations were corrected: unsupported fixture handle mode,
anchor-versus-handle marker selector, explicit locked-layer targeting, existing
guide/parameter command names, and an unstable Multi Path input for the real
Path Remap fingerprint case. Exact-state assertions remain. The large initial
inline-source failure log stays in the workspace; the candidate records its hash
and a concise preparation receipt rather than copying the embedded source dump.

## Candidate and boundaries

Native executable: 10,006,528 bytes, SHA256
`588b0bf28380b7cac16076b44fcccda1107425f76a4736ce5cf9cd8ae13f6666`.
No product/build input changed after the product-source commit; later changes
are verification assertions/comments and this receipt.

All 22 prior executable hashes and 119 sealed files across the latest PR63, PR64
and Fireworks Ring outputs still match. Dedicated executable-adjacent acceptance
profiles isolate test storage. Both dedicated processes exit through the test
close route after preserving their own fixtures; there is no forced termination
or change to a user's running app/document. This CLI fixture route is not OS Save.

The selection applies only to the currently resolved single editable source.
Existing unsupported/generated/ambiguous route protections remain. Preview is
detached and does not regenerate the complete graph/panels on pointermove.
Physical/OS input, native Save/Open dialogs and normal clean cold reopen remain
unverified on this host. No broader performance claim, unrelated audit, main
merge, release or Library upload was performed.
