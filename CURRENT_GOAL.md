# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — OS Text wrap, 2026-10-18

Goal: Continue the user's practical font support before the remaining A direct
selection, selected-point nudge, Alt independent handle and open-end continuation
work. Preserve all previous EXE/ZIP checkpoints, point deletion ownership, generated
path protection, stable identity and one-operation Undo. The parent owns Notion.

Current phase: OS horizontal wrap is implemented and verified; commit/build/deliver
this coherent checkpoint, then advance directly to path interactions. Windows DirectWrite uses whole-word wrapping at an explicit positive
width, auto height and no clipping. Long unbreakable words retain all glyphs and
report overflow. Windows line-break rules are used; full Japanese kinsoku and
vertical wrap are explicitly unsupported. Existing Text wrapping remains intact.

Proven: 4a0a277 is pushed and distributed with Composition references; Library
libfile_a365dacaf9dc819184aec949bd8476d0, ZIP SHA256
694AD543AC71D42BDB20CDFD949112EAC4187B5C44A3A03B17E70D1351682D9B.
It includes font checkpoint 4c4af2f (OS axes, Japanese vertical, DIN isolated factory
and GDI interop). Source/receipts/details are in PR24 issuecomment-6246568900.
New native suite passes 34/34, including actual wrapping, fractional widths, spacing,
alignment, overflow and malformed/vertical requests. New model/codec wrap checks
pass 3/3 with actual native fixtures; production Edge/CSP wrap controls, width/off/Undo, alignment, spacing, refusal,
JSON reload and SVG/PNG passed; 45 focused model/DOM checks pass. A legacy
wrap-schema test now explicitly selects its pre-wrap schema instead of assuming
the current document default.

Next task: Commit and build a separately identified schema32 package; continue
the four remaining
path interactions after comparing current implementation, without stopping at
commit/distribution. Do not redo completed behavior.

Approach: Existing textWrap property and common set-os-font-text preparation,
with atomic source/settings/outline adoption, stale/locked/IME guards and Undo.
New OS reference version4 / directwrite-v3 includes wrapWidth and overflow; older
unwrapped references retain their prior contract. Saved geometry requires no font
service to render/export. Native request remains geometry-only, never font bytes.

Done for next: Actual wrap controls and placement/tracking/lineSpacing/alignment,
width changes/off/Undo, JSON reopen and SVG/PNG are coherent and verified.

State: codex/desktop-production-20261016, HEAD 4a0a277, wrap work uncommitted.
LOCAL_WAIT: Native WebView2 IME/high-DPI/full manual editing. Exposed tools have no
native UI controller. Resume on formal native-control capability or hands-on user
evidence; this does not block independent implementation. OS rich/path/character,
custom leading, color/bitmap remain explicit limits. No universal completeness.

Authority: Current delegated user instruction permits code/tests/commit/dev
non-force push/PR update/EXE/ZIP/Library. No main merge/release/force push or
credentials/auth/permission changes. No Adobe cache/font software extraction or
bundling. Do not mutate Notion.

Double-check: critic gate invoked for saved format/layout boundary. Question is
whether wrap preserves source text, coordinate anchoring, prepared snapshots and
existing routes. Local native/codec/transaction/browser evidence is the bounded
verification route; consultation not-required, zero external sends. No unresolved
product choice; full kinsoku is expressly outside the current capability claim.

Handoff: CONTINUE_CURRENT_TASK. Keep progressing through eligible source work;
only declare human-only remaining work after checking the actual route.
