# Product direction — implementation extract

Reviewed: 2026-09-20, including the 25-answer interaction interview. This is a scoped extract for agents without Notion access, not a second requirements database. Notion retains product discussion and evidence. CURRENT_GOAL.md selects implementation work; acceptance of a direction here does not add the whole roadmap to M1.

## Product and evidence boundaries

Make editable, non-destructive **2D graphics** quick to create and revise. Direct editing, AE-like shape operations and compositing/node capabilities support that workflow rather than becoming separate applications.

Illustrator/Photoshop interoperability, API/MCP and optional Resolve integration remain goals. They do not require a complete Photoshop, animation editor, Geometry Nodes implementation, print RIP or marketplace before the editor is useful.

- Required behavior includes addressable point/handle controls, source-preserving edits, cross-property links, useful text, masks/effects, standalone authoring and API/MCP.
- The standard still-graphics workspace has **no timeline, playhead, time ruler, clip lanes, video transport or timecode** (Notion REQ-178).
- Canvas center / structure left / contextual Inspector right / optional deep editors is a trial layout, not a persistence contract.
- M0 implements Group/Path objects, scalar properties, limited bindings, native JSON and an SVG subset. General operators, fields, instances, full expressions and the UI described below are not implemented by this document.

AE is a behavioral reference for shape operations, properties, effects, blend modes and equivalent shortcuts, not a requirement to copy its timeline workspace. The user explicitly favors expert power over beginner simplification and may be the only user. That does not waive safety, data integrity, responsiveness or repository/distribution permissions.

## Instantiate, then parameterize

**Accepted, interview 1–2, 16–17:** Circle, Rectangle, Polygon and Star use Add -> valid primitive -> parameter editing. Drag-creation may be an optional accelerator, not the default or required creation path. Initial placement/size remain tunable UI details.

Text similarly starts with Add Text. Prefer one editable Text model with switchable layout modes (such as auto-sized versus constrained frame), preserving content, styles and references. Do not require drawing a text box first. For gradients, parameter editing is primary and Canvas handles provide supplementary adjustment.

## Parametric source plus direct corrections

**Accepted, interview 1:** arbitrary point/handle edits on a generated shape automatically add or reuse a visible downstream Point Edit / Path Deform operation. Retain the generator and its parameters:

```text
Circle(center, radius)
    -> generated path
    -> authored point/handle corrections
    -> ordered shape/appearance processing
```

Radius remains editable and disabling the correction restores the generator result. A direct gesture that deliberately edits the radius/center control still changes that source parameter. Do not silently convert the primitive or ask for destructive conversion on every ordinary point edit.

The correction is explicit authored data, not a second authoritative copy of all evaluated anchors. Its IDs, coordinate space and delta/override semantics must be defined when implemented. A radius change with stable topology is not the same problem as changing a Star's point count. Report unmapped corrections rather than reassigning them by list index or promising arbitrary topology-independent editing.

Convert to Path remains an explicit optional operation. It must inspect references to generator-only properties and report/remap/freeze according to the chosen conversion contract; retaining object identity alone does not preserve Radius references.

## Ordered Shape stack and expert power

**Accepted, interview 3–4:** multiple Fill/Stroke entries and movable path/paint/repeat operations. Ordering and group scope are authored parts of the result. Do not impose a fixed Geometry -> Repeat -> Paint sequence merely to simplify the UI.

Use AE behavior for corresponding operations, including Repeater's scope over preceding paths/strokes/fills. AE path operations and paint operations have different evaluation-order rules; a linear list is not necessarily a naive sequence of image filters. Repeater before versus after paint can change compound-path versus individually painted copies. Type-incompatible operations and unsupported behavior must be explicit, not silently aliased.

Primary reference: https://helpx.adobe.com/after-effects/desktop/drawing-painting-and-paths/shapes-and-shape-attributes/shape-attributes-paint-operations-path.html

## One property row for values, links and expressions

**Accepted, interview 5–8:** numeric fields accept expression text, and grow into an inline multiline editor when two or more lines are entered/pasted. Keep the compact numeric/scrub presentation for ordinary values, and distinguish expression source from its evaluated result. Expansion limits, confirmation keys and collapse behavior are prototype details.

A multi-selection absolute edit sets each selected compatible property to that value; it does not move the selection bounds center. A relative edit preserves per-target differences. Example: 100/200/300 -> set 400 -> 400/400/400; relative +10 -> 110/210/310.

**Syntax proposal, not a finalized language:** allow +10 as a relative-edit shorthand and explicit +=10 / -=10 forms; keep a negative literal such as -20 distinguishable from subtraction. A one-shot relative command reads the initial values once and commits atomically; it is not a persistent self-reference. An explicit expression mode/prefix can disambiguate literal arithmetic from a live formula.

