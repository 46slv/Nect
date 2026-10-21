# 2D Repeater odd-copy solid fill

Base: PR58 `codex/repeater-alternate-scale`, verified remote
`b90898c16d1350d8a31b3c3cb459ccc5342ef106`. No desktop Repeater Color operation or
equivalent alternate fill exists. Geometry Repeater accepts unpainted Path only.
It already carries per-copy appearance metadata (`repeaterOpacity`) through its
allowed Transform/paint route; shared rendering splits independent contour groups.
Extend this existing carrier with one optional derived `repeaterFill` Color per
odd copy. No source-wide schema, new paint tree or renderer overhaul is needed.

Add ae-2d **Alternate Fill**, defaultOFF, and **Alternate Color**, using the common
typed Color field/parser/reference/validation. Default Color is the existing Fill
default #36434e/alpha1. Copy index is the existing zero-based stable copy identity,
independent of Offset, paint-order reversal and Alternate Scale. Keep point/contour
IDs, geometry, source, stroke, copy opacity/order and placement unchanged.

The optional Color replaces only the final existing solid fill of odd copies1,3,5…
when each copy group is rendered. No fill remains none. Linear/Radial gradients
remain gradients; unsupported patterns retain their existing rejection. In a
mixed painted composition, only solid leaves originating from this Repeater are
affected; geometry Repeater still refuses painted input. The field hint states
these limits. This is an explicit final-solid-fill override: the last downstream
Fill sets the shared base, while enabled odd copies use Alternate Color. Later
gradient paint suppresses this override. Do not silently change that precedence.
Alpha is the replacement Color alpha; existing copy/group opacity wraps fill and
stroke exactly once. Do not multiply Color alpha by copy opacity in metadata.

DefaultOFF emits no new contour metadata and preserves existing SVG bytes. Old
documents backfill disabled/default descriptors without a version bump. Authored
flag/color variation or bindings require schema46; only Geometry Repeater gains
two parameters. Color expressions retain their existing base/ref/mix semantics,
type/missing-target/cycle errors and atomic Session history. No alternate parser.

Verify OFF equivalence against PR58, odd/even IDs/source/geometry, combined scale,
offset/order/alpha, none/gradient/mixed composition, Transform propagation,
Undo/Redo/JSON/SVG, Color dependency/errors and representative native Canvas.
Build and seal a new candidate while preserving prior outputs. Physical input,
OS dialogs/cold reopen and general paint/gradient redesign remain outside scope.
