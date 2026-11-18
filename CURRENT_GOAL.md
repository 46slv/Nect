# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: the current user resumed development, prioritizing practical panels and color. Edits, tests, coherent commits, non-force development pushes and normal integration after applicable checks and review are authorized. Release, force/history rewrite, credential or billing changes and bypass of safety denials are excluded. The 2026-11-12 instruction removes backward compatibility with old versions as a goal: do not work solely to preserve old formats or APIs. Protect current authored data, IDs, History and current product defaults. Product decisions remain in [Confirmed requirements](https://app.notion.com/p/01b2ff7802fc410ab4cdb8fd26264495); [REQ-211](https://app.notion.com/p/3fbfd279a6f381a9a7c9c36f5089fba1) governs color and the current [Workspace action](https://app.notion.com/p/40ffd279a6f38132856ce92091df60ea) governs the approved staged panel work. Research plans are not adopted product presets; B remains trial-only when explicitly selected.

Coarse route: panels and color → remaining adopted Session, document, shape and text requirements chosen from live dependencies → measured native stabilization and Shiro review.

## ACTIVE CHECKPOINT — Practical panels/color: accepted slices and remaining work

Goal: continue the already-approved practical panels/color work without restarting completed migration or native acceptance. Preserve authored documents, stable IDs, Session commands, Undo/Redo and current product defaults. This checkpoint does not broaden the Mission Brief above.

Current phase (2026-11-18): [PR #323](https://github.com/46slv/Nect/pull/323) is merged (`6dc036d25ff56574a01ec35c05fe63789d3ba501`); Node Foundation migration is not the next integration task. [PR #324](https://github.com/46slv/Nect/pull/324) supplies the accepted first live-Blend vertical, not full Illustrator parity. [PR #325](https://github.com/46slv/Nect/pull/325) supplies editable shared-color value/reference examples and a short usage guide. Observed main is `2d36a9f4a5b1e43ca20514e95a3ecce0d9baa999`; fresh-check it and writer ownership before editing.

Proven, with separate evidence boundaries:

- Migration definition/catalog/runtime comparisons, bounded Boolean performance repair, build and browser flows are recorded in PRs #301–#323. Do not repeat their completed integration work.
- Live Blend's limited supported geometry, source edits, Steps, expansion/export and native editing evidence are in PR #324 and [the Blend guide](desktop/docs/blend-expand.md). Unsupported/follow-on topology, spine and spacing remain outside this first vertical.
- The [Workspace action](https://app.notion.com/p/40ffd279a6f38132856ce92091df60ea) and [dot Worklog](https://app.notion.com/p/3f0fd279a6f38100a5bbffa91c3ae210) hold the 2026-11-18 limited native receipts: dirty-close Cancel/Save/reopen; one SVG Asset picker/Place route; shared-color picker, binding and current-value unlink; cold process restart of a saved binding; selected Composition import; and value/reference reuse across two Compositions. These are not whole-Surface acceptance. The acceptance Asset registry is shared between acceptance profiles; retain its existing fixture root.
- [The editable color examples](desktop/examples/shared-color-reuse.md) preserve the saved JSON content with repository formatting. Focused headless checks cover both Compositions, evaluation/SVG, codec roundtrip and literal-versus-reference behavior. They are not a new GUI or full-suite run. The last copy/reuse process exited, but its final closing actor is unverified.

Next task: respect Dot's DEV-25 PRIMARY priority; Nect remains the approved fallback when that work has no executable item. Before any further Nect GUI launch, establish that the prior unexpected input/window loss no longer reflects competing user or agent activity. Do not ask the user to repeat an already-passed acceptance merely to fill time. While GUI is locally held, select a concrete approved non-GUI gap from current source and management records, without inventing guards, tests or scope. Recheck source/GUI writer ownership at the point of work.

Approach: one writer per affected file or GUI, isolated copies for native acceptance, existing Session/evaluator/renderer authority unchanged. Reuse accepted evidence. Keep private machine paths and raw execution logs out of this public checkpoint. Public PRs establish merged code; access-controlled management records establish the specific native observations.

Remaining: linked palette/Capture native acceptance, physical IME/input, native OS-font discovery and asynchronous/in-flight save/asset conditions have not been established by these slices. Cold process restart is not OS-reboot recovery. The current GUI hold is local, not a declaration that every remaining task is blocked or that the product is complete.

Done for next: one useful approved outcome, its smallest relevant verification and an updated existing checkpoint; do not re-run #323/#324/#325 or accepted native slices absent new failure evidence. A document update alone is not product acceptance or an automatic worker restart.

Authority: retain the Mission Brief's adopted scope and ordinary validated Git integration only. Preserve the earlier automatic-review refusal for #296 and the Field denial; do not retry either through another route. Release, added billing, force/shared-history rewrite, credential changes and unrelated data changes remain outside scope. Earlier stopped work remains stopped unless a current explicit instruction authorizes it.

Handoff: CONTINUE_CURRENT_TASK within these boundaries; no GUI launch until the competing-input condition is resolved.
