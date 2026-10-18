# Named node presets — REQ-23 bounded first phase

Source: https://app.notion.com/p/3e0fd279a6f381a3bc71eb08c627bec7
Base: ca376c67529364a4518bdf06b6893bd3c8896392 (PR31).

The document stores named snapshots of selected source/effect nodes. Applying a
preset appends ordinary editable nodes in any Composition of the same document.
It never replaces Output, its connections, existing nodes or the renderer.

## Snapshot admission contract

- Select one or more distinct existing nodes. Output and Image Output are refused
  (not silently skipped). Composition Reference is refused, including null targets.
- Every retained socket, expression, Reference and Text path-attachment dependency
  must point inside the selection. Inactive/bypassed references count too.
- Image and non-default Text/style fonts must exist as unchanged document assets.
  Their immutable asset records are captured with the snapshot and compared at
  validation/application. Missing or changed assets are refused. Bundled default
  Text uses the existing runtime font contract. No external file is read.
- Existing node validation and duplicateNodes identity/remapping restrictions apply,
  including refusal of edited Path Remap together with its source. Snapshot nodes
  are validated as an isolated graph. There is no automatic dependency expansion.
- Names are trimmed, nonempty, at most 100 characters and unique (case sensitive).
  Preset IDs are stable document identities; name or ID collisions refuse.
- Save/update/apply validate a private candidate and commit one Undo entry. Invalid
  input changes neither document, revision nor history. A snapshot is detached
  from its original nodes. Update explicitly replaces the selected preset snapshot.

Schema 39 adds optional `nodePresets`; old documents retain their version until
the first preset is saved. Existing schema floors and backfills remain in place.
Commands: `save-node-preset` (`layerId`, `nodeIds`, `name`, optional `presetId`),
`update-node-preset` (same plus existing `presetId`; `name` optional), and
`apply-node-preset` (`layerId`, `presetId`, optional finite `[x,y]` offset).
UI and automation dispatch these same CommandSession commands.

Macro public controls, external preset files, shared libraries and effect-stack
redesign are excluded. This is partial REQ-23 coverage, not overall completion.

## Bounded verification (SS-Note-ROG)

- Focused tests: **35/35 PASS** with `node --test tests/named-presets.test.js
  tests/node-clipboard.test.js tests/repeater-ae.test.js
  tests/opentype-features.test.js tests/command-session.test.js`.
- Production `npm run build`: **PASS**, 126 modules. Existing large-chunk warning
  remains. No dependency or lockfile changes.
- Actual Chrome **155.0.8059.80**, disposable profile and fixture documents:
  **PASS**. UI save/apply/ordinary Inspector edit/update, one Undo/Redo per changed
  operation, detached source snapshot, fresh IDs/internal connection, original
  meaningful target Output/graph retained, name/ID collisions, external reference,
  invalid offset, malformed snapshot and missing-asset atomic refusal, JSON reload.
  Page errors: **0**. [Portable receipt](evidence/req23-named-presets-chrome.json).
  Reproduce after build with `node tests/named-presets.e2e.js` (Chrome path can be
  overridden by `CHROME_PATH`). Local screenshot, visually inspected:
  `test-results/named-presets/editor.png`.
- Core tests additionally cover repeated apply, source-node deletion, updated
  snapshot reload, internal expression/Reference and Curve/Multi Path identity
  remapping, embedded Text font, unchanged-update no-op, stale envelope,
  active gesture, locked destination and retained schema floors/backfills.
- `git diff --check`: **PASS**. Scope-only local diff review found no unresolved
  material decision. External consultation gate **not-required**, zero messages;
  current source, focused contracts and browser evidence supplied the checks.
  Mandatory learning gate **PASS / NO_REUSABLE_DELTA**; fixture selection assertions
  and project-specific admission semantics remain in these focused tests.

Budgets: at most 100 presets, 1–1999 nodes per snapshot, 2,000 nodes per target
Composition, existing 50 MB project limit (including saved asset records). Invalid
commands preserve revision and Undo/Redo state. The browser fixture uses synthetic
artwork only; existing worktrees, user documents and old local app servers are
untouched. Notion is read-only in this task.

Native Tauri/EXE and physical IME remain **unverified**. No global full-suite audit,
merge, release/distribution or authentication/permission change was performed.
