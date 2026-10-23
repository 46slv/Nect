# Graph-pan joystick (REQ-285)

Replaces the four-button toolbar pad with a view-only joystick fixed to the
node-graph body's lower-right corner. The scrollbars remain. The control is a
sibling of the scrolling viewport, so graph scroll/zoom never moves it.

## Input and defaults

- Default outer disc: 80 CSS px; recessed knob: 50% of the disc; radial knob
  travel: 22.5% of disc diameter (18 px at the default size).
- The outer disc is translucent; the knob stays opaque and visibly inset.
- Pointer Events support mouse and touch, including diagonal travel. The whole
  disc is a hit target. The knob is bounded radially, not independently per axis.
- Content moves in the knob's **screen direction**. Native scroll offsets move
  in the opposite direction. Existing nonnegative top/left origin clamping is
  retained: right/down content movement stops at that origin. Node coordinates
  are never shifted to invent extra room there.
- A 12% center dead zone prevents small touch/mouse tremor. Speed ramps linearly
  from zero outside it to 600 CSS px/s at full travel. This keeps center control
  precise while traversing a typical viewport in about a second. Speed does not
  change with graph zoom; diagonals have the same total speed as axial travel.
- Movement integrates elapsed frame time, equivalent at 30/60/144 Hz. A stalled
  frame is capped at 50 ms to avoid a large catch-up jump. Native scroll rounding
  retains fractional movement intent, so low speeds do not round away each frame.
- The focused control supports held arrow keys and arrow combinations. Existing
  graph zoom/fit and clipboard shortcuts still recognize it as graph context. Escape
  stops. Release, blur, cancellation and focus movement stop and recenter. Tab
  leaves normally; modified/IME/unrelated inputs are not claimed by the joystick.
- Neutral input and an idle control schedule no animation frame. A held input at
  a clamped boundary may keep the active input loop alive until release.

## Safety and lifecycle

The controller receives only a viewport-pan callback. It has no document,
selection, Session, serialization or history API. Pan updates scroll extents only
when needed, and updates Overview only after a real scroll change.

Presses are owned at window capture, before document menu-dismiss handlers or
native focus changes. Editable input focus, IME, rename, gestures, wire edits,
Pen drafts, variable actions, modal dialogs and open menus block a start. Rejected
presses do not blur/commit a pending field. Ending clicks do not trigger global
panel dismissal or document actions.

Release outside the disc, pointercancel, lostpointercapture, mouse buttons no
longer held, window/control blur, hidden document, compositionstart and Escape
stop immediately and cancel scheduled frames. Structural render disposes the old
controller before replacing its host; document replacement cannot retain its loop.

## Compact layout

The disc scales down from 80 px to fit the actual scrollable graph body with 8 px
clearance on each side of that area, accounting for native scrollbars. A 60 px
high scrollable body gives a 44 px disc. In an exceptionally short body the disc
continues to shrink rather than covering a toolbar or forcing unrelated panel
layout changes; a 44 px touch target cannot fit below that height. Expand the
graph panel for a larger target. A completely collapsed viewport hides and stops
the control until there is usable space again. The Overview shifts beside the disc in short
panels and keeps its original top-right position when enough height is available.

## Verification and limits

Validated on 2026-10-22: 49/49 focused unit/DOM compatibility tests passed,
including 22 joystick-specific tests. `npm run format:check`, `git diff --check`
and `npm run build` passed. The build retains the existing large-bundle warning.
No timeout was raised and no unrelated assertion was removed or relaxed.

Automated deterministic tests cover vector bounds and speed, frame timing,
mouse/touch input, cancellation paths and queued-frame non-resurrection, focused
keyboard behavior, actual window/document propagation, and exact shipped-editor
project/document/export/file-state/command-revision/selection/Undo/Redo invariance
with both history branches populated. Existing navigation, autopan, wires,
snapping, Overview, numeric-edit and Pen compatibility suites remain applicable.

The updated real-browser checks are `tests/graph-pan-joystick.e2e.js` and the
joystick section of `tests/graph-navigation.e2e.js`, both included in the existing
`npm run test:e2e` chain. They were prepared but are
not browser-rendering evidence by themselves. This implementation lane does not
launch a browser/server, validate native WebView2, build a Windows installer or
claim touch-device/native acceptance. No scrollbar-removal decision is made.

## Normal-UI acceptance

Additional Chromium acceptance on 2026-10-22 confirmed left/up/diagonal motion,
release recentering and no subsequent movement, and preservation of an unfinished
numeric field's value and focus. The saved project was byte-identical before and
after navigation, draft cancellation and panel resizing. A 60 px graph body kept
a 44 px disc beside a non-overlapping Overview; expanding restored the 80 px disc.
This does not establish real-touch or native WebView2 acceptance, or manual
coverage of every zoom level and cancellation path.

1. Open a graph with several nodes, select one, and keep its coordinates visible.
   Pan away from the top/left origin using native scrollbars or Space/middle drag.
2. Drag the lower-right knob right, up and diagonally. The visible nodes must move
   in exactly that screen direction continuously; knob travel stays inside the
   disc. Test at 5%, 100% and 200% graph zoom. Return toward the origin and confirm
   the existing top/left clamp instead of a node-coordinate change.
3. Release outside the disc, cancel a touch, switch windows/tabs and refocus.
   Motion must stop immediately and the knob must center. Reoperate successfully.
   Tab to the control, hold two arrows, release one, then Escape; verify direction
   changes and stopping without tool or selection changes.
4. Type an uncommitted numeric value, then press the joystick. The field, text,
   focus and document must remain unchanged. Try while renaming, composing IME,
   a menu/picker is open and during a node/wire drag. Those inputs retain ownership.
5. Open Overview; resize the graph panel down and back up. Neither Overview nor
   scrollbars should be covered. Scroll/zoom the graph and confirm the control
   remains at the body's lower-right. Wheel, direct pan and Overview still work.
6. Make two edits and Undo one; pan with mouse and touch. Dirty state, selection,
   coordinates, wires, Undo and the retained Redo must be unchanged. Switch the
   composition or load another document while moving; no old motion may survive.
