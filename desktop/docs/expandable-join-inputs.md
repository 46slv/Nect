# Join Paths: expandable bounded inputs

Status: implemented 2026-10-17. User-approved display label: **Join Paths**. Canonical serialized type remains `Join Geometry`. This packet preserves the completed schema-22 Multi Path implementation and advances new project factories and expandable Join storage to schema 23.

## User outcome

Connect many geometry sources to one node without repeatedly building Join-of-Join trees. Connecting the last available input exposes another empty input. Disconnecting a source must not retarget other wires. The system remains resource-bounded and reports limits rather than silently dropping inputs.

## Baseline behavior before this implementation

- Canonical node type and factory default name: `Join Geometry`.
- `definitions` has two `Path` parameters, `a` and `b`; output is `Path`.
- Evaluation currently requires both inputs. It concatenates independent contour groups, clones geometry, creates output IDs, and routes Text character occurrences using `nodeId:a` and `nodeId:b`.
- It preserves separate group paint behavior, holes and overlaps. Fill/Stroke occur downstream. `PaintedPath` inputs are rejected; existing different upstream paint styles are not bundled.
- Limits: 10,000 contours, 100,000 points and 256 independent groups per Join result. Layer graph limits include 2,000 nodes and depth 256. Project byte limits remain applicable.
- Bypass is explicitly input `a`; it must not silently switch to another connected input.
- `parameters()` enumerates `n.params`, so generic dependency analysis, parameter lookup, connection handling and wire drawing already see added parameters. There is no general variadic-input schema: validation requires the exact fixed definition's parameter count.
- Session commands clone the document and commit validated candidates through history. `connect()` itself checks types, parameter locks and cycles. Session enforces composition lock. Join currently does not participate in the bounded-geometry transaction guard.
- SVG grouped import currently emits balanced binary Join trees.

Source anchors: `src/core.js`: definitions, node, parameters, activeParameters, bypassInputs, evaluate's Join branch, connect/disconnect, validate, assertClosedOffsetChange, Session connect/disconnect/delete handlers. `src/main.js`: editorParameters, field, node-body/inspector notes, drawWires. `src/svg-import.js`: grouped same-paint import. Existing regressions: `tests/join.test.js`, `tests/node-bypass.test.js`, `tests/character-position.test.js`.

## Terminology and scope

The user approved **Join Paths** on 2026-10-17. New nodes use that default name; menus, picker search, default-name graph headers and inspector display it. The old `Join Geometry` search alias and canonical type remain supported. Existing serialized names, including custom names, are preserved.

This operation is a live ordered geometry bundle:

- Boolean Union resolves overlapping regions into a silhouette; this node does not.
- Welding merges coincident vertices; this node does not.
- Endpoint joining connects curve ends; this node does not.
- Multi Path is an editable authored contour/unit container, including baked outlines. Join combines upstream evaluated Paths and keeps their procedural sources. It does not bake or create editable Multi Path units.
- Downstream shared paint applies to every group, with separate transparency compositing. Mixing already-painted objects is a different feature.

## Implemented durable input model

Keep `params.a` and `params.b` unchanged for legacy consumers. Add:

- `joinInputOrder`: explicit array of stable parameter keys, initially `['a', 'b']`.
- Added key form: `input_<UUID>` generated once with the existing uid mechanism, never derived from a displayed ordinal. All keys are node-local and CSS/selector-safe.
- Each extra parameter has the existing shape `{ type: 'Path', value: null, expression: '', locked: false }`, with normal optional source/sourcePort.
- Order array and parameter-key set must match exactly. Reject duplicates, malformed keys, wrong types, missing order entries, unknown extra fields and more than 1,024 stored slots. Reserve `a` and `b`; do not reassign those identifiers.
- Evaluation and UI enumerate explicit order. Generic `parameters()` should expose Join parameters in that same order. Do not rely on JSON object-key insertion order.

The 1,024-slot structural ceiling bounds malformed or locked-empty accumulation; it is not a promise of 1,024 drawable independent groups. The existing 256-group output cap remains the effective ordinary-artwork bound in this packet. This already replaces up to 255 binary Join nodes with one. Raising the shared group budget is a separate measured change because downstream render/export/modifier paths also depend on it. Never claim literal unlimited inputs.

## Port lifecycle

