# R05 font shaping and derived receipt checkpoint 2a

This is an isolated, unintegrated backend candidate on `dot/font-authoring-01`,
based on `498f32ee36e3ad9b8ef1b8987412819747b2e3e9`. It does not close REQ-15,
G11 qualification, GUI admission or formal MCP parity. Native remains 0.78.

## Implemented boundary

- Keep legacy system collection / family / weight / style selection, then submit
  combined evaluated `wght` / `ital` and nonconflicting additional axes through
  `IDWriteTextFormat3` before creating the layout. Authored doubles are never
  clamped or rewritten. Float rounding is explicit; finite doubles beyond float
  range remain authored and are not submitted. API absence is explicit. Actual
  projection failures still throw through the existing atomic Session boundary.
- Add whole-text `IDWriteTypography` before any measuring or drawing. Removing
  all authored features restores the default typography path.
- Record only actual `OutlineRenderer::DrawGlyphRun` callbacks in `font_runs`.
  Empty text has no invented runs. Existing `used_fonts` and warnings remain.
- `font_request` is the evaluated request (including exact extra-axis intent and
  separately narrowed/submitted values), not proof of the face selected.
- Actual font axis values come from `IDWriteFontFace5`; ranges/defaults come from
  its `IDWriteFontResource`. Nullable per-axis `variable` comes directly from
  `GetFontAxisAttributes`: an expanded range alone does not prove interpolation
  support. Matching noninterpolating values are reported as resolved, with an
  informational warning, rather than falsely declared incompatible. Static,
  missing, out-of-range, unknown and differing
  axes receive explicit assessments. Legacy static weight/italic selection is
  labeled separately, not claimed as variable support.
- Run range units are UTF-16. Family, face, locale and metadata may be unknown.
  Axis values/ranges have independent known flags. Glyph indices and actual
  advances give a before/after oracle; missing glyphs are counted per run.
- File identity is a SHA-256 digest of the copied opaque DirectWrite reference
  key, scoped by the loader identity inside this layout. It is **not a font-file
  content hash**, portable identity or cross-process guarantee. No raw keys,
  local paths or font binary bytes are exposed, copied to artifacts or embedded.
- The reported em-size ratio is actual run em size divided by layout em size.
  It is explicitly **not** the `MapCharacters` fallback scale.
- Bounded full-text `AnalyzeScript` supplies missing script context, intersected
  with the actual draw ranges. `GetTypographicFeatures` uses that actual face,
  script and actual run locale. API availability includes partial support and
  does not establish that the feature changes these glyphs. The tests compare
  before/after glyph indices and advances independently.
- Shared `text_layout` JSON exposes the receipt. Text-on-Path already copies the
  entire `TextLayout` before geometric projection, so no shape-owner edit is
  needed. Receipts are never serialized into native authored documents.

## Verification status and limits

The Release Linux build passes for `nect`, `font_authoring_contract_tests`,
`font_shaping_tests` and `text_tests`. The portable contract passes all 52 checks;
`text_contract` passes. These verify compilation outside `_WIN32`, serialization
and unchanged portable authoring behavior. They cannot compile or prove the
Windows path. The new `font_shaping_contract` returns CTest skip code 77 on Linux
and still checks that projection returns `TEXT_PLATFORM_UNSUPPORTED`.

Windows semantic tests are prepared for a separately managed isolated build:

- independent legacy DirectWrite normal/bold/italic glyph/advance comparison
- whole-text Arial kern off/on effect and removal back to baseline
- Independent DirectWrite family/face, Face5/FontResource variable attributes and
  ranges, and TextFormat3 call admission for the Bahnschrift fixture; only that
  host preflight can justify a missing-capability skip. Once admitted, absent or
  wrong product application/receipts fail the test
- Bahnschrift combined weight/width actual axis readback
- unsupported/static/out-of-range/float-rounded/unrepresentable axes with exact
  intent preserved
- mixed fallback, missing requested family, supplementary UTF-16 ranges and
  missing glyphs, empty text, Japanese horizontal/vertical and Arabic diacritics
- Text-on-Path receipt propagation, shared JSON output, native exclusion and
  feature command Undo/Redo

Absent installed fixture families print `ENV_MISSING_FIXTURE` and cause a skipped
suite, never a product pass. These tests deliberately do not claim a CFF/OTF
feature-effect fixture, GUI interaction, formal MCP transport or all G11 coverage.
`text_authoring_contract` was attempted on Linux and fails immediately with
`TEXT_PLATFORM_UNSUPPORTED`; it is not reported as passing.

Actual runtime missing-metadata branches depend on the installed fonts/backend;
tests verify the known-or-explicit-warning invariant, but do not manufacture a
fake font provider or claim unavailable metadata was observed in real shaping.
Noninterpolating axes with expanded ranges are handled from actual VARIABLE
metadata; absent a suitable installed fixture, that specific branch remains
source-inspected and runtime-unexercised.

## Primary API references

- https://learn.microsoft.com/en-us/windows/win32/api/dwrite_3/nf-dwrite_3-idwritetextformat3-setfontaxisvalues
- https://learn.microsoft.com/en-us/windows/win32/api/dwrite_3/nf-dwrite_3-idwritefontface5-getfontaxisvalues
- https://learn.microsoft.com/en-us/windows/win32/api/dwrite_3/nn-dwrite_3-idwritefontresource
- https://learn.microsoft.com/en-us/windows/win32/api/dwrite_2/nf-dwrite_2-idwritetextanalyzer2-gettypographicfeatures
- https://learn.microsoft.com/en-us/windows/win32/api/dwrite/nf-dwrite-idwritefontfile-getreferencekey
- https://learn.microsoft.com/en-us/windows/win32/api/dwrite_3/ne-dwrite_3-dwrite_font_axis_attributes
