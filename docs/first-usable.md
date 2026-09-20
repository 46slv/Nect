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

## Editable Text slice evidence — 2026-09-20

Native 0.6 Text passed all twelve CTest entries: DirectWrite horizontal/vertical
projection, mixed Japanese/Latin, Arabic contextual shaping, tracking/line spacing,
missing-font diagnostics, color-glyph handling, overflow retention, authoring
atomicity, native 0.1–0.5 migration, Window content/style controls, concurrent
content-edit protection, and formal MCP edit/save/restart/abnormal-exit recovery.
The final numeric-focus fix passed the focused Window suite and actual Windows use.

`scripts/create_text_demo.py` created `examples/typography-poster.nect` and its
outlined SVG through the live API: seven editable Text objects, two Japanese
vertical runs, horizontal Japanese/Latin, a linked subtitle size and the retained
gradient/repeated ornament. No font binary is included. Actual Windows editing
changed the heading from 形と光の庭 to 色と光の庭, then size 54→60; semantic
readback confirmed one revision per edit and linked subtitle size 21.1111.
Two GUI Undo actions restored the entire original authored document. The content
edit was saved separately under ignored `build/text-manual.nect`.

Observed friction: Return in a numeric field rebuilt the Inspector and jumped its
scroll position. Return now restores the same property focus and scroll offset;
focus-out edits still allow the user's new target to receive focus. Rebuilt Text
controls passed the regression test and a visible size 54→56 edit stayed in place.

Visible `canvas_benchmark ... --text` used the same Windows hardware and 1440×900
full Window as the prior baseline, 893×824 Canvas, DPR 1, 96 dpi, 12 warm-ups,
90 measured inputs at 16 ms. The fixture adds eight Yu Gothic 21 du mixed-script
Text objects to two four-anchor curves. Point/handle operations edit a curve;
translation edits a Text object. Qt paint completion includes the shared Session
preview path, not compositor/GPU presentation.

| Operation | p95 interval ms | Max interval ms | >33.333 ms | Release/UI commit ms |
| --- | ---: | ---: | ---: | ---: |
| Pan | 17.15 | 18.16 | 0 | 0.03 |
| Zoom | 17.36 | 18.15 | 0 | 0.00 |
| Point drag | 17.51 | 21.14 | 0 | 13.98 |
| Handle drag | 17.67 | 19.64 | 0 | 13.55 |
| Text translation | 18.96 | 22.72 | 0 | 31.84 |

The 30 fps p95 floor passes; the 60 fps interval target remains unmet. Raw timing
is retained locally in ignored `build/canvas-benchmark-text.json`. This bounded
fixture does not establish performance for arbitrarily large typography documents.

## Color workflow evidence — 2026-09-20

All fourteen CTest entries pass with native 0.7 migration, named-color propagation,
reference-protected deletion, strict rich clipboard handling, draft conflicts,
copy-only bounded history and formal MCP native restart/crash recovery. A later
swatch fix passed focused GUI tests: identical pixels across all eight Qt icon
mode/state combinations prevent selection tint from misrepresenting colors.

`create_color_demo.py` produced `examples/named-color-poster.nect` plus SVG from
the native 0.6 typography poster. Three named colors explicitly drive nine paint
or gradient-stop properties. Windows GUI changed Ivory typography FFF4D6FF to
FFB8C8FF; API readback verified all three dependent Text colors and links. Saved
manual result is in ignored `build/colors-manual.nect`. One GUI Undo restored the
complete authored example, including every unrelated value.

The same visible mixed-Text benchmark after Colors integration recorded:

| Operation | p95 interval ms | Max interval ms | >33.333 ms | Release/UI commit ms |
| --- | ---: | ---: | ---: | ---: |
| Pan | 17.99 | 22.89 | 0 | 0.02 |
| Zoom | 17.68 | 23.38 | 0 | 0.00 |
| Point drag | 19.38 | 25.26 | 0 | 15.46 |
| Handle drag | 19.00 | 22.49 | 0 | 14.31 |
| Text translation | 20.49 | 26.55 | 0 | 39.23 |

