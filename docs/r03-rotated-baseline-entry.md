# R03-BASELINE-03 — quarter-turn horizontal Text baseline Snap

**Packet:** revision 1, Sol-approved bounded implementation. **Source:** clean `codex/practical-alpha@93badfa2119825c640337adce6c0d86d05ae9858`, local/tracking/fresh remote equal before mutation; takeover ACK `build/r03-rotated-takeover-ack.json` and explicit previous-owner writer release. **Authority:** DEC-71 Mission, R03 Route, Confirmed Must REQ-33, P02-C1 and the preceding R03 baseline packets.

## Bounded result

For horizontal Text whose evaluated world baseline is exactly vertical after the composed local, object and parent transforms, offer each measured DirectWrite line baseline as an **X** Smart Snap source/target. A pure X translation aligns parallel baselines; no angle, scale, text direction, authored text or native format changes. Preserve the existing horizontal Y baseline behavior, same-scope/visibility/follower exclusions, frozen candidates, distance-first 6 logical-pixel threshold, priority and stable tie order. Feedback names X, the moving line and target line. Preview is transient; release uses one `TranslateObjects` and one Undo. Later-line sources only match Text baselines. Oblique rotations and native vertical-writing Text remain unsupported until a separate measured geometry/interaction contract exists; glyph bounds do not substitute for a baseline.

## Fixed oracle

Use two same-font horizontal Text objects with measured baselines and matching +90-degree world rotations, separated along the baseline direction. Both start with authored `transform.tx=220`. The source first-line world X is 16 du to the right of the target first-line world X. Raw X drag -15 du snaps to `transform.tx=204`, with `X: first-line baseline Text baseline → target-text first-line baseline` feedback. One Undo restores tx=220. Raw -9 du is 7 du away and must not select Text baseline. A measured moving line 2, 80 du after line 1, reaches the target first line with raw +63 du and snaps to `transform.tx=284` with both line labels. A point or ordinary bounds source, oblique or vertical-writing Text, hidden/moving/follower/cross-scope target, Escape and stale release cannot produce a Text baseline commit. Preserve exact numeric/API translation, native round trip, and existing horizontal metrics. Isolate coincident bounds/Artboard candidates and assert feedback as well as coordinates.

## Verification and residual

Sol owns `src/desktop/canvas.cpp`, focused `tests/canvas_tests.cpp`, this packet, checkpoint, Git/Notion and evidence. Run focused Release Canvas/Text, DPR2 Canvas and serial regression, then proportional owned runtime/native and visible benchmark checks if the production path changes. Interactive GUI evidence must remain `BLOCKED_ENV` if the current Computer Use inventory block persists; do not claim a drag from API or Qt tests. This slice does not close arbitrary-angle, vertical-writing baseline or full R03/REQ-33.

## Candidate evidence (2026-09-26)

Sol reviewed the composed Text path/world transform, axis guards, measured line order, source/target eligibility and unchanged distance/priority/tie and release logic. The focused Canvas fixture passed first-line raw -15 to `tx=204` with exact X feedback, seven-du miss, cancel, one Session commit/Undo and native encode/decode equality. A separate measured line-2 fixture passed raw +63 to preview `tx=284` with both line labels. Existing horizontal, vertical-writing, unmatched-rotation and ordinary-source exclusions remain covered by the Canvas suite. Release build passed; DPR2 Canvas 1/1 and full serial CTest 41/41 passed after the final test change.

The production visible Canvas `--layout` benchmark passed all 30-fps p95 interval gates. Representative object translation p95 was 21.77 ms below 33.33 ms. The scene contains no rotated Text, so this is a general Canvas hot-path check, not a typography stress measurement. Independent desktop API/native cold reopen and interactive GUI drag were not run in this checkpoint; the preceding Computer Use inventory block remains the last GUI capability observation. No GUI PASS is claimed.
