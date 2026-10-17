# Font support: evidence and implementation contract

Status: investigation and proposed staged contract, 2026-10-17. Broad font support includes real weights/styles, variable fonts, Japanese vertical typography, shaping and fallback; corner rounding is a separate geometry capability. This document does not claim universal font or Unicode support.

## Current implementation checkpoint — 2026-10-17 13:41 UTC

Source checkpoint `961e0feb421f538f31c1cde1ca752ff2155ba820` integrates the following bounded parts of this plan:

- F1: actual static family/style/weight metadata and selection, including source-level native catalog metadata. Focused static style tests passed; native Rust compilation remains unverified.
- F3: explicit schema-25 `glyf`/`gvar` variable assets and axis coordinates, ready-only synchronous evaluation, atomic asynchronous preparation/history/open, experimental manual import/axis controls, and cold-process CLI export. Existing static documents retain schema 24 and their parser. Named-instance UI is not implemented.
- Variable support is whole horizontal single-line simple Latin with verified one-to-one mapping. It does not establish F2 cluster-safe shaping or F5 vertical completeness. Unsupported mappings, CFF2, variable vertical/wrap/path/rich-character/maintained-arrangement layouts reject explicitly.
- Real licensed Roboto variable and static style fixtures now exist. Exact provenance and notices live in `tests/fixtures/variable-font` and `docs/licenses/variable-font`; earlier missing-fixture observations below describe the original investigation.
- Integration evidence: core/document 65, UI 4 and CLI 5 focused checks passed, followed by one production build (94 modules). Emitted local Worker: 369,128 bytes. No CSP changes, no main-thread fontkit fallback. Real browser/WebView/IME/native package acceptance remains open; serialized output limits are not hard heap quotas.

See [document bridge](variable-font-document-bridge.md), [UI scope](variable-font-ui.md), [CLI](variable-font-cli.md), and [static style selection](font-style-selection.md) for current contracts. The matrix and staged implementation discussion below retain the original investigation as historical evidence; their “missing” labels are not current for these completed slices.

## Original investigation evidence boundary

Inspected the supplied `desktop` source snapshot; this directory has no usable Git metadata, so a commit identity was not independently verified. Reference file SHA-256 values at inspection:

- `src/fonts.js`: `f7bb133a3982ab2ad5bd8b4592c02085703dda377f9449149aa00128d04b194c`
- `src/text-layout.js`: `df12fa5d04cedf419ebbe09e9f47557310f24e337f6fb308c4eb03100cdb0152`
- `src-tauri/src/system_fonts.rs`: `7cfeee2d5c0fbda6efb706a34e5687a6b9d82eb2260cb5f7d381a369a887d198`

Executed only focused Node checks: `node --test tests/fonts.test.js tests/typography-spacing.test.js tests/rich-text.test.js tests/text-wrap.test.js tests/pair-kerning.test.js`: **66 passed, 0 failed**. No full suite, browser, native Windows, system-font installation or font download was performed. Native behavior below is source-inspected, not newly host-verified. Runtime probes used only bundled DejaVu Sans. Existing test assertions are stronger evidence than merely finding a method in a dependency.

## Existing architecture worth preserving

1. `src/fonts.js`: bounded byte validation, parsing via pinned `opentype.js` 1.3.4, full-byte immutable font assets, byte-based deduplication and bounded parsed cache.
2. `src-tauri/src/system_fonts.rs` → `src/storage.js`: Windows installed-face discovery and bounded face extraction through opaque IDs, not renderer-supplied paths.
3. `src/core.js`: Session font adoption/settings, preview transactions, validation, history and shared evaluation/export. The selected face is a font asset ID, also usable in sparse range styles.
4. `src/text-layout.js`: shared horizontal/vertical layout and outline generation; `src/text-wrap.js` and `src/text-path.js` add bounded composition/routes.
5. `src/character-identity.js`, character geometry and canvas text editing: logical source identities, normalized basis, sparse overrides, suspension rather than retargeting when mapping is unsafe.
6. `src/main.js`: picker and inspector; browser `FontFace` supplies the native input/IME font, while generated outlines and Nect caret geometry remain authoritative.

Keep the same core commands for GUI/headless use. Do not replace deterministic outlines with browser-only text or use font names as durable identity.

