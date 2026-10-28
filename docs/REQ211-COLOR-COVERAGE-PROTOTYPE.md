# REQ-211: visible Color contribution foundation

## Scope and source

This is a pure-core prototype, not an integrated color panel or a completed REQ-211.
The accepted design handoff is the user's 2026-10-28 08:41 UTC message:
https://chatgpt.com/c/6ad20278-6900-83ec-88b2-8dd4822df88e?messageId=e3407c42-6b6d-446e-a4b6-3b2c0c907bf9

The v1.1 mock and current REQ-211 specify visible contribution percentages for
stable C-slot IDs, with unlinked colors shown separately as Other. Coverage is a
soft input to Auto Chroma/Tone, not a prescribed 60/30/10 composition quota.
https://app.notion.com/p/3fbfd279a6f381a9a7c9c36f5089fba1

## Implemented boundary

`measureColorCoverage(slotIds, layers, pixelCount)` consumes normalized alpha
masks in document paint order (back to front). It accounts for occlusion using
source-over visibility. Transparent canvas contributes no painted mass; an
explicit background object does. Missing/unlinked Color references are Other.
No RGB comparison or equal-HEX deduplication occurs. Integer percentages use
largest remainders and stable-ID tie breaking; a nonempty painted result totals
100, and a fully transparent result has zero percentages.

Inputs are never mutated. Invalid IDs, duplicate IDs, nonfinite/out-of-range
alpha and mismatched buffers reject before accounting, including invalid masks
hidden by opaque objects. The prototype bounds samples to 262144 pixels and 256
layers/slots. These are explicit implementation budgets, not promised support
for arbitrary-sized project renders.

## Not implemented

- No import into the production editor, command Session, persisted project schema,
  or UI. Existing assets/workspace PR #106 is a separate frozen workstream.
- No document-level Global/Named Color reference store yet.
- No real renderer extraction, sampling accuracy/performance acceptance, gradient,
  blend, mask, filter or raster attribution contract. The caller must not label
  unsupported rendering as measured coverage merely by feeding guessed masks.
- No Auto Chroma, palette resolver, gamut/contrast guard, or aesthetic guarantee.
- No native Windows validation and no production acceptance claim.

## Next bounded integration

First establish renderer-to-mask identity and opacity semantics on simple
source-over Fill/Stroke/Text fixtures, then compare sampled contribution against
known analytic scenes and visible output. Keep stable slot identity separate
from resolved display colors. Unsupported attribution must remain explicit.
Only then bind the result to authored Color references and the panel, through
typed atomic commands and existing Undo/save/reopen contracts.

## Verification

`node --test desktop/tests/color-coverage.test.js` covers transparency, order,
partial opacity, Other, stable IDs, integer rounding, nonmutation and rejection.
The seven focused cases pass; this is unit evidence, not a browser or real
artboard pixel measurement.

## Renderer bridge checkpoint

`colorCoveragePasses` is an experimental, unimported adapter using the production
`renderElement` implementation. Tests feed real `exportScene` Rectangle/Fill/Stroke
outputs. The adapter preserves scene order, splits fill before stroke, bounds the
pass count, and clips SVG viewports to the artboard. Caller bindings must be explicit
stable provenance; absent bindings remain Other. It does not derive identity from
HEX, create authored Color references, or rasterize pixels yet.

Groups, masks, gradients, images and repeater appearance currently reject the whole
measurement. This is intentionally narrower than the renderer. Twelve focused tests
pass, but actual raster alpha comparison and native/UI integration remain unverified.

SVG width/height are document-space bounds, not allocation dimensions. A rasterizer
must independently downsample to the 262144-pixel budget before allocating buffers.

## Authored shared-color foundation (schema 54)

The desktop document now supports shared `colorSlots` with stable IDs, names and
literal `{hex, alpha}` values. Color parameters bind using a sibling `colorSlotId`;
Color runtime values remain unchanged. Typed create/rename/set/bind/unbind commands
use existing atomic Session history. Unbinding freezes the current resolved value.
Registry changes invalidate evaluation caches. No palette editor UI is added yet.

Composition projections preserve the root registry. SVG replacement preserves it,
and presets validate their bindings against it. Both composition-import paths copy
only referenced slots and remap occupied IDs even if the values match, rather than
retargeting an imported reference to the destination's color. Migration reserves
slot IDs before allocating a legacy Composition ID. Invalid/dangling/conflicting
bindings reject, including inactive compositions and preset graphs. Aliases,
expressions in the slot registry and deletion commands are outside this first slice.

Independent review exposed registry loss on SVG import, missing preset validation,
and silent foreign-import retargeting. These were repaired with regressions. The
19 shared-color tests and related composition/preset/coverage suites total 70 PASS;
production build passed. New exact-head CI is required after publication. Existing
PR #107's four-fixture browser pixel proof applies to the previous isolated adapter,
not automatic authored provenance or a completed panel. Those remain next work.

## Authored paint provenance bridge

The evaluator now keeps paint slot provenance in an evaluator-owned WeakMap and
exports an optional scene-entry sidecar. Exact shared-slot bindings survive direct
Fill/Stroke and Color-node source chains; literals and computed expressions stay
Other. Paint replacement clears the replaced identity, while cloning and supported
bypass routes preserve actual input provenance. Metadata never enters Color values,
serialized geometry or legacy fingerprint preimages. Geometry routes that do not
preserve exact provenance remain conservatively unattributed; unsupported grouped,
masked and repeater coverage still rejects rather than fabricating percentages.

The adapter prefers the authored sidecar over fixture-only maps. The browser pixel
fixture now creates schema-54 bound colors and calls the adapter without supplied
identity guesses. That changed browser fixture is NOT RUN until its exact-head CI.
Seven coverage-pass tests and 90 existing bypass/fingerprint tests passed in the
independent review; a related 53-test local run passed. This is not panel acceptance.
