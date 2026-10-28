# Graphic Asset Library first vertical

Design contract, 2026-10-28. Source owner: the isolated asset worktree,
`codex/nect-graphic-assets-20261028`, based on PR105
`48e41acf51a4cb9b8ddb2b255196a9e0da17d724`. The frozen font candidate,
main, native tester and Hermes remain separate.

## Scope and sources

The user's 2026-10-28 request authorizes design and implementation of this
separate checkpoint. Notion REQ-309 remains recorded as Candidate; this document
does not promote the whole requirement or its later pack-management proposals.
Sources read directly through the Notion connector:

- [REQ-309](https://app.notion.com/p/405fd279a6f381c3812fd8e05dab3000), edited 2026-10-27: visual SVG browsing, two placement routes, painted terminal Join safety, self-contained editable sources, provenance and explicit unsupported cases.
- [REQ-159](https://app.notion.com/p/3e0fd279a6f38190b65de94aefef9807), edited 2026-10-18: explicitly registered folders, hierarchy, search, refresh, source-location tracking.
- [G5 R10](https://app.notion.com/p/3eafd279a6f38171b6cdd3efa74f531f), edited 2026-10-01: root-relative identity, bounded reads, missing references, source preservation. Its Qt/QSettings and linked raster implementation is historical evidence, not production JS/Tauri functionality.

## Storage choice and editing contract

The user's 08:26 UTC clarification treats external folders/paths as one candidate,
requests both SVG and image data, and prioritizes easy addition and re-editing.

| Approach                                          | Implementation / portability                                                             | Source tracking / updating                                                                                      | Safety                                                                                                         |
| ------------------------------------------------- | ---------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------- |
| Registered folders, embed on placement (selected) | Reuses the OS folder picker and current Session importers; documents remain portable     | Exact root-relative identity, source location and original SHA-256; explicit Refresh changes the picker catalog | Originals are read-only; placed documents never change during Refresh                                          |
| Managed imported library                          | Adds a second persistent byte store, import/copy/delete lifecycle and backup/export work | Can keep an immutable library revision and explicit instance updates                                            | Strong long-term portable-pack option, but requires recovery/quota and revision contracts before becoming safe |
| Arbitrary linked paths in documents               | Initially small UI, fragile across machines                                              | Direct links need missing/relink and accepted-revision rules                                                    | Reject for this slice: implicit file updates could change existing artwork                                     |

The selected first slice supports SVG plus PNG/JPEG in one visual browser. Adding
a folder requires no relocation or modification of user materials. It is an
implementation choice for this checkpoint, not a new fixed product requirement.
Managed packs remain a successor option with the same stable provenance seam.

Three editing operations have deliberately different effects:

1. **Original file edit:** performed by the user's source editor. Explicit Refresh
   shows the new library revision. Existing documents retain their accepted copy.
2. **Library item edit:** this slice is a source browser, so it does not author a
   managed vector/raster master. Author/source/license fields are copied into the
   next placement. There is no hidden write-back or automatic instance update.
3. **Document copy edit:** Open source on an inserted SVG opens its editable
   document-local vector graph; Image uses existing image/node controls. Separate
   library placements mint independent SVG source graphs and raster assets.
   Generic graph duplication deliberately retains existing references; if multiple
   nodes share an SVG source, the inspector reports how many nodes it affects.

A later Update from source operation must preview the changed revision, select
specific document instances, preserve stable references where representable, and
apply one atomic Undo. It must never run as a side effect of Refresh or reopening.
Original relocation/overwrite/deletion, palette integration and third-party asset
downloads are outside this authorization.

## Working surface

Job: find a reusable visual part and place it without editing graph plumbing.
Primary actions: Add folder, Refresh, Place in new layer, Add to this graph.
The Assets panel uses the current neutral desktop controls: compact folder and
search controls, thumbnail grid with relative filenames, selected preview and
source/license metadata, and explicit placement buttons. It is separate from
the node-type library. Selection never inserts implicitly. Errors stay visible;
no third-party pack or font data is bundled. Self-authored SVGs are test fixtures.
Required states: unregistered, loading, populated, filtered, selected, unsupported,
missing/changed, cancelled, stale document, refresh error. Keyboard and named
buttons provide the same placement routes as pointer selection.

The current fixed right overlay (up to 570 CSS px) is provisional test placement,
not a final global-workspace layout decision. Closed/catalog/selected-detail
screenshots ground the separate frequent-versus-occasional controls discussion.
This slice does not add docking or further permanently visible controls.

## Session and persistence

`insert-svg-asset` owns the entire insertion transaction. A first-class `SVG`
node references a private editable vector-source Composition, using the existing
painted evaluation/reference engine. It is distinct from generic Composition
Reference and the historical Add SVG command. No second SVG renderer enters
the authored document. The source is accessible through Open source.

Route A adds a visible layer, SVG node, painted Join and Output; existing layers
retain identity, order and content. Route B defaults to an unconnected SVG node;
the user may enable Join before Out. Safe joining reuses only an enabled,
unlocked painted terminal Join whose sole dependency consumer is that Output.
All dependency types, including inactive edges and expressions, count. Otherwise
wrap the existing output input in a new Join. Preserve a bare Path's layer paint
through an explicit Fill on the existing branch. Never route SVG through Fill or
directly to Out. Append after the last occupied Join input; retained sockets,
connections and IDs remain unchanged. Reject locked output/budget/unsupported
cases atomically. One insertion is one Undo/Redo operation.

Native JSON stores the embedded vector graph plus validated SVG provenance on
the SVG node: original registered root ID, relative path, source location,
content SHA-256, author/source/license (unknown stays unknown). Reopening does
not reread the source. Missing/deleted originals cannot break embedded artwork.
The first vertical has no linked auto-reload. PNG/JPEG use the existing bounded
canonical PNG importer and new-layer Image route, with the same validated source
provenance on the embedded image asset. Existing images without provenance remain
valid. Both new SVG nodes and resource provenance require Desktop schema 53.

## Filesystem boundary

Rust owns the persistent registered-folder registry, native directory picker,
bounded traversal and opened-file checks. IPC accepts opaque root ID plus relative
path and refresh generation, never arbitrary absolute paths. At most 16 roots,
2048 visited entries, 8 nested directories, 12 MB per source and 32 MB accepted bytes
per refresh. Explicit Refresh replaces the catalog; no watcher or personal-folder
scan. Reject absolute/traversal paths, symlinks/reparse points, root replacement,
escaped final handle paths, changed bytes/identity and stale generation. Source
reads never write source files. Registry persistence is workspace state outside
Document Undo and must succeed before registration becomes visible.

Browser mode uses only explicitly selected directory Files and retains the same
relative identity and source checks for that selection; it has no persistent OS
directory capability. Re-select a directory to refresh. Native registry survives
restart, while its byte catalog is refreshed explicitly before use.

Browser Add and Refresh carry distinct picker intent. Refresh captures the
selected root ID, validates the complete replacement selection, and replaces its
Files under that ID. It requires the selected folder name; a different name or
Cancel leaves the old root untouched. Refresh never consumes another registration
slot, and clears old previews before reading the new catalog. Browser capabilities
cannot verify an absolute OS path; explicit re-selection supplies that capability.
Native registration returns `{ roots, selectedRootId }`, including an existing
root selected again. The UI never infers that selection from list position.

Browser-only limit: an empty folder can produce the same zero-file/cancel outcome
as dismissing the chooser. That outcome keeps the previous selection snapshot;
it does not claim the physical folder is still populated. The status names the
unchanged catalog and points to Remove as the explicit clear action. Remove
unregisters the root and clears its catalog, leaving originals and placed document
copies untouched. Add the folder again after it contains selectable files. Native
Refresh reads the registered directory directly and returns an empty catalog
under the same root ID when the last source is removed.

In isolated native acceptance mode, selected roots are canonicalized and checked
against the canonical run root before admission. A parent junction inside the run
root cannot register an ordinary descendant whose actual directory is outside.
Production still accepts explicitly selected ordinary roots without that test-only
scope. Owned negative fixtures cover this boundary and its normal positive paths.

## Acceptance and residuals

### Next bounded slice: destination-based drag placement

The user's 10:23 UTC direction chooses the route from the drop destination:
SVG on the current node graph adds an SVG node there; SVG on the artboard adds a
new layer. Only the chosen destination needs to be visible. Existing placement
buttons remain the keyboard/accessibility fallback. This is the next slice;
drag/drop is not claimed by the currently verified button-based vertical.

Current core contracts need a deliberate extension: SVG `at` is artwork-space
translation, while its graph-card position is automatically chosen. Drag needs
a separate validated graph `nodePosition` in the same insertion transaction.
The image command currently centers/fits a new Image layer; artboard image drop
needs an explicit artwork position without changing accepted pixels. The first
compatible image drag route should be artboard/new Image layer; graph image drop
must refuse explicitly until a real Image-node insertion contract exists.

The GUI must invert the current artboard pan/zoom transform for artwork drops,
and separately convert screen coordinates through graph scroll/zoom for graph
card placement. Raw screen pixels are never authored artwork coordinates.
Imported bounds, viewport mapping, paint and source geometry stay intact. Graph
artwork centering is a candidate, not a settled requirement: keep the existing
source-origin `[0, 0]` default for review until an explicit bounds/anchor rule is
selected. Graph-card coordinates must not move the artwork.

Drag start captures document/session/revision, active Composition/graph, catalog
generation, source identity/hash and explicit Join intent. Drop rechecks those
targets after asynchronous source reading. Unsupported targets, stale graph or
document selection, Escape, close and drag-end without a drop admit no history.
One completed drop registers source/asset, layer/node and optional safe terminal
Join as one Undo. Preserve the same exclusive/shared Join rules and no forced
Fill/Out binding. Tests must cover pan/zoom placement, distinct graph/artwork
coordinates, images, cancellation, unsupported targets, stale reads, repeated
placement, Undo/Redo and save/cold reopen before claiming drag support.

The 10:26 UTC preferred layout candidate is a closable panel reached from a thin
right-side panel bar, while the left toolbar stays focused on editing tools.
One-panel-at-a-time and same-button toggle are initial concepts; pinning remains
a proposal. The 10:35 UTC hands-on layout candidate keeps Layers almost fixed,
with adjustable height as a concrete desired capability, and opens occasional
tools above the Inspector. This is design-only; layer resizing and Inspector
placement are not implemented by this checkpoint. No unfinished color panel or
global docking rewrite is authorized by this slice. Exposing the chosen drop
surface, for example by temporarily closing the library during drag, must be
tested without requiring both artboard and graph on screen at once. The current
570px overlay remains a screenshot reference, not the final layout contract.

Tests must exercise both insertion routes; unconnected B; exclusive/shared/
disabled/locked terminal Join; paint and viewport bounds; repeated imports;
malformed/untrusted SVG; exact atomic failure and redo preservation; selection;
Undo/Redo; native project save/reopen; missing original; cancelled picker and
async stale document; registry persistence and boundary negatives. Real browser
UI evidence, build/format/unit checks and CI bind the final candidate. Windows
foreground testing requires a fresh explicit GUI availability grant.

Deferred: drag/drop coordinates, favorites/quick access, pack manifests and bulk
license editing, linked resources/relink, existing-raster provenance migration, palette
integration, SVG features already rejected by the existing importer. No claim of
complete REQ-159/309 acceptance or native hands-on acceptance follows this slice.