## Coverage matrix

“Verified” is intentionally limited to named fixtures and exercised routes. “Partial” does not promise all fonts with the same table tags work.

| Capability                              | Current evidence/status                                                                                                                                                                                                             | Completion gap / acceptance boundary                                                                                                                                                         |
| --------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Static TrueType `glyf` and OpenType CFF | Verified with both Japanese fixtures, finite outlines and JSON/CLI/SVG round-trip. `fonts.js` validates directory and lazy TTF locations.                                                                                           | Expand malformed/composite/CFF corpus; do not infer every legal font is supported.                                                                                                           |
| TTC/OTC collections                     | Partial: native extractor enumerates/repackages static faces, preserving layout/license tables and rebuilding checksums; direct import rejects `ttcf`. Native Rust has extraction tests, not rerun here.                            | Direct import face picker, deterministic face identity and collection bounds; test shared tables and several faces.                                                                          |
| WOFF/WOFF2                              | Missing at Nect boundary: signature rejection, regardless of dependency capabilities.                                                                                                                                               | Decide implementation order after core outline support; decompression needs bounded decoded size and same permission/parser path.                                                            |
| Real weights and italic styles          | Partial: any accepted static face can be selected by full name and used in ranges. No structured family/style/weight selector.                                                                                                      | Read names/OS/2 metadata, show actual available styles, select exact face; never synthesize bold by scaling/stroking.                                                                        |
| Variable axes / named instances         | Missing: both native `reason()` and JS reject `fvar`/CFF2. No authored axes model.                                                                                                                                                  | Backend must apply axes to both outlines and metrics. Exposing sliders or deleting rejection alone is incorrect.                                                                             |
| Horizontal kerning                      | Verified bounded behavior: font kerning toggle, pair replacement, tracking, rich-text/wrap/path composition. Dependency uses GPOS pair adjustments or legacy `kern`.                                                                | Full GPOS positioning is absent: current callback applies horizontal advance, not arbitrary x/y offsets, marks or cursive attachments.                                                       |
| GSUB / ligatures / language features    | Partial dependency defaults: Arabic `init/medi/fina/rlig`, Latin `liga/rlig`; application supplies no general feature/script/language controls.                                                                                     | Explicit shaped-run records with source clusters; feature-aware cache identity; fixture-proven language/feature routing.                                                                     |
| Complex scripts / RTL                   | Not supported as a complete product contract. Some Arabic glyphs are shaped by dependency; 1:1 editing guards reject mapped editing. Hebrew probe retains logical order. Marks and format controls are globally rejected after NFC. | Script/language itemization, bidi resolution, cluster mapping, full positions and caret/selection contract; do not label a successful outline parse as script support.                       |
| Japanese vertical                       | Verified subset: `vertical-rl`, right-to-left columns, `vrt2` preferred over `vert`, GSUB single substitutions + extensions, `vhea/vmtx`, `VORG`; Japanese OTF and TTF fixtures exercise punctuation.                               | Contextual substitutions reject. Orientation is a hand-written subset; full UAX #50, variation selectors/IVS, vertical kerning, tate-chu-yoko and richer composition remain outside support. |
| Vertical fallback placement             | Partial: geometric placement/rotation for punctuation/small kana with warnings; em/ascender fallback without metrics.                                                                                                               | Preserve visible warning; distinguish fallback from real vertical glyph selection.                                                                                                           |
| Missing glyph / fallback fonts          | Missing automatic fallback; explicit failure names the code point. Per-character font overrides can manually supply another face.                                                                                                   | Ordered, document-persisted fallback chain; resolve whole shaping clusters, embed chosen assets and preserve export reproducibility. Never silently use arbitrary installed fonts.           |
| NFC / source editing                    | Verified: evaluation normalizes CR/LF and NFC while original saved text remains; stable scalar IDs and undo, with suspension for unsupported mappings.                                                                              | Clusters are not 1:1 scalars. Maintain raw UTF-16 ↔ normalized scalar ↔ cluster ↔ glyph mappings without overwriting logical identity.                                                    |
| Outline effects / Type on Path          | Verified static outlines pass shared core; path route deliberately horizontal, single-line and no RTL; wrap and path are mutually exclusive. Corner geometry is independently owned.                                                | New shaping must feed same outlines/advances; add cluster-aware path placement rather than splitting a ligature or combining sequence.                                                       |
| Paragraph layout                        | Verified bounded horizontal Japanese/Latin wrap, manual/auto leading, tracking, baselines and range scales.                                                                                                                         | No vertical wrap, full UAX #14 or complete kinsoku. Font coverage is distinct from paragraph-engine coverage.                                                                                |
| Color/bitmap/SVG-in-font                | Explicitly rejected (`COLR`, `CBDT`, `sbix`, `SVG `).                                                                                                                                                                               | Needs a separate paint representation/product decision; monochrome path support cannot promise color-font fidelity.                                                                          |
| Editable project embedding              | Verified full font bytes in JSON, exact reopening and new-character editing within coverage. Parser rejects restricted/preview-only/bitmap-only embedding combinations.                                                             | fsType is technical metadata, not proof of redistribution rights; retain license checks/warnings and do not claim all installed fonts may be shared.                                         |
| Subsetting / interchange                | No runtime subsetting. SVG emits paths, not editable `<text>` or font resources.                                                                                                                                                    | Retain full editable project source. Optional subset export is separate, must respect no-subsetting flags and preserve relevant tables/glyph closure.                                        |
| Native catalog / cache                  | Source-inspected Windows standard system/user directories, static collection faces, max 4096 entries, source up to 128 MB, face up to 20 MB; identity checked at read; session opaque IDs.                                          | Not complete OS font enumeration: no non-Windows roots, no registry/custom/cloud-font coverage. Refresh behavior and stale selection require host acceptance.                                |
| Performance / failure safety            | Existing limits: 8 project fonts, 20 MB each, 32 MB total decoded font bytes, 50 MB project, 10,000 UTF-16 text units, 500,000 outline commands. Frozen assets shared by history; parsed cache ≤8/32 MB estimate.                   | New decode/shaping/instance caches need explicit budgets, cancellation/stale-result tests and cache invalidation. No measured latency budget is claimed here.                                |

