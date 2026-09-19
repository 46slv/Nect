# Nect architecture baseline v0.1

Status: M0 engineering baseline. Product requirements and decisions live in Notion; this file owns implementation boundaries. The iteration contracts below guide future changes; they are not claims that those changes already exist.

## Owners

| Owner | Responsibility | Must not own |
|---|---|---|
| `nect_core` | Authored document, stable identity/hierarchy, property addressing, evaluation, atomic commands, undo/revision | UI state, file paths, MCP transport, proprietary format internals |
| `nect_io` | Native JSON contract, SVG lowering, local argument/result conversion | Independent document state or duplicate mutation rules |
| Desktop client (M1) | Qt selection, hit-testing, Inspector, gestures, view/workspace state | Canonical geometry or expressions stored in Qt widgets |
| Protocol/host adapters | MCP, Resolve/OFX, external codec adapters | A second scripting/editing engine |

These are logical owners, not mandatory services. Keep the present libraries until a real dependency or ownership problem justifies a split.

## Canonical flow

`authored Document -> pure evaluation -> transient resolved values -> renderer/export projection`

`GUI/API/MCP command -> Session candidate copy -> validate/evaluate -> atomic commit -> revision/history`

Native authored state is the single editing authority. Compatibility IR and render trees are derived. Original imported bytes are immutable provenance, not another editable document.

## Relationships

Keep these relations orthogonal:

- Composition / Structure hierarchy owns root paint order, containment and effect/compositing scope.
- Folder is currently modeled as a hierarchical Group; the UI label is not a new data type.
- Transform Parent is a separate stable-ID dependency for AE-like transform following; it does not reparent the item in Structure or define effect scope.
- Artboard is an output frame in shared document coordinates, not an object parent.
- A future Parent Artboard / Artboard Template is a reusable template relation, not object ownership.
- Collection is a non-owning named set.
- Binding is a separate directed property dependency graph.

Structure, Transform Parent, Artboard template assignment, Collection membership and property dependency must reject their own cycles/invalid references without being conflated. Changing a Collection must not reparent its source objects, but can change the result of operators that consume the Collection.

## Reparent/grouping boundary

M0 only implements **neutral GroupContiguous**: ordered contiguous siblings are replaced at the same paint-order position by an identity group. General keep-world reparenting is not implemented.

A future preserve-appearance reparent must explicitly account for transform, masks, blend backdrop, isolation, inherited effects, and z-order. Inverting a parent transform only addresses coordinates and may fail for a singular matrix. If appearance preservation cannot be guaranteed, reject or present a conversion plan; do not silently flatten.

## One live session per open document

M1 desktop owns the live Session for its document. A future MCP adapter forwards commands to that same Session. Do not run a second hidden document copy and call it integration. Different documents may have different Sessions; this is not a global singleton requirement.

Headless execution is an explicitly separate lane. A live protocol target must identify the document/session as well as the revision so that a stale request cannot edit another document after open/restart. This targeting contract is to be added with the live adapter; M0 JSON-lines is not that adapter.

### Automation / self-testing boundary

Semantic document operations must be usable without GUI event replay. The command/API/MCP surfaces should converge on the same create/edit/link/reorder/save/reopen/undo/evaluate/render/export semantics and stable IDs. A machine client may build temporary seeded scenes, execute many commands, and read back revisions/changed IDs/errors/evaluated state.

This is not permission to duplicate the model in a test harness. Stress/scenario generators are clients of the real command boundary. A capability unavailable through automation is reported as unavailable instead of being silently replaced by mouse automation.

Human-free automation can prove many structural, persistence, deterministic and performance properties. It does not by itself prove discoverability, ergonomic comfort or visual judgement.

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

Bake boundaries are dependency/render-context closures, not automatically one layer. Include relevant masks, backdrop and filter support; keep unrelated editable content when the target allows it. Some effects may require a large closure. Report that rather than promising a universally local bake.

Retained source bytes are provenance/recovery only; never splice unknown private chunks into a modified file without a verified contract.

## Evolve by contract, not by freezing M0

Preserve stable identity, authored intent, explicit spaces/units, atomic mutation, save integrity and observable capability limits. These are not promises to freeze C++ layouts, JSON keys, transport method names or a plugin ABI forever.

M0's fixed Group/Path enum, scalar-only binding subset and snapshot history are useful baselines, not a completed operator architecture. A real new domain can require model/codec/evaluator work. Do not hide that work behind a generic JSON blob, nor rewrite the entire application preemptively.

### Presentation versus semantics

- Document: authored objects, values, references and authoring definitions such as guides.
- Evaluated output: resolved geometry/images and caches, replaceable from authored input.
- UI/session view: selection IDs, active editor, zoom, panel layout, visibility and temporary interaction state.

Persist workspace state separately from document semantics. Export must not serialize panel widths, hovered controls or temporary previews. Changing a dial to XY fields or moving an Inspector must not require a native-document migration.

As more property types are implemented, keep ID/type/unit/editability metadata near the existing property owner. Inspector, API and command validation should consume that owner rather than grow independent property tables. Shared basic controls may use metadata; bespoke controls are allowed through the same commands. Do not build a universal UI/plugin framework before actual controls need it.

### Continuous gestures and background results

A committed drag/scrub is one undoable edit; cancellation restores the pre-gesture state. Preview changes must be associated with a Session-owned edit context or equivalent command grouping, not an untracked widget-owned document. Define the conflict behavior when API/MCP edits arrive during a gesture: queue, reject, or explicitly rebase; never silently overwrite.

