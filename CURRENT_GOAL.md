# M1 — desktop loop on the existing kernel

Purpose: make point/handle editing usable on Windows without replacing the tested core.

Implement a thin Qt 6 desktop client:
- Canvas
- object tree
- contextual Path Inspector
- direct point/handle gestures
- native save/reopen
- pick-whip using the same stable-ID binding model

Connect a formal MCP SDK adapter to the same live Session; do not duplicate mutation logic or call
the existing JSON-lines transport MCP.

Before adding or installing dependencies, verify the actual Qt/MSVC/Boost environment.

Do not:
- implement the full parity backlog
- add OFX/AI/PSD stubs
- redesign the core for hypothetical features
- publish/release
- change repository permissions or external accounts
