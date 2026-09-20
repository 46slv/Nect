# FIRST_USABLE — M1 contract

M0 is the headless kernel baseline. The M1 desktop loop was implemented and
exercised on Windows on 2026-09-20; the evidence and its limits are below.
The practical-alpha Mission continues beyond this milestone.

## Windows evidence — 2026-09-20

- F1/F2: actual Windows 1400×900 UI created two Curves; Inspector X=315 was
  independently read through the live API. Canvas widget tests exercise point,
  both handle directions, creation/collapse, transformed-parent coordinates,
  one-gesture Undo and Escape/return-origin cancellation.
- F3: GUI property search linked the second Curve to the first; the linked field
  and anchor visibly changed color. Cross-object drag pick-whip, target retention,
  Undo and cancel are exercised through actual Window mouse events. Name lookup,
  rename, point/order changes and stable references are covered through MCP.
- F4/F6: atomic native Save/Save As with previous-file backup, real restore,
  write failure preserving the existing file, failed open preserving the Session,
  draft/gesture isolation and stale-document identity rejection pass. This is not
  a claim that every disk/power-loss failure is recoverable.
- F5: the formal stdio MCP client test initializes/lists/calls through the sidecar
  into the desktop-owned Session: 24 Paths, seed 7821, 41 mutations, undo/redo,
  save/restart, deliberately killed process and recovery reopened in a fresh desktop.
- F7: native bindings survive export; Python's independent XML parser verifies
  24 paths and CLI tests check hand-specified anchor/Artboard coordinates.
- F8: live low-noise Canvas/tree/Inspector has no timeline. Initial real-use
  friction (recovery status destroying input focus, UUID-heavy property labels)
  was corrected. The visible viewport measurement below meets the 30 fps floor
  on this fixture; subjective comfort continues to be refined during production.

Build: MSVC 19.29.30137 Release, Qt 6.5.3 Windows raster QWidget path, Boost 1.85.0.
Machine: Ryzen 9 3900X, 64 GiB RAM; RTX 3070 Ti and GTX 1650 installed (no claim
that this raster renderer uses either GPU for path evaluation). Window 1440×900,
Canvas 893×824 logical pixels, DPR 1, 96 logical DPI, 1920×1080 60 Hz display.

`canvas_benchmark` creates the full production Window with semantic commands,
warms each operation for 12 inputs and measures 90 inputs at a requested 16 ms
cadence. Timing spans real exposed QWidget paint observations/event-to-paint;
it does **not** measure DWM/compositor or scan-out timestamps. Raw observations
are written to `build/canvas-benchmark.json`; the source reconstructs both fixtures.
No offscreen render or forced `repaint()` is used for this measurement.

| Scene / operation | Interval p50 ms | Interval p95 ms | >33.333 ms frames | Release + UI commit ms |
|---|---:|---:|---:|---:|
| 2 Paths / pan | 16.09 | 17.45 | 0 | 0.54 |
| 2 Paths / zoom | 15.97 | 17.26 | 0 | 0.00 |
| 2 Paths / point | 15.81 | 17.41 | 0 | 5.25 |
| 2 Paths / handle | 15.97 | 18.10 | 0 | 4.90 |
| 2 Paths / transform | 15.94 | 17.65 | 0 | 4.80 |
| 80 Paths, 320 points / pan | 15.96 | 19.31 | 0 | 0.33 |
| 80 Paths, 320 points / zoom | 16.14 | 17.23 | 0 | 0.00 |
| 80 Paths, 320 points / point | 17.45 | 22.53 | 0 | 32.11 |
| 80 Paths, 320 points / handle | 17.21 | 23.65 | 0 | 32.82 |
| 80 Paths, 320 points / transform | 19.63 | 23.22 | 0 | 21.60 |

Initial release stalls reached 131.75 ms because Inspector evaluated the full
document for each row and rebuilt the object tree. One evaluated Inspector
snapshot and structure-only tree rebuilding brought release below 33.333 ms in
this run. The lightweight 60 fps p95 target is **not achieved** (17–18 ms interval
p95, including input/OS scheduling). Do not generalize these results to larger
scenes, later operators, other displays or GPU presentation timing.

Six CTest entries pass: core, desktop-host persistence, Canvas interaction,
Window interaction, headless process and same-desktop formal MCP.

### Post-M1 Shape stack check — 2026-09-20

Native 0.3/ordered-paint implementation: eight CTest entries pass, adding primitive
and shape-stack contracts. The same full Window benchmark was rerun after the
renderer change. Hardware, viewport and measurement boundary match the table
above. Additional `--repeat` mode measures two four-anchor sources, each with
Stroke + Fill + 12-copy Repeater: 24 virtual paths / 48 paint layers. This is
actual exposed Qt paint/input timing, not compositor/scanout latency.

| Scene / operation | Interval p50 ms | p95 ms | >33.333 ms | Release + Inspector ms |
|---|---:|---:|---:|---:|
| 2 Paths / pan | 16.09 | 18.20 | 0 | 0.32 |
| 2 Paths / zoom | 16.11 | 17.17 | 0 | 0.00 |
| 2 Paths / point | 15.90 | 17.62 | 0 | 6.93 |
| 2 Paths / handle | 15.86 | 17.51 | 0 | 7.72 |
| 2 Paths / transform | 16.03 | 17.36 | 0 | 6.11 |
| 80 Paths / pan | 16.03 | 17.79 | 0 | 0.48 |
| 80 Paths / zoom | 16.08 | 17.74 | 0 | 0.00 |
| 80 Paths / point | 22.73 | 30.16 | 0 | 30.83 |
| 80 Paths / handle | 20.90 | 28.48 | 0 | 29.43 |
| 80 Paths / transform | 20.74 | 25.92 | 0 | 30.50 |
| 48 repeated paint layers / pan | 16.03 | 18.24 | 0 | 0.37 |
| 48 repeated paint layers / zoom | 16.10 | 17.23 | 0 | 0.00 |
| 48 repeated paint layers / point | 16.03 | 17.53 | 0 | 17.24 |
| 48 repeated paint layers / handle | 16.08 | 18.11 | 0 | 20.55 |
| 48 repeated paint layers / transform | 16.25 | 17.80 | 0 | 15.99 |

