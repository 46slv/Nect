# Curve-preserving font corner rounding: implementation contract

Status: design and read-only fixture audit, not a claim of implementation or full font parity.
Date: 2026-10-17. Scope: Corner Round over the existing Path cubic representation. No outline flattening, shaping-engine change, or global geometry refactor.

## Recommended next slice

Support regular sharp line–cubic and cubic–cubic junctions using exact retained cubic subcurves and a tangent-continuous cubic blend. Keep the existing line–line circular-arc path and its fingerprint tests unchanged. Do not modify nominally smooth joins. Unsupported/ambiguous geometry remains unchanged with a diagnostic, rather than a false success claim.

The curved-junction result is a G1 blend, **not generally a circular fillet**. Radius is the requested nominal corner scale; its conversion to endpoint trim distance agrees with the current line-only radius formula. If product requirements demand a true radius on curved edges, that is a different solver, described below. This distinction must appear in the node help and acceptance criteria.

Smallest useful implementation order:

1. Introduce immutable directed segment records and exact cubic split/trim utilities, with focused tests.
2. Make duplicate zero-length font connectors transparent to tangent lookup while preserving all original points when no edit occurs.
3. Classify regular sharp joins, implement bounded arc-length trims and tangent-triangle blends.
4. Resolve shared trims once per original segment; reconstruct all kept segment controls together.
5. Validate candidates conservatively against other curve pieces. Reject ambiguous candidates and expose skipped counts/reasons before advertising complete support.
6. Run synthetic geometry, real-font, serialization/export, then browser/native visual checks.

## Existing source seams

`src/corner-round-geometry.js` represents a directed segment from points p to q as:

- P0 = p.position
- P1 = p.position + p.handleB
- P2 = q.position + q.handleA
- P3 = q.position

A line is identified by BOTH incident handles being zero, not by one point's local handles. The current safe prerequisite rounds only line–line joins and clones untouched points/handles. Its exported `roundCorners(source, { radius, prefix })` need not change. Internally replace vertex-only assembly for curved cases with segment-first reconstruction. The existing line-only branch is retained for compatibility, not rewritten through approximate length operations.

Suggested internal records:

- Segment: original endpoint indexes, kind, original controls, bounded length data, accepted [t0,t1].
- Corner proposal: incoming/outgoing segment indexes, signed turn, desired trim lengths, accepted trim parameters, blend controls, status/reason.
- Diagnostics: rounded count, smooth count, skipped irregular/collision/precision-budget counts. Avoid changing the Path schema until the UI diagnostic route is agreed; helper tests can inspect an internal result.

Do not commit independently into files owned by the geometry/UI implementation worker. This document is the sole file of this design task.

## Degenerate font connectors and exact preservation

A zero-length segment is safely transparent only when all four controls are the same point (within the chosen numeric policy; exact equality is safest for the first slice). Coincident endpoints with nonzero controls may form a loop and must NEVER be removed by this rule.

For a constant segment p → q, the logical join has p's incoming handle and q's outgoing handle. Dropping p without transferring its incoming handle destroys the previous cubic. Scan through runs of constant segments with an explicit count bound, including the closing seam. Keep original point/index provenance in the logical view. When neither adjacent segment is edited, return original records unchanged; in particular, O must retain the same coordinates, handles, and point count.

Do not silently normalize arbitrary user paths merely to make an error disappear. Preserve current straight-only zero-length rejection tests for this slice. In a mixed curved/font contour, constant connectors may be bypassed in the logical view; all-constant contours remain untouched with a diagnostic. This compatibility rule is intentional and should be documented/tested, not inferred from contour provenance that the helper does not have.

## Robust tangent and corner classification

For a cubic's outgoing start tangent, use the first significant vector among P1−P0, P2−P0, P3−P0. For its incoming end tangent, use the first significant vector among P3−P2, P3−P1, P3−P0. These are limiting travel directions at endpoint-degenerate controls. An entirely constant cubic has no tangent. Use a local geometric scale and coordinate quantization tolerance, not a fixed length threshold tied to world origin.

After traversing constant connectors, normalize u and v and compute signed turn atan2(cross(u,v), dot(u,v)).

