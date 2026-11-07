# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: the current user directs dot to take over the stopped development, prioritizing practical panels and color. Edits, tests, coherent commits and non-force development-branch pushes are authorized. Mainline integration requires its own exact-stack checks and review; release, force/history rewrite, credential or billing changes and bypass of safety denials are excluded. Product decisions remain in [Confirmed requirements](https://app.notion.com/p/01b2ff7802fc410ab4cdb8fd26264495); [REQ-211](https://app.notion.com/p/3fbfd279a6f381a9a7c9c36f5089fba1) governs color and the current [Workspace action](https://app.notion.com/p/40ffd279a6f38132856ce92091df60ea) governs the approved staged panel work. Research plans are not adopted product presets.

Coarse route: panels and color → remaining adopted Session, document, shape and text requirements chosen from live dependencies → measured native stabilization and Shiro review.

## ACTIVE CHECKPOINT — existing-palette production import

Goal: add linked-color Artwork to a project that already owns a palette, through explicit local Capture, without changing existing artwork/palette identities. Preserve practical fixed panels and later editing.

Current phase: the inherited queue through #220 is integrated and final main checks for application88411895 and documentation47df6ea8 succeeded. The next accepted REQ-287/REQ-89 slice is implemented and locally verified on isolated dot/color-import-workflow-20261107. Previously unmerged PR91 picker cleanup is recovered by normal merge; original branch830d64b remains preserved. Candidate CI is next. The stopped Codex writer remains stopped.

Proven: unmodified main47df6ea8 reproduces the pending browser Cancel/input leak in PR91's regression. Recovered source only changes the browser picker claim-once block; current Session/project guards, checkbox selection, Capture and native route are untouched. Eight repaired picker tests pass; nine related files/93 tests pass; build/native-CSP and four sandboxed Chrome scripts pass. The permanent Capture regression now closes the old Chrome PID and opens its saved file in a fresh process before re-editing the receiving palette. Source and one-Undo/local-value assertions remain exact. Historical CI is not current candidate CI.

Artwork evidence: a disposable linked-B ORBIT copy imported itself into a project with the same live packet/IDs. Explicit Capture preserved receiving16-layer Composition, packet and ten existing slots, remapped five ordinary colors and captured the bound satellite's exact #7b7b7b alpha1. Full960x540 PNG pixels matched. One Undo/Redo, Save, actual Chrome PID3904 exit/new PID87168, UI Open and complete state/pixel readback passed. Later visible C1/delta-L edit changed only receiving live output (#bcbcbc); captured output stayed pixel-identical, one Undo/Redo and exports passed. Both input/original hashes remained unchanged, page errors zero. See desktop/docs/color-composition-capture-restart.md and ignored desktop/test-results/orbit-existing-palette-capture/receipt.json.

Next task: complete exact-candidate review/CI and normal main integration without bypass, then choose the next accepted production obligation. Do not reinterpret final coefficients, held drawer count or independent candidates as adopted features. Native WebView2/physical IME remains LOCAL_WAIT until GUI controller ownership has new evidence; do not repeat failed host inventory.

Approach: one dot writer, isolated branch; preserve recovered source ancestry and all original branches/data; ordinary non-force Git only. Fixed release panel positions are explicit. Development-time arrangement changes do not authorize user-configurable docking. Capture is an explicit existing action, not a new preset or generic cross-project link system.

Done for next: exact reviewed candidate CI is reconciled and integrated, or an exact actionable blocker is recorded. Overall Mission/native/subjective acceptance remains incomplete.

State: main47df6ea8, primary D:/Documents/Codex/2026-11-02/task/Nect-pr178-combination remains clean at2a8808b5 with the same full tree. Active isolated worktree D:/Documents/Codex/worktrees/Nect-dot-color-import-workflow-20261107; read its Git HEAD for the current candidate. Its merge82debc83 recovers PR91 from830d64b. No other writer owns this scope. Node ownership remains released.

Residuals: browser-process restart is not native WebView2 or physical IME/OS color-picker/OS Cancel acceptance. Final experimental presets, legacy palette migration, broader transfer/ambiguous clipboard provenance, performance at10000 points and full REQ-2/3/34/211 remain incomplete. Recovered Image Luminance remains internal/unconnected, with no public node or complete Field workflow claim.

Authority: current user takeover/delegation and Mission permit bounded implementation, tests, documentation, coherent commits, non-force development pushes and normal main integration after current differences/tests/ownership/stops are checked. Preserve original data and current stops. No release, force/history rewrite, protection/security/billing changes, PR approval review or safety-denial bypass.

Handoff: CONTINUE_CURRENT_TASK.
