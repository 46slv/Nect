# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: the current user directs dot to take over the stopped development, prioritizing practical panels and color. Edits, tests, coherent commits and non-force development-branch pushes are authorized. Mainline integration requires its own exact-stack checks and review; release, force/history rewrite, credential or billing changes and bypass of safety denials are excluded. Product decisions remain in [Confirmed requirements](https://app.notion.com/p/01b2ff7802fc410ab4cdb8fd26264495); [REQ-211](https://app.notion.com/p/3fbfd279a6f381a9a7c9c36f5089fba1) governs color and the current [Workspace action](https://app.notion.com/p/40ffd279a6f38132856ce92091df60ea) governs the approved staged panel work. Research plans are not adopted product presets.

Coarse route: panels and color → remaining adopted Session, document, shape and text requirements chosen from live dependencies → measured native stabilization and Shiro review.

## ACTIVE CHECKPOINT — release retired prepared-switch Sessions

Goal: repair the confirmed prepared-switch Session retention introduced by the outer-tab freshness guard, preserving all Copy/History/authority and prepared-font-bank behavior.

Current phase: isolated `dot/prepared-switch-retention-20261109` from main `a0719d117f4e162afede51dd7c7059c9ace641f4`. The preceding outer-tab PR232 passed candidate Core/Windows and real-tab Chrome115, then was normally merged after explicit readiness/merge approval. Its exact-merge post-main checks remain separately tracked. Do not mark its memory health complete from functional CI.

Proven: Node24.22.1 isolated WeakRef/exposed-GC probe, thirty switches and eight event-loop turns/GC cycles: ordinary switches created31/alive1, affected prepared switches created31/alive31. The new regression fails on untouched maina0719d1. An isolated helper counterprobe and actual repaired source both yield prepared31/alive1. Focused127/127 includes the GC child plus existing outer-tab, clipboard, owner, variable-switch and browser-runner cases. This measures retained JS Sessions, not native peak RAM or GUI latency.

Approach: keep the retained prepared fonts reader in a module-helper scope receiving only the stable fonts provider, bridge and ticket. Do not create its closure beside request/clipboard freshness closures. The helper's read/useLive pair preserves existing preview-bank validation and final trusted bank activation. All reentrant Copy/tab checks remain unchanged. No raw clipboard, schema, assets, OS/cross-project transfer or new mutation path.

Next task: complete independent review, full formatting/aggregate/build and exact-head Windows/Core CI in a small follow-up Draft PR. Inspect the new GC diagnostic and existing real variable/outer-tab browser paths. Keep the known baseline artwork-duplication-dom heap OOM explicit; never present it as a fully passing local aggregate.

Done for next: source-bound GC red/green evidence, focused and aggregate results, actual variable/outer-tab behavior, remote head/tree and CI agree. Apply normal readiness/integration checks with matching authorization; do not borrow a previous PR-specific approval or bypass a denial.

State: one writer in a fresh cloud worktree. No RDC, native launch/install/profile change, release, force/history rewrite or security/account/billing mutation. Prior native/physical and SHIRO cleanup remain separate. No source edit or user-data operation was performed by the independent probes.

Previous evidence: PR232 candidatehead8e46b181/treec84d2013 had Unit3903 PASS/4 SKIP, Rust73 and realChrome115/115 including actual outer A/B tabs, Copy snapshot, dirty/History invariance, ID/offset, Undo/Redo and reopen reset. Normalmerge/maina0719d1 has that exact tree. Issue129 remains OPEN; this follow-up addresses memory retention, not broader cross-reference semantics or whole-Mission acceptance.

Authority: current Nect development/maintenance delegation covers this necessary regression repair, tests, documentation, coherent commits and non-force development push/PR. Main integration follows current differences/tests/owner/stops/protection and applicable readiness approval. No RDC, release, protection bypass, force/history rewrite, PR approval review or unrelated action.

Handoff: CONTINUE_CURRENT_TASK.
