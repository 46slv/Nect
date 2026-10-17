# Editing refinement — active specification

## User outcome

Slant editable text without rotating it, select dense path points, zoom naturally,
and adjust every editable numeric control consistently. Existing artwork must keep
its appearance and authored graph connections when opened.

## Approved scope

- Extend the existing Transform: Anchor XY, Position XY, uniform/independent width
  and height scale (100% UI), Skew, Skew Axis and Rotation. Preserve legacy relative
  Offset and scalar scale semantics. Do not claim exact AE signed-matrix parity
  without a measured comparison. Opacity/shutter/motion blur are outside this slice.
- Use the same affine for path geometry, character provenance, inline caret,
  alignment and SVG/PNG export; retain editable text and stable identities.
- Path Remap controls use small screen-space markers with separate hit regions.
  Dense points remain distinguishable; show relevant handles for selection/hover.
  Zoom/pan and downstream transforms must not enlarge markers into overlapping discs.
- Alt + wheel zooms the artboard around the pointer, also while editing Remap.
  Preserve existing pan and numeric editing; never zoom the application/browser page.
- Every editable numeric field supports drag and typing. Preview immediately,
  commit one drag as one Undo, cancel cleanly, respect ranges, locks and drivers.
  Text, selectors and toggles retain their appropriate interaction.
- Suppress browser navigation/Save/Print context menus on graph/canvas surfaces.
  Retain existing Nect socket actions and native text-input editing menus.

## Acceptance

Representative transformed horizontal/vertical text, legacy JSON/connections,
independent scales, dense Remap drag/cancel/Undo, cursor-centred zoom, typed percent
followed by drag, local preference fields, document/guide numeric commands, context
menu routing, save/reload and SVG/PNG. Focus tests on these changes; preserve producer
boundaries for browser, native Windows and physical Japanese IME evidence.

## Current evidence and next work

The initial implementation was delivered as development trial 7496aee on 2026-10-17
07:28 JST. Offline Windows release build succeeded; the application was not launched
for native acceptance in that verification task. Browser tests cover selected
Transform handles, numeric gestures, Remap vertex operations and document guards.
They do not establish physical IME, exact AE skew parity, Alt-wheel interaction feel
or all tangent-handle affordances. These remain explicit acceptance obligations.

On 2026-10-17 09:16 JST Shiro reopened independent dot-side fixes and Brief work.
Inspect every editable numeric control category against drag/typing/Undo/cancel
semantics and repair demonstrated gaps. Do not wait for subjective trial feedback
before such independent repairs. Do not infer acceptance of deferred legacy features.

A practical production checkpoint is editable horizontal/vertical text plus vector
artwork, with transform/paint/alignment, save/cold reopen/re-edit and SVG/PNG export.
Use a representative fixture and source-bound evidence; do not rebuild a new mock UI.
Full Illustrator parity, legacy Qt format conversion and generic integrations remain
outside the adopted first-delivery scope.

## Numeric preference fix — 2026-10-17

A locked active composition previously prevented all numeric dragging, including
Alignment Spacing and layout drafts that remain editable by typing. Pointerdown,
move and pointerup now distinguish local preferences from authored parameters.
Local preference drags remain available; graph/text/command targets still honor
composition locks. Cancel restores the preference and does not alter document
history. The regression first failed (12 remained 12 rather than 32), then passed
after the fix; numeric DOM and pure-unit tests pass 4/4. This is source-test evidence,
not a new native trial qualification.

## Remap hover lifetime fix — 2026-10-17

Hover-only tangent handles previously remained visible after pointer movement left
Canvas. Outside-stage pointer movement now clears hover while preserving clicked
point focus and active gestures. A focused DOM regression reproduced two stale
handles, then passed after the fix; clicked-point handles remain visible. The same
view-navigation test retains Alt-wheel and view-only/document-preservation checks.
This is DOM evidence, not native GUI or physical-input acceptance. The distributed
trial remains 7496aee; subsequent source fixes have not been packaged into a new ZIP.

## Stroke presets — 2026-10-17

The requested dotted/dashed appearance already existed in Stroke's dash pattern,
cap and SVG/PNG path. A named Solid / Dashed / Dotted selector now exposes it in
both graph and inspector. Dashed sets 12 px / 6 px with butt caps; Dotted sets
0 px / 8 px with round caps. Solid clears the dash pattern, retaining the cap.
Manual patterns remain editable and display Custom. Each preset is a single
undoable node-settings command and respects composition locks. Focused stroke
DOM/core tests pass 4/4; no new Windows ZIP has been produced. Pucker/Bloat meaning
is awaiting clarification; no deformation node has been added in this slice.
