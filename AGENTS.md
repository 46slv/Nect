# Agent entry

If you arrived from only a GitHub repository link, read `START_HERE.md` first.

Current scope: `CURRENT_GOAL.md`.

Read `ARCHITECTURE.md` when changing ownership or data flow. For model/codec work read
`docs/model-v0.md` and the relevant schema. For quality decisions read `docs/quality.md`.
The parity backlog is a discovery shelf, not blanket implementation authorization.

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
Do not copy the whole Notion backlog into prompts or implement Candidates unless selected by the current goal.

## Astra rolling mission execution

Astra is the default mission owner, planner and primary implementer when it has the
working tree, build/test path and required UI access.

For a substantial mission, Astra should:
1. inspect the live repository/worktree and current checkpoint;
2. maintain a compact rolling plan of roughly 3–5 bounded semantic tasks;
3. implement the current task end-to-end;
4. update the rolling plan when implementation evidence changes priorities;
5. leave a durable checkpoint before moving to an independent task.

Do not create a permanent planner/coordinator/reviewer/verifier hierarchy for work that
Astra can complete coherently itself. Mission continuity lives in repository state and
checkpoints, not in a long chat transcript.

### Checkpoints and context rotation

At a coherent semantic boundary, record:
- mission / completed task;
- branch / HEAD and exact working-tree state;
- relevant tests, runtime and performance evidence;
- known failures, limitations and blockers;
- current persisted/native-format state when relevant;
- the next bounded task and why it is next;
- a concise execution approach, acceptance and non-goals for that next task;
- any bounded research request.

The checkpoint is both durable state and the handoff instruction for the next executor.

Prefer a fresh Astra context for the next independent task when the previous task is
complete or when most accumulated logs/tool output are no longer relevant. Other useful
rotation points include a major owner/domain change, research-to-implementation transition,
or substantial context compaction. Do not rotate merely because a fixed amount of time
has passed.

A fresh Astra verifies live state, reads the latest checkpoint and relevant owners, then
continues from the recorded next task. It should not require the previous chat transcript.

Suggested checkpoint shape:

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
- Luna: <bounded research question and expected compact output>
```

### Delegation

Delegate primarily to reduce information volume, not to split responsibility.

Use Luna for bounded reading/research work such as:
- long official documentation or specifications;
- broad prior-art / compatibility surveys;
- repository reconnaissance requiring large reading volume;
- summarizing large evidence sets.

Luna returns a compact evidence packet. Astra remains responsible for planning,
implementation, integration and the final local judgement unless explicitly instructed
otherwise.

Do not routinely create separate planner, implementer, reviewer and verifier agents for a
single coherent task. Astra may use a helper only when the work is genuinely independent
or information-heavy enough to justify the handoff.

Use Sol only at a real reasoning boundary: unresolved cross-owner architecture, conflicting
requirements, repeated failure without new evidence, difficult interoperability semantics,
or genuinely ambiguous acceptance. After the decision, execution ownership returns to Astra.