- Small angle: preserve join and all untrimmed geometry.
- Near π reversal: unsupported cusp; preserve, diagnose.
- Ordinary nonzero angle: candidate.
- Interior zero derivatives or tangent reversals in the proposed trim neighborhoods: reject the candidate in the first slice. Endpoint limiting directions do not establish regularity of the interior.

Current font coordinates require a noise-aware angular tolerance. The read-only fixture audit found O's nominally smooth joins deviating by up to 1.38e−6 radians. Using the existing 1e−12 line-angle threshold for font cubics would modify O incorrectly. Start curved-join classification with a separately documented 1e−4 radian smooth threshold, add scale/translation tests, and calibrate against the font coordinate quantization. This is a product precision policy, not proof that every smaller angle is intentional smoothness.

## Radius and bounded arc-length trims

Let θ be the signed original tangent turn. Desired distance on each side is d = radius × tan(|θ|/2). On curved segments this is arc length measured from the original vertex, not chord distance or Bézier parameter. The retained curve is unchanged outside those trimmed intervals. Radius is nominal: local curvature means this blend's actual curvature radius varies.

For each adjacent original segment with length L, cap its proposed endpoint trim to less than L/2 with a small scale-aware residual reserve. This conservative per-end budget prevents neighboring corners from consuming the same segment even if both are rounded. Do not cap each vertex independently to the full edge. Set final intervals from immutable original controls, and assert t0 ≤ t1 before construction. Avoid tiny residuals below numeric tolerance. The existing line-only half-edge equality behavior remains in its separate compatibility branch.

Compute length bounds by recursive de Casteljau subdivision: chord length is a lower bound; control-polygon length is an upper bound. Sum leaf bounds and stop when the global bound gap is below the stated tolerance. Maintain a bounded subdivision/work budget. Invert cumulative arc length using bracketing/bisection with these bounds. If a trim location cannot be resolved within its tolerance/budget, decline that candidate. An arbitrary sample LUT without an error bound is insufficient for a correctness claim. No subdivision points become output outline vertices unless they are actual trim boundaries.

Near-zero turns can underflow requested trims; near-reversals can overflow tan. Guard finite operations before multiplication and clamp from bounded lengths; treat unresolvable tiny changes as unchanged.

## Exact retained cubic splitting

At t, compute A=lerp(P0,P1,t), B=lerp(P1,P2,t), C=lerp(P2,P3,t), D=lerp(A,B,t), E=lerp(B,C,t), F=lerp(D,E,t).

Left controls: [P0,A,D,F]. Right controls: [F,E,C,P3]. To keep [a,b], first split at b and keep the left curve, then split that curve at a/b and keep its right curve (handle b=0 separately). This is an exact polynomial restriction up to floating-point arithmetic, not a fit. Tests compare evaluation of the retained curve at s to the original at a+(b−a)s.

Reconstruct relative handles from each retained segment's NEW controls at BOTH ends. Trimming only one end changes the opposite endpoint's handle length too under reparameterization, even though its position is unchanged. Therefore preserving an adjacent old handle when its segment was trimmed is wrong. Smooth joins need geometric/tangent preservation; literal handle equality is guaranteed only for segments not trimmed on either end.

## A bounded tangent-consistent blend

For accepted trim endpoints A (incoming side) and B (outgoing side), compute unit travel tangents u and v from the retained cubics. Solve the tangent rays:

A + a u = B − b v = T

a = cross(B−A,v) / cross(u,v)

b = cross(u,B−A) / cross(u,v)

Require a>0, b>0, stable non-negligible determinant, and trimmed tangent turn having the same sign as the original turn. Reject excessive tangent swing, opposite rays, or a ray intersection outside the local trim neighborhood. If a candidate fails, halve BOTH trim distances, recompute from original curves, and retry within an explicit geometry/work tolerance and budget. Never fall back to a chord with lost tangency.

Let φ be the absolute angle between u and v. Set h = (4/3) cos(φ/2)/(1+cos(φ/2)). The proposed bridge controls are:

[A, A + h a u, B − h b v, B]