Keep draft text separate from committed authored state. Incomplete expressions, IME composition, type/unit failures and evaluation errors must not replace valid data with zero. Preserve/show the last valid preview as such. Do not silently unlink a driven property when typing a value; provide an explicit replacement or offset-edit action. Accepting text does not grant filesystem/network/process authority to the expression evaluator.

## Source selection without losing targets

**Accepted, interview 6–7:** Pick Source / pick-whip freezes the target property set while the user inspects another object/group/effect. Choosing a source returns to those targets; cancel restores the prior editing context.

Provide Copy Value / Copy Reference and Paste Value / Paste Link. Absolute Link and Relative Link are separate: the latter preserves each target's current difference as an explicit offset. Apply compatible targets in one undoable transaction; report incompatible units/types/spaces instead of guessing conversions.

A searchable property palette shows object path, property name, type/unit and relevant value. Human-readable names are discovery aids; confirmed references resolve to stable IDs. Property search is not only an object-name search. Mixed values remain visibly mixed, not an invented average.

## Groups, parenting, anchors and masks

**Accepted, interview 9–12:** a Group behaves as one selectable/transformable object by default, with deliberate drill-in and a visible breadcrumb/return path. Creation initializes its Anchor to the current center; later child changes do not recenter it. Anchor remains editable through an Anchor Point tool and Inspector, with an explicit Center Anchor command. Derived bounds center and authored pivot are different properties.

Keep Structure hierarchy (ownership/order/effect scope), Transform Parent (following), Collection membership and property dependencies distinct. Mask follow uses Transform Parent; no mandatory special mask-only following engine. Before implementing external parenting, define how it replaces/composes with structural transforms so transforms are not applied twice.

Mask With Top / Mask With Bottom / Put Inside belong in the context menu. Identify the source/targets from paint order rather than an unexplained last-click rule and show the intended operation. Keep access through the same semantic commands.

Normal render visibility and use-as-mask-source are independent. With the target selected, show the hidden mask's faint editable outline when requested. It is a viewport overlay, not exported artwork, and must not turn ordinary source visibility on. Editing it targets the real mask source through the normal commands.

For Group-level effects, evaluate child geometry/appearance/effects first, composite the necessary child result, then apply Group processing. A neutral Group should avoid unnecessary isolation. Pass Through/Isolated blend semantics still require fixtures; Transform Parent does not define effect scope. Coordinate-preserving reparenting does not guarantee arbitrary mask/blend/effect appearance preservation.

## Ordered Artboards and editable templates

Artboard remains an output frame, not an object parent. It belongs to a Composition's coordinate plane; do not conflate separate Compositions into one implicit global plane. Objects may cross frames or sit outside them. Frame move/resize alone must not silently move artwork.

**Accepted delegation, interview 15:** prefer an ordered navigator and tidy automatic presentation/placement over free positioning as the main workflow. A grid/list with deliberate page/export order is the initial proposal. Navigator rearrangement, export order and actual frame coordinates are different. Relocating real frames can change crops; show that effect and make content-moving operations explicit. Do not silently reorganize artwork merely to tidy thumbnails.

**Accepted scope, interview 13–14:** Artboard Templates may provide size/orientation, margins, grid, guides, background, logo, header/footer, page numbers and placeholders. These are optional composable settings/content entries, not a monolithic template engine or a requirement to implement them all in M1.

Overrides are per attribute: moving a template logo locally preserves its placement while changes to the source geometry and other inherited attributes propagate. Distinguish Reset Override from Detach. Source item deletion/identity changes need an explicit conflict policy, not silent retargeting. Reuse the shared definition/reference machinery where appropriate.

References:
- https://helpx.adobe.com/indesign/desktop/create-and-organize-pages/create-and-manage-parent-pages/about-parent-pages.html
- https://helpx.adobe.com/indesign/desktop/create-and-organize-pages/create-and-manage-parent-pages/manage-parent-items.html

## Color values, inventories and shared references

Known colors should be directly editable/copyable as values. Sampling/Eyedropper remains useful for acquiring an unknown visual color, not as the only transfer method.

**Accepted, interview 18–19:** distinguish Document Used Colors (an inventory of actual document usage) from Copied Color History (explicit copy events, opened through a compact history button). User-pinned and document-authored named colors are separate scopes. Do not record every hover/scrub intermediate or monitor unrelated clipboard contents.

