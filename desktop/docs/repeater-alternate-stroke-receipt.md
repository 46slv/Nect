# Repeater Alternate Stroke: verification receipt

Base: verified PR59 remote `3af27bf7030a53fda537fc372a437052b69490fa`,
`codex/repeater-alternate-fill`. Branch: `codex/repeater-alternate-stroke`.
Product/build source: `4d5cafd5eb9cd7e056276fa8b0b826ed0b35097d`.

No equivalent Alternate Stroke existed. Extend the current per-copy appearance
carrier with optional derived stroke Color; defaultOFF Alternate Stroke and the
common typed Alternate Stroke Color are independent of Scale/Fill. Odd stable
copy IDs1,3,5… replace only existing solid stroke hex/alpha. Copy index remains
independent of Offset/paint order. All stroke style fields come from the original
stroke object; none remains absent. Gradient/pattern strokes explicitly reject,
not downgrade. Existing solid-tagged strokes remain supported. Both normal and
painted Transform routes retain the metadata without a new paint tree/input mode.

Source/geometry/handles/IDs, placement/order, fill, copy opacity, width/dash/offset/
cap/join/miter stay unchanged. Replacement Color alpha is unmultiplied; the existing
copy wrapper applies opacity once to both paints. The UI hint states solid-only/
none/unsupported limits. The final solid stroke supplies style/base; enabled odd
copies replace its Color only. Color base/ref/expression/validation uses the same
shared path as existing fields, with no parser/index language added.

Only Geometry Repeater gains two authored fields. Default descriptors backfill
without promotion; authored stroke flag/color variation/bindings require schema47.
Existing Fill admission is preserved by a small shared authorship helper. No
source-wide schema, gradient/painted-input expansion or text/dither change.

Focused model/Session: **26 pass,0 fail,0 skipped**,6 new Stroke checks plus20
adjacent Repeater/Scale/Fill regressions. All8 Scale/Fill/Stroke ON/OFF combinations
are covered for linear/radial fixtures (including size100/60, offsets-1/+1), exact
geometry/source/ID/style/opacity preservation, odd/even and reversed order. New
checks cover alpha0/partial odd tail, line-only and no-stroke outputs, unchanged
fill gradients/mixed leaves, unsupported gradient/pattern strokes, Transform
propagation, Color references/dependencies/type/missing/cycle errors, base retained
under expressions, locks, one Undo/Redo, alpha gesture cancel, JSON/SVG and version
backfill/promotion/retag. Final6-check follow-up passes after adding explicit line-
only/solid-tag coverage. No unrelated full audit.

Actual PR59 defaultOFF comparison: **8 exact geometry and8 exact render-string
comparisons** across both seeds and existing Fill/Scale states. No new stroke
metadata whenOFF; old supported output bytes remain unchanged.

Native Tauri/WebView2, SHIRO-WS: **2 configurations,10 acceptance checks pass**.
Each24-copy scene has five nodes/four connections, no image/dither. Linear is
line-only (Fill disconnected), radial combines all three variations. Scale60%,
Fill enabled, stroke width3/dash4,2/offset1.5/round cap/bevel join/miter7. Actual
checkbox and alpha controls plus synthetic hex-control change, one Undo/Redo,
alpha scrub Escape/late release, exact JSON reload, source/settings/identity and
all SVG stroke-style attributes pass. Every path has exactly one copy-opacity
ancestor. A final gradient fill stays unchanged while solid stroke colors vary;
removing the stroke makes OFF/ON byte-identical. Page errors0, both screenshots
inspected. Viewport1469x827 CSS px, DPR1.0900000334, Fit artboard approximately58%.
Task PID114168 exited normally through guarded finish_close; no forced termination.

All960x540 RGBA Canvas/SVG pixels match:
linear line-only `d3a02c0fbb9b50b40f8a59e9998cb46f5c420f2537e2149eacd7da9f4240e48d`;
radial combined `ced41762b1a8895a031a7a7d9a05f2b699e230fd4fca8c279d55f02b296b3d10`.
One representative production-CSP Chrome run uses the **existing PNG export/
download UI**. Radial combined PNG, exported SVG and Canvas all have960x540 RGBA
digest `ced41762b1a8895a031a7a7d9a05f2b699e230fd4fca8c279d55f02b296b3d10`.
This is browser PNG evidence; native OS save dialogs were not exercised.

Frontend/config, syntax/format and locked/offline native release build pass;
existing Vite chunk warning remains. Before build, all18 prior sealed EXE hashes
and complete PR55–59 manifests (197 evidence files) match. Prior outputs preserved.

Candidate: `task-13/repeater-alternate-stroke-artifact-4d5cafd/nect-svg-desktop.exe`,
10,003,456 bytes, SHA256
`3b649b0f2c79fc91d9110fd0423aafa9d5fac371de2c58801753f820636bf3aa`.
JS `index-jlpj5Zl0.js`, SHA256
`b2970043ba832ac02b87f830a85e3188800b9d8bc56ce330fcd481e39288ea97`.

Not run: physical input, OS color picker, actual OS focus loss, native OS save/open
or cold reopen/PNG dialog, unrelated full/performance audits. Native input is CDP
synthetic plus Color hex DOM change; fixtures unbound/dirty. JSON reload is not
OS-file reopen. The first Repeater variation slice is complete at Scale/Fill/
Stroke. Gradient/pattern variation, painted-input repetition and arbitrary copy-
index language remain outside scope and have not been started. No main merge,
release or Library upload.
