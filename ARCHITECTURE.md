# Nect architecture baseline v0.1

Status: M0 engineering baseline. Notion owns product requirements/decisions; this file owns implementation boundaries. Future contracts below do not imply implemented capabilities. The accepted interaction extract is docs/product-direction.md; CURRENT_GOAL.md selects work.

## Owners

| Owner | Responsibility | Must not own |
|---|---|---|
| `nect_core` | Authored document, identity/hierarchy, property addressing, evaluation, atomic commands, undo/revision | UI state, file paths, MCP transport, proprietary codec internals |
| `nect_io` | Native JSON contract, SVG lowering, local argument/result conversion, persistence IO | Independent editing state or duplicate mutation rules |
| Desktop client (M1) | Qt selection, hit-testing, Inspector, gestures, viewport/workspace state | Canonical geometry/expressions stored in widgets |
| Protocol/host adapters | MCP, Resolve/OFX, external codecs | A second scripting/editing engine |

These are logical owners, not mandatory services. Preserve the existing libraries until a real dependency or ownership problem justifies a split.

## Canonical flow

`authored Document -> pure evaluation -> derived values/geometry -> renderer/export`

`GUI/API/MCP command -> Session candidate -> validate/evaluate -> atomic commit -> revision/history`

Native authored state is the editing authority. Compatibility IR, render trees and caches are derived. Original imported bytes are immutable provenance, not another editable document. A background result does not automatically become authored geometry.

## Relationships and coordinate ownership

Composition owns root paint order and its coordinate plane. Its artboards are output rectangles, not object parents. Object children provide containment and default inherited transforms; Folder is the UI name for a Group, not an extra data model. Collection is a non-owning set; scalar Binding is a directed value dependency.

Native0.9 separates Structure (ownership/order/effect scope) from explicit Transform Parent (following). The effective parent is the explicit reference when present, otherwise the structural parent, otherwise Composition identity. Shared evaluation validates same-composition references, cycles and depth; it never applies both parent transforms. Template assignment, Collection membership and property dependencies remain distinct relationships.

Validate ownership and dependencies at the appropriate property/evaluation stage. A mask source can depend on the target's world transform while the target's render depends on the source mask: this is not automatically a cycle at the whole-object level. A real evaluation cycle is invalid. Do not validate each relation in isolation while missing a cycle across domains, or reject all cross-domain references conservatively as object cycles.

Changing Collection membership does not reparent source objects but can change operators consuming the Collection. Separate Compositions do not implicitly share one coordinate plane; cross-composition operations require explicit context/conversion.

## Grouping, parenting and pivots

**GroupContiguous** replaces ordered contiguous siblings in paint order by an identity Group and initializes its Anchor once from geometric bounds. Transform-parent attach/detach can preserve world coordinates through an explicit command; general structural keep-world reparenting is not implemented.

Future reparenting must distinguish coordinate preservation from appearance preservation. Parent-matrix inversion addresses coordinates only and fails for singular transforms. Masks, backdrop blend, isolation, effects and order can still change appearance. Reject or show a conversion plan when preservation is not possible; do not silently flatten.

An authored Anchor is distinct from derived bounds center. GUI creation initializes it at the current center; it stays fixed through later content changes and supports explicit edits/recentering. The six affine Scalars remain the canonical transform, so editing Anchor alone preserves placement. Derived Position and one-shot rotation/scale commands solve that matrix about Anchor; they do not persist a second authoritative TRS state. See docs/model-v0.md for driven-field and singular-transform refusal boundaries.

## One live Session per open document

The desktop owns its document's live Session; API/MCP forward to that Session. Headless execution is a separate explicit lane. Different documents may have different Sessions; no global singleton is implied.

Live requests identify document/session and expected revision so an old request cannot edit a different document after open/restart. M0 JSON-lines is not a formal MCP adapter. GUI, API and MCP must share commands and failure semantics, not maintain synchronized document copies.

## Semantic automation

Expose real create/edit/link/reorder/save/reopen/undo/evaluate/render/export operations with stable IDs and observable results. Seeded scenarios are clients of those operations, not a second model inside a test harness. Report unsupported capabilities instead of silently substituting mouse automation.

Machine tests can cover structure, persistence, deterministic behavior and many regressions. Displayed-frame timing and interaction behavior require the corresponding viewport/GUI path; headless command timing alone is not GUI evidence. Human judgement is useful for comfort/artistic preference but need not perform every correctness check.

## Presentation and input drafts

- Document: authored objects, parameters/references and authoring definitions such as guides.
- Evaluation: replaceable resolved geometry/images and caches.
- View/session UI: selection, active editor, zoom, panels, overlays and input drafts.

