# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — 2026-10-18 Windows font support

Goal: The user's 13:40 JST instruction prioritizes practical complete font support
before the deferred Illustrator-style path backlog. Preserve authoring and never
extract/copy/embed Adobe cache/font software. Windows OS references may return
geometry only. No universal all-font/all-script completion claim is authorized.

Current phase: Verify/package the schema 29 shaped Windows Text checkpoint on
SS-Note-ROG. Notion remains the parent's responsibility.

Proven: Point Delete ownership is in bad7a6c; OS geometry and DIN Alternate GDI
output were distributed in 6021b3e. Editable schema 28 OS Text is in 0662979.
The current candidate adds real DirectWrite face/weight selection, exact-face
Unicode shaping, multiline, size/tracking/line spacing/kerning/standard ligatures,
portable schema 29 snapshots and missing-font preservation. See
desktop/docs/windows-text-shaping.md for exact contracts, test evidence and limits.
Thirty native tests, 18 OS model/DOM tests, 43 existing outline/variable tests and
the production Edge/CSP typography flow pass. Native startup/package are pending
at this source checkpoint; browser IPC was explicitly stubbed with independently
captured native geometry and is not WebView2 acceptance.

Next task: Build the exact committed source, verify isolated native startup,
package EXE/ZIP separately from earlier deliveries, upload via the current Library
workflow and report confirmed IDs, SHA256, checks and unverified acceptance.

Approach: One source/build writer; bounded native read-only adapters; common JS
Session validates candidates and owns Undo/codec. Pure evaluation/export use
persisted contours without font services. All unsupported operations fail
atomically. Preserve existing variable axes/named instances and static vertical,
rich/path layout rather than silently flattening or replacing them.

Done for next: A separately versioned package with source SHA, checksums and
truthful evidence; then continue practical font acceptance before path extensions.

State: codex/desktop-production-20261016, candidate based on 0662979. Blank/old
documents remain schema 27 or their prior version until new features are authored;
schema 28/29 documents require the new reader. Existing 6021b3e/bad7a6c and older
b9fe3f/schema19 packages remain unchanged and are not the new font checkpoint.

LOCAL_WAIT: Actual WebView2 IME/high-DPI/picker and subjective typography acceptance.
The exposed host tools have no native UI controller; browser automation and actual
native adapter tests are available. Resume this row on native-control capability
or the user's hands-on evidence; it does not block independent font work.
New OS-route variable axes, vertical, wrap, rich/path/character adjustments and
color/bitmap fonts remain explicit limits; other existing routes retain their
documented abilities. Legacy GDI-only families retain their narrower script and
single-line limits. Do not describe font support as universally complete.

Authority: Current user-approved code/tests/commit/dev-branch non-force push/PR
update and EXE/ZIP/Library delivery. No main merge, release, force push or auth/
permission changes. No Notion mutation. Path A direct selection, point nudge,
Alt one-sided handles and endpoint extension remain deferred behind fonts.

Handoff: CONTINUE_CURRENT_TASK through this coherent font checkpoint. Earlier
requirements/history remain in Git and the existing canonical Notion references;
do not recreate a parallel tracker or expose private artwork/font bytes.
