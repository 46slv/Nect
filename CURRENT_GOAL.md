# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — Native entry repair and Japanese basic wrapping

Goal: Repair silent Add-node entry on locked source Compositions and preserve
touching SVG rectangle coverage, finish bounded Japanese kinsoku/vertical wrap,
then build/distribute the next Windows checkpoint. Non-destructive Image Keyer is
the next independent requested feature after this delivery.
Current phase: Implementation/review and integrated verification complete;
coherent commit, native build and distribution are next. The user's running EXE
matched a85aa74 exactly. It was not closed or debug-attached. An isolated copy of
that same EXE/profile reproduced the hidden locked first Composition; choosing an
editable Composition enabled real Add-node and Undo with no JS exceptions.
Actual native font IPC succeeded. Startup is not the reported failure class.
Proven: Focused model/DOM88 and native36 pass. Production Edge/CSP SVG/Layer
workflow passes initial editable Composition selection, locked-picker feedback,
SVG preflight before chooser, prior Add/Import/Solo/scroll/save/PNG behavior.
Production typography workflow passes Japanese basic horizontal/vertical,
tracking, line spacing, fixed-box alignment, Undo/reload, SVG and PNG. Native
boundary probes cover every named policy character in both modes at8/64px.
A private QR replacement was separately delivered and loaded in the real a85aa74
EXE. Existing Composition/output IDs, references, placement, six outside layers,
embedded fonts and original images were retained. Local QR decoding matches the
original PNG and supplied SVG. Its private receipts/files stay outside Git.
Approach: Initial editor selection prefers populated visible unlocked artwork;
authored order/visibility/locks never change. SVG target checks share the common
command gate and run before chooser. Opaque unnamed rectangle batches separate
touching bounds; native rect-vs-path AA differences remain explicit. The new
opt-in textWrap.policy japanese-basic-v1 uses width horizontally and distinct
height vertically, schema34/OS ref5/directwrite-v4 with exact original UTF-16 line
spans. Legacy horizontal settings/older outlines retain their geometry. Review
found and fixed missing-policy default and stale ref4 relabeling loopholes;
regressions cover both. No font software enters an OS outline snapshot.
Next task: Build and verify the new EXE with isolated actual WebView2 controls,
package public examples/receipts, normal dev push/PR update and Library delivery.
Then implement Image→Keyer→Image Output: RGB color/range/softness and alpha preview,
multiply original alpha, preserve source assets/straight RGB, bypass, save/Undo/
PNG consistency with synthetic RGBA fixtures. Read-only integration review done.
Done for next: New EXE/ZIP hashes and confirmed Library ID; native GUI evidence
distinguished from manual chooser/IME/DPI acceptance, then continue Keyer.
State: codex/desktop-production-20261016 candidate from5916421. One writer owns
source/build; read-only worker made no edits. Private helper modules were moved
outside the repository. Prior a85aa74/f055541/de2b589 distributions are unchanged;
the existing SVG package already includes A/nudge/Alt/endpoint continuation.
Authority: Parent's latest delegated 2026-10-18 user requirements and continuous
development authorization. Edit/test/commit/dev push/PR/package permitted; no
main merge, release, force push, auth changes or Notion writes. Personal design
and embedded font bytes must never enter GitHub or the public trial package.
Native OS chooser/manual IME/high-DPI acceptance remains LOCAL_WAIT. Official
WebView2 CDP on an explicitly isolated owned process now supports actual app
controls and JS diagnosis; it never attaches to the user's live document.
Double-check: Exact local contracts, independent bounded reviews and real native
oracles settle this checkpoint; external consultation not required, zero sends.
Handoff: CONTINUE_CURRENT_TASK.
