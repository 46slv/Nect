# Variable-font document preparation bridge

2026-10-17. This packet connects the private pinned fontkit 2.0.4 worker to the
synchronous document/evaluation path. It does not change CSP, introduce a fontkit
main-thread fallback, or migrate static geometry to a new shaping engine.

## Stored contract

Only explicitly opted-in schema 25 documents may contain variable assets. New
ordinary/static documents still use schema 24; schema 1–24 static migrations and
parsing remain on their prior route.

- `fontAssets[id]`: existing `name`, `fileName`, and full base64 `data`, with
  `kind: "variable-glyf-v1"`. No copied per-instance font bytes or generated outlines.
- Variable Text: existing `font: id`, plus
  `variation: {version: 1, fontId: id, axes: {wght: 550}}`.
- `axes` is an explicit object of known finite, in-range coordinates. Validation
  sorts tags and omits values equal to the face default. Missing axis entries use
  that validated face's defaults, but missing variation/axes contracts are rejected.
  Font changes cannot silently reinterpret coordinates. Static switching explicitly
  clears variation through `set-variable-font` with `axes: null`.
- Assets retain the existing 8-file, 20 MB/file, 32 MB/project font limits and
  editable/installable embedding-right checks. Original source bytes remain stable
  through instance previews, history, and JSON. Metadata validation is bounded
  directory/fvar/rights validation, not main-thread fontkit shaping or parsing.

The SHA-256 asset identity, pinned backend/policy, canonical full coordinate set,
exact source text and fixed script/language/features determine worker run identity.
Node and character identities remain authored identities; glyph IDs are not persisted
as replacements. Position and size do not enter design-unit run identity.

## Open / standalone export

Import `createVariableDocumentPreparer` from `src/core.js`.

1. `deserialize(json)` structurally validates bytes, rights, schema and authored
   coordinates. It does not prepare variable outlines.
2. `const bridge = createVariableDocumentPreparer({workerFactory?, timeout?})`.
   The default is the local module worker; Node supplies a real worker-thread adapter.
3. `const ticket = await bridge.prepareDocument(doc, {signal?})`.
4. `bridge.preparedDocument(ticket)` and `bridge.preparedFonts(ticket)` provide
   the frozen validated document and immutable prepared descriptors for a preview.
5. `bridge.activate(ticket)` publishes the prepared bank after the caller verifies
   its own open-request freshness. Construct the Session only then. Pass
   `fonts: () => ({default: bundledDefault, ...bridge.fonts()})`.
6. Export with the same merged font map. `bridge.fonts()` does not provide the bundled
   default font. Keep old-document/runtime state until successful new open activation.

A ticket is opaque and only the latest ticket from that bridge is valid. Activation
is synchronous, freshness checked, and single use. `cancel()` invalidates only the
candidate. `dispose()` closes the document runtime and invalidates its fonts.
A finished worker is released, allowing another document preparer to acquire the
single global worker slot. A failed or canceled replacement never drops the active
bank. Callers must not retain unlimited borrowed preview font maps/snapshots.

## Atomic Session commands

Existing `dispatch` remains synchronous. It never performs worker RPC, imports
fontkit, or accepts a cache miss as permission to use default or previous geometry.
Synchronous commits and history transitions check exact variable readiness.
`evaluate` returns `errorCode: "PREPARATION_REQUIRED"` on a miss; export and bake
reject. Static evaluation retains its prior return shape and geometry.

Use `await session.prepareCommand(envelope, bridge, {signal?})` followed by
`session.commitPrepared(token)`. The envelope uses existing session ID and expected
revision. Supported asynchronous intents are:

- `set-text-content` with the existing text/selection/input intent payload
- `set-text-settings` with existing settings (unsupported variable modes reject)
- `set-variable-font`: `{layerId,nodeId,fontId,axes,asset?: {id,asset}}`
- `undo`, `redo` (prepare the target history document before replacing current state)

`makeVariableFontAsset(ArrayBuffer, fileName, existingAssets)` in `fonts.js` creates
an immutable validated candidate asset. It does not prepare or commit it. Adopt it
with `set-variable-font` and `axes: {}` for defaults, or explicit numeric axes.
Repeated exact bytes deduplicate; slider changes do not create assets.

`session.preparedSnapshot(token)` gives a detached document plus ready font map for
preview. `session.cancelPrepared(bridge)` cancels it. Session/revision checks, active
edit guards, layer/node locks, downstream bounded-geometry checks and final ready-bank
checks precede commit. A stale token cannot overwrite a newer synchronous edit.
Commit is one history operation; preview does not mutate history. Text draft support,
when available, must preserve its exact draft version rather than replaying guessed
selection edits. Do not mutate an active runtime behind the Session's font callback.

