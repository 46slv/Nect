# START HERE — Nect

This is the single entry point for a fresh agent. A repository link is enough to find the current scope; old chat history is not required.

## What Nect is

A standalone-first 2D graphics authoring tool: addressable Bézier properties, non-destructive editing, useful local shape operations, deeper node/effect authoring, interoperability and API/MCP. The full product roadmap is not the current implementation scope.

## Authority and evidence

- The current user instruction and applicable AGENTS.md instructions determine what work is authorized. Reading a repository for review does not itself authorize implementation or an agent launch.
- CURRENT_GOAL.md and its live issue select the implementation slice when implementation is requested.
- Code, schemas, tests and runtime observations establish what currently exists and works. They do not override the user's intended behavior or authorize scope expansion. A test can be wrong; explain a conflict rather than preserving a bug as a requirement.
- Notion owns product intent/decisions; Git owns implementation contracts and history. docs/product-direction.md is a scoped extract for work without Notion access, not a second full backlog. New explicit user decisions take precedence; reconcile meaningful conflicts at the affected boundary.
- Past chats and historical receipts are background, not proof of current runtime state.

## Start sequence

1. Inspect the repository and git status; preserve dirty/uncommitted work.
2. Read AGENTS.md and CURRENT_GOAL.md.
3. Read only the relevant owner: ARCHITECTURE.md for data flow/change boundaries, docs/model-v0.md for persistence, docs/first-usable.md for acceptance, docs/quality.md for quality, DEPENDENCIES.md / docs/technology.md for stack changes.
4. For UI/procedural work, also read docs/product-direction.md. Consult linked Notion material only if the scoped extract lacks a decision; lack of Notion access alone is not a reason to stop a well-specified slice.
5. Run the smallest relevant baseline check and continue the authorized task to a useful result, a real blocker or a coherent checkpoint.

Do not recursively read every document or copy the full backlog into a prompt.

## Working tree

On the user's main Windows machine the intended worktree is `D:\Documents\Nect`. Confirm it exists and points to this repository. Do not assume local state matches main, discard dirty files or force-reset the worktree.

## Astra / Sol execution model

Astra is the autonomy-first primary executor when it has working-tree, build/test and UI access. Use Sol when evidence calls for coordination or a difficult decision: unresolved cross-owner architecture, repeated failure without new evidence, difficult interoperability or decomposition of independent goals. Updating a paragraph in ARCHITECTURE.md does not itself require a second agent.

Do not add permanent coordinator/verifier machinery merely because the project is large.

## Context rollover

Handoff at a coherent checkpoint, not an arbitrary token count. Record in the current issue/PR or existing checkpoint:
- goal/phase, branch/HEAD and relevant evidence
- exact dirty state and pending/unknown operations
- unresolved decisions and the next safe action

Prefer a coherent commit when valid; otherwise preserve and identify the unfinished files. The successor verifies the live state and follows the same start sequence. Keep one clear current handoff pointer rather than competing status documents.

This is a handoff protocol, not proof that automatic spawning or model-to-model rollover exists. Start a successor only through an available, authorized runtime. Do not let two agents unknowingly mutate the same live Session/worktree.

## Scope and evolution

Candidate requirements are a discovery shelf. Preserve data meaning and user work, not every prototype class or panel layout. Small core changes with focused migration tests are allowed when real use reveals a limitation; a wholesale rewrite or general-purpose framework needs concrete justification.
