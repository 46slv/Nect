# Desktop integration and shared editing contract

The desktop editor was adopted from `46slv/Nect-SVG` at
`107a154e8d5a887f4bc551ab7bcafbc88773da87`. Imported licenses and lockfiles
are retained. Tauri/WebView2 hosts the editor; the DOM-free JavaScript Session
owns authored graph state, stable identities, commands and Undo/Redo. Rust owns
native file bindings, not a second editing history. Legacy C++/Qt formats remain
separate references rather than an implicit import or compatibility promise.

## Document lifecycle

The Canvas/Graph/Inspector surface exposes filename, dirty and saving state,
with distinct Save and Save As. New, Open and normal Close share Save, Discard
and Cancel. Conflicts and failed writes preserve current edits. Active gestures,
Pen paths and IME composition must finish or cancel before document transitions.
Normal-close checks do not establish forced-kill or crash recovery.

External disk checks detect observed byte changes. Checking before replacement
is not an interprocess compare-and-swap guarantee against noncooperating writers.

## One command and history model

`core.js` `CommandSession` owns one History. Commands resolve stable IDs and
validate private candidates before committing. Session identity and monotonic
History revision reject foreign or stale requests. Invalid commands preserve
authored state, revision and the redo branch; caller payloads are copied.

GUI controls and `window.nect.dispatch` use the same command entry points:

- Graph wiring and operation changes use typed connect/disconnect commands and
  retain Closed Offset/Text Path protections.
- Parameters, expressions and locks use `set-parameter`,
  `set-parameter-expression` and `set-parameter-lock`, with typed values, domain
  limits, connected/driven checks and expression validation.
- Artwork creation, deletion, ordering, duplication, translation, alignment and
  distribution use atomic commands. Each selected layer is re-resolved by ID;
  a late invalid member cannot leave partial movement. Independent creation
  remains possible beside an existing locked layer.
- Names, layer/document state, node creation/duplication/deletion/preview,
  bypass, references and node settings use typed commands. Deleting a source
  preserves fallback values and diagnosed dangling-reference identity. Failed
  preview creation does not leave a provisional Fill.
- Character offsets, absolute transform patches, pair kerning, scoped/full
  resets, Guide settings/layout and discrete Curve/Remap actions share the same
  candidate validation and history boundary.

The UI owns selection and view state, not a second authored writer. Selection
restoration is associated with the actual stored History snapshot. Visibility
and unlock controls remain available on locked layers; protected authored edits
still refuse. Stable point and handle keys are identities, not array positions.

## Text drafts and asset adoption

Text uses `set-text-content` or Session-private
`begin/stage/commit/cancel-text-draft` commands. `text-draft.js` owns reconciled
character maps and up to 100 native-input snapshots, with a separate draft
revision. UI adapters supply observed UTF-16 ranges, text and composition events;
returned metadata cannot install an authored character map.

Composition captures the initial range and reconciles once at completion.
Known native textarea Undo/Redo restores Session-owned snapshots; ambiguous
edits retain suspended identity rather than inventing mappings. Document
Undo/Redo refuses active composition. Stale or foreign drafts cannot replace
current work. Same-string replacements are real edits when identities change;
no-op commits preserve document redo.

`set-text-settings` covers wrapping, path attachment, leading, alignment,
kerning, font references and writing direction. `set-character-style` covers
selected typography. `adopt-font` prepares bounded raw bytes, preflights outlines
and atomically adopts the asset and default/range reference. Its asynchronous
adapter retains the pre-read Session/revision and source/request/selection
checks, so Undo back to equal bytes does not admit an old result. The documented
bounds are 20 MB per font, 32 MB total, eight fonts and a 50 MB project.

`adopt-image` accepts canonical PNG bytes and a name. Core derives dimensions,
allocates fresh asset/layer/node IDs and commits the asset/reference together.
The browser retains PNG/JPEG decoding and PNG normalization. Malformed or
over-budget data, stale reads and late failures leave document and History
unchanged; no second codec or persistence format is introduced.

## Exclusive gesture drafts

`begin/update/commit/cancel-gesture` uses one exclusive Session-private typed
draft. Begin fixes stable targets and a baseline. Every update derives from that
baseline, runs shared candidate validation and returns an output-only detached
preview. Preview revision is separate from document revision. Invalid updates
retain the last valid preview; ordinary commands and text drafts are excluded
while a gesture owns the slot.

Commit revalidates and records one History entry. A no-op preserves redo and
stored document identity. Exact baseline guards detect out-of-band edits even
without a revision change. Exact-token cancellation remains available after a
stale revision without overwriting committed work. Save and export read committed
state, never an intermediate preview.

Graph position/width, numeric and typography scrubs, artwork/node translation,
shape/Transform handles, position controllers, character movement, Guide
positioning and Curve/Remap point edits follow this model. Relative Curve
handles, Remap enable/fingerprint checks, mixed values and sparse inheritance
are preserved. A zero delta does not allocate retained edits or consume redo.
Wire drops keep their opening Session/revision envelope.

Pen uses the same draft slot. Core allocates layer/node/point IDs and owns the
pressed-point handles, accept/cancel, whole-path cancel and finish. Pointer
interruption cancels only the pressed point; Enter/Escape finishes released
points; short paths discard without History. Mutating a detached preview cannot
inject authored identities. Save during an active Pen draft refuses until
Finish or Cancel.

SVG import lowers browser-parsed XML to portable syntax data. The DOM-free
Session validates the supported subset and constructs fresh IDs/graph in one
undoable document replacement, retiring the old source binding. Unsupported
syntax preserves current work.

## Read-only public projections

