# M1 — desktop loop on the existing kernel

Purpose: make point/handle editing usable on Windows, then iterate from real use without replacing the document/Session on every UI revision.

Live task: https://github.com/46slv/Nect/issues/2
Acceptance and incremental delivery: [docs/first-usable.md](docs/first-usable.md).
Recent product/UI intent: [docs/product-direction.md](docs/product-direction.md), a scoped extract for agents without Notion access.

Start with the manual loop (increment A):
- real path creation/selection through core commands
- Canvas, object tree and contextual Path Inspector
- direct point/handle gestures with cancellation and one-gesture undo
- native save/reopen and SVG output

Then complete binding UX and the formal MCP adapter (increments B/C) on the same live Session. Do not duplicate mutation logic or label the JSON-lines transport MCP. M1 is not Done until its full F1–F8 acceptance is demonstrated.

The standard UI is for still graphics: no timeline, playhead, video transport or timecode. Panel arrangement and control widgets are testable hypotheses, not frozen architecture. Let the user try the manual loop before all M1 work is finished.

Before adding/installing dependencies, verify the actual Qt/MSVC/Boost environment. Preserve existing work in the intended Windows tree; its runtime state must be inspected, not inferred from GitHub.

Do not:
- implement the full parity backlog or build a general field/plugin/UI framework first
- add OFX/AI/PSD stubs or pretend unsupported capabilities work
- redesign the whole core for hypothetical features; bounded changes/migrations for real requirements are allowed
- publish/release or change repository permissions/external accounts

This file selects the next implementation scope. Documentation review or UI exploration alone is not an instruction to launch an autonomous implementation session.