## Deliberately supported layout

Whole horizontal, single-line, verified one-to-one Text uses the exact prepared
Y-down design-unit commands. The bridge multiplies by size/unitsPerEm, applies
Position, optional whole-text tracking and alignment, and uses the existing
OpenType.Path → Paper core → relative cubic contour seam. Shaped x advances are
applied once. There are no synthetic `forEachGlyph`/`getPath` implementations and no
second legacy kerning pass. Units can be reused for Position/size/tracking/alignment.

The backend's original printable ASCII/precomposed Latin eligibility and exact
nominal glyph/count/order/zero-offset guard remain authoritative. `ffi`, combining
marks, controls, RTL and complex mappings reject; there is no feature-disable trick,
normalization guess, fallback face or stale default instance. CFF2 remains rejected.

Variable-specific diagnostics include `VARIABLE_VERTICAL_UNSUPPORTED`,
`VARIABLE_WRAP_UNSUPPORTED`, `VARIABLE_PATH_UNSUPPORTED`,
`VARIABLE_RANGE_UNSUPPORTED`, `VARIABLE_ARRANGEMENT_UNSUPPORTED`, and
`VARIABLE_FEATURES_UNSUPPORTED` (kerning-off is not the fixed prepared policy).
Character identity-only records survive, but rich styles/offsets/transforms/pair edits
are not admitted. Character layout reports an explicit whole-Text-only reason; this
is not a suspended invalid document. Valid variable outlines can be baked exactly
through the existing Multi Path command; unprepared bake fails. These restrictions
apply only to variable Text, not existing static vertical/wrap/rich/arrangement support.

## Residency and bounds

Active and candidate banks reference immutable ready results, never deep-copy them.
Each bank admits at most 8 distinct runs, 100,000 commands and 32,000,000 serialized
UTF-8 record bytes (8 × private 4 MB per-result cap). At most one candidate bank is
retained; replacement, failure, abort-after-ready and cancel discard it. The active
bank is pinned until successful activation, including after full private-cache eviction.

The unchanged private parent ready cache additionally has 8 runs / 100,000 commands.
A conservative aggregate upper bound is therefore 24 records, 300,000 commands and
96 MB serialized record size across active, candidate and private cache; identical
records are shared references and not charged as extra physical copies. Bank stats
report active/candidate runs, commands and serialized bytes. Temporary in-flight
worker transport/output is bounded separately by the existing private adapter.

The owned source-byte store is at most 8 assets/32 MB for the preparer's lifetime.
Removing assets from authored JSON does not reclaim that store until disposal; open a
new prepared document runtime if replacing its whole asset set. Document/history
base64 strings, bounded metadata caches, transport/hash copies, JavaScript object
overhead, and worker font internals are additional residency. Serialized-byte bounds
are not heap quotas; hostile-font worker allocation and real host timeout/memory
behavior remain release gates. See the private adapter integration document.

## Evidence and remaining gates

Focused command (no full suite or application build in this packet):

`node --test tests/variable-font-document.test.js tests/variable-font.test.js tests/fonts.test.js tests/font-metadata.test.js tests/text-outline-baking.test.js tests/text-session.test.js tests/command-session.test.js`

65/65 passed at this checkpoint. Real Roboto weights 100/400/900/550 and width 75,
independent expected A advances 1223/1336/1395, canonical defaults, immutable bytes,
JSON prepared reopen, history, exact preview/bake geometry, full-cache replacement
cancel/failure with unchanged active export, unknown axes/mapping, stale commit,
abort-after-ready bank release, overlapping-candidate freshness, logical character-ID preservation, synchronous miss, static switch, and locks are covered.
Existing focused static byte/geometry/rejection, text Session and baking tests pass.
Private worker tests retain malformed/right/resource/cancellation/deadline and browser
conditional-export evidence. This packet does not claim every static corpus was run.

UI and CLI integration are coordinated separately. Production Vite worker inclusion,
one app web build, offline browser/WebView2 under unchanged CSP, hostile input testing,
IME/desktop interaction and native packaging remain separate acceptance gates. No
native build, full test suite, browser GUI, dependency change or CSP/security-setting
change was performed here. Pinned notices and the release-bundling notice review
remain in `docs/licenses/variable-font/`; no license-absence prohibition is asserted.
