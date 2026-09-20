# Practical alpha Mission — continuous protection integrated; retained Polygon/Star next

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
- native 0.7 editable Text and named/global colors / strict earlier migrations, authored/evaluated origin metadata,
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

Text checkpoint `957e090` after `132434d`: native 0.6 retains editable UTF-8 text, installed
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

Color checkpoint `8b63d5b` after `957e090`: native 0.7 adds stable named RGBA colors and
typed aggregate color properties backed by the ordinary Scalar dependency graph.
GUI/API/MCP share Set/Link/Unlink; deleting a referenced source rejects atomically.
Colors UI separates named colors, exact evaluated paint-input inventory, and
explicit-copy history (32 values, current Window only). Rich clipboard values
retain double precision and explicit sRGB/profile/straight-alpha metadata;
unsupported metadata, duplicate JSON keys and cross-document links reject.
Equal HEX values never establish identity. User-pinned/cross-restart history and
other working color spaces remain pending. All fourteen CTest entries passed;
the subsequent swatch-only fix passed focused Color UI checks.

Actual Windows use produced `examples/named-color-poster.nect` and SVG through
the live API (three palette entries, nine linked paints/stops). GUI HEX change
FFF4D6FF -> FFB8C8FF updated three Text paints; API verified exact values/links,
saved `build/colors-manual.nect`, and GUI Undo restored the complete fixture.
Qt selected icons had tinted swatches: explicit identical mode/state pixmaps
fix this, with an eight-state image check. Full Window mixed-Text performance
still passes the 30 fps p95 floor (worst20.49 ms, zero intervals >33.333 ms);
Text release/Inspector refresh39.23 ms exceeds the33.333 ms release target and
needs follow-up. The 60 fps target also remains unmet. See first-usable evidence.

History checkpoint `7323ec1` after `8b63d5b`: compact reversible deltas retain up to
1,024 edits / estimated 64 MiB, with stable state IDs, atomic state jumps, explicit
pruning and oversized-edit rejection. GUI View > History and the shared API/MCP
use the same Session timeline. New edits discard only the redo branch. Native
format remains 0.7; history is not stored across restarts. All sixteen CTest
entries passed (core 320-edit, exact native state/identity restoration, budget and
gesture failures, GUI 85-edit jump/branch/new-session, MCP 80-edit jump/restart).
An isolated Windows core test measured peak working set 16,519,168 B / commit
11,358,208 B including evaluation and all scenarios. Retained 320-edit estimate
5,964,800 B vs conservative full-snapshot lower bound 192,424,960 B (3.10%). This is
bounded evidence, not a general memory guarantee. Raw build/history-memory.json.

Actual Windows palette poster: 96 committed palette edits, 0.22 MiB estimate;
GUI Home + Return restored state 0 at revision 97. API verified complete original
native equality and all 97 retained state rows. Original fixture unchanged; app
closed. History first-open list row size differed after stylesheet polish;
initial ensurePolished fixes it and focused History UI checks pass.

Measured performance follow-up found eager font dropdown setup dominating Text
release. Share the installed-family model and attach it to the popup on demand;
inline completion remains available immediately. Preserve missing-family text
and suppress duplicate identical TextSource commits on Return. Clean production
build and four focused Window/Canvas/History UI/Text checks pass. Final visible
mixed-Text benchmark release24.77 ms (previous55.51), all releases<=33.333 ms;
worst p95 interval22.78 ms, one65.72 ms interval outlier. The30 fps p95 floor
passes, but this is not zero-stall or60 fps evidence. Raw result:
build/canvas-benchmark-text-history-font.json. Profiling code removed; no GUI
process remains. Worker finished; parent owns all builds and source files.

Next safe actions after this slice: extend retained primitives with Polygon/Star and explicit topology/correction
identity. Do not treat the radial example as full alpha completion. Masks/compositing,
assets, Anchor/Transform Parent, expressions/multi-edit and the remaining explicit Mission
requirements still need implementation and real production fixtures.
Preserve all current files and the development document. No release/publication
or dependency binaries are authorized for distribution. This checkpoint does not
claim practical-alpha completion or relax F1–F8. Do not stop at M1.

Continuous protection checkpoint after `7323ec1`: committed snapshots at edit
completion, one running worker plus newest pending copy, one-second non-postponed
cadence. Encoding/backup/atomic writes/readback/retention run off the UI thread.
Known native/recovery revisions, writing/pending revisions and independent errors
are exposed in hello.persistence and the status bar. Manual save/open/new/close
drain earlier writes before changing target identity. Recovery errors cannot trap
a closing document when its exact current native bytes still verify on disk.

Content SHA256 and canonical cooperative locks protect native replacements;
external change/deletion gives FILE_CHANGED while recovery continues. Windows
case aliases cannot bypass the check; SVG cannot replace the same native alias.
QSaveFile direct-write fallback remains disabled; age-based stale-lock expiry is
disabled. Qt's cooperative-lock and atomic-file contracts were checked against
https://doc.qt.io/qt-6/qlockfile.html and https://doc.qt.io/qt-6/qsavefile.html.
The final non-cooperating writer race and arbitrary hardware failures are not
claimed solved. IO_VERIFY_FAILED discloses uncertain post-replacement readback.

Manual/first-live replacements preserve previous bytes; subsequent automatic
generations use a monotonic30-second cadence, ten owned backups per file. Closed
managed recovery targets20 sessions/128 MiB; active leases, legacy/unrecognized,
modified or unreadable files remain outside cleanup. Metadata records hash and
revision; recovery data with incomplete metadata remains manually recoverable.
Cleanup is best effort, never an asserted hard disk cap. API/MCP open_recovery
opens an unnamed copy. Native0.7 unchanged; examples preserved.

All19 CTest entries passed21.76s, including new storage/protection/live-save
contracts. Slow injected writer allows UI events/API while coalescing revisions
1..12 into two jobs, persists committed12 rather than active preview999, and
drains latest work before Session replacement. Real Windows deny-write/delete
handles, locked backup retention, exact backup restore, external edits/deletion,
independent recovery/native failures and killed QSaveFile staging helper pass.
Formal MCP waits for automatic native+recovery receipts, kills/restarts the real
desktop and reads exact authored data; no explicit Save/recover was used for the
last edit. Retention test covers24 inactive snapshots ->20 plus active/legacy/
modified exclusions;128MiB boundary is not stress-tested. Final Windows case-alias
and monotonic-cadence hardening rebuilt and passed four focused storage/protection/
live-save/Window checks in12.02s.

Actual Windows self-use: copied poster to ignored build/live-save-manual.nect,
dragged English Title to tx38.453038674/ty-17.2375690608, observed pending→Savedr1
without Save. API verified complete live/native/recovery equality and exact
original backup. Source fixture unchanged, app closed. Raw receipt retained in
build/live-save-manual-receipt.json. Visible Text p95<=19.28ms,zero>33.333ms,
release24.63ms;80Path p95<=25.25ms,zero>33.333ms. Point/handle release34.07/33.94ms
on80Paths remains slightly over33.333ms;2Path pan has one68.14ms outlier. All
scenes meet30fps p95 floor,60fps target remains unmet. Raw results:
build/canvas-benchmark-live-save.json and canvas-benchmark-live-save-paths.json.
Worker finished; parent owns builds, no GUI process remains.

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
