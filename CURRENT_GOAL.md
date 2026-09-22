# Practical-alpha Mission

## Mission Brief

**Mission identity:** `Nect/practical-alpha` — durable owner: this file on `codex/practical-alpha`. Every successor Task belongs to this same Mission.

**Final Goal:** Make Nect usable for everyday 2D graphics production as a practical alpha.

**Completion conditions:** A representative editable composition can be created and revised through the Windows GUI, protected/saved/reopened, and exported without loss of native intent. Geometry, Text, images, color and compositing work together; GUI/API/MCP share Session semantics and unsupported requests fail explicitly. Current contracts pass and representative warm interaction meets the recorded 30 fps floor. Close concrete daily-workflow blockers, not the whole backlog.

**Constraints / authority:** The 2026-09-22 user instruction resumes the Mission and supersedes the image-slice stop. Astra is primary executor. Preserve existing work; reversible local UI decisions and checkpoint commits/pushes to `codex/practical-alpha` are authorized. No main push/merge, release, new dependencies, proprietary assets or account-policy changes. Notion owns product intent; `docs/product-direction.md` is the accepted scoped extract. Native authored state remains authoritative.

**Coarse checkpoint map:**
1. Live capability/gap reassessment and image-workflow baseline — completed; evidence in `docs/first-usable.md#practical-alpha-live-reassessment--2026-09-22`.
2. Gentle Snap for everyday object placement — completed (`8aefe4d`); acceptance in `docs/first-usable.md`.
3. Independent object duplication for everyday revision/reuse — active. Other output/import gaps remain provisional.
4. Integrated authoring, recovery, output and performance acceptance; reconcile remaining limits against practical-alpha completion.

Far checkpoints remain provisional. Inherited Artboard content, full AI/PSD, full node graphs, all blends, pixel painting and all Candidates are not automatic gates. The current capability/gap table lives in `docs/first-usable.md`, not a copied backlog here.

## ACTIVE CHECKPOINT

**Goal:** Duplicate selected objects/Groups into independently editable copies through one shared Session command and a discoverable GUI action, preserving native sources and deliberate reference semantics.

**Current phase:** Selected for a fresh Task; not implemented. Snap checkpoint `8aefe4dfa42e3e946ba9ead8247c99bf639df605` is complete and synchronized. This predecessor only saves/dispatches the handoff, and must not implement duplication here.

**Proven:** Snap acceptance/known limits are summarized in `docs/first-usable.md#gentle-object-placement-snap--2026-09-22`. Current live core, adapter and Window have no object-duplication command/action; only Artboard-frame duplication exists (`window.cpp::add_artboard`). Making a second independent variation currently requires recreation. Native model 0.13 already supports retained shapes, Text, Images, masks, references and Transform Parents, so copies must preserve these meanings.

**Next task:** First verify actual runtime Full access / no-approval policy. Then inspect the live repo and implement the smallest coherent object/Group duplication slice using the common Session path. Confirm the ID/reference ownership and placement behavior against current model/code before editing.

**Approach:** A one-shot copy is independent authored state, not a reusable instance/Symbol system. Generate fresh owned IDs for objects and their nested sources/points/operations/gradients/masks as the model requires. Remap references wholly inside the copied closure to copied IDs; preserve outgoing references to unselected objects/shared Named Colors/assets; keep existing inbound references targeting the originals. Retain source/procedural/Point Edit/Text/Image data. Avoid copying selected descendants twice when a selected Group already owns them. Preserve stacking, placement and explicit effective parenting; reject unsupported cross-scope/dependency cases atomically instead of silently flattening or retargeting. Start with duplicate-in-place plus selected copies, with any convenient GUI displacement explicit through the same semantic path. Choose bounded adjustments from live evidence; no clipboard protocol, cross-document copy, general instance architecture or codec work in this checkpoint.

Read `ARCHITECTURE.md`, `docs/model-v0.md` and relevant schema before changing command/data ownership; `docs/product-direction.md` only for reuse/reference/interaction intent. Primary code owners: `include/nect/core.hpp`, `src/core.cpp`, `src/io.cpp`, `src/history.cpp`, relevant expression/reference traversal, `src/desktop/window.cpp` and `canvas.*` selection. Focus tests on core/reference/transform/native round-trip and GUI Undo/selection; consult `docs/quality.md` for acceptance. Do not read historical chats or the full backlog.

**Done for next:** GUI and machine clients can duplicate a representative retained object/Group selection in one Undo; copies are selected and independent edits leave originals intact. Internal references remap, external/shared references preserve documented intent, and originals' inbound links remain unchanged. Group/parenting/stack order and stable IDs survive save/reopen. Invalid requests leave no partial state. Focused contracts plus actual Windows interaction demonstrate the flow. Save a coherent checkpoint, commit/push and verify remote HEAD, select the next bounded Mission checkpoint, create its dedicated fresh Task and end that Task. Do not implement a second checkpoint in it.

**State:** Existing checkout `D:\Documents\Nect`; branch `codex/practical-alpha`; upstream `origin/codex/practical-alpha`. Snap implementation and verified remote HEAD: `8aefe4dfa42e3e946ba9ead8247c99bf639df605`. This handoff's exact SHA is the commit containing this file; verify `git rev-parse HEAD` equals `git ls-remote origin refs/heads/codex/practical-alpha`. Working tree should be clean after handoff commit. Remote: https://github.com/46slv/Nect/tree/codex/practical-alpha. Native 0.13 unchanged. Owned Window/benchmarks closed; no active Session writer, pending mutation or modified examples. One sole successor executor uses this checkout.

Known limits: initial full suite in Snap checkpoint had the previously recorded image-chooser timeout; isolated Image rerun and corrected Snap/batch fixtures passed. Root cause of intermittent chooser teardown remains unproven. Snap ON meets the 30 fps p95 interval floor on both 80-curve and mixed-image scenes, while 80-curve release measured 33.85 ms. See durable acceptance summary; no blanket 60 fps or all-scene release claim.

**Authority:** Same `Nect/practical-alpha` Mission Brief and 2026-09-22 user continuation; this is the bounded next revision gap authorized by checkpoint selection, not the whole Candidates backlog. Reversible local implementation and working-branch checkpoint commits/pushes are authorized; no main merge/push, release, new dependencies or account/repository policy changes. Mandatory one semantic checkpoint per Task and fresh-Task handoff apply.

**Successor permission requirement:** User explicitly requires Full access and no approval prompts on new Tasks/chats. Existing user config was already `approval_policy="never"` / `sandbox_mode="danger-full-access"`; it was not modified. App `create_thread` exposes no permission override. The successor must report its actually supplied runtime permission profile before commands/implementation, and proceed only if Full access/no approvals is active. If restricted, stop as `WAIT_SUCCESSOR_TASK — FULL_ACCESS_NOT_APPLIED`, do not repeatedly request command approvals, and report this app-runtime mismatch. Prompt/config values alone are not proof of granted access. Transfer no old conversation or raw logs.