Moving a panel or replacing a dial with fields must not require a document migration. Persist workspace preferences separately. Overlay outlines, hovered controls and temporary previews are not exportable artwork.

Property metadata belongs near the existing property owner. Inspector, API discovery and validation consume it rather than growing independent property tables. Shared basic controls and bespoke controls both use the same commands; no universal UI/plugin framework is required in advance.

The accepted field UI supports literals, references and expression text, expanding for multiline input. Parse into typed edit intent before mutation: absolute assignment, one-shot relative adjustment, binding or committed expression. Exact syntax is not yet frozen. A negative literal must remain distinguishable from subtraction. Relative batch edits use a start snapshot once; they must not become self-referential expressions.

Incomplete text/IME composition stays a draft. Type/unit/cycle/evaluation errors must not replace authored data with zero or silently remove a binding. A visible last-valid preview is not proof that the new expression is valid. Expression text is not a general shell or filesystem/network capability.

During source picking, target property IDs remain frozen independently of the viewed source. Search resolves readable names to stable references and exposes type/unit/space. Multi-target link/edit commits are atomic and undoable; incompatible targets are reported. Named/global Color links use the same dependency ownership; equal values do not create implicit links.

## Continuous gestures and background evaluation

A completed drag/scrub is one undoable edit; cancellation restores its starting state. Use a Session-owned edit context or command grouping, not unmanaged widget geometry. Define queue/reject/rebase behavior for API edits arriving during a gesture; never silently overwrite concurrent work.

Start synchronously while sufficient. Add background work when a measured operation needs it: snapshot session/revision/parameters, support cancellation and discard stale results. Evaluation publishes matching derived output. A solver intentionally changing authored points proposes a command for Session commit.

Lower-quality display must not silently resample a committed random layout or change final/export inputs. Mark provisional results and select a known revision/quality for export.

## Interactive Canvas budget

M1 target: at least 30 fps equivalent, p95 frame interval <=33.3 ms on a recorded reference machine and warm fixture for pan/zoom/point-handle drag/basic transforms. Lightweight scenes target 60 fps.

Record viewport/DPI, scene/revision, measurement boundary, p50/p95 and over-budget frames plus relevant command/evaluation times. Average FPS does not hide stalls. Instrumentation must not dominate the hot path. Add the smallest measured optimization rather than prebuilding a broad worker/cache framework.

## Geometry sources and authored direct corrections

A parametric primitive owns generator parameters and evaluates to path geometry. For arbitrary point/handle edits the selected default is a **visible downstream Point Edit / Path Deform**, automatically added or reused while retaining the generator. Radius controls still edit Radius; arbitrary path controls edit explicit corrections. Disabling the correction restores the generated result.

Represent source plus correction, not competing full authoritative anchor lists. Define source-element identity, correction space/units and version behavior for the implemented generator. Stable-topology radius edits need not require topology inference. A Star count change can invalidate mappings; report unresolved edits rather than applying them to different indices.

Convert to Path remains optional and explicit. Inspect generator-only references before conversion; preserve, remap or freeze according to a visible conversion plan. Keeping the object ID alone is insufficient.

Shape-local processing is ordered, with multiple paint operations. For AE counterparts preserve AE group/path/paint/repeat behavior; do not mistake the displayed stack for a generic sequential pixel-filter pipeline. Invalid domain/order combinations fail explicitly.

Minimum operator definition: stable type ID, persisted behavior version, accepted input/output domain, ordinary property parameters and evaluation without document mutation. Instances have stable identity and a declared enabled/bypass contract. Canvas handles are presentation, not separate evaluation. Introduce stateful/time-dependent contracts when actual operators require them.

Stacks and graphs share definitions only where equivalent; do not maintain two mutable copies. Only representable linear subgraphs need a stack view. PointSet/Field domains enter with actual callers, not empty scaffolding. Generated identity and source lineage differ from index; seed alone does not solve topology or cross-version reproducibility.

## Masks and Group processing

Normal visibility is independent of evaluation as a mask source. A hidden source may feed masks; its faint selected-target outline is a UI overlay. The context-menu Mask With Top/Bottom/Put Inside actions resolve explicit sources/targets rather than making adjacency the internal authority.

Define mask coordinate space, input stage and alpha/luma/geometry mode. Transform parenting handles following, not mask evaluation or effect scope.

Evaluate each child's own appearance/effects before processing the required composed Group result. Neutral Groups avoid unnecessary isolation. Group opacity/effects and Pass Through/Isolated blending need targeted fixtures; they cannot be inferred solely from hierarchy labels.