Global/Named Color is a normal typed Color property with stable references. Changing it updates only the Fill/Stroke/gradient stops or other properties that explicitly reference it. Equal HEX values do not imply shared identity. Copy Value creates an independent value; Copy Reference/Paste Link preserves linkage.

Keep color-space/profile/alpha information. A structured Nect clipboard payload may accompany text/plain; HEX is not a universal representation of CMYK/Lab/Spot or all profiled colors. Do not silently discard richer color data. Complement/analogous/triadic/tint/shade remain candidate derived operations pending color-space/gamut decisions; use the ordinary dependency model, not a second color-only engine.

## Reuse, resources, history and recovery

**Interview 22:** favor Composition/Group/graph-source reuse over adding an unrelated Symbol system. One definition may be referenced by many placements, with explicit instance differences. The graph is an authoring surface, not something the user must wire merely to place another logo. Referenced authoring definitions and context-dependent rendered results are not automatically the same cache.

**Accepted, interview 23:** distinguish Linked and Embedded assets. Linked provides change detection, Reload and Relink; Embedded retains self-contained asset bytes. No default mode was selected by this answer. Show missing/changed/version state; a normal native save does not preserve every historical version of an external linked file.

**Accepted, interview 21:** History lists meaningful operations and can return to any retained state. Prefer a useful long history within measured memory/disk budgets; do not store unbounded full-document snapshots. Non-destructive structure preserves editable sources/settings, History restores earlier decisions/deletions/links, and backups recover durable prior states. They are complementary. Cross-restart history and branching are not implied requirements.

**Accepted goal, interview 24:** dependable DaVinci-like saving, interpreted as continuous protection of committed edits, visible pending/saved/failed status, versioned backups and recovery after abnormal exit. Live save can also save mistakes, so it cannot replace History or backups. Manual Save/Save As and recovery access remain discoverable.

Only a known durable revision may be labelled saved. Keep incomplete input drafts separate; protect unnamed documents with a recovery location. Do not destroy the last good file before its replacement is safe. Test interrupted writes, disk/permission failures and actual restore, rather than judging reliability from a successful Save call. Save/recovery work must not block interactive gestures. Choose cadence, retention and bounded recovery-loss behavior when implementing persistence; do not promise perfect recovery from every storage or hardware failure.

Inspiration, not a reliability guarantee: https://www.blackmagicdesign.com/products/davinciresolve/collaboration

## Automation-first testability

GUI, API and MCP are clients of the same edit model. The machine surface is a product capability, not a test-only backdoor. Astra/CI should create/open temporary documents, add/delete/reorder/group objects, edit/link/unlink properties, save/reopen, undo/redo, evaluate and render/export through real semantic commands and stable IDs.

Seeded scenarios should verify revisions, state and errors without requiring a person to perform every action. Do not create a second automation document model. Unsupported capabilities are explicit; mouse simulation is not a silent replacement for a missing semantic API.

Headless checks do not establish every aspect of UI quality or displayed frame timing. Use real GUI/viewport evidence where that is the question; human review can focus on comfort and artistic preference rather than routine correctness checks.

## Compositing baseline

After Effects' blend-mode vocabulary and semantics are the compatibility target where applicable:
- Normal: Normal, Dissolve, Dancing Dissolve
- Darken: Darken, Multiply, Color Burn, Classic Color Burn, Linear Burn, Darker Color
- Lighten: Add, Lighten, Screen, Color Dodge, Classic Color Dodge, Linear Dodge, Lighter Color
- Contrast: Overlay, Soft Light, Hard Light, Linear Light, Vivid Light, Pin Light, Hard Mix
- Difference: Difference, Classic Difference, Exclusion, Subtract, Divide
- HSL: Hue, Saturation, Color, Luminosity
- Matte: Stencil Alpha/Luma, Silhouette Alpha/Luma
- Utility: Alpha Add, Luminescent Premul

Source: https://helpx.adobe.com/jp/after-effects/desktop/work-with-layers/work-with-layer-blending-modes/blending-modes-layer-styles.html

This is a product target, not full M1 coverage. Time context, alpha representation, working color space and bit depth need fixtures. Never silently alias unsupported modes.

## Artboard interaction performance

Minimum practical interaction target: **30 fps** on recorded reference hardware and a representative warm scene, with p95 frame interval <=33.3 ms during pan/zoom/point-handle drag/basic transforms. Lightweight scenes target 60 fps. Record viewport size/DPI, scene/revision and the measurement boundary; average FPS or headless evaluation speed alone does not prove responsiveness.

Heavy operations may use visibly provisional preview quality, but final/export output is tied to known inputs/revision and must not silently change seed or layout. Profile the real hot path before adding broad caching/workers. Instrumentation should not itself dominate that path.

