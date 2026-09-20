# Agent entry

If you arrived from only a GitHub repository link, read `START_HERE.md` first.

Current scope is selected by the current user instruction plus `CURRENT_GOAL.md`.
A broad mission may authorize Astra to choose bounded follow-on tasks, but product
Candidates are not blanket implementation scope merely because they exist in Notion.

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

## Astra rolling mission execution

Astra is the default mission owner, planner and primary implementer when it has the
working tree, build/test path and required UI access.

For a substantial authorized mission:
1. inspect the live repository/worktree and latest checkpoint;
2. keep a compact rolling queue of roughly 3–5 bounded semantic tasks;
3. implement the current task end-to-end;
4. revise the queue when implementation evidence changes priorities;
5. checkpoint at a meaningful semantic boundary before an independent next task.

The rolling queue is not a second backlog. Keep each entry compact:
- Goal
- Acceptance
- Non-goals
- Research needed, if any

Do not repeatedly re-plan the whole product. Do not create permanent planner/coordinator/
reviewer/verifier machinery for work Astra can complete coherently itself.

## Checkpoints and context rotation

A checkpoint is both durable state and the handoff instruction for the next executor.

Record:
- mission / completed task;
- branch / HEAD and exact working-tree state;
- relevant tests, runtime and performance evidence;
- known failures, limitations and blockers;
- persisted/native-format state when relevant;
- next bounded task and why it is next;
- concise execution approach, acceptance and non-goals;
- any bounded research request.

Keep checkpoints compact. Point to receipts, files, test names and logs instead of pasting
large outputs or re-summarizing the entire mission.

Suggested shape:

```text
CHECKPOINT
Mission:
Completed:
Branch / HEAD:
Working tree:
Evidence:
Known limitations / blockers:
Persistence / format state:

Next task:
Why next:
Execution approach:
Acceptance:
Non-goals:

Research:
- NONE
  or
- Luna: <bounded question + expected compact evidence>
```

Prefer a fresh Astra context for the next independent task when the current task is complete,
the owner/domain changes substantially, research transitions into implementation, or most
accumulated logs/tool output are no longer useful. A major context compaction is another
signal to rotate at the next coherent boundary.

Do not rotate on a fixed timer, and do not interrupt a coherent task merely to satisfy a
context rule.

If the runtime can start a fresh successor, do so only through the available authorized
mechanism. If it cannot, leave the checkpoint and stop; do not pretend a successor was
started. The next executor must be able to resume from repository state without the old
chat transcript.

## GitHub synchronization

When GitHub access is available, a durable semantic checkpoint should be synchronized
remotely or carry a concrete sync blocker.

For a checkpoint:
- create a coherent local commit when the work is valid;
- push the working branch;
- reuse/update the existing PR or issue when useful rather than creating a new one per checkpoint;
- verify the remote branch/PR HEAD matches the intended local checkpoint SHA;
- record the remote pointer in the handoff when one exists.

GitHub synchronization means preserving the remote checkpoint branch. It does **not**
authorize merging to `main`, closing the mission, releasing, publishing, or changing
repository/account policy. Those actions require current authorization.

Before commits intended for GitHub, verify the configured identity satisfies repository
privacy/protection rules. Prefer the user's GitHub noreply identity when appropriate.

If an unpublished local branch is blocked only because Astra's own local commits contain
a disallowed private author/committer email, Astra may correct metadata on that unpublished
branch while preserving trees and recording the pre-rewrite HEAD. Never rewrite pushed or
shared history for this purpose without explicit authorization.

If synchronization is blocked by authentication, protection, GH007/private-email checks,
permissions, network failure or another external constraint:
- keep the coherent local commit/checkpoint;
- record exact local HEAD and blocker;
- do not claim GitHub is synchronized;
- make restoring synchronization an early next action when it can be resolved without
  changing repository/account policy.

Do not change repository visibility, permissions, protection rules, account privacy
settings, release state or distribution settings merely to make a push succeed.

## Delegation

Delegate primarily to reduce information volume, not to split responsibility.

When Luna or an equivalent lightweight research worker is available, use it for bounded
information-heavy work such as:
- long official documentation/specifications;
- broad prior-art or compatibility surveys;
- repository reconnaissance requiring large reading volume;
- summarizing large evidence sets.

Give the research worker a narrow question and source scope. It returns a compact evidence
packet with conclusions, relevant edge cases and source pointers. Do not forward long raw
research transcripts into Astra's implementation context.

Astra remains responsible for rolling planning, implementation, integration and final local
judgement unless explicitly instructed otherwise.

Do not routinely split one coherent task into planner / implementer / reviewer / verifier
agents. Use a helper only when the work is genuinely independent or information-heavy enough
to justify the handoff.

Use Sol, when available, only at a real reasoning boundary: unresolved cross-owner
architecture, conflicting requirements, repeated failure without new evidence, difficult
interoperability semantics, or genuinely ambiguous acceptance. After the decision,
execution ownership returns to Astra.