The 30 fps p95 floor passes; 60 fps remains unmet. Text release/Inspector rebuild
exceeds the 33.333 ms release target and is an open performance item. Raw evidence
is in ignored `build/canvas-benchmark-colors.json`; hardware, viewport and input
cadence match the Text table above. No claim is made for a large open color list.

## Long History evidence — 2026-09-20

All sixteen CTest entries pass with the compact Session timeline. Core checks
cover 320 edits, arbitrary backward/forward jumps, branch replacement without ID
reuse, exact authored native equality (including points, links, sources, gradients,
groups, collections, Artboards and named colors), independent count/byte pruning,
oversize admission failure and retained gesture preview after failed commit.
GUI checks navigate 85 edits through the actual History button, then branch and
replace the Session. Formal MCP adds 80 edits, restores earlier/later states,
reads dependent changed IDs, and verifies native restart begins fresh History.

The 80-object / 320-edit core case retains an estimated 5,964,800 bytes. A lower
bound for 320 full authored snapshots is 192,424,960 bytes, excluding allocator
and string overhead; compact retention is 3.10% of that lower bound. A separate
Windows process measurement over the entire core test run (including evaluation,
roundtrip and limit scenarios) recorded peak working set 16,519,168 bytes and
peak commit 11,358,208 bytes from 711 samples. This is not a measurement of only
History allocations. Raw result is ignored `build/history-memory.json`.

Actual Windows use applied 96 palette changes to `named-color-poster.nect` through
the live API, opened View > History, selected Initial document and returned in
one GUI action. Revision advanced96→97 while state ID returned96→0. API readback
matched the entire original native fixture and retained all97 timeline rows.
The source fixture was not overwritten. Initial row spacing was found to differ
from later refreshes; initial stylesheet polishing addresses that UI issue.

Text release profiling identified eager font dropdown setup (about30 ms) inside
the Inspector rebuild. A shared font model now attaches to the dropdown only on
opening; inline completion remains ready. Missing-family strings are preserved,
and Return commits once even when Qt emits activation and editingFinished.
Four focused Window/Canvas/History UI/Text authoring checks passed after the fix.

The clean production build, same visible mixed-Text fixture and hardware, records:

| Operation | p95 interval ms | Max interval ms | >33.333 ms | Release/UI commit ms |
| --- | ---: | ---: | ---: | ---: |
| Pan | 17.54 | 22.59 | 0 | <1 |
| Zoom | 17.43 | 22.00 | 0 | 0 |
| Point drag | 20.52 | 22.17 | 0 | 14.53 |
| Handle drag | 19.50 | 25.20 | 0 | 19.86 |
| Text translation | 22.78 | 65.72 | 1 | 24.77 |

All release operations now meet33.333 ms. The30 fps p95 floor passes, while one
Text interval stall and the unmet60 fps target remain explicit limitations.
Raw evidence: ignored `build/canvas-benchmark-text-history-font.json`. Temporary
profiling code is removed; this result uses the production build.

## Continuous protection evidence — 2026-09-20

All19 CTest entries passed after asynchronous live save integration. A deliberately
blocked worker retained only revisions1 and12 while the main event loop and local
API remained responsive; the active gesture preview999 never appeared on disk.
New-session creation drained the worker and protected the latest outgoing edit.
Native conflict and failed recovery destinations advanced only the independently
verified destination. Save As preserved external changes and recovery opened as
an unnamed copy through formal MCP.

Storage tests use actual Windows handles denying writes/replacement, repeated
failed saves, locked backup generations, exact backup restore and a helper killed
after QSaveFile staged/flush but before commit. That helper uses the same disabled
direct-write fallback; it is evidence for interruption at that boundary, not a
power-loss test. Managed retention kept20 of24 inactive recoveries while retaining
active/legacy/modified/foreign files. The128MiB threshold is not stress-tested.
The MCP scenario edits through the real desktop, waits for automatic per-target
receipts, verifies exact disk bytes, terminates abnormally and reopens both the
native and recovery data without loss of the verified revision.

