# Private variable-font preparation prerequisite

2026-10-17. Implemented with exactly pinned `fontkit@2.0.4`, pure JavaScript.
This code is intentionally **not imported by the app UI, static parser, core evaluation,
CLI, persistence or schema**. Existing static-font rejection and geometry remain unchanged.
No CSP, permissions, WASM, Node loader shim, or main-thread parsing fallback was added.

## API and boundary

- `src/variable-font/prepare.js`: `createVariablePreparer()` owns copied, SHA-256-identified
  assets. `await register(Uint8Array)` returns an asset ID. Original caller bytes may be
  mutated afterward; the owned bytes never escape or enter a transfer list.
- `setRevision(nonnegativeInteger)` rejects/terminates a pending different revision.
  `await prepare(assetId, {text, axes}, {signal})` returns a deeply frozen run.
  `read(assetId, {text, axes})` is synchronous, returning a prepared exact-input record or
  `PREPARATION_REQUIRED`. It never parses or sends synchronous worker RPC.
- `releaseWorker()` releases an idle worker and its backend caches, retaining ready records.
  `dispose()` rejects work and clears assets/results. Consumers must dispose at document
  close and release idle workers before another preparer can acquire the global worker slot.
- `worker.js` isolates all fontkit parse, layout and path materialization. Production uses
  a local module Worker URL; injected test transports do not establish browser acceptance.
  A matching request ID and revision are required; obsolete messages are ignored. Timeout,
  cancellation, stale revision, backend failure and worker error terminate the worker.
- `backend.js` is worker-only. Direct calls exist only in focused trusted-fixture tests;
  do not import it into rendering or synchronous evaluation. A different byte identity
  resets its single parsed-face cache. Instances/runs share the base's private byte source.
- `cubic-path.js` bridges **prepared output only** through the existing OpenType.Path →
  Paper seam, emitting closed contours with position and relative handleA/handleB. Output
  commands already flip font Y, remain in font design units, and include shaped x advances
  once. Future callers scale by size/unitsPerEm and must not add legacy kerning again.

## Deliberately narrow eligibility

