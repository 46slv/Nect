# Live editor runbook

## Select the route

- Ordinary Windows Tauri: follow [the G19 transport and release gates](../../../../desktop/docs/g19-live-api-transport.md). Use the approved matching signed editor/helper, independently verified certificate fingerprint, and exact-client local pairing on a disposable document for the first pilot. Inspect does not grant edits. If this is unavailable, report the missing condition; do not use a test flag or change trust settings.
- Development fixture: only for disposable projects. Never register the fixture tools as ordinary Hermes tools. Follow [the Windows fixture setup](../../../../desktop/docs/windows-live-editor-acceptance.md), including the exact fixture EXE, reviewed checksum and new receipt path, then run its documented verifier. Record the exact fixture build and commit.
- Legacy Qt: use its own API/MCP only if that app is explicitly the target. Its tools do not imply Tauri support.
- File-only export: use [nect-file-export](../../nect-file-export/SKILL.md); it never edits the open editor.

## Discover and verify

Initialize the MCP session and use `tools/list`. Read the advertised Inspect schema and call it. Distinguish wrong edition, absent tools and missing grants. Match the document/tab and discover the relevant stable object IDs. For an uncertain write preserve its request ID where supported and inspect the receipt/state, not a new write.

Record app/build, tool and grant, before/after identity, produced file and verification limits. For an edit check exactly the intended fields and Undo. For persistence reopen and check authored state, not just pixels. A fixture save is not evidence of the Windows file chooser or ordinary Hermes pairing.

## Arrange artwork in the Windows development fixture

Use only tools advertised by this run. The `nect_test_*` tools below are
test-only and are not an ordinary Windows or Hermes MCP capability.

1. Inspect and list the authored layer IDs for the same document revision.
   Use exact IDs, never a name match or the currently selected layer.
2. Choose the intent before editing:
   - `nect_test_translate_artwork`: move explicit artwork by a document-space
     `delta` of `[x, y]`.
   - `nect_test_align_artwork`: use `reference: "selection"` (at least two
     layers) or `"artboard"` (one or more); specify an edge or a center axis.
   - `nect_test_distribute_artwork`: evenly space centers or edges while
     keeping the outer artwork fixed. Supply at least three layers.
   - `nect_test_distribute_artwork_gaps`: evenly space the **empty distance
     between shapes**, not their centers. Use `spacing: null` with three or
     more layers to keep the outer edges fixed, or a document-pixel number
     with two or more layers to keep the leading artwork fixed.
3. If any intended layer is locked, missing, or unsupported, stop this edit
   without removing that layer from the request silently. If the revision
   changed, inspect again and compare the intended targets. After an uncertain
   reply, inspect the result before making another relative move.
4. Verify the specific difference your edit should make: preserve the pinned
   artwork, check the moved coordinates or edge gaps, and save a copy. Reopen
   and verify authored positions. A successful export alone is not proof.

## Read and change a literal Fill in the Windows development fixture

When the fixture advertises both tools, inspect and list the layer and its
nodes at the same revision. Give the exact layer ID and Fill node ID to
`nect_test_read_solid_fill_color`. The result is authored RGB and alpha,
not evaluated appearance. A controlled Fill exposes neither its configured
fallback nor its source. Locked or bypassed literals are readable, not editable.

Change RGB with `nect_test_set_solid_fill_color` only for the intended
editable literal. It preserves alpha. Read again at the returned revision
before recording success; after an uncertain reply, re-inspect and read
instead of issuing a new write. For persistence, reopen the fixture copy,
use its new target and verify RGB and alpha again.