Actual Windows poster use copied the example into ignored build storage, moved
English Title via Canvas, and observed Save pending→Saved revision1 without a
Save command. API readback matched the entire native and recovery documents, and
the exact original poster was retained as a backup. The example source remained
unchanged; the edited copy and receipt are build/live-save-manual.nect and
build/live-save-manual-receipt.json. The window closed normally.

Visible production Window performance with automatic protection enabled:

| Fixture | Worst p95 interval ms | Worst interval ms | >33.333 ms intervals | Maximum release ms |
| --- | ---: | ---: | ---: | ---: |
| Mixed Text | 19.28 | 23.98 | 0 | 24.63 |
| 80 Paths /320 points | 25.25 | 26.03 | 0 | 34.07 |
| 2 Paths | 18.61 | 68.14 | 1 | 10.88 |

All scenes meet the30fps p95 floor. The80-Path point/handle release34.07/33.94ms
slightly exceeds the33.333ms release target; the single2-Path pan outlier and unmet
60fps target remain explicit limitations. No favorable rerun replaces these
measurements. Hardware/viewport/input boundaries match the earlier benchmark.
Final case-alias and monotonic backup-cadence hardening passed four focused
storage/protection/live-save/Window checks after rebuilding (12.02 seconds).
Raw artifacts: build/canvas-benchmark-live-save.json and
build/canvas-benchmark-live-save-paths.json.

## Retained Polygon/Star evidence — 2026-09-20

All21 CTest entries passed (23.68s). New core/UI checks cover linked integer count,
reduced angular-role identity, preserved corrections on6→12, atomic refusal when
edited/referenced vertices disappear, reset/Undo, conversion and native0.8 reopen.
Earlier native fixtures remain unchanged; the0.7 named-color poster migrates with
all values/links intact. Formal MCP exercises the same commands before its
history, native restart and abnormal-exit recovery scenario.

The live desktop API created examples/polystar-field.nect and SVG, with Polygon,
Star, linked count, an outer1/6 point correction, two Repeaters, gradient, three
named colors and five editable Text objects (22 exported paint layers). Actual
1402×932 Windows UI changed Polygon count6→12 on a saved working copy. Star
followed12, correction X846 persisted, and exact live/native readback differed
only in the intended count literal. The original example remained unchanged.

Visible benchmark on the same recorded hardware,1440×900 Window,893×824 Canvas,
DPR1: two authored curves plus twelve Polygons/twelve Stars,216 generated vertices
and23 live count links. Measurement is the production queued QWidget paint path,
including full-scene evaluation and normal asynchronous protection; it does not
measure compositor presentation or count-changing gestures.

| Operation | p50 interval ms | p95 interval ms | Max interval ms | >33.333 ms | Release ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| Pan | 16.03 | 17.15 | 20.68 | 0 | 0.03 |
| Zoom | 16.10 | 18.53 | 21.23 | 0 | 0.00 |
| Point drag | 16.46 | 19.32 | 27.22 | 0 | 23.47 |
| Handle drag | 16.45 | 19.81 | 22.28 | 0 | 23.78 |
| Object translation | 16.00 | 21.10 | 26.15 | 0 | 30.36 |

Each sequence observed90 paints/89 intervals after12 warmup inputs. The30fps p95
floor and33.333ms release target pass;60fps remains unmet. Raw evidence:
build/canvas-benchmark-polystar.json and build/polystar-manual-receipt.json.

## Anchor and Transform Parent evidence — 2026-09-20

