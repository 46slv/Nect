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
