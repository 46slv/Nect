# Repeater Alternate Fill: verification receipt

Base: PR58 `codex/repeater-alternate-scale`, verified remote
`b90898c16d1350d8a31b3c3cb459ccc5342ef106`.
Branch: `codex/repeater-alternate-fill`.
Final product/build source: `a8f4e2d5c65abea4a6e821cef40f246025c35929`.

No equivalent Repeater Color operation exists in the desktop source. Geometry
Repeater still accepts unpainted Path only. The bounded implementation extends
its existing per-copy opacity carrier with optional derived Color metadata for
odd copy IDs. Only the existing independent contour-group render path consumes
it, and only when the final paint is solid. This needs no paint-tree replacement,
new painted-input route or source-wide schema. Ordinary Transform explicitly
preserves the metadata; painted Transform/composition children already retain it.

Alternate Fill defaultsOFF; Alternate Color uses the standard typed Color field,
base/expression/reference path and existing publicColor validator. Odd copy IDs
1,3,5… use the replacement Color; Offset, reversed order and Alternate Scale do
not redefine parity. Source, geometry/handles, IDs, stroke, placement/order and
copy opacity remain unchanged. Color alpha is stored unmultiplied; the existing
group opacity wraps fill/stroke once. None remains none, gradients retain their
paint servers and unsupported patterns still reject. Mixed compositions affect
only eligible solid leaves from this Repeater. The UI hint and brief state these
limits and the explicit final-solid-fill precedence over the shared base Fill.

Only Geometry Repeater gains two authored fields. Default descriptors backfill
without schema promotion; authored flag/color variation or bindings require46.
Old retags and missing46 fields reject. No new parser/index language, gradient
redesign, Color math, dither or layout changes.

Focused Node model/Session: **20 pass,0 fail,0 skipped**:5 new fill checks plus15
adjacent existing Repeater/alternate-scale checks. Coverage includes combined
scale/offset/order, source/geometry/ID identity, alpha0 and fractional copy alpha,
unchanged stroke, none/linear/radial/mixed paints, both Transform routes, Undo/Redo,
JSON/SVG, default schema backfill/promotion/retag, retained Color expression/base,
typed references/dependency updates and atomic malformed Color/missing/type/cycle
errors. The fixture-only extraction follow-up has5 pass. No unrelated full suite.

DefaultOFF vs actual PR58 modules: **4 exact geometry comparisons and2 exact
render-string comparisons**, linear/radial with both paint orders. No new contour
metadata is emitted whenOFF, preserving prior render bytes.

Final native Tauri/WebView2 on SHIRO-WS: **2 configurations,9 checks pass**.
Each is24 copies, a five-node/four-connection Curve→Repeater→Fill→Stroke→Output
scene, Alternate Scale60%, no images/dither. Real checkbox and Color alpha inputs,
synthetic Color hex-control change, one Undo/Redo, alpha scrub Escape/late release
and exact JSON reload pass. Only the intended flag/color changes; source, geometry,
copy IDs/settings/stroke and selection remain. Every exported path has exactly
one copy-opacity ancestor, with independent fill/stroke alpha. One representative
final gradient is byte-identical across Alternate Fill OFF/ON. Page errors0.

Canvas shapes vs exported SVG match all960x540 RGBA pixels in both scenes:
linear `3ba516e401a942630644bd5eb6f6da3fa1c29a16d9f4de5314b9cc6732423043`;
radial `7b60780e9f711fa29f770f6145d7585b1d3399d9dab19f602f19b99de12c2359`.
Viewport1469x827 CSS px, DPR1.0900000334, Fit artboard approximately58%. Both
representative native screenshots inspected. Initial PID135904 and final PID7288
closed normally through guarded finish_close; no forced process termination.

The final product differs from the first accepted native build only by a space
between the existing limits and the new applicability hint. The final EXE repeats
all9 bounded native checks; the initial evidence is retained separately.
Frontend/config, syntax/format and locked/offline native release builds pass.
Existing Vite chunk warning remains. Before build, prior17 sealed EXE hashes and
complete PR55–58 manifests (148 evidence files) match; prior artifacts preserved.

Candidate: `task-13/repeater-alternate-fill-artifact-a8f4e2d/nect-svg-desktop.exe`,
10,002,944 bytes, SHA256
`e3f5ebdc923091411b2d954e48374be930612dddd08c444b97f1260f974fed84`.
JS `index-DgJ0-UQz.js`, SHA256
`78a46147d2da00f97a4357fda7fd5cda3ddb0a2f0d73548d107ece92be896b48`.

Not run: physical input, OS color picker, actual OS focus loss, native OS save/open
or cold reopen, PNG export in this slice, unrelated full rendering/performance
audits. Native input is CDP synthetic plus a Color-control DOM change; fixtures
are unbound/dirty. JSON reload is not OS-file reopen. Painted-input repetition,
per-copy gradient/pattern/stroke variation and general index expressions remain
outside this completed solid-fill slice. No main merge, release or Library upload.