All23 CTest entries passed (25.21s), covering affine compatibility, Anchor edits,
keep-world attachment/detachment, effective-parent cycles/depth/domains, singular
transforms, driven-property refusal, cubic/stack/Text bounds and native0.1–0.8
migration. GUI tests exercise centered creation, numeric rotation/scale/position,
parent picking and dragging under a rotated external parent. Formal MCP proves
that structural and explicit transforms do not apply twice and includes native
restart/recovery. Six focused checks passed after the final repairs (10.53s).

The live desktop API created examples/pivot-follow.nect/.svg: a Mobile Group,
four followers owned by a separate translated Group, a gradient and editable Text.
Actual1402×932 Windows UI dragged Mobile's Anchor, preserving every source/matrix
and byte-identical exported SVG. Rotate by12° kept its world Anchor fixed while
the four followers tracked the beam. Both edits were each one Undo; the complete
original document, automatically saved native and recovery matched exactly at
revision8. Source example unchanged. Receipt: build/transform-manual-receipt.json.

Production creation exposed a one-ULP cos(12°) drift caused by Boost1.85's default
imprecise JSON number parser. Native, incoming command and internal DOM parsing
now request precise conversion. Fourteen seeded binary64 values (including
adjacent values, small/subnormal numbers and ordinary transform values) retain
bit identity through native/API/Qt Host paths. The production native/API equality
check now passes without tolerance or value normalization.

Visible production Window benchmark on the previously recorded hardware and
893×824 Canvas/DPR1 (queued QWidget paints,90 paints/89 intervals per operation):

| Fixture / operation | p50 ms | p95 ms | Max ms | >33.333ms | Release ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| 80 Paths / pan | 16.10 | 17.93 | 23.39 | 0 | 0.02 |
| 80 Paths / zoom | 16.06 | 17.62 | 21.51 | 0 | 0.00 |
| 80 Paths / point | 23.16 | 27.91 | 30.32 | 0 | 34.42 |
| 80 Paths / handle | 24.32 | 31.44 | 37.05 | 2 | 26.67 |
| 80 Paths / translation | 22.42 | 25.00 | 27.82 | 0 | 25.42 |

The2-Path scene's worst p9517.38ms, max20.21ms and release10.36ms had no33.333ms
interval exceedance. Both scenes meet30fps p95;60fps remains unmet. A measured
duplicate Canvas projection at commit was removed;80-Path point release fell
from40.31ms to34.42ms, still over target. The two handle stalls remain visible.
Before/after artifacts: build/canvas-benchmark-transform.json and
build/canvas-benchmark-transform-release.json; this is not compositor timing.

## Multi-selection and one-snapshot edits — 2026-09-20

Core/API batch commands share one starting value snapshot for absolute/relative
edits, absolute/relative links and freezing Unlink.274 focused checks cover
atomic failures, aliases, units, driven fields, topology corrections, gestures,
Undo, rotated parents and selected ancestors/followers moving exactly once.
The transform dependency check also follows generated Polygon vertices through
linked point counts and enabled/bypassed corrections. Full validation remains
mandatory after the targeted transform evaluation optimization.

Focused existing core/primitive/Polygon/transform/Canvas/Window/process/MCP and
new UI tests passed. Tests exercise real Ctrl tree selection, Mixed rows,400 to
all versus +=10 per target, frozen multi-target picker accept/cancel across
Composition planes, driven refusal, save/reopen and multi-point/object gestures.
The Inspector now keeps its scroll within the same editing context and hides
advanced affine entries behind the same explicit disclosure as single selection.

Actual Windows use selected Polygon and Star in a working copy of polystar-field.
Their Center X280/680 became300/700 with +=20, then400/400 with an absolute edit.
The entire readback differed only in those two intended literals. Automatic
native and recovery matched exactly at revision2; each edit undid once and the
complete migrated original returned at revision4. The0.8 example remained
unchanged; the owned window closed normally. Receipt: build/batch-manual-receipt.json.