For a symmetric line–line configuration, this is the standard cubic circular-arc approximation. For unequal a and b or curved input, it is a G1 cubic blend only. Since 0<h≤2/3, both controls are within triangle A,T,B. This bounds the blend's convex hull and gives a simple local non-looping geometry in the nondegenerate tangent-triangle case. It does NOT prove that the blend avoids other contours, that holes survive, or that curvature matches at the joins. G2/C2 continuity is out of scope.

Recompute blends after shared-segment limits are finalized. Do not average handles independently at shared points.

## Winding, counters, and collision safety

Use signed local turns; do not assume all outer contours are clockwise or negate the radius for holes. Keep contour order, closed flags, grouping, and fill rules. Rounding concave corners legitimately adds fill where convex rounding removes fill; both need tests. Open endpoints are never rounded; their positions and unused handles stay unchanged, while an incident retained segment's active handle may be recomputed by exact subdivision.

Adjacent trim caps only prevent overlap along the SAME original segment. They do not protect narrow counters, nearby stems, another glyph, or other contours. Checking only endpoint winding or sampled area is insufficient.

For the conservative slice, validate proposed bridge cubics against nonincident retained/source and proposed bridge cubics using bounding boxes/convex hulls with recursive cubic subdivision. Overlap unresolved at the geometric tolerance/work limit is treated as ambiguous and causes rejection, never presumed safe. Exclude only the intentional shared endpoint neighborhoods, not entire adjacent cubics. Checking the control triangle's interior against nearby geometry is a stronger conservative filter that may skip valid corners. A full implementation must also validate mutual edits together and reject affected candidates deterministically until no conflict remains. Use a spatial index and explicit total pair/subdivision budgets to avoid quadratic work on long text.

Containment/counter changes are not proved merely by absence of detected crossings: test winding/inside samples and near-collapse fixtures, and do not claim arbitrary topology preservation without a certified validator. Leave existing self-intersecting or overlapping font contours outside the first supported correctness contract. Do not run boolean cleanup that rewrites all outlines.

## Initial numerical/work limits (explicit, subject to fixture calibration)

- Keep current 10,000-contour and 100,000-input/output-point hard limits.
- Proposed geometry tolerance: max(1e−7 × local segment control-polygon scale, 32 × Number.EPSILON × maximum absolute coordinate, 1e−7 document units). Angular smooth threshold starts at 1e−4 radians. These values are initial engineering policy; validate over supported font sizes before freezing them.
- Length subdivision maximum depth 20; parameter bisection maximum 48 iterations; candidate shrink retries maximum 12, stopping sooner when the trim is below geometric tolerance. Length-bound evaluations share a 200,000-node budget per evaluation rather than multiplying a full budget by every corner.
- Collision subdivision maximum depth 20 and 200,000 tested box pairs per evaluation; use a broad-phase spatial index first. Any unresolved candidate at a cap is skipped with precision-budget/collision-ambiguity reason. Never accept because the budget expired.
- Reject cusp/reversal within 1e−4 radians of π; tangent-ray intersection whose denominator has magnitude below sin(1e−4); negative/non-finite ray lengths; and intersections farther from either trim endpoint than four times the larger accepted trim length. This last bound deliberately trades completeness for locality.
- Reserve at least four geometric tolerances of retained segment length when curved endpoint trims share a segment. If the segment cannot support that reserve, leave its candidate corners unchanged.
- Budget exhaustion must not throw away otherwise valid input geometry or generate partial malformed contours. Resolve plans before output, rollback rejected proposals, then serialize a coherent result. Keep line-only historical error behavior in its compatibility path.

These are bounded-runtime limits, not mathematical proofs or promises of full font coverage. Benchmark long text and pathological outlines; tighten the supported contract if bounds cannot be established at these limits.

## True curved-radius alternative (later, if required)

An actual tangent circle requires solving C = P(t)+σ r Np(t) = Q(s)+σ r Nq(s), with inward trim ranges and a consistent circle branch. Unknowns are t,s for a fixed r. Curved offsets can have singularities and multiple/no intersections; Newton convergence alone does not establish the right solution. A bounded root-isolation/branch-selection scheme and radius reduction policy are needed. After solving, use circular cubic approximation with a known tolerance and preserve the exact trimmed source cubics. Do not describe the tangent-triangle blend as this solver.

