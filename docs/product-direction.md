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
