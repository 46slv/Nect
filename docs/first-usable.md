# FIRST_USABLE — M1 contract

M0 is a tested headless editing kernel. M1 is the first usable desktop loop and is not complete yet.

## Flow

Qt new document -> path creation -> point/handle selection -> Path Inspector numeric edit ->
pick-whip/name reference -> direct drag -> native save -> full restart/reopen ->
formal MCP client mutates the same live document -> GUI reflects it -> Undo/Redo -> SVG export.

## Completion checks

- F1: Windows standalone launches and creates/selects at least two paths.
- F2: point X/Y and all in/out angle/length values are editable in Inspector and by direct manipulation.
- F3: pick-whip/name authoring resolves to stable IDs and survives rename/reorder.
- F4: save/reopen preserves values, bindings and order.
- F5: actual MCP initialize/list/call reaches the same live Session.
- F6: failed edits leave no partial state; GUI and API/MCP mutations participate in Undo.
- F7: SVG export is checked by an independent parser/renderer and native bindings remain.
- F8: UI remains canvas-first, low-noise and spatially stable enough for real use.

## Excluded from M1

Full AI/PSD writer, all fonts/effects, OFX host, complete GPU renderer, RAW, generative providers, full print/RIP production, branching history, marketplace, cloud sync.

## Early probes

- Japanese horizontal/vertical shaping, punctuation, IME, fallback.
- transparent overlap/group isolation/mask/color-space semantics.
- representative SVG/PDF/AI interop observations without modifying originals.

These probes reduce schema risk; they are not gates requiring full product parity.
