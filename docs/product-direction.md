# Product direction — implementation extract

Reviewed: 2026-09-20. This is a small, versioned extract for agents without Notion access, not a second full requirements database. Product discussion remains in the Notion hub linked by README. Current work is selected by CURRENT_GOAL.md; this document does not authorize the whole roadmap.

## The product to optimize for

Make editable, non-destructive **2D graphics** quick to create and revise. Direct Bézier editing, AE-like shape operations and compositing/node capabilities should reinforce that workflow rather than become separate applications.

Illustrator/Photoshop interoperability, API/MCP and optional Resolve integration remain product goals. They do not require building a complete Photoshop, animation editor, Blender Geometry Nodes implementation, print RIP and plugin marketplace before the editor is usable.

## Intent, trial decisions and implemented capability are different

- Required behavior: stable point/handle controls, non-destructive editing, cross-property links, useful text, masks/effects, standalone authoring and API/MCP.
- Current UI constraint: **no timeline, playhead, time ruler, clip lanes, video transport or timecode in the standard still-graphics workspace** (Notion REQ-178).
- Trial layout: Canvas center, structure browser left, contextual Inspector right; deep editors are opened when needed. Position, size and residency are adjustable hypotheses, not file-format constraints.
- Current implementation: M0 Group/Path objects, scalar properties and simple bindings, native JSON and SVG subset. There is no implemented general operator/instance/field system yet. Consult code and capabilities for exact support.

AE is a reference for shape operations, property editing, effects, blend modes and equivalent shortcuts, not a requirement to copy its timeline-oriented workspace.

## Creation preference — instantiate, then parameterize

For parametric primitives, Nect should prefer **instantiate first, then edit parameters** over forcing a draw gesture to define initial geometry.

The user's strongest explicit example is Circle:
- Add Circle immediately creates a valid parametric circle.
- Center X/Y and Radius/Diameter are editable in the Inspector, by scrub/numeric input, and optionally by Canvas handles.
- Click-drag creation may exist as an accelerator, but is not the default or only path.
- Initial placement/size heuristics are prototype details; changing them must not require a different Circle object type.

This is a durable interaction preference, not a one-off shortcut request. Whether Rectangle/Polygon/Star should default to the same model is evaluated from real use rather than assumed.

## Geometry source, explicit path and conversion

Prefer one authored authority per shape.

A parametric Circle is best modeled as a **Geometry Source that evaluates to Path geometry**:

```text
Geometry Source: Circle
  Center X/Y
  Radius
        ↓
Evaluated Path
        ↓
Shape operators / paint / masks / effects
```

This gives the desired “it is a path, but I can still control Radius” behavior without simultaneously treating Radius and arbitrary anchors as competing truths.

- Direct manipulation that maps cleanly to the source may edit source parameters (move Center, change Radius).
- Arbitrary anchor/handle editing may be represented by a later path deformation/override when that is semantically useful.
- `Convert to Path` explicitly replaces the generator with current explicit anchors when the user wants unrestricted Bézier editing.
- Conversion must inspect dependencies on generator-only properties such as Radius. Do not silently break a reference.

Circle remains immediately instantiated and parameterized; conversion is optional, not required for ordinary downstream path processing.

## Ordered Shape stack — AE as behavioral reference

Shape-local processing is an ordered stack. Repeater/paint/path operations may be moved when the input/output semantics remain valid; ordering is part of the authored result.

For operators with an After Effects counterpart, AE's current group/order/scope behavior is the compatibility reference. Adobe documents Repeater as creating virtual copies of the paths, strokes and fills above it in the same group, and supports multiple Repeaters. Nect does not need AE's Timeline UI, but should avoid inventing subtly different ordering for the same named operation.

Source: https://helpx.adobe.com/after-effects/desktop/drawing-painting-and-paths/shapes-and-shape-attributes/shape-attributes-paint-operations-path.html

## Property references with multi-selection

Multi-selection should make referencing other objects easier, not trap the user inside a mixed-value Inspector.

Candidate interaction:

- A property row can `Copy Value`, `Copy Reference`, `Paste Value` or `Paste Link`.
- Starting Pick Source / pick-whip freezes the current target property selection while the user navigates to another object/group/effect/property.
- Choosing the source restores the target selection and creates the stable-ID reference.
- Applying one source to a common property on multiple selected targets is supported.
- A future `Relative Link` may preserve each target's existing delta as an offset; it is separate from ordinary absolute linking.

