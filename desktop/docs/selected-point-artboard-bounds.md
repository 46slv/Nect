# Selected-point Artboard bounds

This is a bounded local usability candidate, not a new editing mode or a claim of
an earlier confirmed requirement.

The existing selected-anchor Node-local summary now has an Artboard companion in
both the graph card and Inspector. Each shows Min/Max X/Y, Width and Height for
the same stable Canvas-selected point IDs. Browsing another contour/unit or page
does not remove selected anchors from either measurement.

Artboard measurements use the exact supported `directSelectionSource` route used
by point alignment/distribution, including downstream rotation and nonuniform or
chained Transforms. They measure transformed anchors, not transformed corners of
the Node-local bounds, handles, curve extrema or stroke extents. The Artboard
label is **document units (px)**: one document unit is one CSS pixel at 100%.
Changing the Artboard dimension display to mm/cm does not convert these values or
the existing alignment/spacing coordinates. Display uses six significant digits;
each output's tooltip retains the full JavaScript numeric value.

Only owned Curve/Multi Path anchors are measured. Empty, stale, locked, driven,
invalid and out-of-range selections have no partial numeric range. An unsupported,
bypassed, driven or singular downstream route displays the existing direct-route
reason while an otherwise valid Node-local summary remains usable. No new route,
unlock, inverse mapping, editing control, command, schema or dependency is added.

Both summaries are derived on the existing selection/panel/command renders,
including Undo/Redo and Transform changes. Selection refresh also replaces both
read-only summaries while a numeric editor keeps focus. Nothing is persisted and
the queries do not mutate document state, revision or history.

Focused checks live in `selected-point-artboard-bounds.test.js` and
`selected-point-bounds-dom.test.js`, alongside the unchanged local-bound tests.
They cover independent scalar calculations, chained transforms, stable compound
membership, filtering, precision, unsupported states, focused refresh, both panel
copies, Transform/point edits and Undo/Redo, and Session neutrality. Synthetic DOM
checks are not browser/native visual acceptance.

A separate rendered Chromium check on 2026-10-22 opened a synthetic vector
document through Project/Open and selected three anchors across contours. The
Artboard values matched independently calculated rotation/nonuniform-scale bounds
within 3e-14 document units. Filtering all selected anchors out of the point list
retained the same measurements, and switching dimension display to mm left the
document-px measurements unchanged. A generated-path route retained Node-local
bounds and displayed its unsupported reason without partial Artboard numbers.
These selection/readout checks retained Saved status and disabled Undo. Native
Windows/WebView2 and performance qualification remain separate.
