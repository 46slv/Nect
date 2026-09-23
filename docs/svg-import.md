# Static SVG artwork intake

Implementation contract for the active vector-intake checkpoint; not full SVG support.

The Qt-backed memory reader in `src/desktop/svg_import.cpp` lowers interchange into
existing Session commands. It belongs to the IO conversion boundary and builds only
with the existing desktop adapter (QXmlStreamReader is Qt Core). Core-only CLI stays
Qt-free. GUI and formal MCP use Host import against the same live Session.

Import produces one editable Group appended to the requested Composition. SVG IDs
become labels; fresh prefix-based native IDs prevent external name/reference reuse.
All parsing completes before one atomic Session apply with serializability preflight.
Original SVG is unchanged; the resulting native geometry/paint is the editing source.
This is artwork intake: width/height/viewBox map coordinates, but viewport clipping
and a new output Artboard are not authored. Off-viewport artwork remains editable.
The GUI and machine result disclose this conversion boundary.

Supported subset:
- SVG/g/path and rect/circle/ellipse/line/polyline/polygon, title/desc text metadata;
  unqualified or SVG namespace. Basic shapes lower to editable paths.
- Absolute/relative M/L/H/V/C/S/Q/T/A/Z, repeated and compact coordinates, multiple
  subpaths, smooth control reflection. Quadratics become exact cubic handles.
- Affine matrix/translate/scale/rotate/skew transforms and hierarchy/paint order.
- Positive unitless/px viewport sizes or viewBox-derived size; nonzero viewBox
  origin; preserveAspectRatio none or xMidYMid meet (default).
- Solid opaque named or #RGB/#RRGGBB sRGB colors, none, inherited fill/stroke,
  fill/stroke opacity and width, nonzero/evenodd, object/Group opacity. Restricted
  inline style overrides presentation attributes. Stroke linecap butt/round/square,
  linejoin miter/round/bevel and finite unitless miterlimit 1–1000 are supported;
  explicit `inherit` is accepted for those three properties only.

Unsupported semantics reject the entire import:
text/images, gradients/patterns, use/links, masks/clips/filters, CSS stylesheets,
classes, variables, dashes, `initial`/`unset`/`revert`, `!important`, miter-clip,
unknown attributes/elements,
physical/percentage lengths, other aspect policies and foreign namespaces.
DTD, entity references, processing instructions, scripts and external resources
never execute or fetch. XML declaration/comments and predefined XML escapes are
ordinary parsing, not an extension mechanism.

Limits:1MiB encoded input,128 non-root drawable/group nodes,32 nested levels,
10000 parsed anchors,1000 generated Session commands (including lowered StrokeStyle
commands); existing model ranges and
native serialized limits still apply. This is synchronous bounded conversion.

Basic-shape lengths accept unitless/px values. Coordinates default to zero. Rect
corner radii and ellipse radii follow SVG2 missing/auto fallback; rectangle radii
clamp to half the corresponding size. Negative dimensions/radii and malformed
point lists reject. Zero-size rectangles/circles/ellipses and point lists with
fewer than two points are explicitly unsupported (not silently omitted).
Path arcs support absolute/relative endpoints, axis rotation, compact single-bit
flags, both sweeps/large arcs and proportional radius correction. Zero radius
becomes a line; coincident endpoints add no segment; negative radii use magnitude.
Curved shapes and path arcs use cubic spans of at most45 degrees, not exact conics or retained
shape generators. Normalized ellipse radial error is tested below5e-6; world
error scales with the radii and ancestor transforms. GUI/MCP/result disclose it.

Sources: [SVG2 basic shapes](https://www.w3.org/TR/SVG2/shapes.html),
[SVG2 paths](https://www.w3.org/TR/SVG2/paths.html),
[arc conversion](https://www.w3.org/TR/SVG/implnote.html#ArcImplementationNotes),
[coordinate systems](https://www.w3.org/TR/SVG2/coords.html),
[painting](https://www.w3.org/TR/2018/CR-SVG2-20180807/painting.html),
[structure](https://www.w3.org/TR/SVG2/struct.html),
[Qt stream reader](https://doc.qt.io/qt-6/qxmlstreamreader.html),
[Qt XML streaming](https://doc.qt.io/qt-6/xml-streaming.html).

## Stroke CP2 r4 acceptance — 2026-09-23

The exact task-owned r1/r2 fixture bytes were verified before implementation:
`build/stroke-cp2-r4-fixtures/` (r1 5597 bytes, SHA-256
`4b23b6f719a335155164786ff3738d277bd9e5ad2908e1b6870a33fa328dd1c1`; r2 5877
bytes, SHA-256 `fc586ecb77076b50d23933bf185ee679f409bea87503017c59ceac166e4e65ee`).
`svg_import_contract` executes the positive/negative cases against real Session
objects, including inheritance, inline precedence, no-painted-stroke handling,
exact 1000-command acceptance, 1001-command rejection and atomic refusal.
Non-default style import is also saved, reopened in a fresh Host and undone as one
transaction. This remains a strict static subset, not general CSS/SVG fidelity.
