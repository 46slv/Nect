# Parameter references in expressions

Numeric expressions now accept a bounded `ref` function:

- Scalar: `ref("node-id", "radius") * 0.5 + 10`
- Vector component: `ref("node-id", "center", "x") + 20`
- Vector2 destination: `ref("node-id", "center", "x"); ref("node-id", "center", "y") + 40`

The searchable expression picker inserts stable node IDs and numeric node-parameter keys. Labels remain human-readable. Renaming does not break references. X/Y are required for Vector2 sources; whole vectors, point/remap-point descriptors and geometry are not numeric expression operands. Color inputs have a separate typed slice described in [Color value and references](color-value-expressions.md). References stay within the current composition. Current/downstream nodes are omitted from the picker to avoid cycles.

References read effective values, including expressions and connected inputs. A destination's connected socket still takes precedence over its expression. Existing arithmetic, constants, functions, and semicolon-separated Vector2 expressions remain available. This is an extension of the existing parser, not JavaScript: arbitrary code, object/property access, and dynamic evaluation are not supported. The 4096-character expression limit, finite-value checks, graph-depth limits, locks, and history transactions remain enforced.

Changes to referenced values invalidate numeric dependencies, including Text's independent Position output, without requiring Text outline generation. Group duplication remaps copied node references; individual copies keep external references. Deleting a source retains an explicit missing-reference diagnostic, so Undo or restoration can recover it. Saved expressions use the existing parameter-expression field; no new graph node or data container is introduced.

Group remapping preserves authored whitespace, including spaces, tabs and newlines between `ref` and `(`. Every internal occurrence is remapped, including both `X; Y` parts and dormant expressions under connected sockets; external and dangling IDs remain unchanged. The bounded remap matcher follows the current restricted quoted-ID grammar, not arbitrary strings or escapes. The parser and command transaction validation remain unchanged; this correction adds no syntax or schema. Regression check: `node --test tests/parameter-reference-expressions.test.js`.

## UI integration

`expressionReferenceTargets(layer, currentNodeId)` returns numeric choices with `nodeId`, `name`, `key`, optional `component`, `label`, and ready-to-insert `expression`. Use the returned expression at the editor selection. `parameterReferenceExpression(nodeId, key, component)` produces the same validated syntax. Apply with the existing `set-parameter-expression` command. Effective results remain read-only. Scalar and Vector2 inputs now expose separate authored Base controls while driven; these use `set-parameter-base`, not the ordinary `set-parameter` command.

## Related interaction corrections

- Splitting Transform scale materializes the current evaluated X scale before Y can change; existing linked/locked Y descriptors remain intact.
- `delete-nodes` accepts `nodeIds`, preserves Output nodes, disconnects surviving input sockets, and commits once. Returned `deletedNodeIds` and `retainedNodeIds` support a clear UI notice. Existing Reference and expression targets remain explicitly dangling.
- Curve/Path Remap `position` gestures accept `pointIds` containing the primary `pointId`. Every anchor receives the same delta in one transaction. Relative handles stay unchanged. Missing, locked, or driven anchors reject the whole group.

## Inline predictive completion

In the ƒ expression dialog, type the beginning of a function, node name (or a word in its name), or numeric parameter key. Up/Down selects a suggestion; Enter or Tab inserts it. A function inserts parentheses and puts the caret inside. Parameter suggestions insert the same stable-ID `ref(...)` syntax as the searchable reference picker, with Scalar or Vector2 X/Y labels and the source node ID to distinguish duplicate names. Typing `ref` also lists eligible references. Only the first 12 matching choices appear; type a longer prefix or use “Insert parameter reference…” for broader searching.

Suggestions use the actual arithmetic grammar: `sin`, `cos`, `tan`, `abs`, `sqrt`, `min`, `max`, `round`, `pi`, `e`, and `value`. They do not add JavaScript, property access, time variables, or whole-vector operands. A Vector2 destination still uses separate `X; Y` scalar expressions. Targets are resolved from the current composition when suggesting and again before insertion, excluding self/downstream nodes.