`historyView` exposes frozen states and copied/frozen History arrays while
retaining the object identity required for selection restoration.
`window.nect.document` is a detached committed snapshot. Debug evaluation uses
its own snapshot so caller changes cannot poison renderer caches. Validation
checks existing graph/socket structure without rewriting it. Shared asset
metadata is deeply frozen. Public snapshots, previews and results cannot install
caller-authored maps or mutation callbacks.

## Search, font browsing and alignment

Ctrl+Space and Add node expose the same searchable insertion list. Text fields,
composition, dialogs and active edits retain their shortcuts. Enter adds one
node with one Undo; Escape or no match adds nothing. The opening Session,
revision and layer remain captured until submission.

Font browsing previews the selected Text on the actual Canvas through a private
font gesture. It fixes source and stable character IDs, retains one candidate
and leaves committed save/export unchanged. Cancel/no-op preserves document and
redo. Invalid faces preserve the last valid preview. Enter during a pending
load waits for that exact request; stale asynchronous results cannot commit.
Favorites use face names and local UI storage rather than catalog indices.

Compact alignment controls remain available beside Layers and the independently
scrolling Inspector. Help is optional; errors, unavailable reasons and
saving/dirty state remain visible when relevant. The panel reuses
`align-artworks`, `distribute-artworks` and `distribute-artwork-gaps`:

- Alignment supports Selection bounds or Artboard, including one artwork against
  Artboard. Distribution fixes endpoints and supports leading/center/trailing
  positions on each axis.
- Auto gaps retain both physical outer edges. A numeric gap fixes the leading
  artwork and places later artwork at that gap. Invalid routes, locks, values
  or ordering refuse atomically. No-op preserves redo.
- Panel visibility/reference/spacing are UI preferences outside the document.
  Updating a spacing field must not replace a pressed action target before its
  click is received.

[Compolign](https://github.com/46slv/Compolign/tree/3bd4d2ad6bd5fd85e5faf12828833970b6790bb9)
provides a comparison for compact controls and explicit reference selection.
Its grid Walk/Dock and Group/Each behaviors are distinct from Nect distribution
and are not implied by these controls. No After Effects-specific parent-layer,
rotation or 3D restrictions are imported.

## Graph selection and direct text editing

Empty-graph drag selects intersecting cards in either direction; Shift adds.
A four-CSS-pixel threshold separates click from marquee. Escape, blur, pointer
cancellation or capture loss restores starting selection without changing
Session revision or Undo/Redo. Wiring retains pointer capture and edge autopan;
completion/cancellation stops the loop and resolves drop targets at their current
screen positions.

Selected supported graph outputs resolve to artwork or Vector2 translation
targets rather than moving graph cards. Source/Fill/Output selections deduplicate,
shared position owners move once, and Curve handles stay relative. Mixed Text,
shape, Curve, Transform, Vector and Null selections use the supported common
translation candidates. Driven, locked, independent or unsafe routes refuse the
whole command visibly. Distribution remains artwork-scoped.

Canvas Text double-click resolves displayed glyph geometry to its Text source;
ambiguous sources do not choose arbitrarily. A transparent native textarea
receives editing while SVG owns visible glyphs, caret and range selection.
Session draft projections synchronize Canvas, Graph and Inspector without
replacing the focused input. Mixed metrics, horizontal/vertical writing,
downstream transforms, zoom and pan feed the geometry. Composition remains at
the last accepted text until completion. Commit is one history operation;
Escape cancels exactly. Unchanged editing preserves original CRLF bytes; real
edits may normalize newlines, and Undo restores the original source.

## Verification boundaries

Historical migration checks included complete JavaScript suites at specific
intermediate revisions (659, 677, 685, 687 and 694 passing tests), focused repair
runs, production Web builds and offline Windows builds. Chromium flows covered
GUI/headless parity, Undo/Redo, stale refusal, gesture preview isolation,
composition exclusion, graph/wiring, Japanese fonts, JSON reload and SVG/PNG.
Later UI repairs were checked with focused tests rather than relabeling an
earlier aggregate as a new full-suite run.

The later direct-text/Vector repair check passed 53 focused tests and scoped
Chromium flows, not a rerun of its earlier 703-test aggregate. A bounded native
WebView2 check then exercised the binary with SHA-256
`A0AD15F43EE8B33213D1AB529E21BECDD3C24C89FF877FE5A3D6E778643EE2E7`,
using synthetic CDP input and inspected native screenshots. It covered point/mixed
alignment, output deduplication, atomic refusal, caret/ranges, typing/deletion,
newlines, transformed/vertical text, CRLF preservation and exact Undo/Redo.
Normal Close/Discard of a disposable document exited successfully. These are
native-application checks, not physical input, physical Japanese IME, or general
production acceptance, and do not qualify the current tree automatically.
Earlier lifecycle/save evidence remains bound to its
original binary and does not automatically qualify later core or UI changes.

Subsequent Transform/Remap/numeric extension work required separate verification:
legacy JSON must not gain optional fields on load; absolute Position and legacy
Offset must compose correctly; explicit Anchor editing materializes Position;
independent scaling, driven fields, locks, typed-percent-to-drag continuity,
screen-sized point hit targets and cursor-centered Alt-wheel zoom must retain
Undo/cancel behavior. An early extension snapshot had focused tests only, not
combined browser/native/cold-reopen/export/build/package acceptance. See
[editing refinement](editing-refinement.md) for the feature-specific contract.

[Adobe's Transform effect documentation](https://helpx.adobe.com/after-effects/desktop/apply-effects-and-animation-presets/list-of-effects/distort-effects.html)
names Skew and Skew Axis but does not establish the signed matrix order. No
comparison raster or parameter oracle established numerical After Effects
parity. Physical IME, subjective interaction quality and any unexecuted native
file-dialog/cold-reopen checks remain explicit separate acceptance obligations.
The documentation-only update does not rerun or expand any of these product
verification claims.
