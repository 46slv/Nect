# START HERE — Nect

This is the single entry point for a fresh implementation agent.

## What Nect is

Nect is a standalone-first 2D graphics authoring tool with:
- addressable Bézier point/handle properties
- non-destructive editing and cross-property references
- vector + raster + compositing as one long-term product direction
- Illustrator / Photoshop interoperability
- first-class API and MCP automation

Do not infer that every product candidate is in the current implementation scope.

## Source-of-truth order

For implementation work, use this order:

1. current user instruction
2. this repository's code, tests, schemas, and runtime evidence
3. `CURRENT_GOAL.md`
4. `AGENTS.md`
5. relevant owner doc only:
   - ownership/data flow → `ARCHITECTURE.md`
   - native model → `docs/model-v0.md`
   - quality/anti-slop → `docs/quality.md`
   - first usable acceptance → `docs/first-usable.md`
   - dependencies/stack → `DEPENDENCIES.md`, `docs/technology.md`
6. linked GitHub issue/PR if the current goal references one
7. Notion only when a product requirement/decision is missing or disputed

Past chat history is background, not continuity authority.

## Start sequence

1. Inspect the repo and `git status`.
2. Read `CURRENT_GOAL.md`.
3. Read `AGENTS.md`.
4. Open only the owner docs/code/tests relevant to the current goal.
5. Run the smallest existing verification that proves the baseline is healthy.
6. Continue the goal autonomously until Done, a real blocker, or a semantic checkpoint.

Do not recursively read every document before acting.

## Working-tree convention

When operating on the user's main Windows machine, the intended local working tree is:

`D:\Documents\Nect`

Confirm the actual repository state before changing it. Do not assume the path exists on other hosts.

## Astra / Sol execution model

Default: **Astra is the autonomy-first primary executor** when it can access the working tree, build/test tools, and UI.

Escalate to Sol only when evidence shows a real coordination/reasoning boundary, such as:
- an ownership/architecture decision would change `ARCHITECTURE.md`
- the same failure repeats without new evidence
- a difficult interoperability boundary is unresolved
- a change crosses several protected owners/invariants
- multiple independent goals need decomposition
- Astra explicitly cannot resolve a decision from repo/runtime evidence

Do not create permanent coordinator/verifier agents just because the task is large.

## Context rollover / next Astra

A fresh equivalent Astra may take over at a coherent checkpoint. Resume from durable repo state, not a long transcript.

Before handoff, preserve:
- current goal and phase
- branch and HEAD
- completed evidence/tests
- dirty/uncommitted state
- pending/unknown operations
- blockers/unresolved decisions
- next safe action

Prefer a coherent checkpoint commit when valid. If work must remain dirty, describe exactly what is dirty.

A successor re-runs the Start sequence above. Old chat context is optional.

## Scope rule

A Candidate requirement or research idea is not implementation authorization.

Implement only:
- the current user instruction,
- `CURRENT_GOAL.md`,
- and any explicitly selected requirement subset.

If those conflict, stop only for the conflicting decision; otherwise continue autonomously.
