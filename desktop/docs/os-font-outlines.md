# OS font outlines (Windows staged support, 2026-10-18)

Some activated fonts are available to Windows applications but absent from the
two ordinary font directories searched by the existing byte-import catalog.
The additional **OS font / Adobe Fonts → paths…** action on a selected Text node
uses the public Windows GDI+ installed-font collection and GraphicsPath wrappers.
It never opens font/cache files, reads font tables or returns font bytes. The
existing imported-font picker and editable Text font handling are unchanged.

Choose a plain Text node, click that action in Inspector, search the active
family (verified here: **DIN Alternate · OS Regular**) and choose **Convert to
paths**. This explicitly replaces Text with one editable Multi Path unit holding
the whole line. The selected OS font reference is preparation-only. Saved JSON
contains the geometry, not a persistent OS font dependency or new font asset.
One Undo restores Text; Redo restores the exact generated identities. Downstream
Path connections and source node identity are retained.

The native binding lists active families/styles, validates the requested family
again, refuses missing glyphs and font substitution, and returns bounded finite
GDI+ points/types plus baseline ascent. Rust prepares this read-only result on a
blocking worker. The DOM-free Session validates source semantics and the native
payload, reconstructs exact line/cubic contours including holes, places them at
the selected visible reference/baseline and performs the sole authored commit.
Cancellation, changed documents, selection, revisions or font choices reject
late adoption. The existing Multi Path codec is used; there is no new schema.

Current scope is **one printable ASCII line**, at most 1024 characters, size
0.01–10000 px, zero tracking, horizontal writing and visible bounds. Wrap, text
paths, variable axes, disabled kerning, maintained arrangements, rich character
adjustments and driven/locked parameters refuse without mutation. Unknown or
unsupported outlines and >10000 vertices/contours refuse. GDI+ styles are OS
styles, not a claim about original font filenames/face metadata. PostScript/CFF
and fonts not exposed by this OS API are not promised. The font must be activated
for desktop applications; this feature does not install or activate Adobe fonts.
This is outline conversion, not continued live DIN text editing.

## Evidence and boundaries

- SS-Note-ROG public native API: DIN Alternate / OS Regular, 377 native points,
  15 contours / 172 Nect vertices. Geometry-only JSON reload, SVG output and
  exact Undo/Redo passed; zero font assets were added.
- 41 focused JS checks passed; the final six OS geometry/picker checks include
  stricter family/style-response and numeric validation. Native tests passed
  27/27, including actual DIN and existing storage/close/font protections.
- Production CSP Chromium test passed picker → conversion → Undo/Redo with the
  real native geometry supplied by an explicit IPC stub; no page errors. This
  tests the emitted UI bundle, not native WebView2 IPC or GUI acceptance.
- Existing Japanese Meiryo native-font smoke passed horizontal/vertical layout,
  codec and cleanup. Its font bytes were not included in a deliverable.
- A fresh document intentionally has `saved = null` and is dirty. The existing
  close guard asks for Save/Discard/Cancel. A DOM/native-adapter check confirmed
  close invokes that choice and Cancel preserves the app. A three-second process
  wait cannot establish a hang; no close behavior was changed. Native dialog
  interaction remains unverified in this execution environment.

The existing `bad7a6c` trial is preserved separately. The OS revision remains a
staged trial. Actual WebView2 picker/IME/DPI/save-restart hands-on acceptance is
open. DIN Alternate's activation provider and underlying file format were not
inspected; no Adobe cache was extracted, embedded, copied or uploaded.

Independent-check gate: `chatgpt-web-double-check` invoked; external consultation
`not-required` after local review of public API contracts, atomic Session/codec
boundaries, real native output and focused/UI tests. No unresolved format or
cross-owner design decision remained; external submissions: zero.

Public API references:
- [GraphicsPath::AddString](https://learn.microsoft.com/en-us/windows/win32/api/gdipluspath/nf-gdipluspath-graphicspath-addstring(constwchar_int_constfontfamily_int_real_constpointf__conststringformat))
- [GraphicsPath functions and data](https://learn.microsoft.com/en-us/windows/win32/gdiplus/-gdiplus-graphicspath-flat)
- [Path point types and markers](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusenums/ne-gdiplusenums-pathpointtype)