## Acceptance fixtures and gates

Read-only audit used the shipped DejaVuSans.ttf through makeFontAsset → Text → evaluate. Constant connectors were logically merged with both external handles preserved, then junctions were classified at 1e−4 radians. These are audit findings, not rounded-output tests:

| Text | Smooth joins | Line–line corners | Line–curve corners | Curve–curve corners |
| ---- | -----------: | ----------------: | -----------------: | ------------------: |
| O    |           16 |                 0 |                  0 |                   0 |
| B    |           18 |                 6 |                  0 |                   1 |
| HO   |           16 |                12 |                  0 |                   0 |
| S    |           20 |                 0 |                  8 |                   0 |
| a    |           21 |                 4 |                  3 |                   0 |
| g    |           23 |                 2 |                  4 |                   0 |

Required focused tests:

1. Existing rectangle/concave/open-line outputs and hashes unchanged; radius zero deep-independent clone; finite/budget validation unchanged.
2. O unchanged at multiple nonzero radii, with exact original point/handle arrays; HO rounds H and preserves O, groups, counters and contour order.
3. B no longer errors on constant connectors; its genuine curve–curve sharp join is rounded at a small safe radius; both counters stay open. S exercises real line–curve corners, and a/g exercise mixed joins. Do not substitute point-count assertions for geometric checks.
4. Add one redistributable pinned serif font with license and hash (for example DejaVu Serif from its official project). Test H/I serifs, B bowls, and a/g. The shipped Japanese fixture is synthetic and does not replace a real serif fixture. Test the existing TTF and OTF import routes too, without claiming all OpenType shaping support.
5. Synthetic straight–cubic, cubic–straight, cubic–cubic sharp corners, their reversed winding and concave versions; translated/scaled versions; closed seam; open ends; collapsed first/last handles; genuine internal cusp; coincident-endpoint loop; zero-length runs.
6. Trim evaluation equality over [a,b]; G1 join cross product near zero with positive tangent dot product; positive bridge control lengths; no finite-coordinate overflow; deterministic IDs; no source mutation.
7. Two corners sharing one short cubic, tiny segments, large radius, near-π reversal, acute tangent triangle, narrow O/B counters, neighboring glyphs, and explicit self-intersection inputs. Assert documented skip/failure behavior and diagnostic, not just absence of exceptions.
8. Source text/font/size changes, serialize/deserialize, undo/redo, SVG export, and direct Text→Corner Round versus Text→Text to Path→Corner Round agree geometrically.
9. Visual before/after at large zoom and export render for O/B/HO/S/serif. Browser and native render checks are later release gates, not performed by this design task.

## Primary references