This is a presentation of the existing property dependency model, not a second link engine.

## Transform Parent, masks and Group compositing

Keep four relationships distinct:

1. Structure hierarchy — ownership/render order/effect scope.
2. Transform Parent — AE-like transform following without moving an item in Structure.
3. Collection membership — non-owning set membership.
4. Property dependency — values driving other values.

A Mask source can follow its target by using the target as **Transform Parent**; removing that Parent leaves the mask source in its own coordinate relationship. Parenting is cycle-checked and uses stable IDs.

Normal render visibility and “usable as mask source” are independent. A source may be hidden from ordinary rendering and still be evaluated for the target mask.

Neutral Groups remain appearance-preserving/pass-through where possible. If a Group has a group-level effect/mask/opacity/non-pass-through compositing operation, evaluate child geometry/appearance/effects first, composite the required child result, then apply the Group-level processing. The exact Pass Through versus Isolated blend behavior needs compositing fixtures; Transform Parent does not define effect scope.

## Center and Anchor Point

Every transformable object/group has:

- a **derived center** from current local geometry/bounds, and
- an **authored Anchor Point** used as the transform pivot.

New objects/groups initialize Anchor to the derived center. Content changes do not automatically chase that center afterward. Anchor can be edited numerically or on Canvas. Moving only the Anchor should preserve the visible world placement by compensating Position where representable. `Center Anchor` explicitly resets it to the current center.

For Circle, geometry Center and transform Anchor begin at the same location but are different properties.

## Multiple Artboards and Parent Artboards

Artboard is an **output frame**, not an object parent. Multiple Artboards live in the shared document coordinate space with stable ID/order and output metadata. Objects may sit outside every Artboard or cross several Artboards; moving/resizing a frame does not silently move artwork.

For repeated multi-Artboard design, explore an InDesign-like **Parent Artboard / Artboard Template** relation instead of turning Artboard into ownership hierarchy.

A Parent Artboard can eventually provide reusable:
- size/orientation/output defaults
- margins, grids and guides
- background/template content
- placeholders or repeated graphics

An assigned Artboard can override selected attributes or detach. Template content is linked/derived rather than copied into each Artboard by default.

This follows the useful part of InDesign Parent Pages: reusable repeating elements/layout guides across pages with local override/detach, while preserving Nect's shared Canvas model.

References:
- https://helpx.adobe.com/indesign/desktop/create-and-organize-pages/create-and-manage-parent-pages/about-parent-pages.html
- https://helpx.adobe.com/indesign/desktop/create-and-organize-pages/create-and-manage-parent-pages/manage-parent-items.html

## Minimal operator contract

Do not design a complete future Geometry Nodes runtime now. The smallest useful operator contract is:

- stable operator type ID
- semantic/version marker for persisted behavior
- typed accepted input/output domain
- normal Nect properties as parameters
- pure/side-effect-free evaluation for authored inputs
- stable operator-instance ID and enabled/bypass state
- optional Canvas manipulator metadata when the operator needs direct handles

Shape Stack and Node Graph may reference the same operator definition/evaluation logic without maintaining separate mutable operator copies. New domains such as PointSet or Field are added only when an actual operator requires them.

## Color workflow — value first, sampling second

Treat Color as a first-class value surface rather than making Eyedropper the primary transfer metaphor.

Candidate Color row:

```text
Fill   [swatch]  #4A73FF   [Copy] [Paste] [History] [Derive]
```

- Known colors move by textual copy/paste.
- Sampling/Eyedropper acquires unknown visual colors from the Canvas/raster and writes into the same Color model/history.
- Recent colors, document-authored colors, and user-pinned colors are separate scopes.
- Nect-to-Nect clipboard should support a structured color payload in addition to text/plain so alpha, color space/profile and future spot metadata are not needlessly lost.
- Hex is a convenient sRGB representation, not the universal color authority. Do not silently collapse CMYK/Lab/Spot/profiled colors to Hex.
- Complement/analogous/triadic/tint/shade can be derived candidates. The chosen color space and gamut mapping must be explicit before these become canonical behavior.
- A future linked derived color should use the normal property/reference model rather than a separate color-only dependency engine.

This direction keeps Eyedropper useful without forcing every color transfer through a tool gesture.

## Automation-first testability

