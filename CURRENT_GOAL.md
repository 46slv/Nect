# Practical alpha Mission — editable Text delivered; color tools next

## Active Mission checkpoint — 2026-09-20

The current user explicitly authorizes continuing beyond M1 toward a practical
2D authoring alpha, selecting small slices from accepted product direction and
real production dependencies. M1 green alone is not the Mission stopping point.

Workspace `D:\Documents\Nect` was an empty unborn `master` with only `.git`, no
remote or dirty files. Fetched the specified repository and created
`codex/practical-alpha` from `d59691d`. Windows build auto-link fix checkpoint:
`ff532a7`. M1 implementation checkpoint: `1fa0b21` on this branch. Push was
rejected by GitHub GH007 (configured commit author email is private). Keep local
commits; do not change account settings or rewrite history to bypass it. No PR
or remote feature branch was created.

Current implemented slice:
- Qt Windows desktop, Canvas/tree/Inspector; empty-document Path creation and
  direct polar-handle editing, group selection/drill-in, pan/zoom and object move.
- Session-owned cancellable previews, one-gesture Undo, API mutation rejection
  during gestures, atomic create/delete/reorder commands, changed-ID readback.
- searchable property picker, cross-object drag pick-whip, explicit name
  resolution, absolute/relative links, copy/paste value/reference and visible
  driven properties.
- native atomic manual save, previous-file backups, committed-revision recovery;
  local API and formal MCP stdio sidecar forward to the desktop's live Session.
- retained Circle/Rectangle sources, normal generator properties and stable
  generated point roles; direct point/handle edits create visible absolute local
  Point Edit overrides, with bypass and explicit reference-protected conversion.
- native 0.6 editable Text / strict earlier migrations, authored/evaluated origin metadata,
  conversion plans and ordered operation discovery through the same semantic API.

Evidence collected:
- MSVC 19.29.30137, Boost 1.85.0, Qt 6.5.3 x64 Release; M0 baseline tests + smoke pass.
- all six CTest entries (core, desktop persistence/backup restore/session identity,
  Canvas, Window, process, formal MCP) pass. The first Canvas test run lacked `qoffscreen.dll`;
  deploying that test runtime dependency fixed it; it was not an application failure.
- `tests/mcp_desktop_tests.py`: seed 7821, 24 Paths, 41 semantic mutations;
  real initialize/list/call, atomic/stale rejection, save/restart, abnormal-exit
  recovery and independent SVG XML readback pass.
- Actual 1400×900 Windows window: created two Curves, typed first-point X=315,
  read it back through live API (revision 2), searched/picked its property for the
  second Curve, saw linked value/handle color (revision 4), saved to ignored
  `build/manual-loop.nect` with two paths and one binding.
- Real use found recovery-status refresh recreating Inspector inputs. Fixed by
  updating only the status label on recovery completion; input stayed stable.
  Picker labels now use readable object/point paths while retaining stable references.
- Full visible Window viewport benchmark: 2 Paths and 80 Paths / 320 points;
  p95 paint intervals 17.23–23.65 ms, no >33.333 ms measured intervals. Inspector
  evaluation sharing / structure-only tree refresh reduced release stalls from
  131.75 ms to <=32.82 ms. See `docs/first-usable.md` for exact hardware, measurement
  boundary and table. This meets the recorded 30 fps floor, not the 60 fps target.

- Primitive slice: seven CTest entries pass, including geometry, correction
  bypass, generator cycles, atomic failure, conversion blockers/Undo/identity,
  actual Window actions and legacy save/edit/reopen migration. Real Windows use:
  added Circle, typed radius 145, dragged its East anchor, observed two overrides
  and retained radius, then read native 0.2/source/Point Edit through the live API
  at revision 3 and saved `build/primitive-manual.nect` (ignored).

