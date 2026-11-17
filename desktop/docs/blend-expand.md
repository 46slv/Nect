# Blend: explicit expansion

REQ-138 / #302 requires intentional conversion to independent editable geometry,
not a geometry-only live Convert to Path operation that drops appearance.

`expand-blend` is a CommandSession command. It evaluates the selected active
Blend, preserves source A/B graphs, replaces that Blend's identity with an
ordered painted Join, and adds editable Multi Path plus solid Fill/Stroke nodes
for each evaluated copy. Ordinary edits remain live; only explicit expansion
breaks the Blend dependency. One Undo restores the exact authored Blend graph.
The existing schema and nodes are reused; no baked proprietary geometry format.

Expansion rejects locked/bypassed nodes, parameter references which would lose
their meaning, blocked export appearance, and unsupported or excessive output.
It validates a private document candidate before committing. Initial acceptance
must cover appearance, source independence, point editing, Undo/Redo and reopen.

## Creating a live Blend from selection

`blend-selected` takes exactly two stable node IDs in selection order (A then B),
validates their live Blend output, and adds one connected Blend in one History
entry. The graph toolbar exposes this action for two selected nodes. Existing
source nodes and Output wiring are preserved, just like ordinary Add node; the
new Blend is selected for preview, and the user can connect it to Output. Invalid
paint/topology or missing sources reject the entire command without a partial node.