30 fps passes; 60 fps remains a target. Raw samples are in ignored
`build/canvas-benchmark-stack.json` and `build/canvas-benchmark-repeat.json`.
The denser ordinary scene now has less margin; profile future rendering changes
against this measured fixture instead of assuming the original M1 timings persist.

## Flow

Qt new document -> path creation -> point/handle selection -> Path Inspector numeric edit ->
pick-whip/name reference -> direct drag -> native save -> full restart/reopen ->
formal MCP client mutates the same live document -> GUI reflects it -> Undo/Redo -> SVG export.

The same semantic create/edit/save/undo path must also be machine-operable without GUI mouse replay so Astra/CI can stress the document model repeatedly.

## Build and try in increments

**A — manual + semantic automation loop.** Real creation/selection of paths, Canvas plus contextual numeric editing, one-gesture Undo/Redo, cancellation, safe native save/reopen and SVG output. Add the necessary create/edit commands to the existing core; the demo document is not a substitute for creating paths. Expose the same semantic commands through the machine surface as they become available so seeded scripted sessions can create/edit/save/reopen/read back documents without waiting for the final MCP wrapper.

**B — connected properties.** Pick-whip and explicit name/path authoring resolve to stable IDs. A driven value is visibly distinguished from its authored source. Test rename/reorder and unbinding without ambiguous retargeting. The same cases must be reproducible through API/MCP readback.

**C — same-session formal MCP.** A formal MCP client initializes, lists tools and edits that same live desktop document. The desktop reflects the edit and can undo it. Different sessions/documents and stale revisions must not be confused. Run deterministic multi-command scenarios against temporary documents and retain revision/error/readback receipts.

The agent can overlap independent work, but should keep a usable manual loop and not wait for all future features before collecting UX feedback. Mark partial completion as A/B/C, not as M1 Done.

## Completion checks

- F1: Windows standalone launches and creates/selects at least two paths.
- F2: point X/Y and all in/out angle/length values are editable in Inspector and by direct manipulation.
- F3: pick-whip/name authoring resolves to stable IDs and survives rename/reorder.
- F4: save/reopen preserves values, bindings and order; source files are not silently lost.
- F5: actual MCP initialize/list/call reaches the same live Session, while the same semantic operations can also be scripted/read back without GUI event replay.
- F6: failed edits leave no partial state; GUI and API/MCP mutations participate in Undo. A completed drag is one undo entry and a cancelled drag leaves no edit.
- F7: SVG export is checked by an independent parser/renderer and native bindings remain.
- F8: UI is canvas-first, low-noise, spatially stable and usable for the actual edit task. The standard still-graphics workspace has no timeline, playhead, video transport or timecode. On the recorded reference machine + M1 fixture after warm-up, pan/zoom/point-handle drag/basic transform maintain at least 30 fps equivalent with p95 frame interval <=33.3 ms; lightweight scenes target 60 fps. Record the scene, viewport, hardware and timing used for the claim.

## Automated operation / performance evidence

Human-free testing is expected for semantic correctness, persistence and much of performance:

- construct seeded temporary scenes through the command/API surface
- run repeated create/edit/link/reorder/save/reopen/undo/export sequences
- assert revision, stable IDs, errors and evaluated state
- collect machine-readable p50/p95 frame/evaluation timing where the desktop benchmark exposes it
- fail on partial commit, stale-session edit, silent unsupported fallback or deterministic-seed drift

This does not replace human/UI evidence when the question is discoverability, comfort, motion feel or visual quality.

## Hands-on decision loop

Use one saved example for a narrow question: e.g. choosing a handle, editing its angle/length, switching selection, linking another object's value, or finding the grid toggle. Make one hypothesis, implement it against the existing Session, perform the task at the actual working window size, then keep/change/remove that interaction.

Record only the observed friction, chosen change and remaining uncertainty in the current issue/PR or checkpoint. A screenshot establishes appearance, not interaction correctness. No permanent A/B framework, exhaustive UI matrix or new validator is required for each experiment.

Before/after a presentation change, check that the same document still opens, the selected ID and bindings have not drifted, and Undo behaves as expected. If persisted semantics must change, use the evolution contract in ARCHITECTURE.md rather than starting a parallel model.

## Excluded from M1

Full AI/PSD writer, all blend modes/effects, OFX host, full PointSet/field/packing engine, complete production renderer, RAW, generative providers, full print/RIP production, branching history, marketplace, cloud sync. Local repeat/mask/scatter features are later slices unless a current instruction explicitly selects a bounded probe.

## Early risk probes

- Japanese horizontal/vertical shaping, punctuation, IME, fallback.
- Transparent overlap/group isolation/mask/color-space semantics, including a small subset of blend modes before full AE coverage.
- Representative SVG/PDF/AI interop observations without modifying originals.

These reduce schema risk; they are not gates requiring full product parity. A failed probe may justify a local seam or migration before that capability is implemented, not a wholesale rewrite.
