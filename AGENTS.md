# Agent entry

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

## Practical-alpha Mission: one checkpoint per Task

For the `Nect/practical-alpha` Mission, the user's 2026-09-22 addendum makes
**1 semantic checkpoint = 1 Task** mandatory. This overrides the optional
fresh-context judgement below. Implementation, debugging, verification and repair
may continue within the current checkpoint; do not implement the next checkpoint
in the same Task after `Done for next` is satisfied.

At completion:
1. save the durable checkpoint, make a coherent commit, push the working branch
   and verify local/remote HEAD equality;
2. select the next checkpoint from live evidence and update the single active
   checkpoint in `CURRENT_GOAL.md` (synchronize any resulting handoff edit);
3. actually create a new Task for that checkpoint when the runtime supports it;
4. bind it to the same `Nect/practical-alpha` Mission through this repository's
   Mission Brief and active checkpoint, with repo/branch/HEAD and needed owners;
5. end the current Task. Transfer implementation ownership once; never leave two
   writers on the same working tree or live Session.

Use a fresh Task, not a conversation fork. Transfer only Mission Brief, latest
active checkpoint, repo/branch/HEAD and the owner documents needed for that work.
Do not transfer old conversation, raw logs or completed-work transcripts.
If new-Task creation is unavailable, leave a complete handoff marked
`WAIT_SUCCESSOR_TASK` and stop without implementing the next checkpoint here.

## Fresh-context rotation

Past conversation, raw logs and completed work should not be carried into the next context in
bulk.

When context becomes large, or a coherent checkpoint boundary permits a fresh context:
1. completely save the current active checkpoint;
2. synchronize the durable state as required below;
3. end the current context.

A fresh context reads only:
1. Mission Brief;
2. latest active checkpoint;
3. current repo/runtime state;
4. owner docs needed for the active checkpoint.

Do not reconstruct state from the full previous conversation.

Useful rotation signals include:
- active checkpoint completed;
- next checkpoint changes owner/domain substantially;
- research transitions into implementation;
- most accumulated tool/log context is no longer relevant;
- substantial context compaction has already occurred.

Do not rotate merely because a fixed amount of time passed, and do not interrupt a coherent
task only to satisfy a context rule.

If the runtime cannot actually start a fresh successor, save the checkpoint and stop. Do not
pretend a successor was launched.

## Autonomous continuation and stop conditions

Continue autonomously while the Mission Goal remains authorized and the next safe action is
clear.

Stop only for:
- Mission Goal completed;
- a concrete blocker that cannot be resolved with current authority/access;
- an authority boundary or required user decision;
- repeated no-progress with no new evidence;
- a required fresh-context handoff that this runtime cannot perform.

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

GitHub synchronization means preserving the remote checkpoint branch. It does **not**
authorize merging to `main`, closing the Mission, releasing, publishing, or changing
repository/account policy. Those actions require current authorization.

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
