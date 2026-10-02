# P02-D — stable batch caption allocation and render-fit oracle

Base: `0c253b85460eef76e8887275c63e6a8ea03ae95c` (native 0.77 unchanged).
Status: cloud automated candidate qualified; exact product Windows and held-live styled GUI qualification remain open.

## Failure and diagnostic boundary

The preceding Windows candidate built/deployed, but full91 was81pass/10fail with no skipped or not-run tests. Nine point/primitive/Repeater normal/2×/1.25× cases failed caption fit; the other failure was the known registry denial. Reached Common-phase local/global geometry was stable with50×80 row hints. Point captions were squeezed to32×80; primitive/Repeater captions received98×80. The independent QLabel probe reported120px minimum width. Those failures occurred before later Mixed/sample assertions; those phases were not proven by that run.

A bounded Windows Qt6.5.3 diagnostic reconstructed the caption strings from source in native `windows`-QPA labels, using Yu Gothic UI9.75pt/13px at logical96DPI. It did **not** observe the actual product widgets or rerun the original suites. At each DPR1/2/1.25,32px fitted0/88 samples,78px56/88,98px88/88 and120px88/88. Tiny signed-g7 fractions required90–94px unbreakable numeric widths. Native minimum hints were36–94px;120px did not reproduce. Copied and fully resolved font probes matched; an offscreen comparison resolved an empty family and reported130px. Environment-sensitive font fallback is a hypothesis, not a proven explanation of the original120px result.

Copied-form fixed98 and measured94(DPR1)/93(DPR2/1.25) minimum candidates each retained stable geometry for156 samples per DPR. The94px cross-DPR target was not directly rerun;98px was. Original report SHA256: `110fcef2b6cdb99ac6a2856a34844a016a57912917dbc87e8fe3d52f905748b0`. This is diagnostic evidence, not exact product acceptance.

## Bounded implementation

The shared row is parented to its real Inspector before caption polish. A one-time minimum uses the directly measured98px floor, enlarged by the effective font's advance/ink bounds for both Common/Mixed headers with the actual frozen target count, orientation, and stable signed-g7 numeric samples. Fractional and exponent forms include tiny doubles; this does not expand permitted authored angles. No minimum is recalculated from live text. Horizontal `Ignored`, stretch1,80px row and44×44 dial remain. Initial `WrapLongRows` wrapping is legitimate; wider Inspectors still expand the caption.

Three-line source copy, Common/Mixed state, count, zero direction, visible delta, g17 numeric/accessibility precision, Ref vectors, gesture/rollback/history and hover semantics are unchanged. No model, schema, storage, Template or font-authoring change is included.

## Independent fit proof

The fit oracle renders the complete plain LTR caption with `QPainter::drawText(TextDontClip)` into a padded transparent DPR-aware image. Its fully resolved font and logical DPI come from the real label. Nonempty ink must remain inside the allocation in both axes, and ink touching the outer canvas edge invalidates the measurement. Scope guards reject framed, margined, rich, selectable, RTL or mnemonic-buddy labels. QLabel probe hints remain diagnostic only; neither a copied hint nor the live fixed-height clamp is treated as painted-fit proof.

Negative controls must positively demonstrate unclipped overflow, with supported/nonempty/nontruncated renders: a200-W unbreakable token with ample height, ordinary multiline text with1px height, and the separate32px squeeze probe. They pass at DPR1/1.25/2. Historical98×80 examples remain distinct from extended tiny/exponent samples at the actual reserved minimum. Geometry captures the reservation as well as local/global row/dial/caption allocations and row hints through Common/Mixed live updates. A wider real Inspector must expand its caption without changing its minimum or compact height.

## Qualification and limits

- Local red on unchanged production: all three real batch fixtures lacked the required initial minimum. The independent32px raster also demonstrated actual overflow; this is not claimed as a reproduction of Windows pointer movement
- All-target Release build PASS; focused19/19 PASS
- Full91 (`QT_QPA_PLATFORM=offscreen`):64passed/1skipped/26failed; exact prior failed-name set, zero added or removed failures
- Fresh-process native/SVG smoke PASS; GUI/MCP flags false
- Final normal/2×/1.25× point diagnostics PASS; OpenAI Sans12px reserved100px, row hints150×80 and actual caption126×80 remained stable through the reached phases
- Independent read-only source and evidence review: no remaining blocking findings
- Exact patched full-product Windows replay and styled held-live GUI remain NOT_RUN

These samples are not a universal proportional-font/digit-sequence/count guarantee. The actual frozen count is measured without a selection cap; larger fonts can enlarge width, but arbitrary large fonts cannot fit80px vertically. Font/DPI changes during a gesture, multi-monitor transitions and arbitrary custom fonts remain unqualified. No unsupported CUA route or synthetic event is presented as held-live GUI evidence.

## Frozen source and receipts

- `src/desktop/window.cpp`: `d770148c965e4357e2f355d493cf4a80f2319a5f8de81dcb911c12717dc18039`
- `tests/batch_angle_geometry.hpp`: `0d8d359be55ba946248a93ce57a57966be7ca1fddfcbf58f817188abf9e54f9e`
- `tests/batch_point_angle_ui_tests.cpp`: `f73d941b7e90fb0f5f27fb4e51060be935725e934e275c4a34fa80ace5105167`
- `tests/primitive_batch_angle_ui_tests.cpp`: `1002cdd6c9a559a4a7df85635755a31d05dbab259c50c235622d7b5a1b772a26`
- `tests/repeater_batch_angle_ui_tests.cpp`: `37b356e7b6b4702127c940d7e957c4be4cce53b55c472e800e50ef28a9dde87e`

Local evidence and exact hashes are recorded in `build/d0-evidence/p02-d-caption-allocation-receipt.json`, including red, final all-target/focused/full/smoke, baseline comparison, raster negatives, final geometry, review and diagnostic-source receipts. No commit, push or CURRENT_GOAL edit was made by this worker. SOURCE and BUILD are released after final review/manifest reconciliation.