Start synchronously while sufficient. When a measured operation needs background work, evaluate a snapshot tagged with session/revision/parameters, discard stale results and support cancellation. Background evaluation publishes matching derived output; it does not write caches back as authored geometry. A solver that intentionally edits authored points must propose a command committed through Session. Lower display quality must not silently resample or replace a committed random layout during export.

### Interactive Canvas performance budget

Pan, zoom, selection, point/handle dragging and basic transform are latency-sensitive foreground work. The M1 minimum contract is 30 fps equivalent on a recorded reference machine + scene after warm-up, measured with p95 frame interval <=33.3 ms; lightweight scenes target 60 fps.

Record p50/p95 frame time, over-budget frames and relevant command/evaluation timing in a machine-readable benchmark lane. Build the scene through the same semantic API where practical so Astra/CI can reproduce a regression.

Do not equate average fps with responsiveness. Avoid long synchronous stalls on the interaction path. Heavy effects/operators may use an explicitly labeled interactive preview, but input remains responsive and final/export output is tied to a known revision and deterministic parameters.

Do not add broad cache/worker/dirty-region architecture only because the target exists. Profile the real path, then add the smallest measured optimization. Performance instrumentation must not materially become the hot-path cost it measures.

### Parametric geometry and operator evolution

A parametric primitive such as Circle owns its generator parameters and evaluates to normal path geometry. Do not make both generator parameters and arbitrary evaluated anchors independent authored authorities. Downstream path operations consume the evaluated geometry.

An explicit `Convert to Path` changes the geometry source from generator-owned parameters to explicit anchors. Before conversion, inspect references to generator-only properties and report/remap/freeze according to the selected conversion contract; never silently leave dangling references.

Keep source geometry and operator parameters authored; evaluated copies are derived. Shape-local processing is ordered. For AE-compatible shape operators, preserve AE-equivalent order/scope semantics where the types correspond. Invalid order/type combinations fail explicitly.

The minimum operator definition is stable type ID + persisted semantic version + typed input/output domain + ordinary property parameters + side-effect-free evaluation. An operator instance has stable identity and enabled/bypass state. Canvas manipulators are optional presentation metadata, not a second evaluator.

A local stack and graph may share operator definitions/evaluation without sharing every UI or socket type. Do not keep separate mutable copies of the same processing definition. Only graph subsets with equivalent order/scope may have a linear-stack view. Add domains such as PointSet/Field only when an actual operator requires them.

When generated elements become editable, define their identity domain and source/operator lineage. An index is not an identity. Rebuilds that invalidate an override must report it or offer an explicit editable snapshot. A stable seed alone does not solve identity, topology changes or cross-version reproducibility.

### Transform pivot and Group compositing

Transformable objects/groups have an authored Anchor Point and a separately derived current bounds/geometry center. Creation initializes Anchor from center; later bounds changes do not silently move Anchor. An anchor-only edit can compensate Position to preserve world placement where the transform is representable.

Transform Parent cycles are invalid. Changing Transform Parent should preserve world placement by default when the required transform conversion is invertible; otherwise reject or require an explicit non-preserving choice.

Normal render visibility does not determine whether an object can be evaluated as a mask source. A hidden source can still feed a mask relation.

A neutral Group should not introduce an unnecessary compositing boundary. When Group-level effect/mask/opacity or other group processing requires one, evaluate each child through its own local appearance/effects, composite the required child result, then apply Group-level processing. Exact Pass Through/Isolated blend semantics require fixtures and are not inferred from Transform Parent.

### Artboard model

Multiple Artboards share the document coordinate space. Each has stable identity/order and frame/output metadata; objects are not owned by Artboards and may cross frames or live outside them.

A future Parent Artboard / Artboard Template may provide repeated frame/layout settings and derived template content to assigned Artboards with local overrides/detach. This relation stays separate from Structure ownership. Moving/resizing an Artboard frame alone does not move artwork unless an explicit content-moving command is invoked.

### Persistence evolution

Before changing persisted meaning, specify the supported version transition and default behavior, keep an old saved fixture, and test load -> migrate -> edit -> save -> reopen with retained IDs/references. Use explicit schema/operator versions where semantics change. Preserve originals during migration and report unsupported data rather than coercing it to empty content. Unknown-extension preservation must not execute unknown code.

Do not implement a hypothetical migration platform now. Add a focused migration when the first real schema change arrives. Unsupported future versions may remain rejected as in M0. Export always remains distinct from native save.

## Change impact guide

| Change | Expected boundary | Continuity check |
|---|---|---|
| Move panels, change control appearance, density or labels | Desktop/view | Same document, references and undo state |
| Add an operator over an existing value domain | Operator/evaluator + metadata + optional handles | Old files unchanged; operator bypass restores source |
| Introduce Text, PointSet, instances, masks or a field domain | Model/evaluation/IO plus its view | Explicit spaces/identity/version; focused migration and fixture |
| Replace a renderer or solver | Evaluation/backend boundary, possibly numeric semantics | Known revisions and quality; compare target fixtures |
| Add a host/codec/third-party plugin runtime | Adapter and any real capability gaps | No second authoring authority; explicit unsupported results |

Use the smallest correct boundary, not the smallest possible line count. If an invariant itself fails a real use case, propose its bounded replacement and migration; architecture documentation is not a reason to preserve a known defect.
