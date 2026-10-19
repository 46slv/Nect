# Independently Sized Compositions — Preimplementation Design

**Status:** Stage 1/2 model conversion and Stage 3 session/lifecycle integration are implemented. The focused owner/model run passes 19 tests; core factory runtime and GUI workflows remain NOT RUN because this checkout lacks required external packages. This is not a native UI acceptance or release claim.

**Scope:** Desktop project model, tab selection, import, history, and per-Composition SVG/PNG output.

## Problem and terminology

The current Desktop project has one root `width`/`height` frame and a root `layers[]` list. A `Layer` owns a node graph; the implementation and parts of the UI call that graph a “composition.” `Composition Reference` points to one of these Layer graphs. This is distinct from the user-facing, independently sized Composition container proposed here. The UI may call the new container “Composition,” while serialized and internal names for the existing `Layer`, graph nodes, and legacy references remain unchanged where practical. Do not mass-rename the existing model.

The proposal adds multiple independent output frames inside one editable project. Each frame owns a Layer stack and canonical pixel dimensions. A horizontally scrollable Composition tab strip sits above the existing Layer list. Switching tabs selects an editing/output scope; it does not resize, scale, or otherwise edit artwork.

## Proposed project model

The proposed saved-project envelope has a `compositionSchemaVersion` separate from the existing node-content `schemaVersion`, plus an ordered top-level `compositions[]` array. Each entry has a stable globally unique `id`, a user-facing `name`, positive finite `width` and `height` in canonical document px, optional per-Composition `editor` state, and the existing `layers[]` records. Keep project-wide assets such as fonts and images at project scope. Preserve each Layer graph and its node semantics as-is. There is no second Artboard entity in this Desktop model: a Composition's width and height define its zero-origin output frame. Guides/grid state is stored per Composition because it uses that Composition's coordinate space.

The module at desktop/src/composition-project-model.js defines this envelope, migrates legacy root frames, validates and serializes the envelope, and projects one Composition into the existing single-Document shape. CompositionSessionOwner connects that projection to existing core commands and project-wide History. main.js uses the full envelope for native/browser Open, Save, recovery and public load, while SVG/PNG export use the active Composition projection. Old single-Document JSON runs through the core deserializer/upgrader before migration. The core factory and actual UI lifecycle still need runtime verification.

When opening the existing single-frame format, migrate its root `width`, `height`, `layers[]`, and coordinate-dependent `editor` state into exactly one Composition, retaining every Layer/node ID and content value. Leave project name, node-content `schemaVersion`, node presets, and font/image assets at project scope. When importing a legacy project, use that source project's own root dimensions. Never substitute the destination Composition's dimensions. The current supported legacy schema requires root width and height; if those fields are absent or invalid, reject the candidate through existing validation rather than silently guessing. Validate an entire open/import candidate before replacing or appending any project state.

Changing a Composition's frame size leaves its authored coordinates and artwork scale unchanged. Scaling artwork to fit is a separate future command and is outside this proposal. Existing display-unit conversion remains a UI setting over canonical px.

## Session, tabs, import, and history

- Keep the existing document-wide Session/history owner. Create, rename, resize, delete, or import one Composition through one atomic command addressed by stable `compositionId`; a width-and-height change is one Undo step. Undo/Redo restores the exact prior project snapshot.
- Preserve the existing `set-document-settings(width,height)` behavior as a compatibility adapter for one-Composition projects. Multi-Composition commands must name a target ID; ambiguous untargeted writes fail validation.
- Store `activeCompositionId` and per-Composition active Layer, node/artwork selection, zoom, and pan as window/session UI state keyed by stable ID. Tab switches create no document revision, dirty state, or History entry. If state is missing on reopen, select the first editable Composition and its initial editable Layer; tab selection need not be persisted in the project.
- Place the horizontal, keyboard-accessible Composition tablist above the current Layer panel. Keep the current Layer controls scoped to the active Composition. Deleting the active Composition selects the nearest remaining one. Reject deletion of the last Composition.
- Change “Import Composition” to enumerate outer Compositions and import the chosen complete container with its source dimensions, ordered Layers, nested references, and required assets. Remap imported IDs atomically and add one new tab. Do not fit content to the destination, merge the imported Layers into the active Composition, or change the active Composition's size. Regular Open continues to replace the whole project after successful validation.

## References and coordinate-dependent state

Existing `Composition Reference` records contain a `compositionId` that currently identifies an internal Layer graph and evaluate as a translated painted group. Preserve this legacy meaning when migrating existing files. Reusing the same command for references to a new outer Composition needs a versioned, unambiguous representation: retain a `sourceLayerId` for a legacy Layer-graph instance, and use an outer `compositionId` for a whole-Composition instance. A whole-Composition instance should default to 1:1 document-unit mapping, translation by its Position, no automatic fit, and clipping to the source frame `[0,width] × [0,height]`; an explicit downstream Transform may scale it. Resizing a source frame must not scale its artwork. Reject cycles and invalid references before committing.

**Decision: defer outer-Composition references.** The first Composition-container implementation keeps the existing `Composition Reference` representation, target IDs, and translated Layer-graph evaluation exactly as they are. It does not add a mode, new reference target kind, or any reference to an outer Composition. If cross-Composition instances are considered later, they require a separate explicit schema and UI decision; no target kind may be guessed from an ID.

