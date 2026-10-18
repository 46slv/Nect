# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — Image Keyer and appearance Mask

Goal: Add non-destructive color Keyer to Image pipelines and shape Mask after
Image or Composition appearance, with original alpha, editable source nodes,
shared preview/export, Undo and exact reopen behavior. No AI cutout/video/feather
expansion. User's 2026-10-18 requests are delegated by the parent Mission owner.
Current phase: Shared image/appearance integration inspected; implement next.
Proven: The prior entry/SVG/Japanese-wrap checkpoint is built from
1641e0c788e25072c01581f59755b02be055416c and normally pushed. Model/DOM88, native36,
production Edge/CSP and actual isolated WebView2 controls/real typography IPC
passed. Twelve packaged examples re-export exact SVG offline. User's running
EXE/document was not modified; owned diagnostic processes stopped. Manual OS
chooser/IME/high-DPI acceptance remains separate. The private QR replacement
was separately delivered, loaded in the real prior EXE and locally decoded to
the same payload as its source; design/font data are outside Git and the package.
Delivery: Library libfile_a100b6bb60648191a424687c4524370a;
task/Nect-Desktop-trial-20261018-1641e0c.zip,4,033,663 bytes/56 entries.
ZIP SHA256 5D419904EDDCA8B22C6047ABA4B8EC435AF53BABD062811CFFCACE9B91FD6998.
EXE9,780,736 bytes, SHA256
95BD6DFD1035577C2FFC03B428C316F828AB2F450E4424022DA36F9522A2C8E9.
PR evidence: https://github.com/46slv/Nect/pull/24#issuecomment-6248062911.
Approach: One writer, schema35 for the two new nodes. Keyer accepts/returns Image;
key RGB/range/softness multiply original alpha, retain straight partial RGB and
zero fully transparent RGB. Reuse bounded PNG decoder; pure canonical PNG
encoder,4MP processing limit,8 active Keyers/document. Evaluated Image carries a
small effect recipe, not a large cloned derived PNG; one bounded result cache.
One image resolver must serve rendering, Keyer chains and luminance consumers.
Mask accepts Image or PaintedPath appearance plus closed Path and invert, returns
PaintedPath using existing painted groups. Keep source alpha and paint/order;
SVG masking is shared by preview and SVG→PNG. Preserve export blockers. Explicit
Use as output can upgrade an Image Output to regular Output, same ID/one Undo;
do not change sockets just by creating a node. Refuse geometric conversion of a
masked appearance rather than discarding its mask. Bounds include conservative
stroke padding. Viewer alpha/checkerboard are temporary inspection state and
must never alter authored artwork or export. Reuse normal Color/node controls;
dedicated image-texel eyedropper is optional, not a blocker.
Next task: Implement bounded processing and Mask model/rendering, then focused
synthetic RGBA/appearance tests and real UI wiring, Bypass, Undo/reload/SVG/PNG.
Done for next: Coherent tested source, normal dev push/PR checkpoint and a new
combined Windows trial only when this image-effects checkpoint is verified.
State: codex/desktop-production-20261016; code is1641e0c, this record changes docs
only. No image-effects edits yet. Source/build owner remains this task alone.
Authority: Latest parent-delegated user requests. Edit/test/commit/dev push/PR and
EXE/Library permitted. No main merge/release/force push/auth changes/Notion writes.
Never publish personal design, original attachment or embedded font software.
Official WebView2 CDP may control explicitly isolated owned acceptance processes;
never attach to or close the user's live window/document. Native manual OS
chooser/IME/DPI remains LOCAL_WAIT, not a shared coding blocker.
Double-check: Bounded local contract/reuse review; external consultation not
required at present, zero external sends. Parent owns the broader spec audit.
Handoff: CONTINUE_CURRENT_TASK.
