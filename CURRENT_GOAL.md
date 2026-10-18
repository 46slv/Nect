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

Current phase: The font checkpoint is preserved at 4c4af2f (schema 30),
with verified Library ZIP libfile_2644c94a2edc819198b6fc4226bfb6a2. The new
same-document Composition Output reference is implemented; validate and build
its separate schema 31 checkpoint. Notion remains the parent's responsibility.

Proven: Point Delete ownership is in bad7a6c; OS geometry and DIN Alternate GDI
output were distributed in 6021b3e. Editable schema 28 OS Text is in 0662979.
The delivered bc5ad1a adds real DirectWrite face/weight selection, exact-face
Unicode shaping, multiline, size/tracking/line spacing/kerning/standard ligatures,
portable schema 29 snapshots and missing-font preservation. See
desktop/docs/windows-text-shaping.md for exact contracts, test evidence and limits.
Thirty native tests, 18 OS model/DOM tests, 43 existing outline/variable tests and
the production Edge/CSP typography flow pass. Its native build/startup and Library
upload succeeded: libfile_cf685ae2da148191bf0ee28b6ff90571. Browser IPC was explicitly
stubbed with independently captured native geometry, not WebView2 acceptance.
The DIN repair preserves 637 GDI families after DirectWrite initialization and
exposes 633 unified real faces, including DIN Alternate Bold/700. Native and
production fixture workflows prove lowercase/multiline DIN, portable reload and
exact SVG/path output. No reactivation/account change is needed for that repair.
The schema 30 candidate adds geometry-only variable axes and vertical snapshots.
Native 33/33, OS model/DOM 20/20 and existing outline/variable/point 49/49 pass.
Production Edge/CSP covers axes/reset/range failure, vertical mode/spacing, Undo,
JSON save/reload, cached-font absence, SVG/path parity and PNG; IPC remains a
fixture using separately tested native output. Actual WebView2 IME/DPI is open.

Next task: Commit and separately build/package this font checkpoint. Then inspect
the existing Reference/evaluation model and add or extend the smallest equivalent
for another same-document Composition's Output: stable target ID, live source
updates, normal placement/Transform/duplication, alpha/holes without a baked
background, save/reload, explicit missing targets and self/indirect cycle refusal.
The user's 14:51 JST request authorizes this next bounded feature; external-file
links and video are excluded. Do not stop at commit/push or ZIP boundaries.

Approach: One source/build writer; bounded native read-only adapters; common JS
Session validates candidates and owns Undo/codec. Pure evaluation/export use
persisted contours without font services. All unsupported operations fail
atomically. Preserve existing variable axes/named instances and static vertical,
rich/path layout rather than silently flattening or replacing them.

Done for next: Actual OS variable-axis and vertical/multiline font evidence,
portable geometry snapshots and a separately versioned verified package; continue
practical font acceptance before path extensions.

State: codex/desktop-production-20261016, Composition candidate based on 4c4af2f. Blank/old
documents remain schema 27 or their prior version until new features are authored;
schema 28/29/30 documents require their corresponding reader. Existing bc5ad1a,
6021b3e/bad7a6c and older
b9fe3f/schema19 packages remain unchanged and are not the new font checkpoint.

LOCAL_WAIT: Actual WebView2 IME/high-DPI/picker and subjective typography acceptance.
The exposed host tools have no native UI controller; browser automation and actual
native adapter tests are available. Resume this row on native-control capability
or the user's hands-on evidence; it does not block independent font work.
OS-route wrap, custom leading, rich/path/character adjustments and color/bitmap
fonts remain explicit limits; other existing routes retain their
documented abilities. Legacy GDI-only families retain their narrower script and
single-line limits. Do not describe font support as universally complete.

Authority: Current user-approved code/tests/commit/dev-branch non-force push/PR
update and EXE/ZIP/Library delivery. No main merge, release, force push or auth/
permission changes. No Notion mutation. Path A direct selection, point nudge,
Alt one-sided handles and endpoint extension remain deferred behind fonts.

Handoff: CONTINUE_CURRENT_TASK through this coherent font checkpoint. Earlier
requirements/history remain in Git and the existing canonical Notion references;
do not recreate a parallel tracker or expose private artwork/font bytes.


## Composition Output reference checkpoint — 2026-10-18

The user's 14:51 JST request adds same-document reusable Composition parts.
Composition Reference stores a stable compositionId, reads the source's live
Output even when hidden, and retains an explicit missing target. Self/indirect
cycles reject atomically. The node's Position, normal artwork Move, duplication,
paint-aware Transform and ordered Join preserve independent fill/stroke/gradient/
image appearance and transparent holes. No background is baked. Geometry-only
consumers require explicit Convert to Path; source typography export restrictions
and warnings remain effective. Schema 31 persists references and paint routing;
old documents retain their prior schema until the new feature is authored.

Evidence: related model/DOM regressions 81/81, including 7 Composition contract
checks, plus 27/27 point/gesture/insertion checks; production Edge/CSP dropdown, numeric Position, actual canvas drag (including one instance inside Join),
painted Transform canvas controls, duplicate, source navigation/edit reflection, Transform wiring, Undo and JSON
reload passed. Actual exported PNG alpha at background/single/hole/other-instance/
overlap pixels was [0,128,0,128,192]. Gradient IDs, stroke and nested Image output
retention were checked. Font production regression again passed DIN, axes,
Japanese vertical, settings, save/reload and SVG/PNG with explicit native fixtures.
Native WebView2 IME/high-DPI remains the same LOCAL_WAIT above.

Double-check gate: critic, saved-format/appearance boundary; question: whether
Composition identity, cycles, cache invalidation and paint/alpha survive without
silently changing existing Path semantics. Local counterexample review adopted
explicit paint routing, typed unconfigured-target states and propagation of source
export restrictions. Consultation not-required; local codec/atomic-command/
production raster evidence resolves this bounded decision. Zero external sends;
no material unresolved design decision, cycle 1. Scope excludes external-file
links and video; edit source paints in the source Composition.