1. Factory retains a/b, preserving existing direct callers and initial two-input appearance. This is the deliberate minimum-two-slot exception to a compact variadic UI.
2. After successful connection, if the last ordered slot is connected or locked, append one unlocked empty Path slot. Empty slots do not contribute geometry. Never allocate before the requested connection passes type/cycle/lock checks.
3. A connection replacement retains the target key and position. Connecting the same source to multiple slots is permitted and preserves independent groups/character routes.
4. On disconnect, remove empty, unlocked _additional_ slots except one trailing empty slot. Never remove connected or locked slots. Keep legacy a/b even when empty, including an interior legacy hole. Never renumber remaining keys. Ordinal labels may change but data-key/source bindings must not.
5. Run the same compaction after deleting upstream nodes and after unlocking an empty extra slot. Source deletion is the existing explicit delete action; normalization must not invent a separate permission or history action.
6. An empty locked slot remains visible and is never reused automatically. If no trailing unlocked slot can be allocated at the structural cap, show a clear limit message and retain the valid existing state. Do not reject a valid connection merely because a spare cannot be appended at the cap.
7. Do not normalize/mutate during rendering, evaluation or wire hit-testing. All changes belong to the same candidate command/history entry as the connect/disconnect/delete/unlock action.
8. No manual reorder feature is required. If later introduced, move stable keys in the order array, not references or parameter objects. Explicit order changes paint stacking order and must be undoable.

## Evaluation and identity

- Zero connected inputs: diagnosed ordinary intermediate state, `Connect a path to Join Geometry`. One connected input: valid geometry, processed through the same grouping/identity logic. Two or more: ordered concatenation. Ignore empty sockets; never read their null fallback as geometry.
- Resolve connected inputs in `joinInputOrder`. Accumulate contour/point/group counts before allocating cloned output arrays, and fail at existing budgets. Do not truncate or silently omit the excess.
- Retain current hole/group separation and output Path typing. Flat and nested Joins should have equivalent geometry/paint order within budgets.
- Character route is `nodeId:stableInputKey`, not ordinal. Keep a/b routes exactly unchanged for legacy inputs; appended inputs use their new stable key. Same Text connected twice must produce distinct occurrence routes.
- Existing output contour/point IDs use flattened group ordinals. Preserve that scheme for compatibility in this packet, especially existing a/b graphs and saved downstream Remap bindings. Do not silently replace legacy IDs with a new port-prefixed scheme. Removing an earlier input can change topology identities, as it already can when source groups change: retain unstable/fingerprint checking and suspend stale Remap edits. Port-key stability guarantees wire identity, not stable contour topology after removing geometry.
- Preserve bypass contract `a`. If a is empty while b or an extra slot is connected, bypass reports missing identity input instead of choosing a different source. Structural cycles remain prohibited even in bypassed/otherwise unevaluated inputs.

## Persistence, migration and transaction integrity

Schema 23 gates explicit Join order/extra sockets; schema 22 remains the introduction version of Multi Path. older binaries should reject rather than silently discard the new representation.

For legacy a/b-only nodes, loading adds order a,b deterministically, retains IDs, sources, locks, node names and order, and may append one spare if both are occupied. Migration changes only the in-memory loaded document; it does not write files. Do not accept malformed extra parameters in an older schema by treating them as migration input. New-schema validation is strict. Save/load must preserve the explicit input order and stable keys, including locked holes and the trailing spare, without repeated allocation. A repeated validate/load operation must be idempotent.

Avoid generating a random spare during repeated validation: migration/factory may create it once, while subsequent validation only checks the normalized representation. Legacy validation that is used on cloned candidates must not change serialized state differently on every call. Consider a deterministic first spare key for legacy migration, reserving a fixed valid key distinct from a/b, while later commands use uid.

Connect, disconnect, automatic spare creation and compaction are one Session transaction and one undo step. Failure leaves document bytes, revision and history unchanged. Undo/redo restores exactly the keys and order, with no regenerated IDs. Duplicate-node operations clone port keys/order as node-local identifiers and remap source node IDs through the existing duplication path. Do not alter the originals.

Add Join to the bounded-geometry guard (`assertClosedOffsetChange`) and give `boundedInputs` all ordered Join sources. This checks over-budget connection and upstream changes against prior diagnosed errors and rolls back new failures. Keep incomplete zero-input state editable. Read-only evaluation must still enforce budgets for externally constructed documents.

## Implementation seams

### Core owner

1. Small Join helper module or local helpers: ordered keys, strict shape validation, spare/compaction normalization and budget constants. Helpers should not change other node definitions.
2. Factory and ordered parameters enumeration; preserve definition a/b compatibility.
3. Join evaluator: iterate connected ordered inputs, preserve a/b routes and output ID rules, count before allocation.
4. connect/disconnect plus Session delete/unlock hooks: normalize after successful state changes. If low-level connect is extended, preserve rollback for every mutation, including order/params, and never mutate on `validating=true`.
5. Validation/schema migration and version promotion; add Join to bounded transactional guard.
6. Existing bypass remains a. No new geometry type, Paint type, Boolean logic or Multi Path editing changes.

