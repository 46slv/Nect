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
