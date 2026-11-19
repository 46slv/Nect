# Blend: explicit expansion

REQ-138 / #302 requires intentional conversion to independent editable geometry,
not a geometry-only live Convert to Path operation that drops appearance.

`expand-blend` is a CommandSession command. It evaluates the selected active
Blend, preserves source A/B graphs, replaces that Blend's identity with an
ordered painted Join, and adds editable Multi Path plus solid Fill/Stroke nodes
for each evaluated copy. Ordinary edits remain live; only explicit expansion
breaks the Blend dependency. One Undo restores the exact authored Blend graph.
The existing schema and nodes are reused; no baked proprietary geometry format.

Expansion rejects locked/bypassed nodes, parameter references which would lose
their meaning, blocked export appearance, and unsupported or excessive output.
It validates a private document candidate before committing. Initial acceptance
must cover appearance, source independence, point editing, Undo/Redo and reopen.

## Creating a live Blend from selection

`blend-selected` takes exactly two stable node IDs in selection order (A then B),
validates their live Blend output, and adds one connected Blend in one History
entry. The graph toolbar exposes this action for two selected nodes. Existing
source nodes and Output wiring are preserved, just like ordinary Add node; the
new Blend is selected for preview, and the user can connect it to Output. Invalid
paint/topology or missing sources reject the entire command without a partial node.

## Bounded acceptance measurements

On 2026-11-17, Node 24.14.1 on the development Windows machine, the bounded
`node scripts/benchmark-blend.mjs` probe measured five calls after one warm-up:
4-point input / 100 Steps: median 0.697 ms, max 1.273 ms; 128-point input /
100 Steps (13,056 output points): median 10.829 ms, max 16.846 ms. This is the
synthetic geometry/paint evaluator only, not Session, rendering, IPC, native GUI
responsiveness or a universal timing guarantee. The benchmark is deliberately
not a timing-sensitive CI assertion and does not run automatically.

CommandSession regressions separately cover Steps 0/1/5/100, deterministic
re-evaluation, source edits and reopen, plus source deletion diagnostic,
Undo recovery and explicit disconnect/reconnect without stale geometry.

## Editable example

Open [live-blend.json](../examples/live-blend.json) through Project → Open JSON.
The [short guide](../examples/live-blend.md) explains editing the connected sources and Steps;
[live-blend.svg](../examples/live-blend.svg) is its initial vector appearance.
This reusable example does not replace pending native-window acceptance.

## Specified Distance: bounded follow-on design

