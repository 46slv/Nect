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
