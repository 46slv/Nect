# Unified Font UI integration checkpoint

This development draft wires the unified picker into the normal and Graph
Inspectors on frozen color PR #127, retaining accepted PR #126 core admission.
The source is not yet fully application-tested; native fonts are not promised
in browsers, and mock fixtures do not prove real native font availability. Source inputs were read
at accepted PR #126 commit `d9bdaeb66eace9f9ab1a15fea80665415a4628bf`;
`evidence/input-blobs.json` records their exact Git blobs. The color writer released production ownership at PR #127 commit
`891c1101f0e4723a29a0423bef3ed9e2633c0a85`, followed by the isolated
Advanced-disclosure fix at `0bc87cf435c1cf31e43aa75d92bc97ca59d48024`; `evidence/base127-blobs.json`
records the exact integration inputs. Color controls/tests remain unchanged.

## User interaction

- A normal **Font / Change font** entry opens one catalog of document fonts and
  available exact Windows geometry-only faces. Unproven byte-only installed
  discoveries remain visible but disabled: **Font file import is required; use
  Advanced for a file you may embed**. This is a capability/import requirement,
  not a judgment that all OS fonts are forbidden. Normal selection never reads
  installed font bytes or copies them into saved JSON. Source is metadata, not
  a decision about rendering engines.
- Existing search, localized aliases, real family/style metadata, Favorites and
  static canvas previews remain. Family headings expand/collapse with pointer or
  Enter/Space/Left/Right keys; the selected family starts open and search reveals
  matching families/faces. Headings use exact adapter namespace plus real family
  metadata, never merge equal labels across unproven sources, and retain every
  child opaque face ID. Unknown metadata stays per-face, not a guessed family.
  Rows sort by normalized family, real weight,
  style, label and opaque source identity. This is Nect's deterministic order;
  Illustrator's internal ordering algorithm is not known or claimed.
- Unsupported choices remain visible with a reason. Native face discovery is
  not preparation/coverage proof. Actual Session admission remains authoritative.
- Static/manual variable imports live in **Advanced font import**. Explicit
  **Convert to editable paths** lives in **Text actions**, using current
  appearance and warning that Text/font editing will be removed. Choosing a font
  never silently outlines Text, removes overrides or downloads fonts.
- A normal static font's implicit/Legacy liga-off intent is passed explicitly
  as `settings.ligatures:false` to native preparation and Session admission.
  Saved native true/false intent is retained. A mismatched prepared response
  refuses. Explicit static OpenType settings remain blocked by the accepted
  bridge; none are deleted to enable native selection.

Priority acceptance examples are activated Adobe Fonts and Morisawa faces,
especially 中ゴシックBBB, the still-ambiguous A1 family variants, and 筑紫明朝.
These fonts are not installed on the user's WS, so this checkpoint claims no
real-font testing or particular A1/筑紫 variant. Readable font bytes/fsType flags
do not prove provider permission. Native geometry stores only actual document
text snapshots under the existing contract, never a complete substitute font.
Current native discovery supplies only its canonical family/style names; actual
localized aliases require an additive discovery-metadata extension. Do not
claim Japanese aliases searchable unless present in real metadata, translate
names by guessing, or match native and byte choices by similar names.

## Main integration points at PR #126

1. Import `fontPickerCatalogs`, `fontPickerItems`, `selectedFontPickerId`,
   `migrateFontFavorites`, `createFontChoiceController` and the Inspector
   templates. Keep original read-only discovery adapters and exact descriptors.
2. `textControls` (around lines 3317–3342) must use the same normal Font entry
   for static and saved Windows Text. Remove separate **Choose OS font**,
   **OS font / Adobe Fonts → paths** and **Use DejaVu Sans** bypass controls.
   Put import and outline templates after the ordinary typography controls.
   Keep native axes, multiline, vertical and wrap controls intact. Rename the
   native typography heading, not its capability. Split `variableControls`
   (around 3240) so its manual-load button is not duplicated outside Advanced.
3. Replace `openFontPicker` (around 5727) with one catalog/controller adapter.
   Freeze owner/envelope/layer/node/character IDs before async work. Existing
   `openSearchPanel` still owns keyboard/IME, search/filter/Favorites and dialog
   Cancel/Escape. `onSelect` calls controller.select; `onCommit` calls apply;
   `onCancel` calls cancel. Use row.key, not display names, as picker IDs.