REQ-138 (Confirmed, Should) and issue #302 permit staged spacing support.
[Adobe's definition](https://helpx.adobe.com/illustrator/desktop/manage-colors/apply-transparency-and-blending/blend-options-overview.html)
measures corresponding edges, not the empty gap between shapes. Its public page
does not specify rounding, numeric limits or a general deformation metric.

This Nect stage supports two identical closed contours related by translation,
after the existing seam/winding correspondence. Relative Bezier handles must
match; scaling and morphing remain available in Steps, not Distance. Colors may
still interpolate. Distance uses evaluated document coordinates (document px),
not screen pixels or stroke bounds. It is a **maximum interval**: for translation
length L and positive finite d, use max(1, ceil(L/d)) equal intervals, retaining
both endpoints. At most 255 intervals and the existing 100000-point budget are
allowed; overflow diagnoses rather than truncates. Coincident inputs retain the
two endpoints. These rounding and scope choices are Nect's, not claimed Adobe
numeric parity. Spine, Smooth, rotation and multiple sources are not this stage.

The typed Blend declaration adds distance and Steps/Distance operations. Legacy
generic Boolean operations migrate to Steps; Steps values and old documents retain their meaning.
Distance does not overwrite Steps. Author edits use existing CommandSession
parameter/operation commands and History; computed copy count is never authored.
Codec roundtrip, Undo/Redo, source edits, SVG and explicit expansion must preserve
this distinction. The inspector must state the translation-only limitation.
Invalid or unsupported inputs produce diagnostics, not stale output or partial
commands. Native GUI acceptance remains separate from headless regression.

The evaluated coordinate space is the common Composition/document space, before
viewport zoom. For n intervals the actual spacing is L/n and there are n-1
intermediate shapes. This is not a stroke-outline gap. All anchors and relative
Bezier handles must match after seam/winding alignment; closed status and segment
count must match. Every corresponding control therefore undergoes the same
translation. Comparison uses an absolute 1e-9 document-unit tolerance, not a
coordinate-magnitude-relative tolerance that could hide a large deformation at
large coordinates. Differences lost to floating representation cannot be recovered;
representable large/small cases are tested, and uncertain mismatches diagnose.
Source edits that violate this condition produce an error, never a silent switch
to Steps. The user can explicitly select Steps to continue morphing.

Legacy generic Boolean operation tokens were ignored by Blend. Reading them
normalizes to Steps and supplies the new distance default; this is semantic and
rendering compatibility, not a promise of byte-identical reserialization.

Composition projects can retain legacy operation tokens without passing through
legacy-document migration. The typed registry therefore resolves the four old
Boolean tokens as Steps aliases for interface/evaluation, without mutating those
projects or exposing aliases in the inspector operation menu. Direct binding
calls accept the same tokens. Unknown tokens remain errors.

## Spine Steps: bounded implementation contract

REQ-138's Confirmed/Should acceptance explicitly includes a blend spine and allows
stages. [Adobe's spine documentation](https://helpx.adobe.com/illustrator/desktop/manage-colors/apply-transparency-and-blending/modify-spine-of-blended-objects.html)
explains editing/replacing a path to control blend alignment and direction.
[Its orientation options](https://helpx.adobe.com/illustrator/desktop/manage-colors/apply-transparency-and-blending/blend-options-overview.html)
distinguish page alignment from tangent alignment. These references do not define
Nect's pivot, numeric sampling or compatibility contract below.

### Contract

- Add an explicit `spine-steps` operation and a typed Path `spine` input. Existing
  Steps and Distance remain unchanged and do not implicitly enable a spine.
  Spine Steps requires the input; disconnect/deletion diagnoses rather than
  silently reverting to the straight blend. Switching explicitly to ordinary
  Steps or Distance restores those operations, with the dormant spine wire kept.
- Keep the current two supported painted closed source contours, Steps 0..254,
  geometry/paint interpolation and output budget. The spine is exactly one open
  contour, 2..512 finite anchors with finite relative Bezier handles, positive
  finite length. Reject closed, multi-contour, degenerate and unsupported grouped
  paths. Straight segments and cubic curves are supported; the spine is not drawn.
- All three evaluated inputs share Composition/document coordinates, before zoom.
  Let C_A and C_B be the centers of the source contours' geometric curve bounds
  (not stroke bounds, screen bounds or mean of authored vertices). At t=i/(steps+1),
  interpolate geometry/solid paint as now, then translate each generated contour
  by P(t)-((1-t)C_A+t\*C_B). P(t) is the spine's point at fraction t of its arc length.
  The first and last source pivots therefore land on the spine's exact start/end;
  authored source nodes remain unchanged. Their original positions are not kept
  in the generated output. Unequal shapes still morph; no rotation, scaling or
  tangent alignment is applied by this placement stage.
- Reuse the established Paper adapter (`toPaper`, curve `bounds`, `length`,
  `getPointAt`) already used by Resample Path; create no new sampling subsystem.
  Use exact authored start/end positions for endpoint samples. Interior arc-length
  locations inherit Paper's numeric approximation, not an exact analytic-length
  promise. Reject non-finite/missing samples. Remove temporary Paper items in
  finally blocks. A bound of 512 spine anchors times at most 256 samples keeps
  sampling work below Resample's existing 2000000-work ceiling.
- Orientation is fixed: preserve page-space source orientation and relative
  handles. Tangent-following and Distance along a spine are separate explicit
  operations (see below). Smooth and multi-source blends remain out of scope.
  UI must name Spine Steps and state the open-path/fixed-orientation limit.
- Stable generated identities follow the existing Blend copy order. Expansion
  uses the final translated evaluation, preserving appearance. Session connect,
  parameter/operation edits, Undo/Redo and project readback use existing commands.
  Add the new input's default only on applicable legacy validation; preserve old
  Composition projects and Boolean aliases without requiring fixture rewrites.

### Focused acceptance before integration

Straight spine with matching endpoints must agree with ordinary Steps; a cubic
spine must place intermediate pivots at the reused sampler's locations. Cover
asymmetric source pivots, Steps 0, source/spine edits, fixed orientation/handles,
invalid closed/empty/zero-length/multiple contours, finite/budget limits, missing
and deleted wire diagnostics, switching modes, old Steps/Distance/legacy aliases,
Session Undo/Redo, codec/SVG and Expand appearance equivalence. Do not substitute
headless success for native GUI acceptance; GUI remains on hold.

The intermediate pivot is the virtual reference lerp(C_A,C_B), not generally the
intermediate morphed shape's actual bounds center. Only endpoint bounds centers
are guaranteed to coincide with spine endpoints. Inactive spine connections are
retained on explicit Steps/Distance selection and are visibly labeled inactive;
there is no automatic mode switch or connection removal.

## Spine tangent orientation: bounded contract

REQ-138 explicitly adopts Orientation and staged implementation. This stage adds
an explicit `spine-tangent-steps` operation beside the existing fixed Spine Steps.
Ordinary Steps, Distance, old aliases and fixed Spine Steps retain their meaning.
It is not a native-window acceptance or a claim of exact Illustrator parity.

[Adobe's orientation definition](https://helpx.adobe.com/illustrator/desktop/manage-colors/apply-transparency-and-blending/blend-options-overview.html)
distinguishes page and path alignment. Nect fixes the following rotation reference:
page +X maps to the directed spine tangent, so page +Y maps to its perpendicular.
In document coordinates Y increases downwards; positive rotation is clockwise.
A horizontal +X spine is the identity; an upward spine rotates by -pi/2.

For each existing arc-length sample t, retain the virtual pivot
C(t)=(1-t)C_A+t*C_B and rotate the interpolated contour by
p'=P(t)+R(theta(t))*(p-C(t)), with theta=atan2(tangent.y,tangent.x).
Rotate both relative Bezier handles by R without translation. This includes both
endpoints: their geometric shape and paint are retained, but their output position
and orientation change. Authored sources and their coordinates are never mutated.
The virtual intermediate pivot is not generally the morphed contour's bounds center.
Solid paint/stroke widths, generated IDs, export blockers and count budgets retain
the existing Blend contracts. A rotation does not introduce scaling.

Use the same Paper path, length and `getLocationAt` as the existing spine sampler.
Read the directed tangent at that curve location. Zero, missing or non-finite
sample tangents diagnose; never guess an angle or silently use the fixed mode.
Paper's endpoint limiting tangent handles an ordinary straight segment whose
Bezier handles are zero. Interior stationary points without a tangent reject.
For a location at curve time 0 or 1 at an interior join, compare the two adjacent
curves' directed endpoint tangents. Reject zero/missing/non-finite tangents and
joins whose normalized vector components differ by more than 1e-10. This is a
named numerical direction tolerance, not an offset into the path. No epsilon
position probes or smoothing through corners are performed. Curve locations and
endpoint classification inherit Paper's numerical conventions. A corner not
sampled by the selected Steps count is not itself a rejected contour.

The first/last positions still use exact authored spine endpoint anchors. At
those ends only the one available directed tangent is required. Closed, multiple,
zero-length or unsupported spines remain rejected. UI names the new tangent mode
and explains its open-spine/sample-corner limitation. Changing to fixed Spine Steps
is an explicit recovery choice and preserves the spine connection.

Acceptance covers horizontal/upward/reversed and curved spines, rotated anchors
and both handles, endpoint/virtual pivots, sampled corners and stationary points,
unchanged sources/paint/blockers, edit diagnostics and mode recovery, Session
Undo/Redo, codec roundtrip and expanded SVG equivalence. Native interaction stays
unverified while the GUI hold remains active.

As with fixed Spine Steps, an arc-length fraction means Paper's computed length
and location, not an analytic arc-length solution. In particular, a symmetric
cusp is not guaranteed to be sampled at its mathematical midpoint. The tangent
checks apply to the actual returned curve location; they are not a whole-spine
proof that no cusp exists between samples. Explicit mode edits may retain a
diagnostic graph, as existing Blend modes do; expansion refuses invalid output
atomically, and Undo or an explicit fixed-mode change restores a valid result.

## Spine Distance: bounded fixed-orientation contract

REQ-138 / #302 extends the existing open-spine mode with an explicit
`spine-distance` operation. Its active inputs are A/B, Distance and Spine.
In computed Paper arc length L > 0 and finite positive maximum interval d
(document px), use n = max(1, ceil(L/d)) intervals, n+1 samples including
the exact authored spine ends, and n-1 intermediate shapes. Equal interval
length L/n is measured along the spine, not as a chord. At most 255 intervals
and the existing 100000-point geometry budget apply. Floating evaluation uses
Paper's numerical length; no tolerance-based rounding or analytic guarantee.

Spine Distance uses the same virtual pivot, fixed page-space orientation,
interpolation, source validation, endpoint paint and blocker propagation as
Spine Steps. Unlike straight Distance, it accepts unequal morphable sources
because interval count depends only on spine length. Open-path, finite bounds,
sampling and budget failures diagnose without fallback or mutating inputs.
Existing operations, Steps values and dormant wires are preserved. Verify
interval counts, exact endpoint placement, mode/parameter edits and Undo,
reopen, SVG and expansion equivalence headlessly; native GUI stays unverified.

## Smooth Color: bounded contrast-based spacing

The explicit `smooth-color` operation activates A and B only. Determine sample
count after the same solid paint compatibility checks as Steps. For each
matching fill or stroke color, use its premultiplied linear-light sRGB channels
and alpha. D is the largest absolute component difference across matching
paint slots (0 to 1). Use n = max(1, ceil(32D)) equal parameter intervals,
including both endpoints (2 to 33 shapes). The existing 100000-point geometry
budget still diagnoses rather than truncates. Geometry, paint interpolation,
authored endpoint colors and export blockers follow the existing Steps rules.

This spacing responds to paint color and opacity only, not shape, width or
distance. Equal premultiplied colors (including differing hidden RGB at zero
alpha) generate just the endpoints. Use Steps or Distance for geometric
spacing. This is not a perceptual guarantee after hex quantization or
Illustrator numerical parity. Other Blend modes retain their authored values.
Spine is inactive in this mode. Native-window acceptance is separate.

[Adobe's description](https://helpx.adobe.com/illustrator/desktop/manage-colors/apply-transparency-and-blending/blend-options-overview.html)
describes automatic spacing based on color transitions or distance depending on
the source paint, not a numeric algorithm. The fixed 1/32 resolution (up to
32 intervals) is specific to Nect. Equal-color sources do not fall back to
distance in this stage.

## Three-source Steps: isolated first slice

REQ-138's saved “三つのsourceでのSteps” section authorizes an explicit
`three-source-steps` operation with A → B → C. Steps s is the intermediate count
per adjacent span. Reuse the existing pair interpolation; output B once, hence
2s+3 shapes. The existing maximum of 256 output shapes permits integer s=0..126
(255 shapes at 126); 127 is rejected, never clamped. The combined output obeys
the existing 100000-point budget, not a separate budget per span. All three
sources must have compatible existing single-contour solid appearance. Failure
of either span rejects the entire evaluation, without mutation or partial output.

C is active only in this mode; prior operations and the two-selection shortcut
retain their semantics. Legacy documents missing C gain a null inactive default;
a three-source document without C is rejected. Existing source IDs and A/B order
are not rewritten. No Spine, Distance or Smooth combination, four-plus sources,
or native GUI acceptance is included. Output uses independent span ID namespaces.
This is an isolated prototype, not released or integrated.
