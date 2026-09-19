# Nect architecture baseline v0.1

Status: M0 engineering baseline. Product requirements and decisions live in Notion; this file owns implementation boundaries.

## Owners

| Owner | Responsibility | Must not own |
|---|---|---|
| `nect_core` | Authored document, stable identity/hierarchy, property addressing, evaluation, atomic commands, undo/revision | UI state, file paths, MCP transport, proprietary format internals |
| `nect_io` | Native JSON contract, SVG lowering, local argument/result conversion | Independent document state or duplicate mutation rules |
| Desktop client (M1) | Qt selection, hit-testing, Inspector, gestures, view/workspace state | Canonical geometry or expressions stored in Qt widgets |
| Protocol/host adapters | MCP, Resolve/OFX, external codec adapters | A second scripting/editing engine |

These are logical owners, not mandatory services.

## Canonical flow

`authored Document -> pure evaluation -> transient resolved values -> renderer/export projection`

`GUI/API/MCP command -> Session candidate copy -> validate/evaluate -> atomic commit -> revision/history`

Native authored state is the single editing authority. Compatibility IR and render trees are derived.

## Relationships

- Composition owns root paint order and a coordinate plane.
- Object children define one ownership/transform tree.
- Folder is currently modeled as a hierarchical Group.
- Artboard is an output rectangle, not a second object parent.
- Collection is a non-owning named set.
- Binding is a separate directed property dependency graph.

Ownership, membership, and dependency are independent.

## Reparent/grouping boundary

M0 only implements **neutral GroupContiguous**: ordered contiguous siblings are replaced at the same paint-order position by an identity group. General keep-world reparenting is not implemented.

A future preserve-appearance reparent must explicitly account for transform, masks, blend backdrop, isolation, inherited effects, and z-order. If preservation cannot be guaranteed, reject or present a conversion plan; do not silently flatten.

## One live session

M1 desktop owns the live Session. A future MCP adapter forwards commands to that same Session. Do not run a second hidden document copy and call it integration.

Headless execution is an explicitly separate lane.

## Extension / analysis direction

Introduce typed operator inputs/outputs only with actual callers. Image, Mask, RegionSet, PathSet, ObjectCollection, etc. are distinct. OpenFX, if adopted, operates at an image boundary and is not the canonical vector/text model.

Analysis outputs are observations/candidates with source revision and coordinate metadata; they are not semantic truth by themselves.

## Interchange

Native persistence preserves authored structure. Interchange output is derived.

Future export planning chooses:
1. direct mapping
2. geometry expansion
3. bounded appearance bake
4. explicit refusal

Bake boundaries are dependency closures, not automatically one layer.

Retained source bytes are provenance/recovery only; never splice unknown private chunks into a modified file without a verified contract.
