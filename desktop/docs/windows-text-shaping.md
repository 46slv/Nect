# Windows shaped Text

2026-10-18 implementation checkpoint, supplementing [editable OS Text](os-font-text.md).

Choose OS font lists real DirectWrite faces by family, actual face label and
weight. Simulated faces and color fonts are excluded. GDI-only families remain
available through the previous geometry adapter (including DIN Alternate when
visible there); that route retains its documented single-line/script limits.
No adapter reads font files, tables, Adobe caches or font software into Nect.

DirectWrite Text remains editable Text. Horizontal multiline Unicode layout uses
the exact selected face, explicit size, tracking, line spacing, pair kerning,
standard ligatures and start/center/end alignment. Each native request prepares
geometry only; the common JS Session validates and adopts it atomically with one
Undo. IME input remains a draft until composition ends. Async results are checked
against the source document, revision, selection, request and chosen face.
Missing glyphs, fallback runs, missing faces and invalid/oversized results refuse
the whole operation while preserving the previous Text and snapshot. DirectWrite
Draw may ignore individual renderer errors, so a sticky error is checked before
the geometry callback; mixed supported/missing strings are tested explicitly.

Schema 29 adds `Text.osFont.version:2` with an exact face descriptor and bounded
`directwrite-v1` outline snapshot. Version 1/schema 28 remains readable. Original
documents retain their schema until a new feature is authored. The snapshot
records normalized text, size/spacing/features/alignment, raw contours, baseline,
advance width and line/glyph counts. It contains no font bytes. Validation rejects
unknown fields and source/settings mismatches. Evaluation, SVG, PNG and conversion
to Multi Path use the same stored contours without OS access; absent fonts retain
display/export/reload and explicitly refuse new preparation. Size/spacing edits
must obtain a new snapshot, preserving shaping rather than scaling stale spacing.
Position/reference alignment can reuse the snapshot.

Bounds: 4096 UTF-16 units, 256 lines, 20000 glyphs, 300000 raw outline points and
100000 parsed vertices; explicit Multi Path baking retains its 10000-vertex cap.
Color/bitmap glyphs, wrapping, vertical Windows Text, custom leading, rich
character/path adjustments and variable axes on this OS route are unsupported
and refuse. Existing imported static vertical/rich/path layout and the prepared
variable TrueType axis/named-instance workflow remain available separately with
their existing limits. No claim of all fonts/scripts or complete typography is made.

Evidence on SS-Note-ROG:

- 576 real faces enumerated. Native outline preparation verified Arial Black,
  Times New Roman Bold, Meiryo Bold/Regular, Segoe UI Black, and a CFF Kuramubon
  Regular face. Greek/Cyrillic used Arial Regular; Arabic and Hebrew used Segoe UI
  Regular. Arial Japanese and mixed Latin/Japanese fail atomically instead of
  substituting. Calibri standard ligatures produced 8 versus 13 glyphs; kerning,
  tracking and line spacing change measured geometry. Empty/space/tab/newline
  preserve a font reference without invented outlines.
- 30 native tests passed. 18 focused OS model/DOM checks passed, including actual
  Windows fixture codec/evaluation and exact SVG equality after path conversion.
  43 existing outline/variable tests passed, including vertical/wrapped/rich/path
  baking, axis preview/cancel/history, named instances and point handles.
- Production Edge with native IPC explicitly stubbed using independently captured
  Windows geometry passed Meiryo selection, IME event isolation, typing, six
  typography settings, Undo/Redo, JSON save/reload, SVG equality, PNG signature,
  missing-font draft preservation and conversion of cached outlines. No font-byte
  call or browser page error occurred. This verifies production CSP/frontend,
  not native WebView2 IME, high-DPI or subjective typography acceptance.

Actual native WebView2 editing/IME acceptance is LOCAL_WAIT: this host's exposed
tools provide browser automation but no native UI controller. Resume when native
computer-use capability or the user's hands-on feedback is available. Native EXE
startup and package evidence are recorded with each distribution.

Review gate: chatgpt-web-double-check invoked; no external submission required.
The bounded geometry-only adapter extends the accepted OS-font contract; exact
native assertions, pure codec/output equality and production UI evidence are the
local acceptance gate. No proprietary font/text material was sent externally.
