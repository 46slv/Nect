# Windows shaped Text

2026-10-18 implementation checkpoint, supplementing [editable OS Text](os-font-text.md).

The current opt-in horizontal/vertical wrapping contract is documented in
[Japanese basic wrapping](japanese-basic-wrap.md), schema34 / OS reference5.
Earlier snapshot versions below remain readable without migration.

## DIN catalog repair (05:35 UTC)

The subsequent read-only diagnosis reproduced process-local catalog loss: two
fresh GDI enumerations returned 637 families including DIN Alternate; creating
the shared DirectWrite factory then reduced that same process's GDI enumeration
to 365 without DIN. Low- and medium-integrity processes in user session 1 both
independently produced the same DIN 377-point geometry before that interaction.
An isolated DirectWrite factory preserves all 637 families across repeated
catalog/preparation calls. No font activation, account or permission change was
needed. The earlier claim that the user needed to reactivate DIN was unproven.

The unified catalog also obtains real face references through the public GDI
interop API for families absent from the ordinary DirectWrite collection. It
rejects synthetic faces, verifies the selected GDI family and reports actual
face/weight/style/stretch. This host's DIN Alternate is **Bold / 700 / normal**,
even when GDI was asked for Regular. It is now shown with that real metadata.
The outline renderer uses IDWriteFontFace5::Equals when available, including
axes in the identity; older systems retain the exact face-reference comparison.
DIN's face-reference aliases compared unequal while Windows' face equality
returned true. Font-name equality alone never authorizes a substituted run.

The repaired unified catalog exposes 633 real faces. Three native typography
tests pass, including unchanged repeated GDI catalogs, actual DIN lowercase and
multiline shaping, strict Japanese-in-DIN refusal, and existing Unicode/spacing
checks. The captured DIN geometry passes pure JSON/evaluation/SVG/path equality
and production Edge/CSP selection with the Bold/700 label, multiline Text,
reload without fonts and exact conversion output. Browser IPC remains an
explicit native-geometry fixture; native WebView2 acceptance remains unverified.

Creative Cloud and CoreSync were running. The existing Adobe Fonts DIN search
page showed several added DIN families, and its management menu distinguishes
installing fonts for other applications through Creative Cloud. That web state
does not identify DIN Alternate's supplier or prove all those families are
installed for desktop apps. Native UI is not exposed by the current tools; no
activation or account UI action was taken and no user action is presently needed
for the proven DIN repair.

## Schema 30 axes and vertical extension

The next checkpoint adds schema 30 / OS reference version 3 / `directwrite-v2`
snapshots for OS variable axes and vertical writing. Axis metadata includes the
OS range, font default, selected-face base, current value and hidden flag (max 32
axes). Unknown, incomplete or out-of-range axes refuse instead of clamping.
The exact varied face is used by shaping and the renderer's identity check;
automatic optical-size adjustment is disabled when explicit axes are supplied.
Vertical layout uses Windows' per-run orientation transform, upright Japanese,
rotated Latin and right-to-left columns. Snapshots additionally retain writing
mode and advance height; pure evaluation positions their stored geometry on the
corresponding inline/cross axes. Versions 1/2 remain readable without migration.
Bahnschrift width/weight and Segoe UI Variable optical-size/weight were measured
at both range limits. Real Meiryo glyph geometry confirms Japanese orientation,
Latin rotation and column direction. The production fixture workflow covers axis
edits/reset/failure, vertical mode/spacing, Undo, JSON save/reload and exact SVG
and cached path output; PNGs were visually inspected. Actual WebView2 IME/DPI
remains unverified. The saved-format review gate was invoked: external submission
not required because the accepted geometry-only boundary and local native/codec
tests settle this bounded extension. External submissions: zero.

## Initial schema 29 contract

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
