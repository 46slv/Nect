# Live editor runbook

## Select the route

- Ordinary Windows Tauri: follow [the G19 transport and release gates](../../../../desktop/docs/g19-live-api-transport.md). Use the approved matching signed editor/helper, independently verified certificate fingerprint, and exact-client local pairing on a disposable document for the first pilot. Inspect does not grant edits. If this is unavailable, report the missing condition; do not use a test flag or change trust settings.
- Development fixture: only for disposable projects. Never register the fixture tools as ordinary Hermes tools. Follow [the Windows fixture setup](../../../../desktop/docs/windows-live-editor-acceptance.md), including the exact fixture EXE, reviewed checksum and new receipt path, then run its documented verifier. Record the exact fixture build and commit.
- Legacy Qt: use its own API/MCP only if that app is explicitly the target. Its tools do not imply Tauri support.
- File-only export: use [nect-file-export](../../nect-file-export/SKILL.md); it never edits the open editor.

## Discover and verify

Initialize the MCP session and use `tools/list`. Read the advertised Inspect schema and call it. Distinguish wrong edition, absent tools and missing grants. Match the document/tab and discover the relevant stable object IDs. For an uncertain write preserve its request ID where supported and inspect the receipt/state, not a new write.

Record app/build, tool and grant, before/after identity, produced file and verification limits. For an edit check exactly the intended fields and Undo. For persistence reopen and check authored state, not just pixels. A fixture save is not evidence of the Windows file chooser or ordinary Hermes pairing.

## Create a primitive in the Windows development fixture

Use only a disposable document with the fixture tool advertised. For a
Rectangle, Circle, Ellipse, Polygon, or Star, inspect the document and
record its initial layer IDs. Call `nect_test_create_artwork` with that
`target`, the exact `artworkType`, and `at: [x, y]` in artboard coordinates
(the shape center, not screen coordinates). This appends a new artwork
layer without replacing an existing output. For the next write, use the
returned target, not the one from before creation.

Check the new stable layer and node IDs by listing at the new revision.
If the reply was lost, inspect and list again; reconcile against the
initial IDs and authored state instead of blindly creating a second copy.
After Undo/Redo, confirm the restored IDs before editing. Save to
the fixture copy, reopen and check the authored center, type and ID order.
Verify SVG and PNG separately from project persistence. Use the primitive
shape run in [Windows fixture setup](../../../../desktop/docs/windows-live-editor-acceptance.md)
to reproduce all five shapes; this is not a normal-app pairing test.

## Inspect and edit Vector2 wiring in the Windows development fixture

With the fixture tools advertised, inspect for a fresh target, then use
`nect_test_list_artwork` to find exact layer and node IDs. Use
`nect_test_read_node_ports` for the available input/output IDs and types
and `nect_test_read_node_input_wires` to confirm the existing input wire.
These are different reads. A `null` output ID means the default output;
for Text Position use the named output `position`, not the default Path.

Use `nect_test_set_vector_wire` only for an active Vector2 input in the
same layer. Send the target, layerId, nodeId, input, sourceId and sourcePort.
For a detach, set both source fields to `null`. The adapter refuses locked,
expression-driven or incompatible sockets; Session rejects cycles. After the reply,
read the wire at the returned target. If the reply was lost or the target
is stale, inspect and compare the actual wire first rather than replaying
the request. After save/reopen, read the edge again at the new target;
verify the stored fallback separately when it matters.

Reproduce with the Vector2 wiring run in
[Windows fixture setup](../../../../desktop/docs/windows-live-editor-acceptance.md).
The documented proof is one same-layer Text Position flow with Undo/Redo
and persistence. Do not use this as an ordinary Tauri or Hermes connection.

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

## Reconcile a text edit in the Windows development fixture

These are fixtures, not an ordinary live editor or a reason to change its
pairing settings. Use them only when discovered for the current run.

1. Inspect and enumerate the target document and layer. Use the exact Text
   node ID, not the selected object or its label.
2. When available, call `nect_test_read_text_content` at the inspected
   revision. Keep the original Unicode string, including whitespace. This
   reads authored text, not IME composition, rendered glyphs, or font data.
3. Call `nect_test_set_text_content` for that same target. Read at the
   returned revision. If the result is uncertain, inspect and read the
   exact node before considering another write. If the revision changed
   unexpectedly, reconcile the new state; do not overwrite it silently.
4. After save and reopen, inspect the new target and compare the entire
   authored string again. An accepted edit or matching read does **not**
   establish exportability: the font may lack a glyph. Verify SVG and PNG
   separately; on `unsupported-glyph`, keep the saved text and report the
   missing export. Follow the fixture documentation for the positive
   embedded-font and negative missing-glyph tests, not an ad hoc font swap.
