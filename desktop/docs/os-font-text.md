# Editable OS-font Text checkpoint

User scope (2026-10-18): retain the selected OS font while editing text, support
save/reload when the font is absent, never copy/embed font software or Adobe caches.

Implementation contract:

- Schema 28 adds optional `Text.osFont = {version:1,family,style,outline}`. The
  outline is a bounded, validated native geometry snapshot keyed by NFC text,
  family/style and its preparation size. It contains no font bytes. Old documents
  remain their existing version. Older readers must reject schema 28 explicitly.
- Pure evaluation uses the stored contours and scales/translates them from the
  snapshot size using Text Position/Size and visible reference settings. It never
  substitutes the bundled font or contacts the OS. Saved output is therefore
  available even if the font is missing, including headless SVG output.
- A typed Session command adopts a prepared OS reference/snapshot and commits
  text changes atomically with one Undo. The UI holds native text/IME drafts,
  obtains new geometry asynchronously and checks document/session/revision and
  request identity before adoption. Cancellation/failure keeps authored Text and
  the previous outline intact. Unsupported shaping uses the shared Unicode policy.
- Inspector font availability is runtime state. Absent fonts keep cached display
  but refuse new text preparation; the user can activate the font, choose another,
  keep the draft, cancel it, convert cached outlines to Multi Path, or explicitly
  switch to ordinary bundled-font Text. No silent fallback is permitted.
- Position and Size remain editable from stored geometry. Rich per-character
  adjustments, type on path, wrapping, variable axes and complex/RTL/emoji shaping
  remain explicit refusals for this OS route. Existing normal/variable Text flows
  retain their existing semantics.

Acceptance: native preparation across multiple families and Japanese; source
typing/IME draft/cancel/stale result; exact one-step Undo; saved snapshot reload and
SVG without any OS service; missing-font editing refusal; malformed snapshots,
locks and schema gates; existing Text/variable/point regressions; native build.

Checkpoint evidence: 13 focused model/DOM checks passed, including retained Text,
Unicode IME event isolation, missing-font failure retaining the draft and previous
snapshot, empty Text retaining its OS reference, schema/unknown-data rejection,
pure no-font-service reload/SVG and one-step Undo. Existing Text outline and
variable UI regression run passed 24 checks. Production build passed (103 modules).
Two real native tests passed all four OS styles of four families and Meiryo
Japanese, plus empty/space geometry and missing-glyph refusal. Actual WebView2 IME
acceptance remains open. Multiline/spacing and wider shaping remain the next
font-critical-path checkpoint; this is not a claim of complete font support.

Review gate: `chatgpt-web-double-check` invoked for the saved-format boundary.
External message not required: explicit user scope fixes no-font-byte persistence;
the stored geometry and existing typed Session/codec establish the local contract.
Implementation must verify these invariants before committing this checkpoint.