Retained primitive checkpoint: `6b4320d`. The following checkpoint adds native
0.3 ordered Fill/Stroke/Repeater, scalar operation properties, legacy stroke
address-preserving migration and shared core paint evaluation for Qt/SVG. Core,
primitive, ordered-stack and migration process checks pass. Actual desktop API
authored `examples/radial-ornament.nect`: five sources, point correction, property
link, Group and two Repeaters -> 85 SVG paint layers; viewed on Windows and saved.
`scripts/create_radial_demo.py` reproduces it from an empty live document.

All eight CTest entries pass, including GUI paint-order pixels/copy hits and an
extended seeded formal MCP scenario with 16 Repeater edits, changed-ID/paint-plan
readback, native restart and crash recovery. A long Composite combo had forced
546 px Inspector content into a 304 px viewport, breaking pick-whip drops. Bounded
combo sizing/wrapped forms fix it; drag scrolling and visible-source checks remain.
The visible benchmark still meets 30 fps: 80-Path point-drag p95 30.16 ms, maximum
33.24 ms, release 30.83 ms; 48-layer repeated-paint fixture p95 <=18.24 ms and release
<=20.55 ms. All 15 operation runs have zero >33.333 ms measured intervals. The
lightweight 60 fps target remains unmet. Raw evidence is in ignored
`build/canvas-benchmark-stack.json` and `build/canvas-benchmark-repeat.json`;
the durable table is in `docs/first-usable.md`. No intentional GUI app remains open.
Keep the sample and development recovery data. Parent owns future builds; worker
has completed its bounded UI work.

Gradient checkpoint `73815b8` after `9ca7e1e`: native 0.4 adds retained linear/radial
gradients, stable stop IDs, scalar/link editing, Solid bypass, Canvas start/end
gestures and SVG projection. All nine CTest entries pass, including gradient
identity/removal/cycle/range atomicity, real Window pixel/gesture controls, strict
0.1/0.2/0.3 migration, and formal MCP gradient edits/save/crash recovery. The final
point-selection/tool-exit fix was rebuilt and Window/Canvas focus tests passed.
Actual 1400x900 Windows use authored `examples/gradient-ornament.nect` via the live
semantic API: five sources, 85 paint layers, three gradients and linked stop colors.
Dragged the background endpoint from (960,640) to (838.8845,578.8048); live API
readback confirmed one committed revision, then Undo restored the original frame.
The manual result is retained in ignored `build/gradient-manual.nect`; original
example/SVG are saved with readable ray colors. The visible app is closed.
Coincident stop offsets reject explicitly (Qt would collapse them); radial focal
offset and alternate spread/color spaces remain unsupported. No new performance
claim is made from the static gradient example. See `docs/model-v0.md`.

Artboard checkpoint after `73815b8`: native 0.5 frame CRUD/order and same-Composition
width/height inheritance. UI-only active frame/Composition, compact ordered list,
crop-only numeric editor, per-dimension override/reset/detach, Fit active/all,
active-plane object tree and add/export routing are implemented. Core/GUI/Host/
formal MCP and 0.1–0.4 migration checks pass: all ten CTest entries. Parent deletion
cannot break children; cross-plane/cyclic parent bindings reject atomically.
Empty leading Compositions no longer hide a later valid output plane.
Actual Windows use created `examples/artboard-studies.nect` with three ordered crops
and numbered SVG exports through the live API. In the GUI, Banner height 300→320
preserved inherited width 600 and every artwork object. API readback confirmed
one revision; Undo restored the complete authored document. Manual edit retained
in ignored `build/artboard-manual.nect`; visible app closed. Actual use identified
a redundant Composition prefix clipping names in the narrow navigator: removed
for single-plane files, explicit elision/tooltips retained, no horizontal scroll.
Parent owns builds; worker is idle. Parent size is a foundation only; inherited
template content/layout attributes remain pending.

