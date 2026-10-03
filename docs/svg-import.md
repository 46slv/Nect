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
- Solid named and #RGB/#RRGGBB sRGB colors, CSS alpha-last #RGBA/#RRGGBBAA,
  transparent (zero-alpha black), numeric rgb()/rgba(), none, inherited fill/stroke,
  fill/stroke opacity and width, nonzero/evenodd, object/Group opacity. Restricted
  inline style overrides presentation attributes. Stroke linecap butt/round/square,
  linejoin miter/round/bevel and finite unitless miterlimit 1–1000 are supported;
  explicit `inherit` is accepted for those three stroke properties.
- Inherited `color` on svg/groups/shapes, defaulting to opaque black; fill/stroke
  `currentColor` resolves against the same element's final color, including inline
  style overrides. `color:currentColor` and `color:inherit` retain the inherited
  color. Color uses the qualified solid sRGB subset below, including alpha.
- Internal `url(#id)` linear/centered radial gradients in `defs`, with explicit
  `gradientUnits="userSpaceOnUse"`, finite unitless/px x1/y1/x2/y2 (linear) or
  cx/cy/r (radial), pad spread and sRGB interpolation. This covers the native
  gradient exporter subset and forward references. Radial focal attributes
  fx/fy/fr must be omitted, retaining the centered, zero focal-radius defaults.
- Linear `objectBoundingBox` gradients, including omitted gradientUnits and the
  SVG default vector (0,0) to (1,0). Coordinates accept finite unitless fractions
  or percentages, including fractions outside [0,1]; px/other units refuse.
  Each paint uses its target's local geometric box, including cubic extrema and
  zero-length subpaths, excluding stroke width and ancestor/object transforms.

Color functions accept legacy comma-separated RGB channels with matching numeric
or percentage units and optional alpha, or modern whitespace-separated channels
with independent numeric/percentage units and optional slash alpha. Alpha accepts
a number or percentage and multiplies fill/stroke opacity; object/Group opacity
stays separate. Modern components must be finite and in range: RGB 0..255 or
0..100%, alpha 0..1 or 0..100%; values outside that subset reject without clamping.
The existing legacy comma behavior retains CSS clamping within its finite 1e7
numeric safety bound. Both forms reject missing (`none`) components, expressions,
relative colors, comments and mixed separators. Color syntax follows the bounded
subset of [CSS Color 4](https://www.w3.org/TR/css-color-4/#rgb-functions).

Static `currentColor` becomes editable literal RGBA; its live linkage to `color`
is lost. Inherited fill/stroke keywords resolve using each descendant's color,
not the ancestor's color. Color alpha multiplies fill/stroke opacity once;
object/Group opacity stays separate. Same-element declaration order does not
change resolution. Unsupported color declarations refuse, including overwritten
inline declarations, rather than applying CSS invalid-value fallback. Gradient
stop `currentColor` and color declarations on defs/gradients/stops remain
unsupported: their paint-server inheritance is outside this slice.
See [CSS currentcolor resolution](https://www.w3.org/TR/css-color-4/#resolving-other-colors)
and [SVG color](https://www.w3.org/TR/SVG2/painting.html#ColorProperty).

Gradient stops use the qualified solid colors above and independent stop opacity;
color alpha multiplies stop opacity, then native paint alpha applies fill/stroke
opacity once. Object/Group opacity remains separate. Definitions are limited to
128; each editable native gradient requires 2..64 strictly increasing offsets in
[0,1] (number or percentage) and an endpoint distance greater than 1e-9. The pinned
native model cannot preserve 65..256 stops or coincident hard edges, so those cases
explicitly refuse. Coordinates, stop offsets and RGBA remain editable native
Scalars. Each paint receives its own gradient and stop IDs: shared SVG definitions
are cloned, so linked-edit sharing is lost. Gradients are not flattened or sampled.
Radial start is (cx,cy), with a canonical native end of (cx+r,cy); the exporter
does not retain the original native endpoint direction. Radius must exceed 1e-9;
the canonical endpoint must remain within the 1e7 coordinate bound and preserve
radius to relative error at most 1e-12, otherwise intake refuses.
See [SVG paint servers](https://www.w3.org/TR/SVG2/pservers.html).

Bbox linear gradients become editable local native endpoint literals; changes to
the geometry box no longer automatically remap them. On a non-square box, the
conversion preserves gradient color planes using the inverse scale of the vector
normal, rather than simply scaling both endpoints. The native end can therefore
differ from the mapped SVG end while preserving the same linear paint function.
Shared definitions still clone per paint, with each target resolved separately.
Bounds use the existing cubic-extrema geometry API. Empty/zero-width/zero-height
boxes, bbox radial gradients and bbox use on SVG arc paths explicitly refuse;
arc lowering is approximate and cannot supply the exact source arc box here.
Converted endpoints retain the 1e7 bound and 1e-12 relative vector precision.
See [SVG object bounds](https://www.w3.org/TR/SVG2/coords.html#BoundingBoxes)
and [bbox units](https://www.w3.org/TR/SVG2/coords.html#ObjectBoundingBox).

Unsupported semantics reject the entire import:
text/images, objectBoundingBox radial gradients, explicit radial focal attributes,
gradient transforms/inheritance,
repeat/reflect spread, alternate interpolation, patterns, external paint URLs,
use/links, masks/clips/filters, CSS stylesheets,
classes, variables, dashes, `initial`/`unset`/`revert`, `!important`, miter-clip,
unknown attributes/elements,
physical/percentage geometry or userSpaceOnUse lengths, other aspect policies and foreign namespaces.
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