### UI owner, only after core contract is agreed

1. `editorParameters` consumes ordered keys; `field` supplies readable “Path 1”, “Path 2”, etc., and “Add path” for the trailing empty input. Keep stable actual `data-key` values.
2. Both node-body and inspector helper text replace “A and B / both” with “inputs / all”; explain geometry-only bundle and Union distinction.
3. Existing wire rendering should work through `parameters`; verify rerender/reflow, drag-to-spare, selector dialog, deletion and undo. Long nodes must remain reachable using existing graph navigation; optional collapse/scroll presentation must not hide connected wire targets accidentally.
4. Do not rename canonical node type or overwrite custom names. Display-label work can be a separate approved patch.

### Optional importer follow-on

After the core/UI contract is tested, replace grouped SVG's binary-tree loop with one Join and ordered inputs using the shared helpers. Preserve same-paint checks, group order, existing import caps and output appearance. Do not flatten arbitrary authored Join trees automatically. This follow-on is not necessary for manual expandable inputs to work.

## Focused acceptance matrix

- Three, dozens and 256 one-group sources connected directly to one Join; no user-created chains needed; trailing empty port after each successful connection below limits.
- Empty node remains editable; one-source pass-through works; legacy two-source geometry/SVG unchanged.
- Disconnect middle/first/last extra input, delete an upstream source, reconnect, lock/unlock empty input: remaining keys/wires retain exact targets, a/b retained, no growing unlocked-empty debris.
- Same source twice: distinct groups and character occurrence routes; a/b routes unchanged; text routes for surviving extra inputs survive other removals.
- Undo/redo/serialize/deserialize/duplicate preserve ordered keys, locks and sources. Repeated validation is byte-stable after first migration. Malformed/duplicate/missing/oversized order rejects atomically.
- Paint/Image/Scalar/Vector source mismatch, cycle through a third or later socket, bypassed cycle, composition lock, and target-port lock all reject without mutation.
- 256 independent groups accepted; 257 rejected with useful error and unchanged history. Independently test 10,000-contour and 100,000-point boundaries; one source may already consume many groups. Existing graph/project limits unchanged.
- Shared Fill/Stroke/Gradient, transparent overlap, holes, opposite winding, downstream Transform and Remap retain existing behavior. No Boolean union/weld/endpoint attachment occurs.
- DOM test verifies stable input data-key, newly appended port, wire endpoints and undo; existing join/bypass/character/paint regressions remain green. Full-suite/native verification is for the integrating owner, not this design task.

## Verification and remaining limits

Focused checks run for this packet:

- `tests/expandable-join.test.js`: initial/empty/single input states, 100 distinct direct sources, 256-group acceptance and 257th connection atomic rollback, upstream parameter-change rollback, independent 10,000-contour/100,000-point boundaries, stable disconnect/lock/delete/duplicate behavior, history and persistence, malformed schema/order rejection, cycles/types/composition locks, deterministic migration, strict a-only bypass, and the 1,024-slot structural cap.
- `tests/expandable-join-dom.test.js`: real pointer wiring, spare allocation, ordered labels, stable data keys and measured SVG wire endpoints, Undo/Redo, disconnect stability, Join Paths and legacy Join Geometry search, and preservation of stored custom/legacy names.
- Existing `join.test.js`, `character-position.test.js`, and `text-outline-baking.test.js` regressions cover separate paint groups, holes, overlap, shared transforms, Remap fingerprints, repeated character routes and Multi Path compatibility. Character tests now also cover stable extra-slot character routes after removal of an earlier extra input.
- Five targeted `node-bypass.test.js` regressions cover Join identity, cycles, history/persistence, Type-on-Path duplication and downstream Offset provenance. The full file has an unrelated pre-existing expectation mismatch: Luminance Instances is bypassable but omitted from its supported-type list; this packet does not change that test policy or runtime behavior.
- One Vite Web build; no full-suite, native build or browser run in this packet.

The implementation retains 256 independent groups, 10,000 contours, 100,000 points and 1,024 stored slots. A valid last-socket connection at the structural cap succeeds without another spare; the UI explains the socket limit. Inputs are not silently truncated. Empty additional unlocked slots compact during disconnect/delete/lock commands; connected or locked slots retain their keys. Factories now produce current schema 23. Authentic legacy a/b-only projects migrate deterministically using `input_legacy_spare`; repeated loads/validation preserve bytes after migration.

Existing authored Join trees are not rewritten. SVG import still uses its established balanced binary Join construction. This feature does not weld vertices, connect endpoints, perform Boolean Union, bundle painted styles, or provide unlimited memory. Raising shared geometry budgets requires separate measurement and authorization.
