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

In isolated native acceptance mode, selected roots are canonicalized and checked
against the canonical run root before admission. A parent junction inside the run
root cannot register an ordinary descendant whose actual directory is outside.
Production still accepts explicitly selected ordinary roots without that test-only
scope. Owned negative fixtures cover this boundary and its normal positive paths.

## Acceptance and residuals

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
