# M1 — desktop loop on the existing kernel

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
