# Agent entry

## 2026-09-25 standing Mission ownership (DEC-71)

For Nect, the current user instruction and [DEC-71](https://app.notion.com/p/3e6fd279a6f381219079f6b9a024fece) supersede older packet text that stopped at DESIGN_GATE, packet completion, review readiness, a commit/push, or a Task boundary. Codex Sol is the Mission Owner: it selects the next eligible Route step, resolves ordinary engineering ambiguity within accepted product intent, freezes/approves bounded packets, dispatches fresh disposable Luna Max Workers, reviews exact candidates, repairs and rechecks, reconciles Git/Notion/runtime evidence, and continues through durable checkpoint and fresh Task rollover. ChatGPT is optional external review, not a mandatory checkpoint relay. P02 implementation is authorized from `P02-ENTRY-01` on `codex/practical-alpha`.

Mission continuity is above a Task boundary. One Sol Task may execute multiple coherent semantic checkpoints while its context remains healthy. Ordinary checkpoint/packet/review/commit transitions, including P02-A to P02-B to P02-C, do not require a fresh top-level Task or a user-facing stop. Rollover only for material context pressure, a meaningfully independent next phase, a capability/authority boundary, or a clear safety/clarity advantage. On rollover, save the durable checkpoint, create a fresh Sol successor, verify its Nect Project/model/effective authority/HEAD/single-writer state and `TAKEOVER_ACK`, then transfer ownership and end the old Task. If creation is unavailable, leave `HANDOFF_READY` with a complete resume prompt. A terminal Worker is disposable; dispatch a fresh one for later repair unless new evidence is still being developed in the same packet.

Ordinary reversible lifecycle inside the current Mission is authorized: edit/build/test, coherent commit, non-force push, branch/PR maintenance and non-force merge after required checks. Human confirmation is reserved for actual hands-on/subjective product acceptance, material preference-dependent product/UX/art choices, replacement or major expansion of the accepted Goal, platform-required confirmation, credentials/account/billing, destructive or irreversible action, release/publication and other public external action. Existing confirmed requirements and non-goals remain protected. Do not interpret this authority as permission for force push, history rewrite, unrelated mutation, or unverified completion claims. Where older sections below are narrower on Mission progression or ordinary Git lifecycle, this dated section controls; their Document/Session, identity, evidence and safety rules still apply.

If you arrived from only a GitHub repository link, read `START_HERE.md` first.

Current scope is selected by the current user instruction plus `CURRENT_GOAL.md`.
A broad mission may authorize bounded follow-on work, but product Candidates are not
blanket implementation scope merely because they exist in Notion.

Read `ARCHITECTURE.md` when changing ownership or data flow. For model/codec work read
`docs/model-v0.md` and the relevant schema. For quality decisions read `docs/quality.md`.
Do not recursively read the full backlog or historical chat.

Protected boundaries:
- GUI, CLI, API and MCP use the same core Session commands.
- Stable point IDs are not array positions; rename/reorder must not retarget references.
- Binding/evaluation is pure. Failed commands do not commit partial authored state.
- Preserve native authored state when producing lossy interchange output.
- State unsupported capabilities explicitly; a JSON-lines server is not an MCP server.
- Do not distribute fonts, credentials, proprietary SDKs or unlicensed plugin binaries.

Build/test entrypoints are in README. Focus checks on the change; release/host validation
is not mandatory for every small edit. Do not redesign the architecture for a local task.

Notion owns product requirements/decisions. This repository owns implementation truth.

## Long Mission model

Treat substantial work as a long-lived Mission whose continuity is durable in the repo,
not in one chat context.

At Mission start:
1. confirm the final Goal, completion conditions and hard constraints;
2. record them in `CURRENT_GOAL.md` as the Mission Brief;
3. divide the Mission into a coarse sequence of semantic checkpoints;
4. make only the active checkpoint concrete; keep distant checkpoints intentionally coarse.

Do not freeze a detailed far-future plan. Results from the current checkpoint may change the
order, split or contents of later checkpoints without changing the Mission Goal.

For Nect implementation, prefer one primary executor that owns planning, implementation,
integration and local judgement for the active checkpoint. Avoid permanent planner /
implementer / reviewer / verifier hierarchies.

## Mission Brief and active checkpoint

`CURRENT_GOAL.md` is the default durable home for:
- the Mission Brief; and
- exactly one active checkpoint.

The Mission Brief should remain compact:
- Final Goal
- Completion conditions
- Constraints / authority boundaries
- Coarse checkpoint map

The active checkpoint is the only checkpoint that should be detailed. It must contain at least:

```text
ACTIVE CHECKPOINT
Goal:
Current phase:
Proven:
Next task:
Approach:
Done for next:
State:
Authority:
Handoff: CONTINUE_CURRENT_TASK | NEW_TASK
```

Meaning:
- **Goal** — the bounded semantic result for this checkpoint.
- **Current phase** — where execution currently is.
- **Proven** — only durable evidence already established; point to tests/receipts/commits.
- **Next task** — the best current next action, not an irrevocable instruction.
- **Approach** — concise intended route for the next action.
- **Done for next** — what must be true before advancing to the following checkpoint.
- **State** — branch/HEAD, working-tree state, relevant format/version and live blockers.
- **Authority** — the current instruction/source that authorizes this work and any stop boundary.
- **Handoff** — whether a material rollover condition exists. Default to `CONTINUE_CURRENT_TASK` across coherent checkpoints; `NEW_TASK` requires an actual context, independence, capability/authority, or safety/clarity reason.

Historical checkpoints belong in Git history, an existing issue/PR, or evidence receipts.
Do not keep multiple old checkpoints active and do not paste completed logs back into the
current one.

## Checkpoint execution

Within an active checkpoint:
1. inspect current repo/runtime state;
2. implement the bounded work;
3. verify the real acceptance path;
4. fix issues exposed by that verification;
5. save a durable checkpoint only after implementation and verification are coherent.

Keep evidence proportional. Point to receipts, files, tests and logs instead of copying large
outputs. Human review is not required for correctness that can be established through the
real semantic API/tests/runtime, but visual/interaction claims require actual UI evidence.

`Next task` is a candidate based on current evidence. If live code/runtime state conflicts
with it, prefer the live source of truth and replan within the same Mission Goal. Record the
change rather than following stale checkpoint text mechanically.

## Practical-alpha Mission: checkpoint continuity

The 2026-09-25 DEC-71 clarification replaces the older one-checkpoint-per-Task rule. A Sol Mission Owner normally implements, verifies, reviews, repairs if needed, commits, non-force pushes, updates the single active checkpoint in `CURRENT_GOAL.md`, and continues to the next eligible checkpoint in the **same Task**. A checkpoint is a durable semantic state, not an automatic Task boundary. Keep only the active checkpoint detailed; use Git history, existing issues/PRs or evidence receipts for closed ones.

At each checkpoint closure, record exact local/remote state and the next best action from live Route/requirements/runtime evidence. Set `Handoff: CONTINUE_CURRENT_TASK` unless there is material context pressure, a meaningfully independent next phase, a capability/authority boundary, or a concrete safety/clarity advantage to fresh context. Do not rotate because a fixed amount of time elapsed, a packet became REVIEW_READY, tests passed, or a commit/push completed.

When `Handoff: NEW_TASK` is justified, complete and synchronize the durable checkpoint first. Transfer only the Mission Brief, single active checkpoint, repo/branch/HEAD, runtime state and owner documents needed for the next work; do not transfer raw logs or whole conversation history. Create a fresh Sol Task in the same Nect Project with authorized Full access/no routine approvals when the runtime supports it. The successor must fresh-verify its actual model/profile, effective authority, HEAD, dirty state and single-writer ownership, then record `TAKEOVER_ACK` **before** the old Task ends or the successor mutates the shared worktree/Session. Transfer one writer once. If fresh Task creation is unavailable when rollover is genuinely required, leave `HANDOFF_READY` and a complete resume prompt.

## Task boundaries and fresh-context rotation

Use the current Task for coherent P02-A/B/C/D checkpoints while context remains healthy. Rotate only for one of the DEC-71 conditions above. A fresh Task is an operational context change within the same Mission, never a product completion gate or reason to ask the user for routine approval. The successor reads live state before acting; it does not reconstruct authority or results from the old conversation.

## Successor Task access and approval policy

A successor Task should not be launched in a restricted mode that forces routine approval
prompts for work already authorized by the Mission.

When the runtime supports per-Task access/approval settings, launch the successor with the
broadest **project-scoped** access already authorized by the current Mission, including as
needed:
- read/write access to the Nect worktree and normal build/test artifacts;
- build/test/process execution;
- Windows UI/runtime interaction needed for Nect validation;
- ordinary network/tool access needed for documented dependencies or research;
- Git operations and push/update of the current working branch.

For these already-authorized, reversible project operations, prefer a non-interactive /
no-routine-approval mode so the Task can run autonomously. Routine edits, tests, local process
control and working-branch synchronization should not stop for confirmation merely because a
new Task was created.

This access policy does **not** expand Mission authority. It does not authorize:
- merging without the current Mission's required checks or by force;
- release/publishing/distribution;
- repository visibility, permissions, protection or account-policy changes;
- destructive rewrite of pushed/shared history;
- credential disclosure or unrelated filesystem/account access;
- irreversible external side effects outside the authorized project scope.

If the runtime cannot provision sufficient project-scoped access without interactive
approval, do not start a crippled successor and then repeatedly ask for routine approvals.
Save the handoff and stop with `WAIT_SUCCESSOR_ACCESS`, naming the exact missing capability.

Task creation and access provisioning are operational mechanics, not product decisions. When
the Mission has already authorized the work, do not ask the user again for approval merely to
continue the same authorized Mission in a fresh Task.

## Autonomous continuation and stop conditions

Continue autonomously while the Mission Goal remains authorized and the next safe action is
clear. An ordinary semantic checkpoint, REVIEW_READY state, packet closure, commit/push or
P02 phase transition is never by itself a stop condition.

Stop only for:
- the Brief/Mission completion gate actually achieved;
- a DEC-71 Human Gate requiring actual hands-on/subjective acceptance, a material product
  preference choice, platform confirmation, credentials/account/billing, destructive or
  irreversible action, release/publication, or major Goal replacement/expansion;
- a concrete runtime/capability blocker that cannot be resolved with current access;
- repeated no-progress with no new evidence, recorded as a concrete blocker;
- a genuinely required rollover when Task creation is unavailable, with `HANDOFF_READY`
  and a complete resume prompt.

Ordinary reversible implementation/UI decisions are not stop conditions.

## GitHub synchronization

When GitHub access is available, a durable semantic checkpoint should be synchronized
remotely or carry a concrete sync blocker.

For a checkpoint:
- create a coherent local commit when the work is valid;
- push the working branch;
- reuse/update the existing PR or issue when useful rather than creating one per checkpoint;
- verify the remote branch/PR HEAD matches the intended local checkpoint SHA;
- record the remote pointer in the handoff when one exists.

GitHub synchronization means preserving the remote checkpoint branch. The current DEC-71
Mission authority allows a non-force merge after the relevant required checks; a push alone
does not prove those checks or close the Mission. Release, publication, repository/account
policy changes and forced history changes remain outside ordinary synchronization.

Before commits intended for GitHub, verify the configured identity satisfies repository
privacy/protection rules. Prefer the user's GitHub noreply identity when appropriate.

If an unpublished local branch is blocked only because the executor's own local commits
contain a disallowed private author/committer email, metadata may be corrected on that
unpublished branch while preserving trees and recording the pre-rewrite HEAD. Never rewrite
pushed/shared history for this purpose without explicit authorization.

If synchronization is blocked by authentication, protection, GH007/private-email checks,
permissions, network failure or another external constraint:
- keep the coherent local commit/checkpoint;
- record exact local HEAD and blocker;
- do not claim GitHub is synchronized;
- make restoring synchronization an early next action when it can be resolved without
  changing repository/account policy.

Do not change repository visibility, permissions, protection rules, account privacy
settings, release state or distribution settings merely to make a push succeed.

## Delegation and model routing

Delegate primarily to reduce information volume, not to split responsibility.

Use a lightweight research worker for bounded information-heavy work such as:
- long official documentation/specifications;
- broad prior-art or compatibility surveys;
- repository reconnaissance requiring large reading volume;
- summarizing large evidence sets.

Give the worker a narrow question and source scope. It returns a compact evidence packet with
conclusions, relevant edge cases and source pointers. Do not forward long raw research
transcripts into the implementation context.

For the current model family:
- prefer Astra when the active checkpoint needs difficult implementation, tool-heavy debugging,
  Windows/GUI operation, or long-horizon software integration;
- prefer Sol for reasoning-heavy architecture/specification synthesis when direct host
  interaction is not the hard part;
- prefer Luna for focused/repetitive reading, extraction, classification and short edits.

Start with the lowest reasoning effort likely to succeed and raise it only when evidence shows
the task needs more. Do not create extra agents merely because a stronger model is available.

Use a higher-reasoning consultation only at a real boundary: unresolved cross-owner
architecture, conflicting requirements, repeated failure without new evidence, difficult
interoperability semantics, or genuinely ambiguous acceptance. After the decision, execution
ownership returns to the primary executor.