Only sfnt TrueType with glyf, gvar and fvar, editable/installable embedding rights,
validated directories, loca and horizontal metrics is admitted. No collection/WOFF/CFF2,
color/bitmap, VVAR/MVAR, vertical, RTL, wrapping, fallback or arbitrary cluster path.
The fvar axis and named-instance coordinate records are bounds-checked. Named-instance
names are exposed as bounded display labels by the opt-in inspector, never used as identity. See [named-instance UI bounds](variable-font-ui.md#named-instance-metadata-bounds). Axis values must be finite, known and in range;
effective coordinates are sorted, and explicit defaults disappear from sparse coordinates.

Source eligibility is ASCII printable plus selected precomposed Latin ranges, with fixed
latn/default-language/ltr/default features. No feature toggles are accepted to force mapping.
Even inside that range, layout must preserve exact glyph count and order, each nominal
cmap glyph ID and exactly its single input code point, and zero offsets/y advances.
Thus `ffi` ligatures and combining marks reject. The admitted UTF-16 spans are the verified
one-code-unit source positions; they are **not reconstructed generic clusters**. Repeated
letters and precomposed composite `é` are exercised. Some otherwise valid Latin inputs may
reject intentionally. No claim of complete Latin or Unicode shaping is made.

Identity contains backend/version/policy, exact byte hash, face zero, canonical effective
axes and text plus fixed script/language/direction/features. Parent ready-cache lookup is
exact authored input (sorted axis entries), so semantically equivalent explicit-default
requests can have separate bounded entries; worker instance/run identity canonicalizes them.
No production axis schema or persistent field was introduced.

## Resource envelope and remaining security limits

- Parent: at most 8 assets / 32 MB total; each at most 20 MB. Concurrent registration
  reserves count/bytes before asynchronous hashing. SHA-256 deduplicates identical bytes.
- One active Worker across preparer instances, one in-flight request per preparer; callers
  receive BUSY/WORKER_LIMIT rather than an unbounded queue.
- Worker: one asset, at most 16 axes, 4,096 font glyphs, 1,024 UTF-16 source units;
  8 variation instances and 8 runs, 100,000 cached commands.
- Each result: 50,000 commands and at most 4 MB serialized UTF-8. Parent cache likewise
  at most 8 records / 100,000 commands. Coordinates/advances must be finite and bounded.
- Default deadline 5 seconds; callers may shorten, never raise it. This is an engineering
  ceiling, not a validated latency SLO. Worker termination is the reclamation boundary.
- Structured clone intentionally copies, never detaches shared source assets. Peak memory
  includes caller bytes, owned bytes, hashing/transport copies and worker font internals.
  Count/byte/output limits are **not a hard worker heap quota**. Fontkit materializes a path
  before output checks and may allocate heavily inside synchronous decoding/layout. Workers
  make interruption possible; an allocation spike before termination remains possible.
  Hostile-font corpus/fuzzing and real host memory/timeout behavior remain release gates.
- No exception text from font parsing is exposed; unknown backend failures become
  BACKEND_FAILURE. The previous valid document stays the future consumer's responsibility.

## Evidence and release gates

`node --test tests/variable-font.test.js tests/fonts.test.js tests/font-metadata.test.js`
checks real 100/400/900/550 weights, width 75, composite/repeated text, deterministic runs,
fontTools-derived A advances 1223/1336/1395, canonical axes, invalid metadata/rights,
unsupported mappings, text/command caps, worker cancellation/deadline/staleness, immutable
byte ownership, cache miss/cleanup, unchanged static variable rejection and static tests.
The browser bundle test resolves `fontkit/dist/browser-module.mjs`, with no external imports,
Node builtins or dynamic code generation. It tests the actual worker entry with esbuild.
The ordinary application web build remains separate and does not ship this unexposed code.

No native build, browser GUI, WebView2/CSP/offline run, full test suite, undo/redo/import
roundtrip or application async-mutation gate was tested here. Production Vite worker asset
integration and execution under unchanged CSP remain required before exposure. Fontkit
browser conditional-export resolution is build evidence, not a real WebView acceptance.

Fixture: `tests/fixtures/variable-font/Roboto-variable.ttf`, official Google Fonts unmodified
488,584-byte file, SHA256 d7598e12c5dbef095ff8272cfc55da0250bd07fbdecbac8a530b9b277872a134.
OFL and source provenance are alongside it. Runtime/embedded dependency terms and integrity
inventory are in `docs/licenses/variable-font/`; release packaging must carry those notices,
including documented upstream packages that omit full LICENSE files. No font installation.

Recorded checkpoint: **29/29 focused tests passed**, followed by one successful
`npm run build` (Vite 7.3.7, 89 modules). The existing >500 kB main-chunk advisory
remains; the private adapter is not in that main chunk. No other test/build category
is implied. Review regressions cover stale worker errors, Node Buffer copy semantics,
invalid-axis cache bypass and rejection of padded fvar tags that fontkit would trim.

### Notice audit follow-up: explicit MIT declarations, packaging review

Exact-source audits retain fontkit 2.0.4
(fbf3b9ef21eebd219eb73e666faed573af0fba09), dfa 1.2.0
(3f5e603e5082caedabefefdf53e8a6ca3111e966), and brotli 1.3.3
(e36051345f6d27a56e5f39ea70d2f789b8574046) MIT declarations, pinned source
inventories and provenance. A complete project-level notice was absent; this does
not invalidate the explicit MIT declaration or establish a legal prohibition.
Synthetic MIT files were removed. npm supports SPDX identifiers, and SPDX provides
canonical MIT terms; those source links are in the notice README. Final application
bundling still needs to carry declared terms and available exact attribution and
embedded notices. This scoped packaging review does not stop development.
The exact Google Brotli vendor LICENSE was recovered at its pinned submodule
commit; it is not attributed to the JavaScript port's authors.
Only notices and this evidence section changed during the follow-up. No additional
build or runtime acceptance is implied.
