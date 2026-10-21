# Numeric Escape / wrap migration maintenance receipt

Base: PR54 `codex/text-wrap-reference-resize`, verified remote
`a6fec92666fa98c6af4674c00d41614aa309beaf`.
Product/build commit: `94a6595a899f5790420a5836c5d837e16d35c14d`.
Branch: `codex/legacy-numeric-wrap-tests`.

| Inherited failure                                 | Cause and repair                                                                                                                                                                                                                                                                                                                       |
| ------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `numeric-scrub-dom.test.js:119`, preview500 vs480 | Actual product defect. The early numeric-expression Escape branch reset the input but returned before cancelling its active numeric Session gesture. Late pointerup committed the preview. One product line excludes started scrubs from keyboard-expression input classification so existing gesture cancellation/key ownership runs. |
| `text-wrap-dom.test.js:82`, schema24 vs8          | Obsolete test assumption that a feature minimum is the current document version. Assert max(existing,8) separately from real7-to8 promotion, persisted reload, validation rejection and exact Undo/Redo. No schema/model change.                                                                                                       |

The scratch trace disproves a stale-control/mock hypothesis: controls stayed
connected. Committed X460/revision0 and displayed460 remained after Escape,
but the gesture still owned preview480. Pointerup committed480/revision1; the
next drag then previewed500. Exact base main/test sources, trace and derived
`root-cause.json` are retained, not a repeated full baseline audit.

All original assertions remain. Numeric cancellation now explicitly requires
gesture release before pointerup and exact JSON/file/command neutrality after
late pointerup. Active operator keys cannot turn the number into an expression
draft. Ordinary typed12+3 still commits15 and one Undo restores100; typed Escape
still discards an uncommitted expression. The new test waits for the scheduled
Undo-button render rather than clicking a stale disabled button.

Wrap checks load an actual unwrapped schema7 document, enable width-only wrap,
require schema8, reload persisted JSON, reject the same wrapped data tagged7,
Undo exactly to7 and Redo exactly to8. Current factory24 stays24. The old vertical
rejection expectation also predates the adopted `japanese-basic-wrap.md` contract:
writing-mode changes convert width to basic vertical height, require minimum34,
reject tag33 and Undo exactly to the prior width-only mode/schema. Product
vertical behavior is unchanged. The numeric suite's previously blocked local
preference case now opens the existing Align panel through its toolbar toggle;
its locked-document/no-history assertions still run unchanged.

Focused verification: **7 pass, 0 fail, 0 skipped** across both repaired suites,
`numeric-scrub.test.js`, `character-numeric-scrub-dom.test.js` and
`authored-value-dom.test.js`. A schema-only follow-up with the stricter basic
minimum assertion also passes. No unrelated suite or overall audit rerun.

Native Tauri/WebView2, SHIRO-WS: one focused scenario run before/after using CDP
synthetic mouse/keyboard on an actually focused number input. Base EXE from PR54
reproduces the violation: X200 -> preview220 -> Escape leaves a parameter gesture
-> pointerup commits220, revision0->1, Undo enabled. Patched EXE cancels the
gesture on Escape, leaves X200/revision0/Undo disabled, and pointerup preserves
exact pre-gesture JSON/SVG/file/history state. Page errors0 in both runs; viewport
1469x827 CSS px, DPR1.0900000334. Receipts/screenshots retained; patched screenshot
inspected. Baseline violation is diagnostic evidence, not a passing contract.
Both owned processes86400/136844 exited normally; no force termination.

Because the cancellation fix changes product code, frontend/config checks and
locked/offline release native build were run and pass. Existing Vite chunk-size
warning remains. No Rust/core/schema, Y-reference, Color, dither or panel changes.
All thirteen earlier sealed EXE hashes match; diagnostic-only outputs remain.

Candidate: `task-13/numeric-wrap-maintenance-artifact-94a6595/nect-svg-desktop.exe`,
10,002,432 bytes, SHA256
`9441c9e0870c4540460f7b31ad516ea74699cbccf0dd3b00a95432ebf9e9df6c`.
Built JS `index-hpp_NWm1.js`, SHA256
`d3bd58a24f46c152389557daca8ade20f25efc41fff40537ae410ea12d4f5b4d`.
Dedicated evidence retains focused/schema logs, trace/base sources, before/after
native JSON/screenshots, build/normal-close receipts and prior candidate hashes.

Not run: physical input, actual OS focus loss, OS save/open/cold reopen, IME,
unrelated geometry/rendering/performance audits. Y Center/Bottom Canvas wrap
support remains a separate unmet capability. No main merge, release, Library
upload or history rewriting. The two scoped inherited failures are resolved;
this is not an all-Nect completion claim.