Nect should be operable semantically without a person driving the mouse. GUI, API and MCP are clients of the same edit model; the machine surface is a product capability, not a test-only backdoor.

Astra/CI should eventually be able to create/open a document, add/delete/reorder/group objects, edit/link/unlink properties, save/reopen, undo/redo, evaluate, render/export and inspect the result through stable IDs and machine-readable receipts. A deterministic seeded scenario may perform many such operations and verify revisions/invariants without GUI event replay.

This does **not** mean every pixel of UI quality can be proven without a human. Semantic correctness, persistence, automation and many performance regressions can be exercised headlessly; discoverability, comfort and visual judgement still need UI evidence when those are the question.

Do not build a second automation document model. Unsupported GUI/API/MCP capability must be explicit instead of silently falling back to mouse simulation.

## Compositing baseline

After Effects' current blend-mode vocabulary and semantics are the compatibility target for Nect's final compositing model where applicable. The target set includes:

- Normal: Normal, Dissolve, Dancing Dissolve
- Darken: Darken, Multiply, Color Burn, Classic Color Burn, Linear Burn, Darker Color
- Lighten: Add, Lighten, Screen, Color Dodge, Classic Color Dodge, Linear Dodge, Lighter Color
- Contrast/complex: Overlay, Soft Light, Hard Light, Linear Light, Vivid Light, Pin Light, Hard Mix
- Difference: Difference, Classic Difference, Exclusion, Subtract, Divide
- HSL: Hue, Saturation, Color, Luminosity
- Matte: Stencil Alpha/Luma, Silhouette Alpha/Luma
- Utility: Alpha Add, Luminescent Premul

Source: https://helpx.adobe.com/jp/after-effects/desktop/work-with-layers/work-with-layer-blending-modes/blending-modes-layer-styles.html

This is a compatibility target, not an M1 promise that every mode is already implemented. Time-dependent modes, alpha behavior, working color space and higher-bit-depth behavior need fixtures; never silently alias an unsupported mode to another one.

## Artboard interaction performance

The Artboard/Canvas should feel unusually light. The minimum practical interaction contract is **30 fps** on a defined reference machine + representative scene after warm-up, measured as p95 frame interval <= 33.3 ms for pan, zoom, point/handle dragging and basic object transforms. Common lightweight scenes target 60 fps.

Use machine-readable frame timing so Astra/CI can reproduce regressions from seeded fixtures. Average fps alone is insufficient if large stalls remain.

If a heavy effect/operator requires an interactive preview, lower preview quality explicitly while editing and return to the committed/final quality deterministically. Do not freeze input while waiting for export-quality evaluation, and do not let export silently produce a different random/layout result.

Do not prebuild elaborate caching/worker systems solely for the target. Measure the real hot path first, then add dirty-region/cache/background work where the evidence warrants it.

## UI direction worth retaining across experiments

Property rows need precise numeric entry and continuous adjustment where appropriate. A dial, slider, XY control, curve or custom editor is a presentation of a property, not a new property owner. Schema-driven basic controls are a fallback, not a prohibition on purpose-built UI.

Keep the selected point/handle context legible. Panel rearrangement must not change the authored document, references or undo history. Selection, viewport, workspace and temporary gesture state are not extra persistent geometry.

Grid, margin, guide and snapping utilities belong near the Canvas. Separate fast visibility toggles, direct-edit modes and detailed setup. Changing a grid definition does not implicitly reflow, move or clip existing artwork. An explicit alignment/reflow command may do that.

Keep routine functions discoverable without hover-only state or mandatory shortcuts. Use restrained neutral surfaces, limited accents and subtle proximity feedback without moving hit targets. Exact spacing, residency, control choice and motion require hands-on trials; no generated mockup has authority over implemented behavior.

## Three authoring surfaces, not three engines

1. Direct Canvas/Inspector editing of authored points and properties.
2. A shape-local stack for convenient linear operations such as a repeater or offset.
3. A graph for branching, multiple inputs, collections and deeper effect construction.

Reuse operator definitions and evaluation rules when their semantics actually match. Do not maintain separately editable stack and graph copies and attempt bidirectional synchronization. A stack may present a representable linear part of a graph; an arbitrary branching graph need not convert into a stack. Keep it opaque or open the graph rather than discard connections.

A preset configures values; a Macro packages a graph with published parameters; an action records editing commands. These are related reuse tools, not synonyms or three general scripting engines.

