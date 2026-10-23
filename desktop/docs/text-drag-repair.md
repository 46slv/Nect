# Text click / drag repair

The former pointer-down double-click detector opened the canvas text editor on
a second press within500ms, before that press could become a drag. Reproduced
at source9f2db9c: click Text, immediately drag, expected position250/260, observed
220/240 with the editor open.

The editor now opens after two completed stationary clicks. Movement of3 CSS
pixels or more, including an out-and-back drag, disqualifies a click. Normal
Select/Move and the common move gesture keep their existing ownership/Undo.
Pointer cancel, document/session replacement and changed revisions invalidate the
pending click. Locked/bypassed Text cannot enter canvas text editing.

Select/Move keeps the existing supported Text and saved OS-font routes, including
one terminal Transform. Moving final artwork edits its artboard-space position
or terminal Transform offset. Text/font settings and stored OS outlines are
retained. The explicit Move node stage marker keeps its pre-transform source
semantics. Existing unsupported graph/type-on-path/driver guards remain.

Production bundle/CSP evidence in `tests/text-drag.e2e.js`:8 movement variants
(Text/OS snapshot, Transform, zoom, negative scale and locked source position
under a movable Transform),4 lock variants, double-click/Escape, net-zero drag
and exact one Undo/Redo. Existing canvas-gesture/caret model checks:7 pass.
OS fixture geometry is synthetic for browser coverage; a separate isolated EXE
check uses a previously captured real Windows shaped Text snapshot too.

This changes UI gesture arbitration only. No document schema, font shaping,
font substitution, native storage or recovery autosave change is included.