The isolated active-Document bridge requires each existing Layer-graph reference target to be a Layer in the same outer Composition. A missing target or a reference crossing outer Composition boundaries is rejected rather than losing its dependency when the active Layer stack is projected.

Guides and layout-grid coordinates should be scoped to the Composition whose coordinate space they describe. General presentation preferences may remain project/window-wide. This scope needs a persistence audit before implementation because current editor state is not uniformly scoped per output frame.

## Save, reopen, Undo, SVG, and PNG

- Save/reopen round-trips every Composition's ID, name, dimensions, Layer order/content, project-shared assets, and reference data. Failed validation leaves the current project untouched.
- SVG/PNG export targets the active Composition in the UI. Core APIs and CLI must receive an explicit `compositionId`; they must not depend on hidden active-tab state. Export evaluates only that Composition's visible Layer stack, uses its own `width`/`height` as the zero-origin frame, and leaves authored data untouched.
- SVG root dimensions and `viewBox` derive from the selected Composition. Existing SVG unit choices remain independent from artboard display units.
- PNG dimension calculation receives the selected Composition's frame dimensions. Keep existing scale/PPI and final integer-pixel rules unchanged; do not apply another Composition's dimensions or implicitly scale artwork.
- Multi-Composition batch export is outside scope. A caller must request one Composition at a time until a separate batch-export design is accepted.

## Acceptance criteria

1. Opening a valid legacy one-frame project creates one Composition with exactly the old dimensions, Layer order, node values, and stable content IDs; saving and reopening it preserves them.
2. A project with at least two different frame sizes saves/reopens without size or content drift. Resizing one frame leaves every other frame and all artwork coordinates unchanged.
3. Importing a differently sized legacy or new project Composition preserves its source dimensions, Layer stack, nested-reference closure, and required assets; no destination-size fallback or implicit fit occurs. A failed import makes no partial changes.
4. A size/name/import change is one document History operation with exact Undo/Redo. Switching tabs does not change document revision, dirty state, or History, and restores that Composition's editing context.
5. SVG and PNG exported for each Composition independently match its frame dimensions and visible Layer output. A two-Composition fixture proves the inactive frame is neither rendered nor used for sizing.
6. Invalid dimensions, duplicate IDs, missing references, reference cycles, and unsupported schema states fail atomically. Tests cover the existing maximum dimension and output pixel limits.
7. If outer-Composition references are accepted, test 1:1 placement, frame clipping, explicit downstream scaling, source-frame resize without artwork scaling, and rejection of cycles. Legacy Layer-graph references must retain their current rendered result.
8. Manual UI acceptance creates two differently sized tabs, switches between them while preserving per-tab Layer/selection/view state, imports a third size from another project, exports each frame, then saves/reopens and repeats the checks.

## Staged implementation boundary

**Stage 1 (implemented, isolated):** validate the Composition envelope and migrate a legacy root {width,height,layers,editor?} frame into one outer Composition while preserving project-level fields and assets. The conversion is detached and rejects ambiguous or already-migrated inputs, invalid/missing dimensions, and duplicate entity IDs. It carries the legacy source dimensions; it never consults destination dimensions.

**Stage 2 (implemented, isolated):** JSON serialize/deserialize validates new envelopes and invokes the Stage 1 legacy migration once for old-format JSON. An active-Composition adapter creates a detached legacy Document for existing single-Document processing, then maps its edited dimensions, Layer stack, editor state, and shared project fields back to the selected Composition. It rejects an unknown active ID, invalid edits, duplicate IDs, missing Layer-graph references, and Layer-graph references crossing outer Compositions. Other Composition records are copied unchanged. Fonts and images remain shared at project scope.

**Stage 3 (implemented, runtime verification pending):** CompositionSessionOwner routes existing commands and chronological Undo/Redo through the active Document projection while recording complete project snapshots. Add Composition and tab switching are connected to the UI. Native/browser Open, Save, recovery and public load use the project envelope; legacy JSON passes through the core upgrader. SVG and PNG export use the active Composition's dimensions and Layer stack. Project-shared fonts and images remain at the root. The owner tests use doubles, so they do not prove the production core factory or UI lifecycle.

**Remaining:** whole-container project Composition import is not connected. Selection, active Layer, zoom and pan are reset on switching instead of saved per Composition. Core factory runtime and browser/native GUI acceptance for multi-Composition Open/Save/recovery/export are NOT RUN because dependencies are missing in this checkout. The project-envelope tests do not substitute for those checks.

## Later implementation touchpoints

Use the production core factory and real project lifecycle with an authorized local dependency/runtime. Then verify multiple differently sized Compositions, shared fonts/images, chronological Undo/Redo, legacy-file migration, browser/native Open and Save, recovery, public load, and active-Composition SVG/PNG output. Whole-container import and per-Composition view-state retention remain separate follow-on work.

**Decision state:** the envelope and active-Document bridge are connected to the current Desktop lifecycle and editing call sites. Outer-Composition references remain deferred; existing Composition Reference continues to identify a Layer graph. Core runtime and UI acceptance are still pending, so this checkpoint does not claim the lifecycle is verified end to end.