Visible production Window on the previously recorded machine,893×824 Canvas,
DPR1,90 paints/89 intervals, normal asynchronous protection. The representative
scene has80 Paths/320 points with12 objects or points selected; the handle run
edits one point. Both selected and unselected scene content is evaluated/rendered.

| Operation | p50 ms | p95 interval ms | Max ms | >33.333ms | Release ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| Pan | 16.03 | 17.01 | 17.27 | 0 | 0.02 |
| Zoom | 16.08 | 17.19 | 17.85 | 0 | 0.00 |
| 12-point drag | 22.82 | 28.61 | 31.87 | 0 | 20.01 |
| Single handle | 26.88 | 29.65 | 36.08 | 1 | 29.32 |
| 12-object translation | 23.47 | 29.93 | 30.71 | 0 | 25.32 |

The2-Path/2-target scene had worst p9517.37ms, max18.28ms, no33.333ms intervals,
and maximum release9.89ms. The30fps p95 and release budgets pass, while60fps and
the single representative handle stall remain limitations. Initial multi-object
translation p9548.20ms failed; resolving only affine roots and their dependencies
through the existing evaluator reduced it to29.93ms. No cache or alternate
transform evaluation semantics were added. One intervening measurement stopped
with an interrupted gesture before reaching the representative scene; its cause
was not established and the incomplete data is not a passing measurement.
The subsequent run with better interruption diagnostics completed normally.
Artifacts: build/canvas-benchmark-multi.json, build/canvas-benchmark-multi-transform-subset.json
and build/canvas-benchmark-multi-transform-subset-observed.json.


## Expression authoring evidence — 2026-09-20

Native0.10 adds bounded expressions through the existing property graph and Session,
with strict old-version readers, source-preserving History/save, driven protections,
inline multiline drafts and searchable reference insertion. All27 CTest entries
passed in26.26s, including syntax/unit/cycle/range/topology/conversion failures,
native0.1–0.9 migration, Qt inline input/stale drafts/link replacement, formal MCP
changed-ID propagation and saved/restarted expression source. Initial desktop build
found unqualified `property` colliding with QObject; qualification fixed it.
No failure was waived. Tests are evidence for the supported language, not JS parity.

`scripts/create_expression_demo.py` created `examples/phase-form.nect` and SVG through
an empty live desktop Session:16 retained Circles,32 expressions, one Star controller,
five Named Colors and editable type,31 SVG paint paths. Changing controller angle
and radius updated all16 wave objects; independent numeric oracles and exact one-step
Undo/native readback passed. Native retains source; export_plan discloses evaluated
SVG values. The actual Windows viewport showed a coherent poster and legible controls.

Actual UI worked on `build/expression-manual.nect`, never the source example:
Center Y's fx editor accepted `330 +` newline
`ref("phase","","generator.outer_radius")`. Draft showed375 while Canvas and revision4
stayed unchanged; Ctrl+Enter committed375 at revision5. Native/recovery saved the
exact multiline source. One toolbar Undo restored the complete original at revision6;
native/recovery/example readbacks were exact and the owned app closed normally.
Receipt: `build/expression-manual-receipt.json`. The helper's accessibility tree lagged
some new controls; screenshot-grounded inputs and semantic readback verified them.

Visible benchmark: `build/canvas-benchmark-expressions.json`, same hardware/Window and
measurement boundary as above, viewport893x824, DPR1,90 paints/89 intervals per action.
Every Path's Stroke width references the first point's X, so dragging it reevaluates
all paints. Representative80 Paths/320 points/80 expressions,12 selected for point
and object movement (handle remains one point):

| Operation | p50 ms | p95 ms | max ms | intervals >33.333ms | release ms |
|---|---:|---:|---:|---:|---:|
| Pan |16.00|17.52|18.84|0|0.02|
| Zoom |15.99|17.38|21.71|0|0.00|
|12 points|23.08|28.67|30.92|0|23.74|
| Handle |22.80|25.69|26.45|0|30.61|
|12 objects|24.08|31.78|32.06|0|22.64|

