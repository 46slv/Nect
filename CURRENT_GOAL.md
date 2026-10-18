# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — Image Keyer and appearance Mask

Goal: Non-destructive color Keyer after Image and closed shape Mask after Image
or Composition appearance. Keep original alpha, editable nodes, common preview/
SVG/PNG, Undo and exact reopen. This bounded checkpoint is implemented/delivered.
Current phase: Verified Windows trial delivered; parent owns user follow-up.
Proven: Source9cde8a44e8028c1527310da0a73d902a035be939, schema35. Focused model/
DOM38 pass. Production Edge/CSP checks actual node creation/settings, Bypass,
Mask output upgrade, inversion, Alpha Viewer, Undo/reopen exact SVG and PNG
download; source alpha128/255 preserved. Actual isolated Windows EXE/WebView2
repeats controls/Undo/reopen/pixel checks, real native IPC returns654 faces,
no stub/page errors. Owned diagnostic PID33608 stopped; user's process/document
never attached to or modified.14 packaged examples re-export exact SVG offline.
Evidence: desktop/tests/image-effects*, desktop/docs/image-effects.md;
task/native-image-effects/receipt.json, package IMAGE-EFFECTS-UI-RECEIPT.json and
NATIVE-IMAGE-EFFECTS-RECEIPT.json. Older image-tone tests retain7 schema/fingerprint/
CLI fixture failures, also reproduced at c4fcfb0 in an isolated source copy.
Delivery: Library libfile_d60022e6d96c81918b7825b50fde0858;
task/Nect-Desktop-trial-20261018-9cde8a4.zip,4,058,140 bytes/65 entries.
ZIP SHA256 36A8009DBCE05F64C1780C14AA14C640EA8D8208A3EBF9C29CBC923BA3AF9470.
EXE9,790,464 bytes, SHA256
E17BBA8BD66DE747133818793B80A429E632262E3006183B628EBE8308F15553.
Library finalize succeeded; returned EA version0 applied and hash unchanged.
Private QR replacement remains separate: libfile_d837e47e864481918f8a3bdf52a8d81c,
bold-3-to-2-svg-qr-verified.json, schema33, compatible a85aa74 onward. Prior real
EXE load/SVG/QR decode confirmed. No private design or font software in Git/ZIP.
Approach: Keyer RGB distance/range/softness multiplies original alpha, retains
straight partial RGB and zeroes fully transparent RGB; bounded4MP PNG processing,
8 active Keyers/document. Tiny evaluated recipes use one derived-asset resolver
for rendering and luminance. Mask preserves painted-group children, closed Path,
invert and source blockers; shared SVG luminance mask. Explicit Use as output
upgrades Image Output to regular Output, stable ID/custom name/one Undo; locks
protected. Mask is PaintedPath; lossy geometric conversion is refused. Viewer
checkerboard/Alpha are temporary inspection state. No feather, AI cutout, video,
dedicated texel eyedropper, or image-only downstream consumers after Mask.
Next task: Parent supplies the verified Library links and collects actual-use
feedback. Hands-on Windows chooser/IME/high-DPI remains unverified; do not infer
it from browser controls. No further source/build worker is active or needed.
Done for next: Current delegated features and trial are concrete/reviewable;
follow-up work follows the latest parent/user instruction.
State: codex/desktop-production-20261016; source9cde8a4 normally pushed, remote SHA
verified. This delivery-record update changes docs only. Single source/build
writer remains this task until parent explicitly reassigns it.
Authority: Latest parent-delegated user requests. Edit/test/commit/dev push/PR,
EXE and Library permitted. No main merge/release/force/auth changes/Notion writes.
Double-check: Bounded local contract/reuse/pixel review; no external sends.
Handoff: CONTINUE_CURRENT_TASK for authorized follow-up; parent receives outcome.
