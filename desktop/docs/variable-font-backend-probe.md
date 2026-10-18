# Variable font backend: bounded executable feasibility probe

2026-10-17. **Final recommendation: use pinned fontkit 2.0.4 for the next variable-only, one-to-one-guarded adapter under unchanged CSP.** HarfBuzz is a proved future cluster-aware option; its WASM packaging/CSP gates are not necessary for this next slice. The first-stage HarfBuzz evidence below is followed by the successful pure-JS comparison at the end. Real TTF variable geometry and cluster records work in the isolated probe. This is not complete font support, an application integration, or browser/native acceptance.

## Delivered evidence

Artifacts: `/workspace/shared/nect-variable-font-probe/` contains source scripts, pinned isolated npm lock, unchanged licensed fixture, recorded output, sample commands, current Nect cubic contour JSON, and browser bundle evidence. Reproduction is in its README. Only this documentation file was added inside Nect; no source/package/lock/schema/parser changes.

- `harfbuzzjs` **1.6.3**, npm registry latest observed at execution; its `versionString()` is **HarfBuzz 14.6.0**. Package MIT license; preserve upstream notices when shipping. No runtime dependency installation scripts executed.
- Official Google Fonts Roboto variable TTF, **488,584 bytes**, SHA256 `d7598e12c5dbef095ff8272cfc55da0250bd07fbdecbac8a530b9b277872a134`. Unmodified bytes and full SIL OFL 1.1 notice retained; provenance has URLs and license hash. No system installation.
- Fixture has `glyf/gvar/fvar/avar/HVAR`, GSUB/GPOS/GDEF and fsType=0. Axes: wght 100/400/900, wdth 75/100/100. No CFF2, VVAR or MVAR, so none are demonstrated by this font.

`node probe.mjs` passes explicit assertions:

| Coordinates | “office AV” advance (font units) | Commands |
| --- | ---: | ---: |
| wght=100, wdth=100 | 7690 | 195 |
| wght=400, wdth=100 | 8135 | 195 |
| wght=900, wdth=100 | 8492 | 195 |
| wght=400, wdth=75 | 7171 | 195 |

Outline hashes at wght 100/900 differ independently of the coordinate metadata. Twenty new-instance reruns produce identical full record/outline hashes. Invalid/NaN/unknown axes and a one-command output budget reject. Independent fontTools 4.61.1 static instances give A advances 1223/1336/1395 at weights 100/400/900; HarfBuzz matches all three exactly (`verify-metrics.py`, `verify-metrics.mjs`). This oracle checks metrics, not every outline point or intermediate coordinate.

`ffi` becomes one glyph with UTF-16 source span [0,3). `x` + U+0301 becomes two glyphs sharing [0,2), the mark having x/y offsets 58/-10 and zero advance. The first attempted U+0305 fixture was missing a glyph in Roboto and correctly failed; it was replaced with a supported acute-mark test. No silent fallback was added. These are specific shaping proofs, not Unicode conformance.

`bridge.mjs` reuses Nect's installed opentype.js 1.3.4 and Paper 0.12.18. The output passes through the actual current command/SVG/Paper seam and yields **14 closed contours, 167 finite points**, with `position`, relative `handleA/handleB`, and deterministic contour/point IDs. No `cubicPath`-named function exists in the inspected snapshot: this is the current cubic path representation produced by `fromPaper` in core.js. The sample is in font units; production additionally applies size/unitsPerEm and layout transforms.

## Why this backend

The installed opentype.js 1.3.4 source reads fvar metadata but has no gvar outline-instancing implementation in its parser; Nect explicitly rejects fvar/CFF2 before parsing. Removing that guard or merely displaying axes would be wrong. Its existing per-glyph callback also lacks authoritative source clusters/full shaped positions.