4. Adapter freshness checks must include Session identity, exact revision,
   active layer, selected node and selected stable character IDs. Reject live
   Text drafts before opening. `onPreview` updates only detached `fontPreview`
   and Canvas; `onApplied` closes the exact picker then renders/announces.
5. Native preparation calls existing
   `prepareDesktopText(source, exactFace, options.settings)`; the controller's
   callback options include the preserved effective ligature intent.
   The controller compares every face descriptor field and ligature intent before
   Session `set-os-font-text`. It never falls back to family-name lookup or the
   old legacy four-style outline adapter.
6. Prepared-variable adapter returns a ticket with **both** `commit()` and
   `cancel()`. It calls owner.prepareCommand with the captured envelope and
   existing variable bridge, then owner.commitPrepared/cancelPrepared. Only
   document fonts enter this callback from the normal picker. Installed bytes
   require explicit file import through Advanced; do not add a catalog-ID-to-
   asset fallback or infer provider permission. Explicit static import on
   variable Text remains one prepared `set-variable-font` commit with the
   parsed asset's actual ID and `payload.asset`, not clear/adopt.
7. Pointer/keyboard delegation currently recognizes `[data-font]` and selected
   character font. Add `[data-change-font]` and route both normal Inspector and
   Graph inspector to the same picker. Keep character **Inherit default font**
   as the explicit UI-only special row; its actual empty font update retains
   the fixed stable character range. No accepted catalog ID rule is weakened.
8. Manual `commitFont` (around 8212) must choose `replace-os-text-font` for OS
   Text, prepared static asset replacement for variable Text, and `adopt-font`
   otherwise. Preserve its read/revision/character-selection guards. Failed
   preparation or unsupported intent must leave snapshot/history untouched.
9. Remove obsolete normal-flow `openOsOutlinePicker` and its handlers after
   replacing assertions that exercised it. Explicit conversion uses only the
   existing bake-current-appearance command in Text actions.
10. `arrangeTextInspector` currently returns unless `.text-controls [data-text]`
    exists. It must also handle saved/native Text's `[data-os-text]`, keep the
    unified Font entry primary, keep Advanced import/Text actions separate, and
    preserve character mode plus normal/Graph ownership.

## Verification after shared-file handoff

Run focused existing core/static-font/native/variable tests plus the new module
tests. Update existing UI tests without removing their old safety assertions:
`font-dom`, `font-style-dom`, `system-font-dom`, `os-font-outline-dom`,
`variable-font-ui`, `text-inspector-layout`; real-browser `system-font`,
`font-switch`, `desktop-font-text`, `os-font-outline`, `text-inspector-layout`.
Add browser mock coverage for a single normal entry, available native and
embedded choices in one search, native-to-static atomic replace/Undo/Redo,
unsupported reasons, Cancel/Escape during preparation, rapid selection, stale
document/selection, no font-name fallback and no implicit outline conversion.
Mock tests prove routing and UI, not actual Windows discovery/preparation.

Full formatting/unit/build plus Windows Chrome/native CI must be checked at the
exact final integration head. No local Chromium/socket retry or package install
is authorized in this checkpoint. Do not publish these unwired modules as a
completed UI or merge/release them.

## Bounded follow-on: app-private file fonts (not implemented)

The normal OS catalog does not load uninstalled font files. The supplied BBB/A1
samples are not real-font acceptance fixtures and are not uploaded to public
CI. Their preview/print-only embedding flags must continue to reject byte
embedding; no original font bytes, format or flags are changed.

After higher-priority color/interaction work, the approved follow-on direction
is an explicit native file picker that grants an opaque capability for one
selected file, a content fingerprint and its actual faces. TTF/OTF files and
TTC/OTC collections must be enumerated honestly: choose an actual member face
with its file fingerprint and face index, not a guessed first face or a rewritten
font format/permission flag. Prefer a
process-private DirectWrite collection, not OS registration/installation. Shape
only the actual document text into the existing geometry snapshot. Save no font
bytes, file paths, private glyph cache or sensitive cache metadata in projects.
A bounded authored source marker must distinguish file-native from installed
faces, retain saved appearance and honestly mark a missing file on reopen.
Reattach requires the same content fingerprint; equal names are not evidence.

This requires a separately reviewed backend/schema scope and an authorized
local native environment for private supplied-font acceptance. Existing native
path/security validation stays strict; JavaScript cannot submit arbitrary native
paths. Licensed fixtures and synthetic native adapters are the only CI inputs.
Current UI integration does not close this loading/acceptance gap.
