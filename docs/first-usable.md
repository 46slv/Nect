# FIRST_USABLE — M1 contract

M0 is the headless kernel baseline. M1 is the first usable desktop loop and is not complete yet. The increments below expose usable work early; they do not reduce the final M1 acceptance.

## Flow

Qt new document -> path creation -> point/handle selection -> Path Inspector numeric edit ->
pick-whip/name reference -> direct drag -> native save -> full restart/reopen ->
formal MCP client mutates the same live document -> GUI reflects it -> Undo/Redo -> SVG export.

## Build and try in increments

**A — manual editing loop.** Real creation/selection of paths, Canvas plus contextual numeric editing, one-gesture Undo/Redo, cancellation, safe native save/reopen and SVG output. Add the necessary create/edit commands to the existing core; the demo document is not a substitute for creating paths. Let the user try this before every panel, effect or protocol feature is ready.

**B — connected properties.** Pick-whip and explicit name/path authoring resolve to stable IDs. A driven value is visibly distinguished from its authored source. Test rename/reorder and unbinding without ambiguous retargeting.

**C — same-session automation.** A formal MCP client initializes, lists tools and edits that same live desktop document. The desktop reflects the edit and can undo it. Different sessions/documents and stale revisions must not be confused.

The agent can overlap independent work, but should keep a usable manual loop and not wait for all future features before collecting UX feedback. Mark partial completion as A/B/C, not as M1 Done.

## Completion checks

- F1: Windows standalone launches and creates/selects at least two paths.
- F2: point X/Y and all in/out angle/length values are editable in Inspector and by direct manipulation.
- F3: pick-whip/name authoring resolves to stable IDs and survives rename/reorder.
- F4: save/reopen preserves values, bindings and order; source files are not silently lost.
- F5: actual MCP initialize/list/call reaches the same live Session.
- F6: failed edits leave no partial state; GUI and API/MCP mutations participate in Undo. A completed drag is one undo entry and a cancelled drag leaves no edit.
- F7: SVG export is checked by an independent parser/renderer and native bindings remain.
- F8: UI is canvas-first, low-noise, spatially stable and usable for the actual edit task. The standard still-graphics workspace has no timeline, playhead, video transport or timecode.

## Hands-on decision loop

Use one saved example for a narrow question: e.g. choosing a handle, editing its angle/length, switching selection, linking another object's value, or finding the grid toggle. Make one hypothesis, implement it against the existing Session, perform the task at the actual working window size, then keep/change/remove that interaction.

Record only the observed friction, chosen change and remaining uncertainty in the current issue/PR or checkpoint. A screenshot establishes appearance, not interaction correctness. No permanent A/B framework, exhaustive UI matrix or new validator is required for each experiment.

Before/after a presentation change, check that the same document still opens, the selected ID and bindings have not drifted, and Undo behaves as expected. If persisted semantics must change, use the evolution contract in ARCHITECTURE.md rather than starting a parallel model.

## Excluded from M1

Full AI/PSD writer, all fonts/effects, OFX host, full PointSet/field/packing engine, complete GPU renderer, RAW, generative providers, full print/RIP production, branching history, marketplace, cloud sync. Local repeat/mask/scatter features are later slices unless a current instruction explicitly selects a bounded probe.

## Early risk probes

- Japanese horizontal/vertical shaping, punctuation, IME, fallback.
- Transparent overlap/group isolation/mask/color-space semantics.
- Representative SVG/PDF/AI interop observations without modifying originals.

These reduce schema risk; they are not gates requiring full product parity. A failed probe may justify a local seam or migration before that capability is implemented, not a wholesale rewrite.