### Small probes: avoid overstating dependency support

With bundled `public/fonts/DejaVuSans.ttf`, `layoutText` at 40 px produced:

- `AV`: two mapped glyphs, no character-mapping warning.
- `ffi` and `office`: three/six glyphs in this font/dependency combination; these strings are **not** a verified ligature fixture.
- `سلام`: three glyphs for four source scalars, with `characterReason` reporting no verified 1:1 mapping. Current glyph metadata assigns source indices by glyph ordinal and therefore must not be consumed as authoritative complex-script clusters.
- `אבג`: glyph IDs in logical input order, with the same unsupported character-mapping reason. No bidi correctness claim.
- `a\u0305`: explicit unsupported combining-sequence error.

## Next implementation: F1, real static styles

This is the smallest user-visible advance toward broad support without replacing layout or disturbing the concurrent corner work. Keep later phases coarse until F1 results are known.

**Goal:** choose a family and its actual face/style/weight, for a whole Text or selected range, preserving exact font bytes, geometry and history.

**Scope and file ownership for a future implementation packet:**

- `src/fonts.js`: add pure metadata extraction from the parsed/validated face (typographic family/subfamily preferred, legacy names as fallback; display full/PostScript names; OS/2 weight/width and style flags). Do not alter asset identity or relax parsing restrictions.
- `src-tauri/src/system_fonts.rs`, `src/storage.js`: optional backward-compatible catalog descriptors read from bounded metadata, without loading every complete font. Preserve opaque IDs and opened-handle checks. Coordinate native owner before edits.
- `src/main.js`: family + style selection with truthful unavailable reasons; use existing preview/cancel/adopt-font command route, including selected ranges. Preserve a flat fallback for legacy catalog responses.
- `src/core.js` / `src/character-identity.js`: reuse existing face-ID adoption and range style records. Avoid a schema change if all metadata is derivable from embedded bytes; family/name is display metadata, not document resolution authority.
- New `tests/font-metadata.test.js`; extend `tests/font-preview-session.test.js`, `tests/system-font-dom.test.js`, `tests/font-dom.test.js`, and focused range tests as required.