The native0.11 subset resolves visibility, ordinary Scalar opacity, geometry
masks and twelve separable blends through one transient `evaluate_scene` tree.
One evaluated shape per leaf is shared by artwork and mask consumers. Structural
scope and world-space mask geometry are explicit; Qt/SVG consume this projection.
Compositions start with transparent artwork, independent of viewport paper UI.
Nonneutral scopes aggregate before clip/opacity/blend; neutral Groups pass through.
Renderer surface bounds and SVG reader requirements are visible capability limits,
not permission to silently flatten or omit native effects. See `docs/model-v0.md`.

## Artboards, templates and reusable sources

Artboards are output frames on their owning Composition's plane, with stable identity and output metadata. Objects may cross frames or live outside them. Ordered navigator layout, export order and actual frame coordinates are independent. Auto-tidy presentation must not change crops or translate artwork. Physical frame/content relocation is an explicit command with its impact shown.

Templates may provide optional frame/layout settings and repeated content with per-attribute inheritance/override. Reset Override differs from Detach; deleting a referenced template element requires a conflict policy. Reuse the Composition/Group/graph definition-and-instance contract rather than creating another Symbol engine. Graph editing is optional for simple placement/reuse.

Linked and Embedded resources are explicit. Show link change/missing/version state; reload uses the normal document command boundary where it changes the result. Embedded bytes are self-contained; saving a linked reference is not a backup of all external historical bytes.

## Property expression boundary

Scalar has one driven source: Binding or versioned Expression, never both.
Expression compilation is pure and transient; stable property references re-enter
the existing property dependency visitor for units, cycles and generated topology.
Requested-root transform evaluation uses that same path. The bounded v1 arithmetic
language has no script runtime or external authority. Numeric-row multiline drafts
and their validation result are UI state; only explicit Session commands modify
and persist source. A stale draft revision rejects rather than overwriting edits
from the API. SVG is an evaluated projection; native source remains authored.

## Interchange and source preservation

The optional desktop SVG reader performs memory-only interchange lowering into
existing Session commands (`src/desktop/svg_import.cpp`, logical IO responsibility).
It uses already-linked Qt Core XML and owns no editing state; core-only `nect_io`
and CLI do not acquire a Qt dependency. Host supplies bounded local bytes and
applies the complete serializable batch once for GUI/API/MCP. See `docs/svg-import.md`.

Native persistence preserves authored intent; interchange is a projection. Plan direct mapping -> geometry expansion -> bounded appearance bake -> explicit refusal. A safe bake boundary includes necessary inputs, masks, backdrop, filter support, resolution and color context; it is not guaranteed to be one layer/subtree. Preserve unrelated editability where possible and report the actual loss.

Retain imported source bytes only as provenance/recovery. Do not splice unknown private chunks into a modified file without a verified structural contract. Unknown-extension preservation must not execute unknown code.

## Persistence evolution, history and recovery

Preserve identity, units/spaces, atomic mutation and user work, not every M0 class or JSON key. When persisted meaning changes, specify the supported version transition, retain an old fixture and test load -> migrate -> edit -> save -> reopen with IDs/references preserved. Keep originals and report unsupported data. Add focused migrations when needed, not a speculative migration platform.

Non-destructive structure, operation History and recovery backups serve different purposes. The accepted direction includes a visible long retained History, within an explicit memory/disk budget. Do not interpret that as unbounded full snapshots, permanent cross-restart undo or mandatory branching history. Restore/history navigation must leave one coherent Session state.

Continuous-save work protects committed revisions separately from in-progress drafts/gestures. Distinguish pending versus durably saved versus failed revisions. Do not destroy the previous valid save before replacement is safe. Protect unnamed work through recovery storage; keep independent backup generations because autosave also records mistakes. Manual Save/Save As remains available and native saving remains separate from export.

Choose flush/cadence/retention/recovery-loss bounds with the persistence implementation and test interrupted writes, disk/permission failure, backup restore and revision readback. A successful method return is not durable-storage evidence. Keep IO off the latency-sensitive interaction path where needed. No design promises survival of all storage/hardware failures, and linked source versions outside the document are not automatically protected.

## Change impact guide

| Change | Expected boundary | Continuity check |
|---|---|---|
| Panel/control layout, density or labels | Desktop/view | Same document, references and undo state |
| Operator on an existing domain | Operator/evaluator + metadata + handles | Old files unchanged; declared bypass behavior |
| Text, PointSet, instances, masks or fields | Model/evaluation/IO and its view | Explicit space/identity/version; focused migration |
| Renderer or solver replacement | Backend/evaluation, possibly numeric semantics | Known revision/quality; representative comparisons |
| Host/codec/plugin runtime | Adapter plus actual capability gaps | No second authority; explicit unsupported result |

Use the smallest correct boundary, not the fewest lines at any cost. If an invariant blocks a real requirement, propose its bounded replacement and migration rather than preserving a known defect or rewriting unrelated subsystems.
