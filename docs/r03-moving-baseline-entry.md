# R03-BASELINE-02 — moving multiline Text baseline Snap

**Packet:** revision 3, Sol-approved bounded implementation. **Source:** clean `codex/practical-alpha@b9ec85f052a7442ebe9eea22ae0aaa3d714d528b`, local/tracking/fresh remote matched at freeze. **Authority:** DEC-71 Mission, R03 Route, Confirmed Must REQ-33, [R03-BASELINE-01](r03-baseline-entry.md) and repo `AGENTS.md`. The focused oracle corrected a wrong 96-du moving-line assumption and isolated an Artboard-min collision; implementation scope is unchanged.

## Bounded result

For one moving, horizontal, axis-aligned multiline Text object in the entered scope, offer every *measured* DirectWrite line baseline as a Y Smart Snap source feature against a stationary eligible horizontal Text baseline. This packet adds source line 2. Keep the existing stationary first/subsequent-line targets, visibility, follower and scope exclusions, distance-first 6 logical-pixel inclusive threshold at frozen zoom, DPR independence, priority and stable tie order. Feedback names both the moving source line and stationary target line. Preview stays transient; release remains one `TranslateObjects` Session transaction and one Undo. No authored text/native 0.14, core command, API numeric placement, control or setting change.

Vertical or rotated Text offers no horizontal line baseline. Glyph-box bounds and estimated line height never substitute for a measured line. The packet does not close rotated/vertical baseline, full R03 or REQ-33.

## Fixed positive and negative oracles

- `R03-BASELINE-02`: moving `H\nH` at origin `(40,80)` and stationary `H` at `(320,80)`, both with authored `line_spacing=80`, same installed font and identity transforms. Set the Artboard top to `y=20` to isolate its min edge from the intended Text candidate; keep the 640×480 unit-scale scene. DirectWrite measures moving line 2 **80 du** below line 1, and the stationary first line aligns with the moving first line before the drag, so target first minus moving second is **-80 du**. A Y-only raw body drag of **-79 du** snaps committed `transform.ty=-80`; feedback includes `line 2 baseline Text baseline → target-text first-line baseline`. One Undo returns `ty=0`; native save and independent reopen retain the final `ty=-80`.
- `R03-BASELINE-NEG-02`: a raw -73-du drag (7 du from source-line-2 alignment) does not use line 2. A one-line moving source offers no line-2 source. Vertical or rotated moving source offers no baseline. Hidden, moving/follower and cross-scope targets remain ineligible. Escape or stale release leaves authored bytes/revision/history unchanged. Exact numeric/API translation remains exact without Snap.
- Isolate or diagnose coincident ordinary bounds/Guide/Grid/Artboard corrections; assert feedback as well as coordinates so an ordinary candidate cannot satisfy the positive oracle by accident. Preserve source first-line and target second-line behavior.

## Ownership and verification

A fresh disposable Luna Max Worker owns only `src/desktop/canvas.{hpp,cpp}` and focused `tests/canvas_tests.cpp`. It must not edit other files, commit/push, touch Notion or user projects. Others are working in the repository; do not revert their edits and adjust to concurrent changes. Sol owns packet, review/repair, verification, Git/Notion reconciliation and the next checkpoint.

Run focused Release Canvas/Text tests, then serial regression. Exercise owned GUI/API/native save and cold reopen when host control is available; classify the current Computer Use app-inventory block explicitly if it persists. Use proportionate visible benchmark if the change affects drag performance, and keep its scene scope explicit.

## Closure evidence (2026-09-26)

Sol reviewed the source-kind check, line order, unchanged distance/priority/tie logic, and exclusion of later-line sources from ordinary targets. The measured fixture uses matching `line_spacing=80`; first baselines coincide and moving line 2 is exactly 80 du below them. The `-79` raw drag chooses the Text-baseline candidate and commits `ty=-80` in one Session transaction. Focused Release Text/Canvas passed 3/3; the Canvas test covers the 7-du miss/cancel, one-line source, point/bounds exclusions and rotated/vertical source omissions. `QT_SCALE_FACTOR=2` Canvas passed 1/1. Full Release build and serial CTest passed **41/41**.

An owned desktop API Session `5af8f70e-7c53-47a3-90ba-d4ce010afa89` created the two Text objects and applied exact numeric `dy=-80` at revision 2. Native **0.14** `build/r03-moving-live-01a0d9be/moving-baseline.nect` has Document `b04570bb-c324-44ee-b68a-9f46a34862f1`, `ty=-80`, SHA-256 `58719DA4923871DE794626B26407D4FA47542638CA796F524C5CCB923A233971`. A separate desktop Session `0c5ac3dd-e556-477f-95c0-33961837938e` cold reopened that Document at revision 0 with `ty=-80`. Both owned processes were closed. This API/native exercise verifies placement and persistence; the focused Canvas test verifies Snap selection and feedback.

The visible production Canvas `--layout` benchmark passed every 30-fps p95 interval gate. Representative object-translation p95 was **21.44 ms** under the 33.33-ms budget; the benchmark layout scene has no dense multiline Text, so it is not a typography stress result. Standalone interactive GUI acceptance remains `BLOCKED_ENV` because Computer Use returned no app/window inventory in this Task; no visible drag claim is made.

Rotated/vertical Text baseline semantics and full R03/REQ-33 acceptance remain open. R02 non-Scalar and R11-I design gates are unaffected.
