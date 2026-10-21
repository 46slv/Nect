# Guides Escape maintenance

Bounded brief: investigate only the inherited Guides DOM failure after Escape,
from PR62 `a333a88a7ccebdc1b7158faf4a2fdfd69f49429e`. Keep the received Fireworks
Ring sample/output and previous candidates unchanged. No feature or panel redesign.

## Two distinct findings

The reported `guides-dom.test.js:76` null click is an obsolete test sequence.
Exact PR60 main/test sources already reproduced it. `main.js` deliberately closes
transient panels on Escape and outside clicks; Grid, Guides and Layout Grid are
independent surfaces (interaction.md). The test was using controls after their
panel closed and later expecting the guide list inside the separate Layout panel.

A targeted connected-input probe exposed a real related defect. A guide drag
retains number-input focus. Escape entered the numeric-expression early return;
the active guide Session gesture survived and late pointerup committed its preview.
The sealed baseline native EXE reproduced this naturally: focused-guide retained
focus, guide600 previewed near640, Escape retained `guide-position`, pointerup
committed639.9999406368627 and enabled Undo, with page errors0.

PR55 fixed the same early-return family for started numeric scrubs. That exact
numeric guard still works. Guides had no equivalent key ownership and also hit
the later generic editing-control return; excluding numeric classification alone
did not fix this path. The final product change adds only a focused-control guide
Escape branch before both gates, using existing cancelInteraction/cancel-gesture.
Body Escape and normal typed editing retain their existing routes. No snapshot,
restoration, history or cancellation behavior is omitted.

## Minimal repair and verification plan

- Product: five added main.js lines; no core, schema, geometry, grid settings,
  dither, Repeater, Text or panel layout changes.
- Test: explicitly open the correct existing panel after dismissal or toolbar
  Undo/Redo. Check scroll preservation while that panel exists; do not assume
  Guides and Layout controls coexist. Keep all guide/grid/lock/preview/history
  assertions. Lock the composition with Session rather than mutate a snapshot.
- Strengthen guide pointercancel/body Escape/focused-input Escape with an active
  preview, immediate gesture release, late pointermove/up document neutrality,
  file/command/history flags and selected node identity. Native before/after uses
  natural focus and a verified guide hit, not a forced post-drag focus change.
- One legacy Undo comparison checks every authored key/value and ordered array
  with deep equality instead of assuming JSON object member order. Unchanged
  preview/cancel JSON and SVG assertions remain exact; no tolerance/skip added.
- Run the repaired Guides DOM case and nearby guide/grid/numeric focused suites.
  Verify one patched native case against the observed baseline, including a
  normal typed guide edit and Undo. A real keyboard fix requires frontend/native
  build and a separately sealed candidate. No unrelated full audit.

Native input is CDP synthetic. Physical input, OS dialogs, Save/Open and clean
cold reopen remain unverified. Dedicated dirty fixtures are CLI-preserved before
direct test closure; user profiles/Recovery are not used or cleared.