Lightweight2 Paths/2 expressions: worst p9517.45ms, max19.74ms, maximum release10.59ms.
The30fps p95 and release floor passes in this run; the60fps target remains unmet.
This measures bounded arithmetic/reference sources, not all worst-case256-node formulas.

## Geometry masks and Group compositing — 2026-09-20

Native0.11 retains independent source visibility, stable geometry-mask references,
Group opacity, explicit isolation and twelve supported blend modes. Top/Bottom
mask creation and preserve-world PutInside use atomic Session commands, as do
Inspector source edits, bypass and formulas on opacity. Full30 CTest entries
passed25.86s, including native migrations, source/reference protection, exact Undo,
formal MCP save/restart/recovery and actual Qt context menus. Pixel checks cover
independent W3C blend formulas, aggregate opacity, pass-through/isolation, hidden
source editing and renderer limits.87 Canvas checks also pass at DPR1.5.

`scripts/create_compositing_demo.py` authored `examples/colour-cut.nect` and SVG
through a fresh live Session: two retained circular masks,16 repeated stripes,
Multiply/.88 Group opacity, five Named Colors and eight editable Text objects.
On an owned working copy, actual Windows UI changed hidden-source radius157 to124.
Only that literal changed at revision5; automatic native/recovery matched exactly.
One toolbar Undo restored the entire original at revision6; source fixture stayed
unchanged. Receipt: `build/compositing-manual-receipt.json`. Self-use moved mask
controls above transform controls, making Edit Source immediately reachable.
The final cropped renderer and corrected Inspector were visually checked again;
the owned preview closed normally without authored edits.

`scripts/probe_svg_compositing.py` produced SVG through the normal CLI Session and
read pixels through Chromium153's SVG renderer.31/31 independent probes passed:
twelve blends at alpha1/.5, Group overlap/opacity, transformed vector mask,
pass-through/isolation and first-layer Screen on transparency. Largest channel
difference was2/255 (tolerance3); production SVG visually matched Qt. Receipt:
`build/svg-compositing-browser-receipt.json`. Local server/tab were closed.
This establishes the supported SVG/CSS reader contract, not every SVG consumer.

Visible production Window benchmark uses the same recorded machine,893x824 Canvas,
DPR1 and90 paints/89 intervals. Representative85 leaves include80 authored curves,
two hidden mask sources, two isolated Groups and one Screen leaf, maximum isolation
depth2; all normal validation and asynchronous protection remain active.

| Operation | p50 ms | p95 ms | max ms | intervals >33.333ms | release ms |
|---|---:|---:|---:|---:|---:|
| Pan |16.12|17.45|17.63|0|0.02|
| Zoom |16.03|17.19|17.55|0|0.00|
| Point |23.86|30.54|31.88|0|23.50|
| Handle |23.52|30.66|32.71|0|24.10|
| Object translation |24.57|26.38|28.18|0|25.49|

Lightweight7-leaf scene: worst p9517.52ms, max21.46ms, release13.00ms, no interval
over33.333ms.30fps p95/release floor passes;60fps remains unmet. Initial full-viewport
isolated images failed at point37.73/handle40.76/move39.22ms p95. Cropping surfaces
to pixel-aligned mask or true paint bounds reduced paint10–11ms to4–5ms, but point
still failed34.58ms. Phase profiling found Session update13.56ms and Canvas values
8.68ms means; unnecessary success-path diagnostic-string allocations were removed.
Inspector also reuses the just-evaluated committed Canvas values. All validation
and error semantics remain. No cache framework or relaxed threshold was added.
Temporary instrumentation was removed. Original/intermediate/final evidence remains
in `build/canvas-benchmark-compositing.json`, `canvas-benchmark-compositing-cropped.json`,
`canvas-benchmark-compositing-profile.json` and `canvas-benchmark-compositing-diagnostics.json`.