**F1 acceptance contract:**

1. Existing DejaVu reports family `DejaVu Sans`, subfamily `Book`, weight 400, width class 5. Japanese fixtures remain selectable despite distinct family names.
2. Minimal generated test metadata covers typographic names overriding legacy style-linking names; localized/non-English names; missing optional fields; identical display names with distinct identities; invalid bounds. Use synthetic fonts or in-memory metadata, not mislabeled real faces.
3. A real bold/italic geometry acceptance fixture is needed before claiming those visual styles tested. Existing repository inventory contains only regular faces. A metadata-only synthetic fixture proves picker logic, not bold design fidelity. If creating a synthetic original fixture, document that limit.
4. Select a style and verify exact asset ID/bytes, whole-text and range outlines, preserved character IDs, one undo entry, redo, cancellation and stale preview rejection. No faux weight, stroke or horizontal-scale substitution.
5. Save/reopen and CLI SVG equality; an unavailable face never silently substitutes. Old projects keep identical SVG.
6. Catalog selection still uses opaque IDs; malformed metadata never becomes a filesystem path. Native Windows visual/IME acceptance remains a separate host row, not a blocker to pure JS tests.

**Done:** the above focused checks pass, existing font/range checks still pass, documented support is “static family/style selection.” Do not call variable fonts complete.

## Subsequent checkpoints and dependencies

### F2: shared shaping seam and cluster-safe editing

Introduce a bounded shaped-run adapter rather than a new document system: inputs are exact face/instance, source span, script, language, direction and feature settings; outputs include glyph ID, source cluster span, x/y advance and offset, font identity and outline access. Begin by adapting the current backend without changing legacy outputs. Pin engine/version in the implementation contract and keep browser/headless behavior identical.

Evaluate a maintained shaping engine such as HarfBuzz for this seam; HarfBuzz is a shaper, not a complete paragraph/bidi/fallback/outline-extraction solution. Verify WASM/worker packaging, offline/CSP and CLI compatibility before choosing a dependency. An outline provider must agree on face index and variation coordinates. Do not promise a library upgrade alone fixes all these contracts.

Acceptance fixtures: actual GSUB ligature font or original synthetic `ffi` ligature, GPOS pair x/y adjustments, mark-to-base and cursive positions, Arabic/Hebrew mixed with Latin/numbers, combining sequences, style boundary across a cluster. Preserve old sparse adjustments; disable unsafe cluster-interior manipulation explicitly until implemented. Bidi visual order must not reorder saved source. UAX #9 and #29 fixtures are required before conformance claims. Font selection, line fitting, drawing, hit-testing, source editing and export must reuse the same shaped records.

### F3: variable outline instances

After the backend seam proves it can share exact instance geometry/metrics, support a bounded first variable format, then extend. `fvar` axes/named instances, `avar` normalization and instance metrics must be applied consistently. TrueType `gvar` and CFF2 need distinct coverage; HVAR/VVAR/MVAR and variable positioning must not be silently ignored. Persist selected coordinates, validate finite bounded axes, include coordinates in every relevant cache key, and preserve original editable bytes.

Acceptance fixtures: licensed/original two-master `wght` fixture; minimum/default/maximum and intermediate outlines and advance widths; non-default named instance; `wdth`/custom tag; malformed axis input; identical instance in GUI and CLI; axis preview cancellation/undo; old documents unaffected. No such variable binary currently exists in the inspected repository. A new fixture must have reproducible generation/provenance and a compatible license before addition. Do not download arbitrary fonts or install them as part of testing.

### F4: deterministic fallback and import parity

Add document-local ordered fallback after cluster-safe shaping, selecting a font for a complete cluster and validating successful shaping rather than only individual cmap presence. Existing manual range selection remains authoritative. Explicitly show unresolved glyphs and selected fallback. Embed adopted fonts through existing bounded transactions so headless export never depends on the machine's font set.

Collection import can be implemented independently of shaping after F1: select a face before adoption, retain/repack permitted tables with the native extractor's safety model. WOFF/WOFF2 follow only with maintained bounded decoders and parity fixtures. Never treat a webfont wrapper as permission to embed or redistribute.

### F5: vertical and layout completeness

