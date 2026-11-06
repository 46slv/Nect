# Node Foundation: Value, Vector and Expression

This slice moves the three existing value-source nodes into the trusted built-in manifest. Their declarations share the `builtin.read-value.v1` implementation binding; the existing host still resolves incoming connections, safe numeric expressions and authored fallback. No registry kernel, Session, codec or per-type UI behavior is added.

## Preserved contracts

- Value and Expression keep Scalar defaults of 100; Vector keeps Vector2 `[300, 250]` and the existing `canvasPositionHandle: false` factory field.
- The legacy unnamed output, unsupported bypass, factory operation field, definition/catalog order and Japanese search metadata remain unchanged. Named `default`, `output` and `x` ports remain invalid.
- Connected values take precedence over expressions, followed by authored fallback. Schema 26 document data does not acquire declaration or implementation metadata.
- Parameter changes, failed connections, Undo/Redo, cache reuse/invalidation and save/reload use the existing common host and CommandSession routes.

## Local verification

Baseline: `99d011c283c8738428dbc9e9bf59ebaf017ef657`, including Combine Vector and main `7da07c75`. The work was performed in a separate value-source worktree, without editing the primary owner's worktree or using a new Codex task.

Focused Node tests: 61 passed, 0 failed, skipped or cancelled, across these 12 files, run serially with `node --test --test-reporter=tap --test-concurrency=1`:

- `node-foundation.test.js`, `registered-node-projections.test.js`, `math.test.js`
- `node-foundation-separate-vector.test.js`, `node-foundation-separate-vector-ui.test.js`
- `node-foundation-vector-math.test.js`, `node-foundation-combine-vector.test.js`, `node-foundation-combine-vector-dom.test.js`
- `node-foundation-value-sources.test.js`, `node-foundation-value-sources-dom.test.js`
- `authored-value-expressions.test.js`, `parameter-reference-expressions.test.js`

The production Vite build succeeded with 226 transformed modules. Its existing large-chunk warning remains. Changed JavaScript syntax, formatting and `git diff --check` are checked separately before commit.

A deterministic before/after oracle was byte-identical, covering all definition and catalog order, picker metadata, each factory, ports/types, evaluation precedence and errors, cache statistics, serialized documents/SVG and Session Undo/Redo. Both files have SHA-256 `42fb8ae24755bc130b579d171a2035e6c5bc8cc6050a9aee3a9024c5db85046e`.

The new HappyDOM test uses a prepared document and normal Graph selection, Inspector edits, Undo/Redo and save/reload. It is not evidence of Add Node interaction, native Tauri/IME behavior or real-browser acceptance. The local checks are not the full repository suite or remote CI; exact-head CI is required for the PR before an ordinary merge. No release, protection bypass, approval review or unrelated stopped work is included.
