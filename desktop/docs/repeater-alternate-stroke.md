# 2D Repeater odd-copy solid stroke

Base: verified PR59 `codex/repeater-alternate-fill`, remote
`3af27bf7030a53fda537fc372a437052b69490fa`. No equivalent Alternate Stroke exists.
Use the existing copy-group appearance carrier/render path, with an independent
defaultOFF Alternate Stroke and standard typed Alternate Stroke Color. Only
zero-based odd stable copy IDs1,3,5… replace an existing solid stroke's hex/alpha.
No stroke stays none. Gradient/pattern strokes are unsupported and reject clearly,
never silently becoming solid. The controls hint states these limits.

Keep geometry/handles/source/IDs, placement, width/dash/offset/cap/join/miter,
fill and copy opacity/order unchanged. Color alpha stays unmultiplied; the existing
copy group applies opacity exactly once. Scale/Fill/Stroke vary independently,
and combinations use the same index regardless of Offset or paint order. The
final solid stroke supplies style/base; enabled odd copies replace its Color only.
Ordinary Transform preserves derived stroke metadata; painted compositions retain
it in existing children. No painted-input route or renderer redesign.

Only Geometry Repeater gains two fields. Default descriptors backfill without
promotion; authored flag/color variation or bindings require schema47. Reuse the
common Color parser, base/ref/value/math, validation and atomic Session history.
Do not add an index language, palette or gradient model. DefaultOFF must match
PR59 geometry/render bytes exactly, including existing Alternate Scale/Fill.

Focused verification: all combinations of three independent variations, odd/even
and reversed order/Offset, immutable geometry/source/IDs, stroke style/alpha and
partial copy alpha, none/gradient/mixed paints, Transform propagation, Color
dependency/errors, Undo/Redo/cancel, JSON/SVG and version admission. Use two native
24-copy representatives and the existing browser PNG export path once if usable;
state native OS-dialog/physical input limits. Preserve prior artifacts and build
a separate candidate. This completes the first scale/fill/stroke variation slice;
do not automatically expand to gradients, painted input or arbitrary index syntax.