## What to prove next

| Work | Timing / boundary |
|---|---|
| Real path creation, selection, point/handle edits, one-gesture undo, save/reopen, SVG | M1 manual loop first; use it before UI is polished |
| Script the same semantic create/edit/save/undo path through the command/API surface | Build alongside the manual loop so automated stress/readback is possible |
| Whip/name binding and formal MCP driving the same live Session | Remaining M1 acceptance, not a prerequisite to the first manual trial |
| Instrument the reference M1 Canvas interaction and demonstrate >=30 fps p95 floor | M1 performance acceptance; record machine/scene/timing |
| One local repeater, fill/style and a saved radial design | Next small procedural slice selected explicitly; no full node editor required |
| A simple explicit hard mask plus overlapping objects | A focused compositing/UX slice; no full PSD writer required |
| Region scatter and circles/ellipses from per-item values | Following slice once generated identity/domain semantics are defined |
| Packing, membrane generation, full fields and per-instance overrides | Separate experiments driven by results; not cheap UI additions |
| Full AE blend-mode coverage, AI/PSD round-trip, OpenFX host, production typography/color/print | Dedicated capability work with fixtures; small risk probes may run earlier |
| RAW, generative providers, 3D assistance, PDF/VT, imposition, marketplace | Backlog unless a current goal explicitly selects one |

The table is a sequencing guide, not a promise that every row is mandatory or equally easy.

## Reference tasks and corrections

**Radial ornament.** Start with a manually authored filled Bézier motif and repeated transforms. A dedicated Spark generator is optional. Preserve source edits, change count/spacing, and save/reopen. A changed count may require an explicit fit-to-arc versus fixed-step rule and may not preserve a six-step pattern at the seam.

**Bubble/membrane graphic.** The supplied image is a desired visual result, not evidence of how it was made. Overlapping rings, local connectors, mask/field processing and packing are alternative recipes. Do not mandate non-overlap or a physical solver from the image alone. First try editable circles/ellipses and explicit proximity controls. If packing uses per-item radii/aspect, assign those before solving. A filled-disc union that removes all white interiors does not match a hollow-ring target.

**Mask interaction.** Prefer explicit source/target selection over hidden adjacency requirements. Still define coordinate space, source visibility, evaluation stage, mask mode and cycle handling before calling it correct. An explicit reference is not the entire compositing contract. PSD clipping groups may require grouped blend/opacity semantics and cannot always be translated to independent alpha multiplications.

Reference images remain research inputs; do not add private or unlicensed image bytes to this public repository. Synthetic fixtures may exercise the same behavior without claiming exact reconstruction.

## Corrections to earlier extensibility claims

- A searchable Add menu and generic Inspector make discovery/presentation extensible; they do not implement new value domains, evaluators or solvers.
- Authored stable IDs, generated element identity and transient list indices are distinct. A seed alone does not guarantee stable per-item values after topology/order changes. Define generator identity and algorithm-version behavior when that slice starts.
- A per-item field is evaluated in a domain/context. It is not just an ordinary scalar binding with a longer list of values. Do not prebuild a general field engine for M1.
- Preview and final quality must not silently change the committed layout or random seed. Any provisional result is visibly provisional; export selects a known revision and declared quality.
- Non-destructive authoring does not make arbitrary topology changes reversible. Offer an explicit editable copy or scoped override with visible invalidation, not a hidden destructive conversion or a promise of infinite undo.

## Change policy

Preserve data meaning and user work, not every M0 class or every proposed feature name. Internal refactoring is allowed when a real task exposes a limitation. Prefer a local change with a migration/compatibility test over a parallel replacement engine. ARCHITECTURE.md owns the iteration boundaries; docs/first-usable.md owns M1 acceptance.

### Focused primary references

These inform the design; they are not Nect implementation claims:
- Blender Fields: https://docs.blender.org/manual/en/latest/modeling/geometry_nodes/fields.html
- Blender developer field context: https://developer.blender.org/docs/features/nodes/fields/
- Qt undo concepts (command compression/macros): https://doc.qt.io/qt-6/qundo.html
- Adobe After Effects blend modes: https://helpx.adobe.com/jp/after-effects/desktop/work-with-layers/work-with-layer-blending-modes/blending-modes-layer-styles.html

The existing Session remains the history owner; Qt's undo facilities are reference material, not authorization to add a competing history stack.
