# Parameter references in expressions

Numeric expressions now accept a bounded `ref` function:

- Scalar: `ref("node-id", "radius") * 0.5 + 10`
- Vector component: `ref("node-id", "center", "x") + 20`
- Vector2 destination: `ref("node-id", "center", "x"); ref("node-id", "center", "y") + 40`

The searchable expression picker inserts stable node IDs and numeric node-parameter keys. Labels remain human-readable. Renaming does not break references. X/Y are required for Vector2 sources; whole vectors, point/remap-point descriptors, colors and geometry are not expression operands in this slice. References stay within the current composition. Current/downstream nodes are omitted from the picker to avoid cycles.

References read effective values, including expressions and connected inputs. A destination's connected socket still takes precedence over its expression. Existing arithmetic, constants, functions, and semicolon-separated Vector2 expressions remain available. This is an extension of the existing parser, not JavaScript: arbitrary code, object/property access, and dynamic evaluation are not supported. The 4096-character expression limit, finite-value checks, graph-depth limits, locks, and history transactions remain enforced.

Changes to referenced values invalidate numeric dependencies, including Text's independent Position output, without requiring Text outline generation. Group duplication remaps copied node references; individual copies keep external references. Deleting a source retains an explicit missing-reference diagnostic, so Undo or restoration can recover it. Saved expressions use the existing parameter-expression field; no new graph node or data container is introduced.

## UI integration

`expressionReferenceTargets(layer, currentNodeId)` returns numeric choices with `nodeId`, `name`, `key`, optional `component`, `label`, and ready-to-insert `expression`. Use the returned expression at the editor selection. `parameterReferenceExpression(nodeId, key, component)` produces the same validated syntax. Apply with the existing `set-parameter-expression` command. Numeric controls driven by an expression remain read-only until the expression is cleared.

## Related interaction corrections

- Splitting Transform scale materializes the current evaluated X scale before Y can change; existing linked/locked Y descriptors remain intact.
- `delete-nodes` accepts `nodeIds`, preserves Output nodes, disconnects surviving input sockets, and commits once. Returned `deletedNodeIds` and `retainedNodeIds` support a clear UI notice. Existing Reference and expression targets remain explicitly dangling.
- Curve/Path Remap `position` gestures accept `pointIds` containing the primary `pointId`. Every anchor receives the same delta in one transaction. Relative handles stay unchanged. Missing, locked, or driven anchors reject the whole group.

## Inline predictive completion

In the ƒ expression dialog, type the beginning of a function, node name (or a word in its name), or numeric parameter key. Up/Down selects a suggestion; Enter or Tab inserts it. A function inserts parentheses and puts the caret inside. Parameter suggestions insert the same stable-ID `ref(...)` syntax as the searchable reference picker, with Scalar or Vector2 X/Y labels and the source node ID to distinguish duplicate names. Typing `ref` also lists eligible references. Only the first 12 matching choices appear; type a longer prefix or use “Insert parameter reference…” for broader searching.

Suggestions use the actual arithmetic grammar: `sin`, `cos`, `tan`, `abs`, `sqrt`, `min`, `max`, `round`, `pi`, and `e`. They do not add JavaScript, property access, time variables, or whole-vector operands. A Vector2 destination still uses separate `X; Y` scalar expressions. Targets are resolved from the current composition when suggesting and again before insertion, excluding self/downstream nodes.

Completion only edits the draft. Apply (or Enter with no active suggestion) commits through the existing command and Undo history; Cancel leaves the document untouched. Escape first dismisses suggestions, then closes the dialog. Shift+Tab and Tab without suggestions retain normal focus navigation. Suggestions stay hidden inside quoted strings, for selected text, and during IME composition. The original searchable insertion button remains available.

Focused checks: `node --test tests/expression-completion.test.js tests/expression-completion-dom.test.js tests/parameter-reference-expressions.test.js`.