Completion only edits the draft. Apply (or Enter with no active suggestion) commits through the existing command and Undo history; Cancel leaves the document untouched. Escape first dismisses suggestions, then closes the dialog. Shift+Tab and Tab without suggestions retain normal focus navigation. Suggestions stay hidden inside quoted strings, for selected text, and during IME composition. The original searchable insertion button remains available.

Focused checks: `node --test tests/expression-completion.test.js tests/expression-completion-dom.test.js tests/parameter-reference-expressions.test.js`.

## Authored base expressions (schema 26)

`value` reads the input's saved `q.value`, before its expression. It never reads the previous evaluation result, adds a dependency edge, or accumulates across reevaluation. Numeric expression examples:

- Scalar: `value + 10`
- XY: `ref("neighbor-id", "position", "x") + value; ref("neighbor-id", "position", "y") + value`

XY remains two scalar expressions separated by `;`: the left `value` is Base X and the right `value` is Base Y. With neighbor `[100,200]` and base `[20,10]`, the result is `[120,210]`; changing Base X to 30 gives `[130,210]` without adding a node. IDs must come from a real numeric reference target. This is not whole-vector arithmetic: `ref("neighbor-id", "position") + value`, `value + 10` on XY, Color arithmetic, arbitrary JavaScript, `input`, and automatic broadcasting remain unsupported. Effective self-references via `ref` still reject as cycles.

Driven numeric fields show Base controls, a read-only Result, and collapsible saved Expression. Base edits and horizontal scrubs use the dedicated core `set-parameter-base` command (payload: `layerId`, `nodeId`, `key`, numeric `value`, optional XY `axis`). Enter applies typed Base, Escape restores the typed draft; dragging commits once and Escape cancels the gesture. Parameter/composition locks, finite/type/domain checks, revision protection and atomic history still apply. Ordinary effective/canvas control edits retain their driven guard. Transform scale Base is displayed in percent while `value` uses its stored multiplier (50% means `value = 0.5`).

The precedence is unchanged: source connection > expression > base. Connected fields explicitly label their result and paused expression. Base editing while connected updates only the saved base; it does not change the source or the connected result. Disconnect restores the retained expression (if any), which may or may not use the saved base. Missing references show “Cannot evaluate”; no saved base or last-good value is displayed as a successful result. Invalid active expression results reject a base edit atomically.

The `va` completion offers `value` first and labels Scalar or the current X/Y component. Existing IME, quoted-string suppression, stale-target revalidation and 12-result limit remain intact.

No new parameter field is stored. Applying a bare `value` expression promotes the document to schemaVersion 26, including a dormant expression under a connection. Existing schema 24/25 documents without the new syntax retain their version; quoted `"value"` parameter keys do not trigger promotion. Importing bare `value` under a pre-26 version rejects rather than silently changing old semantics. Earlier readers reject version 26 as unsupported. Clearing an expression does not automatically downgrade a document.

This is the numeric/component slice, not all-input support. Whole-XY operands, Boolean/text expressions, typed-reference chips, dependency navigation/repair and the full design mock remain later work. Synthetic DOM verification is not real-browser rendering or desktop acceptance.

Focused checks: `node --test tests/authored-value-expressions.test.js tests/authored-value-dom.test.js tests/expression-completion.test.js tests/expression-completion-dom.test.js tests/parameter-reference-expressions.test.js tests/numeric-scrub-dom.test.js tests/command-session.test.js tests/gesture-session.test.js tests/transform-anchor-dom.test.js`.

## Typed Color expressions (schema 42)

Color inputs, including Linear/Radial Gradient stop colors, accept `value` and `ref("node-id", "color-key")`. Their existing saved `{hex, alpha}` base stays editable beside the expression and read-only result. Color references use the same composition-local stable IDs and reference picker, filtered to Color properties. Numeric operands cannot read Color values. There is no Color arithmetic or palette storage in this slice. See [the scoped contract and verification](color-value-expressions.md).
