# Nine-point grid origin verification

Base: PR60 `codex/repeater-alternate-stroke` at verified remote
`e169a84a3ddf8823086825050ed8f37339f086aa` in `https://github.com/46slv/Nect.git`.
Product/build source: `01df466d51a8945c984b13be094f4a4fba1e2350`.
Scope/compatibility: [brief](grid-origin.md). No equivalent nine-point selector
existed at the base; only top-left/center were supported.

## Result

Grid panel Origin now selects all corners, edge midpoints and center. Both
background pattern and grid snapping use resolveGridOrigin. The stored relative
anchor follows canvas resize; artwork, absolute guides, world/ruler coordinates,
spacing, opacity and SVG are unchanged. Existing missing/top-left and center
phases retain their incoming versions. Only the seven added authored anchors
require schema48. The three product files are guides.js, core.js and main.js
(45 added/9 removed lines); no Image/Shape dither, Text or Repeater code changed.

## Passed

- 15 focused checks, zero failures/skips: grid-origin.test.js (4),
  grid-origin-dom.test.js (1), guides.test.js (10). All nine anchors across odd,
  fractional and resized canvases; grid-only snap at 0.5x/2x; legacy/default and
  schema gates; prototype/unknown fallback; invalid/stale atomic rejection;
  same-value no-op; one Undo/Redo; exact JSON and unchanged geometry/guides/SVG.
- After the final small control-row markup change, the affected DOM check was
  rerun and passed. The unchanged model tests were not needlessly repeated.
- Final native Tauri/WebView2 on SHIRO-WS: three cases (bottom-right,
  top-center, center), 100%/120% with pan, 961x541 canvas. Real renderer select
  value, pattern origin, actual CDP guide drag onto the resolver's expected grid
  line, cancelled drag, one Undo/Redo, exact JSON reload, selected node identity
  and unchanged artwork/SVG passed. Pointer hit target was verified before drag.
  Three renderer screenshots captured and inspected; page errors zero.
- Native zoom labels match the requested scale. CTM layout quantization is
  bounded by 1/64 CSS pixel across the SVG width, separately from exact document,
  snap and SVG assertions. This measures no physical input latency.
- Changed-file format/syntax, native build configuration, frontend build and
  locked/offline custom-protocol release build pass. Existing Vite chunk-size
  warning remains. Default Cargo cache lacked rfd; the existing task-local
  cargo-home cache resolved the build without installation or downloads.
- Eighteen earlier EXE hashes, plus eight sealed manifests/315 evidence files,
  match before/final checks. PR60's EXE and outputs remain preserved. The new
  runtime is a separate directory and fresh acceptance profile. Only the test's
  own dirty fixture was CLI-preserved and directly closed; no force termination.

## Classified limitations and diagnostics

Native interaction was CDP synthetic, not OS/physical input. OS Save/Open,
native file binding and clean Save/Exit cold reopen remain unverified; JSON debug
reload is not equivalent. No unrelated full audit, release, merge or Library
upload was performed. Ordinary user AppData/Recovery and existing user apps were
not operated on. Dedicated test profiles/Recovery were retained, not deleted.

The pre-existing guides-dom.test.js stops at line76 when it clicks a lock control
after Escape dismissed the transient panel. The exact PR60 test with exact PR60
main.js reproduced this failure in an ignored Git-object probe. It is outside
the new independent Grid DOM check and remains unfixed, not skipped or made
green by weaker assertions. Its later centering call was updated to the new
selector. No full-suite pass is claimed.

Happy DOM20.8.9 returns a stale select.value after repeated innerHTML replacement;
a four-option isolated probe records expected0/1/2/3 versus actual0/1/1/1, while
the selected HTML attribute stays correct. The DOM check asserts both authored
state and exact selected option. Actual select.value passed in native WebView2.

Early native harness attempts exposed view reset to Fit on debug load, a leftover
transient panel covering a view button, CSS subpixel quantization, and an
offscreen test pointer at the higher zoom. The harness now restores its view,
dismisses its panel via Escape, bounds measured CSS quantization, and requires
the real visible guide hit. Failure logs are retained separately; these were
test preparation issues, not extra product fixes.

Automatic approval review rejected a command that would list all CARGO/RUST
environment variables because credentials might be exposed. It did not execute
and was not retried or bypassed. Safe directory-name inspection found the known
workspace cache; the authorized build completed. No credential/security changes.

## Candidate

Standalone EXE: `grid-origin-artifact-01df466/nect-svg-desktop.exe` in task-13.
Bytes:10003456. SHA256:
`5f2cc08239259ddf6e52a8dcb5d8c08a4683202a0545874dfb49553f1082b4f1`.

The separate output contains built frontend assets, candidate/publication
metadata, manifest, this brief/receipt, focused/build/native logs, renderer
screenshots and editable fixtures. Runtime profiles and Recovery are excluded.
Publication metadata records final remote HEAD/PR after the receipt commit;
product inputs remain exactly those of the build source above.
