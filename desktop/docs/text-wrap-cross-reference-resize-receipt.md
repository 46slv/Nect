# Canvas wrap Y Center/Bottom: verification receipt

Base: PR55 `codex/legacy-numeric-wrap-tests`, verified remote
`10daa5687b33aa683c5e9e22a5425fa694d20bdd`.
Product/build commit: `4a46570ca4dde0cabae3d6c11533caef991fd8a1`.
Branch: `codex/text-wrap-cross-reference-resize`.

Product scope is the existing logical frame helper plus two controller lines to
capture/freshness-check Y reference. No layout/core/schema/style change. Keep
source-wide font-metric height and relative line offsets; compute frameTop from
unchanged Position.y minus height times0/0.5/1 for Top/Center/Bottom. First-baseline
behavior stays unchanged. Existing layout already anchors the pre-character
visible block; do not move glyphs/baselines a second time or chase later free
character offsets. This is a width-budget/metric-height guide, not an ink/stroke
containment box. Existing positive axis-aligned transforms apply after this
node-local calculation; unsupported routes remain hidden.

Focused Node model/actual-main DOM: **23 pass,0 fail,0 skipped**. One compact
model matrix covers all nine X Left/Center/Right and Y Top/Center/Bottom pairs,
two reflow widths, exact numeric-command JSON/SVG equality, reference/handle
geometry, frame inspection neutrality and cancel. It also checks that actual
unadjusted glyph ink is already Y-anchored by layout. Rich style/manual leading
plus a later70px character translation verifies that the logical frame stays at
Position rather than double-applying/chasing that offset. Four DOM boundary
combinations check smaller/larger auto-height, numeric-control equality,
character/Position/selection identity, one Undo/Redo, cancellation/clean lifecycle.
Existing start/baseline, bounds/no flip, pending release, stale and guards remain.
The old unsupported-Y fixture was replaced by a valid unsupported vertical scene;
invalid Y strings correctly fail document loading before a UI guard can run.

Native Tauri/WebView2 on SHIRO-WS: **two configurations,13 acceptance checks pass**.
X Center/Y Center source at100%/advance reference and X Right/Y Bottom whole
artwork at120%/visible reference with a static scale1.5/translation. Widths near
200 and640 increase/decrease height. Four native width commits match equivalent
numeric edits in exact JSON/SVG, preserve Position/characters/selection, have one
Undo, restore through Undo/Redo and survive JSON reload. Frame X/Y references
stay fixed; frame-top change equals half/all of height change as appropriate.
Six cancel checks restore exact committed/visible/file/history state. Clamp/no
flip/Undo, numeric overflow/export and rotated-artwork rejection complete13.
No duplicate nine-case native matrix. Preview Graph DOM mutations0, page errors0.

Viewport1469x827 CSS px, DPR1.0900000334. Actual SVG CTM scales0.9999893875 and
1.1999872424, translated matrices and received pointer coordinates are retained.
Existing inverse-input oracle tolerance1e-9 document units and separate synthetic
quantization bound1/1024 CSS px remain. Source frame pin(140,160), transformed
pin(230,270). Both representative screenshots inspected. Native PID132796 exited
normally via finish_close; no force termination or other user's process use.

Frontend/config and locked/offline release native build pass; existing Vite chunk
warning remains. All14 earlier sealed EXE hashes match. PR55's complete30-file
sealed manifest also verifies, preserving the numeric Escape bug fix/migration
proof without rerunning unrelated suites. No change to that repair, Color,
Image/Shape dither, fixed height, vertical or general resize.

Candidate: `task-13/text-wrap-cross-reference-artifact-4a46570/nect-svg-desktop.exe`,
10,002,432 bytes, SHA256
`316b1f48a51adf280c482ad0121014b61b673630c726bd8fc5b8c3bcf7db74aa`.
JS `index-B3nmuSb7.js`, SHA256
`db28e008ccd7cb6f9f026995421e48336c9373ba8d40938a948bd3fd1a3dc4d2`.
Dedicated evidence retains focused/build/native config/JSON/SVG/screenshots,
normal-close proof, prior EXE hashes and PR55 evidence-manifest verification.

Not run: physical input, actual OS focus loss, OS save/open/cold reopen, IME,
full native9-pair matrix, unsupported resize transforms/OS/variable/attached Text,
multiple Text, unrelated rendering/performance/full audits. Native input was CDP
synthetic mouse/keyboard and renderer cancel events; fixtures unbound/dirty.
DOM verifies clean lifecycle. JSON reload is not OS-file reopen. Rotated whole
artwork width handles remain a concrete next unmet scope; source mode retains
its pre-downstream convention. No main merge, release or Library upload.