- [FreeType glyph outline representation](https://freetype.org/freetype2/docs/glyphs/glyphs-2.html): glyphs have outer/inner contours with line, quadratic, and cubic segments. This supports testing counters and both curve import types.
- [FreeType outline conventions](https://freetype.org/freetype2/docs/glyphs/glyphs-6.html): decomposition details and off-curve representation. It does not certify this rounding algorithm.
- [Pomax, A Primer on Bézier Curves](https://pomax.github.io/BezierInfo-2/): de Casteljau subdivision and arc-length discussion; retained subcurves can be represented with cubic controls instead of output flattening.

The bridge formula, staged contract, and rejection policy here are design derivations for Nect. They require implementation and the above evidence before describing font support as complete.

## Implementation evidence — 2026-10-17 (append-only follow-on)

The design/history above is retained. The implemented conservative slice is in `src/corner-round-cubic.js`, routed by `corner-round-geometry.js`; straight-only inputs keep their historical branch and output hashes. This is regular local G1 blending, not true circular fillets or full font support.

### Implemented support and safety boundary

- Exact de Casteljau restrictions reconstruct BOTH active handles of every trimmed segment. Immutable source controls drive all trims. Constant connector runs are bypassed by original segment provenance, retaining both external handles; untouched contours retain all original geometry records. Coincident-endpoint nonconstant loops are never treated as constant connectors.
- Arc lengths use recursive chord/control-polygon bounds, depth 20, bounded bisection (48), at most 12 symmetric shrink attempts. Each endpoint gets less than half its original segment's lower-bound length, leaving four geometry tolerances. Shared evaluation budgets are 200,000 length nodes and 200,000 collision operations. Exhausted work is rejected, never accepted. Input/output point and contour limits remain enforced.
- Tangent-triangle blends require positive bounded rays, consistent turn, limited tangent swing, and derivative-control positive projection on each removed piece. The removed incoming/outgoing pieces must occupy opposite vertex half-planes. These are deliberately restrictive sufficient local conditions; a rejected candidate can shrink, otherwise it stays unchanged.
- Collision validation is stronger than bridge-only intersection sampling: the convex hull of BOTH removed source pieces and proposed bridge is a replacement region. Recursive control-hull separation proves this entire region disjoint from all nonincident source geometry, with exact single-endpoint contact permitted only for incident retained pieces. No adjacent cubic is wholesale excluded. An indexed broad phase bounds cell insertion/query work; mutually accepted replacement regions must be strictly disjoint. Line-only contours are historical rounded outputs before they become obstacles for mixed-contour edits. Unresolved overlap at tolerance/depth/work cap rejects the candidate.
- This local-clear-region contract preserves existing geometry outside each accepted region. It does not certify the global simplicity of arbitrary input, repair preexisting self-intersections, or provide exact-arithmetic topology proofs. Historical all-line rounding retains its earlier limitations. Nearby overlaps, cusps and uncertain cases may conservatively remain unrounded. No flattening or Boolean repair is applied to output.
- Helper output exposes `cornerDiagnostics` for mixed inputs (`rounded` counts only mixed-contour edits, `smooth`, and grouped `skipped` reasons). Core converts skipped counts into bounded visible warnings. Node help explains nominal-radius G1 behavior and limitations.

### Calibrated smooth threshold

The initial design's 1e-4 radians covers Sans numerical noise but not DejaVu Serif O: its outer contour has a near-smooth 0.00448693-radian (~0.257°) junction. Mixed contours therefore preserve turns below **0.005 radians (~0.287°)**. This is an explicit font precision policy, not a claim that all such turns are quantization noise; intentional tiny corners also remain unchanged. Both fonts' O retain exact point/handle arrays at sizes 48/96 and radii 0.25/2/24/1000. Straight-only classification is unchanged.

### Reproducible evidence

- `tests/corner-round-cubic.test.js`: exact subcurve evaluation, G1 line/cubic and cubic/cubic joins, mirrored/scaled/translated cases, closed seam, shared short segment, constant connectors, reversal/loops, crossing/narrow-counter rejection, shared budget exhaustion and source immutability.
- Focused geometry/core/text/DOM tests retain historical line fingerprints and command undo/redo; test actual B/S/a/g at 48/96 px and nominal radius 0.25/2, unchanged O, HO, groups, cold reopen, direct versus Text-to-Path route, and existing TTF/glyf plus OTF/CFF imports. B counter interior samples and winding are preserved at radius 24.
- `tests/fixtures/DejaVuSerif.ttf` is an unmodified, pinned real serif font with license/provenance in its adjacent README. It supplements, rather than replaces, the shipped Sans and synthetic Japanese fixtures.
- `node scripts/cubic-font-corners-example.js` creates the editable example, SVG and verification JSON. The matrix covers B/S/a/g/O/HO/HI × Sans/Serif × 48/96 px × radius 0.25/2/24. At these settings, all audited mixed sharp joins are accepted (27 Sans / 36 Serif for the combined phrase); O remains exact. Large requested radii are clamped/shrunk by geometric limits, so they do not imply a 24 px actual circular radius. Synthetic narrow-counter/crossing fixtures separately verify explicit rejection rather than forcing ordinary B to fail.
- `artifacts/cubic-font-corners.png` is a headless Inkscape rasterization of the actual exported SVG, visually reviewed for before/after comparisons and open counters. It is not Nect browser/native validation. Browser/native interaction checks and broader font/shaping support remain later gates.

Final focused gate: 37/37 tests passed across the five Corner Round files; one `npm run build` passed (87 modules, existing large-bundle warning only). No full-suite run and no Nect browser/native gate was performed. The regenerated labeled SVG raster was reviewed after the final build inputs were frozen.