Text checkpoint after `132434d`: native 0.6 retains editable UTF-8 text, installed
font references, weight/italic/locale, auto/frame sizing, Japanese horizontal and
vertical layout, alignment/tracking/leading and normal linked distance properties.
DirectWrite glyph outlines feed the same Fill/Stroke/Repeater Canvas/SVG pipeline.
Text bounds remain selectable, overflow is visible and no glyphs are truncated.
Missing-font fallback and actual families are diagnosed; color-only glyphs reject.
SVG outlines are disclosed by Inspector, export_plan and SVG descriptions. No
fonts are distributed. Non-Windows native metadata remains portable; geometry
projection explicitly requires Windows. Rich text/on-path/glyph editing remains
pending, not silently flattened into authored Paths.

All twelve CTest entries passed; the final focus/scroll fix passed focused Window
regression and visible use. Actual seven-Text Japanese poster is retained in
`examples/typography-poster.nect` plus SVG, reproducible with `create_text_demo.py`.
GUI content and size edits propagated to API/readback; two Undo steps restored
the complete source. Manual content edit is retained in ignored build storage.
The eight-Text full-Window benchmark passed the 30 fps p95 floor for pan/zoom,
point/handle edits and Text translation (worst p95 18.96 ms, release31.84 ms,
zero measured intervals above33.333 ms). 60 fps remains a target. Full evidence
is in docs/first-usable.md. Visible app closed; worker idle, parent owns builds.

Next safe actions after this slice: save a coherent checkpoint, then continue
with named/global colors, color inventory and color history in
thin slices; do not treat the
radial example as full alpha completion. Masks/compositing, assets, long History,
continuous save, expressions/multi-edit and the remaining explicit Mission
requirements still need implementation and real production fixtures.
Preserve all current files and the development document. No release/publication
or dependency binaries are authorized for distribution. This checkpoint does not
claim practical-alpha completion or relax F1–F8. Do not stop at M1.

---

## Delivered M1 scope (acceptance retained)

Purpose: make point/handle editing usable on Windows, highly scriptable, and responsive enough to iterate from real use without replacing the document/Session on every UI revision.

Live task: https://github.com/46slv/Nect/issues/2
Acceptance and incremental delivery: [docs/first-usable.md](docs/first-usable.md).
Recent product/UI intent: [docs/product-direction.md](docs/product-direction.md), a scoped extract for agents without Notion access.

Start with increment A:
- real path creation/selection through core commands
- Canvas, object tree and contextual Path Inspector
- direct point/handle gestures with cancellation and one-gesture undo
- native save/reopen and SVG output
- the same semantic create/edit/save/undo path callable from the machine/API surface, without GUI mouse replay
- basic frame timing for pan/zoom/point drag/basic transform; record the reference fixture/hardware and meet the >=30 fps p95 floor before claiming M1 performance acceptance

Then complete binding UX and the formal MCP adapter (increments B/C) on the same live Session. Do not duplicate mutation logic or label the JSON-lines transport MCP. Add deterministic multi-command/readback scenarios so Astra/CI can operate the document heavily without a person performing every action. M1 is not Done until its full F1–F8 acceptance is demonstrated.

The standard UI is for still graphics: no timeline, playhead, video transport or timecode. Panel arrangement and control widgets are testable hypotheses, not frozen architecture. Let the user try the manual loop before all M1 work is finished.

Before adding/installing dependencies, verify the actual Qt/MSVC/Boost environment. Preserve existing work in the intended Windows tree; its runtime state must be inspected, not inferred from GitHub.

Do not:
- implement the full parity backlog or build a general field/plugin/UI framework first
- implement all AE blend modes in M1 merely because AE compatibility is a product target; probe only the compositing semantics M1 actually needs
- add OFX/AI/PSD stubs or pretend unsupported capabilities work
- redesign the whole core for hypothetical features; bounded changes/migrations for real requirements are allowed
- prebuild large cache/worker systems before profiling the real Canvas path
- publish/release or change repository permissions/external accounts

This file selects the next implementation scope. Documentation review or UI exploration alone is not an instruction to launch an autonomous implementation session.