## UI priorities across experiments

**Accepted, interview 20, 25:** Snap has ON/OFF and a gentle default attraction. Tune its screen-space tolerance through use; do not snap an exact API numeric edit unless explicitly requested.

Reduce tool switching and repetitive hierarchy expansion without hiding structure. Test contextual controls, clear grouping, property search, pinned controls and breadcrumbs. Simple operations must not require building a graph. Preserve discoverability through GUI commands; shortcuts remain accelerators.

Use neutral low-noise surfaces, restrained accents and subtle proximity feedback without moving hit targets. Panel width, order, density, icons, control widgets and motion remain replaceable UI hypotheses. Numeric precision and expressive controls can coexist. Schema-generated basic rows are a fallback, not a ban on bespoke controls.

Keep grid/margin/guide/snap utilities near Canvas. Separate visibility toggles, direct-edit modes and detailed setup. Changing a grid definition does not automatically reflow existing artwork; a deliberate alignment/reflow command can do that.

## Three surfaces, one processing definition where semantics match

Direct Canvas/Inspector editing, shape-local stacks and node graphs are different views/workflows, not independently mutable processing engines. Share operator definitions/evaluation when equivalent. Only representable linear subgraphs need a stack view; keep other graphs opaque as a Macro or open the graph rather than discard connections.

A Preset configures values, a Macro packages a graph with published parameters, and an Action records commands. Do not turn these into three unrelated scripting runtimes.

The minimal operator contract is stable type/version, accepted input/output domain, normal property parameters, evaluation without document mutation, stable instance identity and declared bypass behavior. Optional Canvas handles are presentation. Add PointSet/Field or stateful/time-dependent contracts when real operators require them, not as empty scaffolding now.

## What to prove next

| Work | Timing / boundary |
|---|---|
| Real path creation/selection, point/handle editing, gesture Undo/Cancel, save/reopen, SVG | M1 manual loop first; use it before UI polish |
| Same semantic operations via machine commands, seeded readback | Build beside the manual loop |
| Whip/name links and formal MCP to the same live Session | Remaining M1 acceptance |
| Actual Canvas timing on a recorded reference fixture/machine | M1 performance evidence |
| One local repeater, fill/style and a radial design | Explicitly selected next procedural slice, not a full node editor |
| Simple mask and overlap examples | Focused compositing/UX slice, not full PSD |
| Point distribution and shape instances | Later slice with generated identity/domain contracts |
| Packing, membrane, full fields, per-instance overrides | Separate experiments, not cheap UI-only additions |
| Complete blend coverage, AI/PSD, OpenFX, production text/color/print | Dedicated capability work; early small risk probes allowed |
| RAW, generative providers, 3D assistance, PDF/VT, imposition, marketplace | Backlog unless explicitly selected |

Approved interview answers refine behavior; they do not silently add every row to CURRENT_GOAL.

## Reference tasks and cautions

**Radial ornament:** begin with an authored motif and repetition. A dedicated Spark generator is optional. Changing count needs an explicit fit-to-arc versus fixed-step rule; a periodic style sequence may not close at every count.

**Bubble/membrane:** the image specifies a visual target, not its manufacturing method. Rings, connectors, masks/fields and packing are alternative recipes. Do not mandate non-overlap. If packing depends on per-item radii/aspect, assign them before solving. A union of filled discs that removes white interiors is not automatically the desired membrane result.

**Masks:** explicit references still need spaces, evaluation stages, visibility and dependency semantics. PSD clipping groups can require grouped blend/opacity behavior, not just independent alpha multiplication. Check representability rather than blindly restructuring layers.

Use synthetic fixtures unless rights permit storing the original references. Do not add private images, fonts or plugin binaries to this public repo.

A shared Inspector/Add menu does not implement a new evaluator/domain. Authored identity, generated identity and index differ; a seed alone does not preserve per-element values after topology changes or across algorithm versions. A field is evaluated in a domain/context and is not merely a scalar with a longer list. Non-destructive editing is not a promise of unlimited history or arbitrary inverse topology conversion.

## Change policy and references

Preserve data meaning and user work, not every M0 class, widget or proposed feature name. ARCHITECTURE.md owns change/persistence boundaries; docs/first-usable.md owns M1 acceptance. Prefer focused refactoring/migration over a parallel replacement engine.

Additional primary references, not claims of Nect capability:
- https://docs.blender.org/manual/en/latest/modeling/geometry_nodes/fields.html
- https://developer.blender.org/docs/features/nodes/fields/
- https://doc.qt.io/qt-6/qundo.html

Session remains the history owner; Qt undo documentation is reference material, not permission to create a competing history stack.
