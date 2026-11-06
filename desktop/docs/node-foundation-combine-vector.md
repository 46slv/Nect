# Combine Vector: registered Node Foundation slice

## Scope and ownership

Implemented on the isolated `dot/node-foundation-combine-20261107` worktree from
`ba042631` (main after #208/#209). This slice owns only the existing Combine Vector
migration and its tests. The primary editor worktree and its color/lifecycle work
are unchanged. Existing publication HOLD and unrelated stopped scopes remain in
force. No new Codex task, native app, browser server, font or image fixture is used.

The current Node Foundation design is
[Node Foundation v1](https://app.notion.com/p/40ffd279a6f381a8a19adccfddc85829).
The previous Foundation owners released their bounded M3 scopes. This is a small
independent continuation, not a claim of Foundation or product completion.

## Implementation

- `combine-vector-declaration.js` owns the existing two Scalar inputs, zero defaults,
  one Vector2 legacy-default output and existing Japanese catalog metadata.
- `combine-vector-binding.js` reads X then Y. Existing host parameter resolution,
  numeric validation and evaluator cache retain authority.
- One trusted manifest entry connects the declaration and binding. Its placement
  preserves both definition and catalog order after Separate Vector.
- The old definition, catalog row, output-type entry and evaluator branch are
  removed. The common registry, Session, codec, Graph and Inspector need no new
  type-specific behavior.

The authored type/name, fallback values, `operation: 'subtract'` factory field,
undefined default output port, source/sourcePort identity, expression priority,
unsupported bypass and save schema are unchanged. The implementation ID and
other declaration metadata are not written into native documents.

## Verification

Final focused command, from `desktop/`:

```sh
node --test --test-reporter=tap --test-concurrency=1 tests/node-foundation.test.js tests/registered-node-projections.test.js tests/math.test.js tests/node-foundation-separate-vector.test.js tests/node-foundation-separate-vector-ui.test.js tests/node-foundation-vector-math.test.js tests/node-foundation-combine-vector.test.js tests/node-foundation-combine-vector-dom.test.js
```

Result: **28 passed, 0 failed, 0 skipped, 0 cancelled**. This is one final run,
not the sum of earlier runs. It includes six new Combine tests and the existing
Separate/Math cache and normal synthetic Graph wiring checks.

The new tests cover factory independence, immutable registry binding, field and
port projection, X/Y read order, source/expression/fallback, invalid ports and
nonfinite input errors, atomic failed Session edits with Redo retained, exact
Undo/Redo, save/reload, and normal synthetic Inspector editing plus Graph output.
The DOM test loads a prepared fixture; it does not claim Add Node UI acceptance.
Two initial test expectations were corrected to the observed legacy contract:
parameter `locked: false` and the distinction between raw catalog order and the
alphabetically sorted picker. No product behavior was changed for those failures.

A deterministic production oracle was captured **before** migration and replayed
after the final source. Factory state, all passive definition/picker projections,
ports, connected and fallback evaluation, encoded documents, SVG and Session
Undo/Redo snapshots are byte-identical. Both JSON files have SHA-256:

`36519691a1d17873c396e332dc19c90c3e3ec4006f52f38f162cc78f6c110f8a`.

Local evidence is under ignored `build-node-foundation-combine-20261107/` at the
repository root: `oracle.mjs`, `before.json`, `after-final.json`,
`focused-final-2.tap` and `build.log`. Final TAP SHA-256 is
`92daf5ab1eb34a61fe87a1af020df737eae5802a03aa67a7767c539e0880b619`.

The isolated Vite build passed with `envFile: false`, `emptyOutDir: false` and
224 modules transformed. The existing large-chunk warning remains. Changed-file
Prettier checks, JavaScript syntax and `git diff --check` pass.

## Limits and rollback

No full-suite, CI, actual browser/native runtime, physical IME or subjective
acceptance was run. This does not verify the entire Node Foundation, general
Path/PaintedPath adapters, script execution or Custom Node Builder. No remote
publication, merge, release or stopped-scope resumption is part of this slice.

The migration is one isolated commit. Reverting it restores the old three legacy
consumer entries and evaluator branch without a native-file migration.
