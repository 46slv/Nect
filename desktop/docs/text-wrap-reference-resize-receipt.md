# Center/end Canvas wrap width: verification receipt

Base: PR53 `codex/text-wrap-canvas-resize`, verified remote
`61941a1865d4703d53670de577a44a12c962d1cd`.
Product/build commit: `0ff6ec73daaf927ad0d0dfc747cbf81a442a33ed`.
Branch: `codex/text-wrap-reference-resize`.

Product delta: three controller lines capture/apply/check the Text Position
reference; the existing geometry helper places the width-budget frame around
that reference. Center retains one right handle, changes width by twice local
horizontal motion and expands both sides. End uses one left handle, reverses
width delta and keeps the right reference fixed. Anchor crossing clamps at
0.01, with no flip; maximum remains100000. Existing numeric width/reflow is
canonical. Per-line advance/visible reference is preserved; no paragraph
alignment, geometry, core/schema/style or general-resize rewrite.

Focused Node model/actual-main DOM: **26 pass, 0 fail**. Includes start regression,
center/end with advance/visible reference, shrink/grow, Canvas scale1/2 and
positive target scale/translation, numeric command/actual-control JSON/SVG
equivalence, fixed Position/frame reference, character IDs/selection, clamping,
one Undo/Redo, exact clean-lifecycle cancellation, stale targets and existing
object-resize/Canvas marquee contracts. A whole-artwork numeric-control test
initially addressed the absent Text Inspector field; it now uses the existing
Graph width control. No product repair or weakened expectation was needed.

Native Tauri/WebView2 on SHIRO-WS: **24 acceptance cases pass**, page errors0.
Center/end each run at UI zoom100%/120%, with pan. Native100% uses Text source
and advance reference;120% uses whole artwork, visible reference and target
Transform scale1.5. Eight shrink/grow gestures each match the equivalent native
numeric edit in exact JSON/SVG, commit one Undo, restore with Undo/Redo, and
survive JSON reload/export. Twelve renderer cancel routes restore JSON/SVG,
file/history/selection and visible Canvas exactly. Two Position-crossing cases
clamp without flip and Undo exactly; numeric overflow/export and rotated
whole-artwork rejection complete the matrix. Preview Graph DOM mutations0.
Viewport1469x827 CSS px, DPR1.0900000334. SVG CTM scales0.9999893875 and
1.1999872424; full translated CTMs and received pointer coordinates are recorded.
The oracle checks their inverse within1e-9 document units, separately bounding
synthetic input quantization below1/1024 CSS px. Product retains existing floats.
Screenshots inspected for center/end, source/transformed modes at both zooms.

Inherited failures remain separate and were **not rerun or changed** here:
`numeric-scrub-dom.test.js:119` (500 vs480), pre-existing numeric-preview
expectation discrepancy, fixture/product root cause still unresolved;
`text-wrap-dom.test.js:82` (schema24 vs8), obsolete exact schema expectation
against the current baseline. PR52 reproduction logs/loader and their explicit
classification were carried forward. Neither is counted as a pass.

Frontend/config checks and locked/offline release native build pass; existing
Vite chunk-size warning remains. Owned native PID85588 exited normally via
`finish_close`; no forced termination or other user's process use. All twelve
prior sealed EXE hashes match. PR48/51 diagnostic-only receipts remain.

Candidate: `task-13/text-wrap-reference-artifact-0ff6ec7/nect-svg-desktop.exe`,
10,002,432 bytes, SHA256
`0a64c3a8d925e86dc8b5780279486297552dad5432aa660473d18eb07300c7de`.
Built JS `index-DGUm8FIv.js`, SHA256
`3ca0377656fde6392a72ad8b9a46031b19f9b45a076a86d5c4d666b6ec9af11c`.
Dedicated output retains native JSON/SVG/screenshots, focused/build/close logs,
prior hashes and inherited-failure evidence.

Not run: physical mouse/touch/pen, actual OS focus loss, OS save/open dialogs or
cold reopen, IME. Native input was CDP synthetic mouse/keyboard and renderer
cancel events; native fixtures were unbound/dirty. Clean-file cancellation was
verified in actual-main DOM. JSON reload is not an OS-file reopen claim.
Cross-axis Center/Bottom, rotation/skew/reflection, vertical/multiple Text,
fixed-height/threading remain unsupported Canvas resize scope. No protected-main
merge, release or Library upload. This is not an all-Nect completion claim.
