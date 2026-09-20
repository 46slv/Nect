# Practical alpha Mission — retained primitives delivered; appearance/repetition next

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
- native 0.2 writer / strict 0.1 reader migration, authored/evaluated origin
  metadata and conversion plans through the same semantic API.

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

Next safe actions: add practical ordered Fill/Stroke and Repeater so a radial
design can be authored and revised from one motif. Read the accepted shape-order
semantics before choosing the bounded stack representation; do not build a
general operator framework. Circle/Rectangle slice is complete, not the Mission.
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
