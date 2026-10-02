# R03/R10 Artboard Guide alignment / bounded vertical

Status: CORE/JSON + QT/HOST VERTICAL QUALIFIED / AGGREGATE PLATFORM_LIMITED. Base c257e15d162b450694bce0f090d8c5343aace1c7.
Owner: dot, sole cloud writer. Authority: Confirmed REQ-157 and REQ-205, G5 and
the existing Guide packet's explicit Align residual. No background/product change.

Use existing AlignObjects and Session transaction semantics. Add optional
`guide_artboard` to `align_objects` while retaining `reference: "guide:GUIDE_ID"`.
Target-Artboard ID and stable Guide ID are separate fields, never delimiter-packed
or resolved by name/index. Omission/null retains legacy Composition-global Guide
semantics. The scoped field is invalid with non-Guide or legacy artboard aliases.

Resolve the exact local/inherited occurrence through effective_artboard_guides;
Composition coordinate is target Artboard origin plus evaluated local position.
Explicit min/center/max one-shot alignment only. Missing, disabled or suppressed
occurrences, wrong axis and cross-Composition targets reject atomically. Preserve
existing global-Guide behavior. Distribution and baseline-to-Guide remain outside
this slice. No persistent constraint, snap/drag change, native schema change or
implicit clipping constraint on artwork placement.

The existing Window selector carries Guide ID and target-Artboard ID separately,
labels the frame and occurrence, and exposes only enabled scoped occurrences.
Selection restore uses both identities. A removed/disabled selection falls back
to selection without moving artwork; it never silently picks another occurrence.

Oracle: source S/GX local40; A originX1000 override90, B originX2000 inherit40.
Align a Rectangle left to A/GX ->1090, B/GX ->2040. Source50 changes B to2050,
A remains1090 until Reset ->1050. Move A to1100: next explicit Align ->1150.
Test authored local Guide, edge/center, same-name/source occurrences, Undo/Redo,
native cold read and JSON parity. Negatives preserve bytes/revision/History:
wrong axis, missing/suppressed/disabled occurrence, wrong Composition, invalid
scope/reference combination, stale revision, driven transform, hierarchy overlap,
and a later failing command in the same batch. Retain legacy global Guide checks.

Linux geometry-only core/API and Qt offscreen control tests are sufficient for
this bounded behavior, not hands-on/Windows/Host/MCP or whole REQ completion.

## Focused verification — 2026-10-01 UTC

Dedicated core fixture PASS46 checks. Dedicated Qt offscreen Window/Host fixture
PASS28 checks: exact A/B occurrence selection, axis controls, refresh identity,
Undo/Redo, disabled-reference fallback with no mutation, canonical Host Save and
native validation in a fresh core process. CTest registers both targets; both PASS.
The initial core test could not compile before the new field existed (red).
Independent read-only review found no functional must-fix; hierarchy overlap,
legacy-alias conflict, JSON null/omitted scope and malformed-ID cases were added.
MCP help now describes the canonical command; Python syntax check PASS. Formal
MCP runtime is not claimed because this cloud cannot start its local server.

The final all-target desktop Release rebuild passed. Full CTest:36 passes,1
explicit WIC skip,26 failures of63. Failed-name comparison with the prior
61-test aggregate shows no added/removed failure names; these remain platform,
fixture and golden-output obligations, not passes. Real-process smoke passes.
Do not infer full-suite success from the focused checks. Prior platform-limited
aggregate evidence remains in docs/r10-template-duplicate-boundary.md. Logs live
under ignored build/d0-evidence/guide-align-*. No product Candidate, background,
Grid discovery, snapping, distribution-to-Guide or native schema was changed.