Preserve the verified Japanese subset while moving vertical GSUB/GPOS into the common backend. Test Japanese language selection, contextual vertical substitutions, UAX #50 orientation and metrics/origins. Add full vertical wrapping, tate-chu-yoko and advanced Japanese composition as separate acceptance rows; do not conflate them with basic font loading. Existing OTF and TTF fixtures already provide real `vert`/`vrt2` punctuation and vertical metrics, so use them before acquiring anything new.

### F6: scope extensions and publication gates

Color font paint, editable-text SVG/PDF export, non-Windows installed-font discovery and optional subsetting each need their own bounded contract. If a design decision would sacrifice color fidelity, editable source or document portability, ask the user. Ordinary parser/backend choice, reversible UI controls, fixture generation, error handling and cache tests are engineering decisions within accepted broad font support; they do not require a new preference question.

## Fixture inventory and evidence discipline

- `public/fonts/DejaVuSans.ttf` + `LICENSE-DejaVu.txt`: actual static Latin, Arabic and Hebrew coverage, useful as a probe; not proof of correct bidi/shaping.
- `tests/fixtures/fonts/NectJPFixture-Regular.otf` and `.ttf`: derived Noto CJK 2.004 regular test fonts, 158 cmap entries/210 glyphs, vertical features and metrics; `OFL.txt`, `manifest.json`, `build_fixture.py` and README record provenance. These are limited test fixtures, not bundled full Japanese fonts.
- Existing native `system_fonts.rs` tests construct a collection from bundled font bytes; reuse this technique for malformed/multiface collection tests without distributing proprietary fonts.
- Missing real fixtures: style family variants, variable fonts, known Latin ligature behavior, full complex-script positioning, IVS, color fonts and WOFF2. Mark absent rows **not tested** until lawful fixtures and exact assertions exist.
- For each completed matrix row record format/face, content hash, source/license, engine version, text/features/direction/axes, expected glyph/cluster/position result, JSON/CLI/export parity and separate host/UI evidence. Golden output must be from an independently verified result, not merely current output blessed as correct.

## Primary references

These sources define requirements; they do not prove the current Nect implementation meets them. Reviewed 2026-10-17.

- [OpenType naming table](https://learn.microsoft.com/en-us/typography/opentype/spec/name): IDs 16/17 describe extended typographic families; IDs 1/2 are legacy style-linking groups. Use metadata rather than guessing weight from display strings.
- [OpenType variation overview](https://learn.microsoft.com/en-us/typography/opentype/spec/otvaroverview): axes, named instances, normalization and varied metrics/outlines. An axis changes more than outline width.
- [OpenType GSUB](https://learn.microsoft.com/en-us/typography/opentype/spec/gsub) and [GPOS](https://learn.microsoft.com/en-us/typography/opentype/spec/gpos): substitution and positioning are separate responsibilities; pair kerning is only one positioning capability.
- [Registered vertical features](https://learn.microsoft.com/en-us/typography/opentype/spec/features_uz): vertical alternates/rotation feature semantics.
- [HarfBuzz shaping concepts](https://harfbuzz.github.io/shaping-concepts.html) and [cluster model](https://harfbuzz.github.io/working-with-harfbuzz-clusters.html): multiple source characters and glyphs can share a cluster, including reordered runs; source index cannot be inferred from glyph ordinal.
- [Unicode bidirectional algorithm, UAX #9](https://www.unicode.org/reports/tr9/), [text segmentation, UAX #29](https://www.unicode.org/reports/tr29/), and [vertical text layout, UAX #50](https://www.unicode.org/reports/tr50/): explicit standards targets for later script/orientation claims. Pin the tested Unicode data version in any implementation.
- [OS/2 embedding flags](https://learn.microsoft.com/en-us/typography/opentype/spec/os2#fstype): editable/restricted/preview-print, no-subsetting and bitmap-only constraints must be respected. Flags do not supersede the font license.
- [SIL OFL official text](https://openfontlicense.org/open-font-license-official-text/): preserve license/copyright and applicable reserved-name conditions for redistributable fixture derivatives.

Dependency behavior was additionally inspected directly in pinned `node_modules/opentype.js/src/font.js` and `src/position.js`; current upstream feature lists are not evidence for the installed 1.3.4 version.