The current [HarfBuzz JS API](https://github.com/harfbuzz/harfbuzzjs) provides `Font.setVariations`, shaped glyph info/positions, and drawing callbacks. Local installed type declarations and implementation, not historical examples, were used: v1 takes `Variation[]`, buffer `addText` uses UTF-16, and `drawGlyphOrFail` reports absent outlines. This permits **one exact face/coordinate object for both positioning and geometry**, avoiding mixed-engine variation disagreement.

[Fontkit](https://github.com/foliojs/fontkit) is a credible alternative with variation instances and layout/paths; registry 2.0.4 was observed (MIT, unpacked package 5,610,637 bytes). Its [current TTFFont source](https://github.com/foliojs/fontkit/blob/master/src/TTFFont.js) recognizes variable glyf/gvar and CFF2 routes. At this initial checkpoint it had not yet been installed or benchmarked; the subsequent pure-JS trial below supersedes that limitation and the initial selection. HarfBuzz was initially chosen for this smallest executable test because its explicit cluster and draw API directly addresses both missing seams, not because Fontkit was proven incorrect. Package unpacked size is not a browser bundle comparison.

## Bounds, lifecycle and packaging

Probe limits: 20 MB input fixture check, 10,000 UTF-16 input units, 20,000 output glyph check, 500,000 emitted commands, finite bounded coordinates, known finite axes within advertised ranges. Drawing callbacks receive M/L/C/Z; omitting the quadratic callback lets HarfBuzz convert quadratics to cubics. No SVG-string parsing or unbounded intermediate `glyphToJson` is needed before the command cap.

Important: output checks cannot cap work already performed inside synchronous WASM, including malformed-font parsing, shaping or callback work. This probe is **not an untrusted-font security boundary**. Production must retain independent directory/fsType validation and unsupported-format guards, run the backend in a terminable worker, cap decoded bytes/glyphs/commands/cache entries and isolate timeout/cancellation. Reuse the document's 8-face/32 MB budgets; include font-byte identity, face index, canonical coordinates, engine/version, script/language/direction/features in cache identity. No proposed numeric timeout is claimed validated by this tiny corpus.

The installed wrapper uses `FinalizationRegistry` and exposes no public deterministic destroy/dispose in its declarations. Avoid creating one DrawFuncs per glyph in production as this simple probe does; reuse bounded callback state and prove worker teardown/restart and cache eviction. Worker lifetime is the practical hard reclamation boundary pending API review. Do not call undocumented pointers to free objects, which risks double destruction.

Observed Node 24.19 run: import/WASM startup ~12 ms; first short run ~12 ms, subsequent cases ~1.4–1.8 ms; 20 repeated short runs ~0.86–4.59 ms. These are cloud-host observations, not latency/memory guarantees, tail percentiles, maximum-font benchmarks, or desktop/browser measurements.

Package runtime assets: index.mjs 82,693 bytes; harfbuzz.js 31,763; harfbuzz.wasm 441,359. A minimized ESM test bundle is **42,158 JS + 441,359 WASM = 483,517 bytes**, gzip separately **11,935 + 178,262 = 190,197 bytes**. Excludes the fixture. harfbuzz-subset.wasm (667,183 bytes) is unnecessary and must not be included for this feature.

Default esbuild browser bundling fails on Emscripten's guarded dynamic `import("module")`. An explicit `--external:module` build succeeds; the Node branch remains guarded. The configured bundle imports and reports 14.6.0 in Node. This is **build proof only**, not execution proof in a browser. The WASM URL is relative to import.meta.url, so copy a versioned local WASM next to the entry or supply a verified Vite asset strategy. Test production Vite/Tauri offline loading, top-level-await/ES2022 support, CSP WebAssembly permission, workers, and stale-result handling without weakening CSP. No browser GUI or native test was run.

## Smallest next integration slice

1. Add a private bounded shaped-run adapter plus fixture tests; keep legacy documents on legacy outputs. Inputs: exact bytes/face/axes, UTF-16 source span, explicit script/language/direction/features. Outputs: glyph IDs, source cluster spans, x/y advances/offsets, outline command ranges and instance identity. Do not persist glyph IDs as logical character identity.
2. Establish worker bootstrap/lifecycle, offline production browser/CLI parity and cancellation first. Use the single font instance for shapes and outlines. Preserve existing export geometry and disable unsafe cluster-interior edits explicitly.
3. Enable only opt-in monochrome glyf/gvar variable TTF after named-instance, intermediate/custom-axis, malformed-variable-table, composite glyph, persistence/undo/preview, and legacy-equality tests pass. Preserve original bytes; keep CFF2 blocked until independently exercised. Fallback, RTL paragraph reordering and Japanese vertical stay separate staged coverage.

Essential remaining decisions: versioned legacy-vs-new layout selection for old documents; worker integration with today's synchronous evaluation/CLI interface; durable axis schema and feature defaults; cluster-interior caret/edit policy; cache/timeout/resource envelope and production CSP packaging. These engineering choices need an integration packet, not another broad audit.

No complete bidi algorithm, script itemization, UAX #29 segmentation, paragraph line breaking, fallback selection, Japanese vertical metrics/variation, CFF2, color fonts, malformed-font corpus or cross-platform numeric equality was tested. HarfBuzz shaping alone cannot supply those product contracts. CFF2 requires its own licensed fixture, blend/metrics oracle and bounds before a support claim.

## App-specific gate review and narrowed integration recommendation

Read-only follow-up inspected `src-tauri/tauri.conf.json`, core.js, fonts.js, main.js and cli/export.js. **The above configured bundle is an experiment, not approved production packaging.**

### Actual CSP blocker; separate permission required

Tauri currently sets `default-src 'self'; script-src 'self'` with no `wasm-unsafe-eval`. core.js explicitly documents keeping script-src free of unsafe-eval. Standard CSP therefore blocks WebAssembly compilation ([MDN script-src](https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Headers/Content-Security-Policy/script-src)). Actual packaged-host enforcement remains untested, so do not infer a host exception. A local worker does not authorize circumventing this policy.

Adding the narrower `wasm-unsafe-eval` would authorize WASM compilation while leaving JavaScript eval prohibited, but it is still a security-policy change requiring separate action-time user approval before editing that configuration. No such change was made or assumed approved. Do not add broad `unsafe-eval`, remote loaders/CDNs, blob exceptions, or a less-restricted worker to evade the existing policy. Until that decision, continue the adapter design/probe or evaluate a pure-JS alternative; do not advertise variable fonts in the app.

### Browser packaging is still a gate, not solved by externals

`--external:module` merely permits bundling: the resulting file retains `import("module")` and the Node createRequire/fs branch. Emscripten's environment guard normally skips it in browsers, but this is **not acceptable as the final browser-only runtime** under the no-hidden-Node-fallback requirement. A further `--define:process=undefined` trial also retained the dead guarded import; files/logs record this rather than claiming removal.

Required next packaging proof: upstream-supported browser-only Emscripten build/loader, or a small audited deterministic build-time transform of the pinned loader that removes Node branches (with source integrity tests). Produce separate CLI and browser artifacts, assert the browser artifact has no Node imports/createRequire/fs fallback, and execute it under the intended production CSP after any necessary approval. Do not use a fake Node-module shim or rely on runtime failure of the branch. Existing bundling byte counts remain feasibility measurements, not final ship sizes.

### Smallest variable-only slice, without global shaping migration

Keep all current static font assets and default font on the identical opentype.js/layout path, including existing metrics, kerning, IDs and exports. Add an explicit variable-only prepared-face adapter (glyf/gvar only) and typed authored coordinate record. Legacy schema documents never opt into a new shaper by inference. Retain static `parseFont` unchanged; add a separate validated variable preparation function rather than relaxing its reject condition in place.

Initially limit variable layout to the same verified one-to-one mapping eligibility as existing horizontal Japanese/Latin paths, and do not broaden accepted scripts/marks/format controls. Use nominal glyph mapping and bounded per-pair positioning to reproduce the current simple model, or shape an explicit run then reject it unless each resulting glyph maps exactly to the corresponding original scalar/cmap glyph with no unsupported reordering, multi-scalar clusters or offsets. Do not silently turn off font features merely to force a mapping. Full ligatures/mark positioning demonstrated here remain backend evidence for a later cluster-editing stage, not a reason to bypass today's guards. Variable vertical, wrap/path, rich-range transitions should each stay disabled until tested rather than inheriting compatibility accidentally.

Both geometry and metrics for a variable instance must come from the same backend/coordinates; the output adapter can expose the subset of current font/glyph methods needed by the admitted path. Persist source bytes and coordinates, not sampled glyphs as a substitute for a real font. Existing safe edit suspension stays authoritative. A UI weight slider is last, after import/roundtrip/undo/preview and strict cache invalidation tests.

### Async initialization versus synchronous parse/evaluate

main.js already fetches the bundled font asynchronously, then synchronously parses it and rerenders. `makeFontAsset`, `validateFontAssets`, `resolveFonts`, Session operations and core export/evaluate are synchronous. CLI uses top-level await for filesystem reads but calls deserialize/export synchronously. Simply adding `await import('harfbuzzjs')` inside parse/evaluate cannot preserve these contracts.

Two levels must remain distinct:

1. A single lazy `ensureVariableBackendReady()` promise initializes the engine before any variable import/open/axis preview is committed. Static editing remains available; loading/error are explicit. CLI awaits it before variable-document validation/evaluation. Synchronous variable adapters return a typed NOT_READY error if called early; they never produce a default/static substitute. Initialization readiness alone permits synchronous calls but does **not** solve cancellation or hostile-input blocking on the UI thread.
2. For production worker isolation, async preparation must create the exact immutable run/glyph/metric records required by the intended document revision before synchronous evaluation uses them. A ready-cache is keyed by the full text/font/axis/features/style/layout inputs. On edits, prepare the candidate revision in the worker; commit/preview it atomically only after success and freshness verification. Sync evaluation is a deterministic pure consumer, with a typed PREPARATION_REQUIRED result on a miss, never blocking RPC or an implicit Promise. CLI can await the same preflight before export. Worker failure/stale results preserve the previous valid revision and report failure.

The second option is more than initialization wiring and requires a deliberately scoped preparation gate around load/import/text/axis mutations and export. If that cost is rejected, a main-thread initialized adapter is only a bounded trusted-fixture prototype, not the promised resource-safe general-font implementation. Decide this interface before touching the application parser or UI.


## Final comparison: pure-JS fontkit succeeds under the unchanged policy

Following the app gate review, installed **fontkit 2.0.4** only in the isolated probe and exercised the exact same official Roboto fixture. Package exports explicitly select `dist/browser-module.mjs` for browser ESM; esbuild metadata confirms that entry was selected. Browser build command used `--platform=browser --format=esm --target=es2022 --minify` with **no external-module exceptions, no Node shims and no dependency source edits**.

`browser-build/fontkit.mjs` is **365,930 bytes; 149,085 gzip**. Its metafile records zero external imports. Source/bundle checks find no Node imports, createRequire/fs loader, eval or Function construction. This package bundles its browser-compatible binary utilities; it requires no host Buffer global. The exact bundle was evaluated via `vm.SourceTextModule` in a fresh browser-like realm, with external imports rejected, no process/require/Buffer/fetch/DOM globals, WebAssembly explicitly undefined, and both string and WASM code generation disabled. That is stronger than a successful build but is still not a real browser/CSP/Tauri execution test.

`node --experimental-vm-modules fontkit-probe.mjs` passes:
- Real outline differences at weights 100/900, variable total advances 7690/8135/8492 at 100/400/900, and width-75 total 7171.000000000001.
- Independent fontTools A advances exactly 1223/1336/1395.
- Twenty identical instance result reruns; invalid axes/NaN/range rejection; bounded adapter output rejection.
- Current Nect OpenType.Path/SVG/Paper conversion: **14 closed contours / 167 finite cubic points** (`fontkit-cubic-path.json`).

The fontkit path input has 185 commands versus HarfBuzz's 195, yet the bridge gives the same contour/point counts. This is not a proof of point-for-point equality between engines. Fontkit emits fractional advances (for example ffi 1748.0000000000002), whereas HarfBuzz returns integral scaled positions here. Pin the chosen backend and canonical serialization/rounding policy; do not switch engines opportunistically or promise identical outputs across them. Old static outputs remain on the existing backend.

The browser-like realm measured ~35 ms module preparation/evaluation and ~101 ms for the whole validation batch including four cases, twenty repeat instances, mark/ligature and negative checks. Earlier run was ~42/~98 ms. These are small-corpus host measurements, not production percentiles or per-keystroke budgets. Output size/count and timeout policy still need adversarial/large-font tests.

### Why this is the narrower next choice

Fontkit instances expose variation-aware glyph paths/advanceWidth and layout positions, so they can implement the variable-only adapter without WASM initialization or a CSP change. Keep the previous section's static path, accepted-script/one-to-one editing guards and typed preparation gates. An ordinary lazy ESM import is the only async engine initialization; after module readiness its instance and path APIs are synchronous. Browser loading can occur during asynchronous import/open/preflight, with explicit ready/error state; CLI can await the same adapter initialization before deserialize/validation. Pure JS does not remove the need for worker isolation and a preparation cache if hostile-font interruption/resource containment is part of the production contract.

Fontkit `GlyphRun` publicly exposes glyphs/positions; each glyph has codePoints, **not HarfBuzz-style authoritative source offsets**. The probe records ffi substitution and x+acute mark offsets but does not manufacture generic clusters by accumulating codePoints, especially with repeated characters, substitution/reordering or normalization. Therefore do not select Fontkit alone as proven complete cluster-safe shaping. It is a viable variable-outline backend for the existing one-to-one scope; explicit source-cluster reconstruction/engine choice remains a later checkpoint.

Path extraction also differs: `glyph.path.commands` materializes the glyph before the adapter can cap emitted commands. The output cap does not prevent that allocation. Preserve input/table/glyph bounds and use worker timeout/teardown; no claim of parser safety follows from the fixture. CFF2 code exists in this package but remains untested and must stay blocked in the first integration.

**No security-policy approval is needed for this demonstrated pure-JS route, and no CSP change is proposed for the next slice.** Real production bundle/offline/CSP and worker host acceptance still precede shipping. If a future cluster stage chooses HarfBuzz, separately resolve the browser-only loader and explicitly approve the minimal WASM policy change described above; do not bundle those changes into ordinary font development.

Reproduction additions: `fontkit-check.mjs` is a host-independent assertion body; `fontkit-probe.mjs` runs the no-Node/no-codegen realm; `fontkit-results.json`, `fontkit-run.log`, `fontkit-browser-meta.json`, `fontkit-sample-outline.json`, `fontkit-cubic-path.json` retain the evidence. Run `node bridge.mjs fontkit-sample-outline.json fontkit-cubic-path.json` for the cubic bridge.

## Ready-to-assign integration contract (fontkit 2.0.4)

- **Dependency/import:** propose exact `fontkit: "2.0.4"` in the future app package, with its lockfile integrity, MIT license and bundled third-party notices. Use the documented `import { create } from 'fontkit'` from a browser-targeted adapter; package conditional exports resolve to `dist/browser-module.mjs`. Do not import `src/node.js`, open/openSync or the Node `dist/module.mjs` in the renderer. Keep Node file IO in the CLI boundary. Add a build assertion that the renderer uses the browser entry and has zero Node builtins/external loader imports. This probe changed only its own package/lock, not the app's.
- **Font bytes:** retain one immutable full-byte asset per existing byte deduplication rule. Sliders never instantiate a new embedded asset or subset. Derive axis metadata/named-instance display metadata from the validated asset; names are not identity. Cache parsed base fonts separately from ephemeral instances.
- **Authored axis model:** introduce an explicitly versioned sparse per-Text coordinate object tied to its fontAsset ID; a future selected-range override may reuse the same object shape. Do not assign a production field name/schema number in this probe. Permit known axis tags only, finite values within the advertised range, maximum axis count from bounded validated metadata. Omitted axes mean the font's documented defaults; canonicalize by sorting tags and dropping explicit default values for persistence/cache purposes. If a face changes, reject or intentionally reset incompatible coordinates in one authorized command, never reinterpret them silently. Named instances resolve to actual numeric coordinates at authoring time.
- **Instance/cache identity:** base key = immutable full-byte identity + face index + backend/version. Instance key = base key + complete canonical effective coordinates; do not key by family/name/weight class. Run keys additionally include exact source/normalization revision, allowed feature settings, kerning/direction/script/language and all metric-affecting style inputs. Retain bounded instance/run caches; frequent previews should evict superseded coordinates and never grow stored project bytes. Commit one authored-coordinate history change on acceptance; cancel restores the prior coordinates/geometry.
- **Units:** fontkit outlines, advanceWidth and GPOS x/y positions are font design units. Divide by that face's unitsPerEm then multiply by authored font size, followed by existing range width/layout transforms. In the Y-down document, flip glyph Y and yOffset/yAdvance consistently; do not flip X. The provided sample already flips Y but intentionally leaves font-unit scale. M/L/C/Z command values feed the existing OpenType.Path-to-Paper conversion; handles in final contours are relative to their anchor. Kerning/advance/tracking must be applied once, not duplicated between fontkit layout and legacy pair logic. Pick a single adapter policy before tests.
- **First acceptance:** real glyf/gvar min/default/max/intermediate and width values; composite glyphs; bounded metadata/axes; font bytes stable across repeated previews; exact cancel/undo/redo; persistence/reopen and CLI output equality at identical instance keys; unchanged static corpus geometry; one-to-one rejection retained; no implicit fallback. Browser production bundle plus offline WebView2 execution under unchanged CSP remains mandatory host acceptance. The VM proof is not WebView2 or GUI evidence.
- **Deferred:** CFF2, variable vertical/VVAR/MVAR, arbitrary complex clusters/RTL/bidi, automatic fallback, WOFF decoders and collection import. Do not relax their guards because fontkit recognizes the formats.

The next packet can implement the adapter and pure tests first, with separate ownership for persistence/GUI/host tests. No application changes were made by this research packet.
