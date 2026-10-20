# Graph autopan work

Baseline: `acca2616ab38c703bcf3e4b0ef465eeae7c62ffc`
(`codex/nect-regression-validation-v2`).

The Node/happy-dom probe imports the shipped `main.js`, dispatches real pointer
events, and counts entry into `updateGraphGesture`, `updateGraphBounds`, and
`drawWires`. It uses the six-node default document, a controlled animation-frame
clock, and an explicit 500 × 500 scroll viewport with browser-like clamping.
These are function-call counts, not browser layout or frame-time measurements.

| Operation                                     | Baseline gesture / bounds / wires | Fixed gesture / bounds / wires |
| --------------------------------------------- | --------------------------------- | ------------------------------ |
| One node pointer move                         | 1 / 1 / 1                         | 1 / 1 / 1                      |
| Node held still, 60 frames in viewport center | 60 / 60 / 60                      | 0 / 0 / 0                      |
| Node held at clamped top-left, 60 frames      | 60 / 60 / 60                      | 0 / 0 / 0                      |
| Node first edge frame, zero elapsed time      | 1 / 1 / 1                         | 0 / 0 / 0                      |
| Node frame that actually scrolls              | 1 / 1 / 1                         | 1 / 1 / 1                      |
| One wire pointer move                         | 0 / 0 / 1                         | 0 / 0 / 1                      |
| Wire held still, 60 frames in viewport center | 0 / 0 / 60                        | 0 / 0 / 0                      |
| Wire first edge frame, zero elapsed time      | 0 / 0 / 1                         | 0 / 0 / 0                      |
| Wire frame that actually scrolls              | 0 / 0 / 1                         | 0 / 0 / 1                      |

The baseline probe completed its semantic assertions, then failed the final
call-count assertion with the baseline counts above. The fixed probe passes.
Its regression assertions cover synchronous pointer preview, unauthored drag
preview, single-entry Undo/Redo, cancellation, continued autopan with a stationary
edge pointer, commit of the scrolled preview, wire preview, connect/disconnect,
and cancellation of the repeating callback.

The fix only gates the autopan callback's preview/draw work on a change in actual
scroll position. The callback still checks edge motion every frame, advances its
clock, grows the graph, and scrolls. Pointer and modifier handlers retain their
synchronous updates. Comparing actual scroll positions also avoids unnecessary
work when an edge velocity is nonzero but the browser clamps the scroll.

## Validation

From `desktop`, using Node 24.14.1 and the committed lockfile:

```sh
node --test tests/graph-autopan-work-dom.test.js tests/graph-node-snap-dom.test.js tests/graph-node-snap.test.js tests/gesture-session.test.js tests/command-session.test.js tests/direction-snapping.test.js
node --check src/main.js
node node_modules/prettier/bin/prettier.cjs --check src/main.js tests/graph-autopan-work-dom.test.js docs/graph-autopan-work.md
node node_modules/vite/bin/vite.js build
```

The focused test group passes 29/29. Before source edits, the existing
`graph-overview-dom.test.js` failed at line 124: it expected six overview nodes but
found zero. The same assertion fails after the fix. This baseline failure is
outside this patch; the test is retained.

## Limits

No live UI was operated. Browser frame times, representative user document size,
subjective responsiveness, and the installed Windows EXE version remain
unverified. No EXE was built or distributed. The source build does not establish
EXE behavior.

This change does not coalesce multiple pointer events, optimize per-connection
DOM queries, or eliminate every possible draw during actual scrolling (the
existing wiring scroll listener can also draw). Overview remeasurement within
remaining `drawWires` calls is unchanged. There is no claim that every layer/node
evaluation recomputes: core evaluation has reuse caches, and `computed`/`reused`
statistics and real runtime timings were not measured here. No changes were made
to core evaluation, luminance masking, shared DOM structure, or PR35 ordering.
