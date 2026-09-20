# Native Document Schema v0

## M0 vocabulary

Implemented: Document, Composition, Artboard, Group, Path, Text, Contour, Point,
Scalar, Binding, Expression, Collection, Named Color, retained Circle/Rectangle/Polygon/Star sources,
Point Edit, gradients, local Fill/Stroke/Repeater stacks, geometry masks and common compositing.

Layer is the UI presentation of an Object; there is no duplicate Layer state model.

Raster, Resource, general node graphs, addressable generated Instances and full compositing are not stubbed ahead of real callers.

## Identity / ownership

IDs are opaque, document-unique strings. Names and array indices are not identity.

Rename and point reorder do not retarget references.

Objects are owned exactly once by a Composition root or Group child. Ownership is derived from roots/children, not duplicated in a parent field.

Collections are non-owning ordered sets. Collection membership does not alter the source object's parent or transform inheritance.

## Coordinates / handles

M0 stores `du96` (1/96 inch), Y-down coordinates, clockwise-positive degrees.

Each point stores X/Y plus incoming/outgoing handle angle and length as addressable Scalars.

Polar handle data is canonical in M0; Cartesian handle vectors are derived. A zero-length handle still retains its authored angle.

Transform is a six-value affine matrix. Do not also store decomposed TRS as another authority.
Native0.9 adds an authored local Anchor and independent Transform Parent; see its
bounded contract below. Position/rotation/scale actions edit the canonical matrix.

## Binding

Current subset:

`evaluated = evaluate(source_ref) * scale + offset`

Bindings use stable IDs. Name lookup is only an authoring convenience that resolves to an ID.

Driven properties reject direct Set until explicitly unlinked. Unlink freezes the current evaluated value into the literal.

Bindings must have compatible units and may not form cycles.

Native0.10 expressions compile into the same typed property/dependency visitor;
there is no JavaScript runtime or second document state. See the bounded language below.

## Session

Session owns committed mutation.

A batch is applied to a candidate document and validated before commit. Failure preserves both the original document and revision.

Undo/redo revisions are monotonic. History retains reversible changes to affected
Objects/Named Colors and changed Composition/Collection vectors; unchanged
document content is not copied into every retained operation. The default budget
is 1,024 edits and 64 MiB of conservatively estimated retained payload, including
owned container/string storage. This estimate is not a process heap/RSS reading.
Oldest entries are pruned to meet both bounds. A single transition exceeding the
budget rejects atomically with `HISTORY_LIMIT`, preserving the existing document,
revision and history rather than committing an edit without Undo.
New authored members must participate in structural equality and retained-size
accounting; otherwise a future delta could omit data or bypass its budget.

`history` returns labelled stable state IDs, current state, retained byte estimate,
limits and prune count. The earliest row is the retained boundary; subsequent rows
represent committed batches or completed gestures. `restore_history` takes a
state ID plus expected revision and traverses the retained changes atomically.
Returning to another state increments revision once; returning to the current
state does not. State IDs never get reused within a Session. A new edit from an
older state removes the redo branch, while simple history navigation keeps it.
Unknown/pruned IDs reject with `HISTORY_STATE_NOT_FOUND`. Failed edits, failed
history requests and cancelled gestures leave retained history unchanged.
History is current-session UI/edit state, never part of native authored JSON;
opening/recovering a file starts a new history. Backups provide durable prior files.

M1 adds CreatePath, AddPoint, RemovePoint, CloseContour, DeleteObjects and
ReorderObjects to the same command boundary. Deleting a referenced property is
rejected unless dependent targets are explicitly unlinked/frozen in the atomic
batch. Collection members of removed objects are pruned without reparenting.
Creation/reorder does not use array positions as identity. These commands do not
change the native 0.1 schema.

Session owns an optional gesture preview evaluated from the committed starting
snapshot. Committed reads/save/recovery retain the start document until commit;
the Canvas uses `preview_document`. Other mutations and history navigation reject
while a gesture is active. Cancellation or returning to an empty preview creates
no revision/history; successful completion commits once. Failed previews keep the
last valid preview and never partially commit.

## Persistence

Native JSON stores authored data only, not evaluated caches, Qt widgets, or session revision.

Unknown fields/versions/kinds, duplicate JSON keys, invalid references, non-finite values, unsupported color/unit claims are rejected explicitly.

Current M0 limits are safety bounds, not product performance targets.

## Native 0.2: retained primitives and point corrections

The primitive slice introduced 0.2; the current writer emits 0.9 and the reader
accepts strict 0.1 through 0.9. Migration of 0.1
preserves authored values, IDs and bindings, with no geometry conversion. The
historical linked fixture in `tests/fixtures/native-v0.1-linked.nect` is loaded,
edited, saved and reopened in separate processes. Unknown fields and behavior
versions remain errors. See `schemas/native-v0.2.schema.json`.

A Path owns either authored contours or one retained primitive source, never
both. `nect.shape.circle` and `nect.shape.rectangle` version 1 map typed local
distance parameters to a closed cubic path. Source instance IDs are stable;
derived contour and point IDs append fixed semantic roles (east/south/west/north
or corner names). Source IDs have a 64-character bound; generated IDs fit the
normal 96-character bound. The correction instance namespace is reserved.

Circle has center_x, center_y and radius; Rectangle has center_x, center_y, width
and height. `generator.*` parameters are ordinary linkable distance properties.
Derived point/handle fields join the same dependency evaluator; generator/point
cycles reject atomically. Circle uses four cubic arcs, with handle length
`radius * 0.5522847498307936` (a cubic approximation, not an exact rational circle).

Setting or linking a generated point field creates/reuses the visible downstream
`nect.path.point-edit` version 1 instance. Only changed fields are stored, as
**absolute local scalar overrides**, not a duplicated evaluated path. Unspecified
fields continue to follow the generator. Disabling Point Edit retains authored
overrides and evaluates the generator. Editing while bypassed re-enables the
correction instance. Topology editing requires explicit conversion. Unmapped
corrections reject; they are never silently assigned by array index.

`get`/`properties` distinguish authored, generated, point_edit and
bypassed_point_edit origins; generated fallback has `authored: null` and a
separate evaluated value. Native save stores no derived anchors. SVG and Canvas
read the same evaluated point values.

`convert_to_path` preserves object, contour and point IDs and active point
bindings. It freezes generator-derived values and removes source/Point Edit;
bypassed corrections are discarded explicitly in the conversion plan. Surviving
references to generator-only properties block conversion until the caller
explicitly unlinks/retargets them. `conversion_plan` is read-only; the mutation
still checks the expected revision. Undo restores the complete procedural state.
Conversion changes the geometry source only; later Fill/Stroke/Repeater operations
remain authored and editable.

## Native 0.3: ordered local Shape stack

A Path has one ordered `stack` of stable operation instances. Supported version 1
types are `nect.paint.fill`, `nect.paint.stroke` and `nect.shape.repeater`. Each
has an enabled/bypass flag and ordinary scalar parameters addressed as
`op.<instance-id>.<parameter>`. Reorder never changes identity or links. Deletion
that would break a surviving reference fails atomically. `operator_types` exposes
exact creation templates/units; `render_plan` exposes evaluated paint grouping.

The 0.1/0.2 Stroke becomes one real stack instance, with a deterministic unused
document-unique ID. The `legacy_stroke` string retains the old `stroke.*` address
as an alias to that instance; it is not another copy of color/width data. All old
authored Scalars and Binding references survive, including a fixture where an
existing ID collides with the preferred migration instance name. Native 0.3 stores
only the stack. New property discovery uses canonical operation addresses.

The pure core `evaluate_shape` owns ordered geometry/paint semantics, producing
immutable shared cubic contours, transformed path instances and ordered paint
layers. Canvas and SVG lower this same result. Neither renderer implements an
independent Repeater. Source handles edit the original geometry; clicking a
virtual copy selects its owning object. Copies are not individually editable
authored objects or addressable point identities.

Scope/order follows the bounded AE counterpart:
- Fill/Stroke apply to paths preceding them. Earlier paint entries appear above
  later ones by default (`composite: below`); explicit `above` overrides this.
- Repeater duplicates preceding paths **and** their paint layers. Before paint,
  copies are painted as one compound path; after paint, each copy is painted
  independently. Nested Repeaters compose these results.
- Repeater uses integer Copies (0–1000), a fractional transform Offset, fixed-step
  position/rotation and positive multiplicative X/Y Scale. Each copy's transform
  uses copy index + Offset around the local Anchor. Composite controls copy order;
  Start/End Opacity interpolates across existing paint copies. A Repeater before
  any paint has no paint opacity to alter. No fit-to-arc mode is implied.
- Fill supports nonzero/evenodd. Stroke uses butt caps and miter joins (limit 4).
  Width zero is invisible. Color is sRGB RGBA, normal source-over compositing.

Limits are explicit: 128 entries, 4096 generated path instances, 8192 paint layers
and 250000 expanded cubic anchors per object's geometry/paint result. Invalid
parameters, unsupported versions/options, nonfinite transform output and excessive
expansion reject the whole mutation. Bypass retains authored parameters and links.
Group-local stacks, arbitrary path operations, dashes, per-paint blend
modes and full AE interchange remain unsupported at this checkpoint.

Behavior reference: [Adobe shape paint/path operations](https://helpx.adobe.com/after-effects/desktop/drawing-painting-and-paths/shapes-and-shape-attributes/shape-attributes-paint-operations-path.html).

## Native 0.4: retained gradients

Fill/Stroke optionally own a version 1 gradient with document-unique component
and stop IDs. Linear and centered radial gradients use local start/end coordinates;
radial radius is their distance. The original solid RGB remains authored as a
fallback; paint alpha is overall opacity and multiplies each stop's alpha once.
The gradient enabled flag switches to Solid without deleting stops or links.
`set_gradient` creates/replaces/removes the component atomically; callers must
retain existing IDs and Scalars when editing its structure. `gradient_types`
provides exact creation templates. Native 0.3 files migrate without other changes.

The four coordinates and stop offset/RGBA channels are ordinary linkable Scalars:
`op.<paint>.gradient.<gradient>.start_x` (also start_y/end_x/end_y) and
`op.<paint>.gradient.<gradient>.stop.<stop>.offset` (also r/g/b/a). Coordinates
are distances; offsets/colors are scalars in [0,1]. Reordering stops never retargets
a reference. Removing a referenced stop/component rejects until surviving links
are explicitly frozen or retargeted. Undo and gesture preview use the same Session.

Authored stop order is preserved; evaluation sorts by offset. There are 2–64 stops,
with distinct evaluated offsets, including when bypassed. An enabled gradient
on an enabled paint requires start/end distance greater than 1e-9. Pad spread and
independent sRGB component/alpha interpolation are explicit. Coincident hard edges,
radial focal offsets, repeat/reflect spread and alternate color spaces are not
yet supported. Qt 6.5.3 [replaces coincident stops](https://github.com/qt/qtbase/blob/v6.5.3/src/gui/painting/qbrush.cpp#L1475-L1527);
rejecting ties prevents a silent loss between authored state and Canvas rendering.

Canvas uses QGradient ComponentInterpolation, with original-source start/end
handles. SVG uses userSpaceOnUse, sRGB and pad gradients. Repeating after paint
transforms its gradient with every copy; repeating before paint shares one gradient
over compound paths. `render_plan` exposes the same resolved stops and coordinates.

## Native 0.5: ordered output frames and parent size

An Artboard remains an output rectangle on its owning Composition's plane. Its
stable ID is independent of vector position and display name. `add_artboard`,
`update_artboard`, `delete_artboard` and `reorder_artboards` are atomic Session
commands. Reorder changes page/export order only; x/y/size edits change the crop
only. No Artboard operation moves artwork or changes object ownership. The
`artboards` read operation returns ordered authored/evaluated frame pairs.
Changed IDs include frames whose inherited dimensions change, not just their
owning Composition. Deleting the last frame through commands rejects explicitly.

Optional `parent_size: {artboard, width, height}` references a frame in the same
Composition. Each Boolean chooses whether that dimension follows its parent;
false keeps the frame's local width/height. The retained local values are valid
fallbacks, not a second evaluated authority. Reset Override means enabling the
corresponding Boolean. `detach_artboard_parent` freezes both effective dimensions
and removes the reference. Position/name never inherit. Chained parents are
supported; cycles, cross-plane references and deletion of a referenced parent
reject atomically. A caller may detach/retarget children and delete their parent
in one explicit batch. Limits: 1024 frames per Composition, parent depth 256.

This is the size-inheritance foundation of Parent Artboards, not reusable content
or logo/guide/page-number inheritance. Those require the shared definition/instance
contract and remain pending. Native 0.1–0.4 frames migrate with no parent binding;
their coordinates, order, authored artwork and SVG output remain unchanged.

## Native 0.6: editable Text

`kind: text` owns one `text` source and the same ordered paint/Repeater stack as
Path. It has no duplicate authored contours or generated point IDs. `create_text`
adds a default Fill; `update_text` replaces source metadata while retaining its
stable source ID. Callers preserve existing numeric Scalars when editing content
or font choices. Text content is UTF-8, bounded to 32768 bytes. Unsupported control
characters, malformed encoding, unknown fields and invalid parameters reject
atomically. Older native versions cannot carry a Text source.

Source version 1 stores `content`, `family`, `locale`, `weight` (1–999), `italic`,
`layout` (auto/frame), `direction` (horizontal/vertical) and `alignment`
(start/center/end). Bindable distance properties are `text.origin_x`, `origin_y`,
`font_size`, `frame_width`, `frame_height`, `tracking` and `line_spacing`.
Line spacing zero uses the font metrics; a positive value is uniform line advance.
Auto sizing does not soft wrap. Frame text wraps at its inline extent; overflow
stays visible and is diagnosed, never silently truncated or clipped.

The Windows projection uses installed DirectWrite shaping and script-aware glyph
orientation. Vertical text runs top to bottom with columns progressing right to
left; CJK is upright and Latin uses the shaped orientation. Auto layout is measured
and resized before outline extraction so vertical placement is near its authored
origin. Canvas and SVG consume the same cubic glyph contours and paint evaluation.
Missing families keep their authored name and report fallback plus actual families.
Supported color-font glyphs render their monochrome outline with an explicit
warning; color-only glyphs reject. Fonts are neither embedded nor distributed.
Non-Windows builds can preserve and edit the authored model but report
`TEXT_PLATFORM_UNSUPPORTED` when asked to project text geometry.

`text_defaults` returns an exact source template; `text_fonts` lists installed
families; `text_layout {object}` returns dimensions, glyph count, overflow,
warnings and actual fonts. `export_plan {composition,artboard}` discloses which
text will be outlined. SVG also carries that disclosure in each Text object's
description. Native content remains editable. Rich text runs, text-on-path,
per-glyph editing, variable-font axes and editable SVG text import/export remain
unsupported; the projection does not pretend to preserve them.

The GUI adds Text directly. Its content editor holds an IME draft independently
of Inspector/recovery refresh and commits one undo step on Apply. Cancel discards
only that draft. Concurrent content changes or document replacement reject Apply
while retaining the draft for copying. Style and numeric edits use the same
Session commands and references as the API.

## Native 0.7: typed colors and named definitions

Document `named_colors` contains document-unique stable IDs, names and four
ordinary RGBA Scalars. Definitions do not belong to a Composition or object tree.
Their channels are normal `{object: color-ID, point: "", field: "color.r"}`
references (and g/b/a), evaluated with the same unit checks, range checks,
dependencies and cycle rejection as other properties. Names may change without
retargeting references. Deleting a referenced definition rejects unless its users
are explicitly detached or retargeted in the same atomic batch.

A typed Color property aggregates four existing channels rather than duplicating
paint data. Its Ref has empty point and field `color` for a named owner,
`op.OP.color` for Fill/Stroke, or `op.OP.gradient.GRAD.stop.STOP.color` for a stop.
`get` and `properties` expose typed Color values alongside numeric properties.
Each value declares `space: srgb`, `profile: srgb`, `alpha: straight` and four
normalized channels. Other spaces/profiles/alpha modes reject explicitly; HEX
alone is never treated as a lossless encoding of richer color data.

`set_color` changes independent channel literals and rejects driven channels.
`link_color` creates four ordinary exact channel bindings; `unlink_color` freezes
their evaluated values. Copying equal RGBA values does not create a link. An
aggregate link is reported only when every channel references the corresponding
source channel with scale 1 and offset 0. Partial/channel-expression bindings
remain valid ordinary dependencies and are visible in channel inspection.

`create_named_color`, `rename_named_color`, `delete_named_color` and all typed
color mutations use Session revision/undo/atomicity. `color_properties` reports
the full addressable inventory. `used_colors` groups exact evaluated RGBA values
and returns their usage refs under `scope: enabled_paint_inputs`: active solid
paint colors and enabled gradient stops. It excludes palette definitions,
bypassed paints and a gradient's inactive solid fallback. This is an inventory of
authored paint inputs, not sampled rendered pixels: repeats do not inflate counts,
and clipping/crops/transparency do not imply additional color identities.

Native 0.1–0.6 migration adds an empty named palette and preserves all existing
paint/channel bindings. SVG evaluates named references into the declared export
subset; the native document remains the source for editable identities.

## Native 0.8: retained Polygon and Star

`nect.shape.polygon` and `nect.shape.star` version 1 use the existing retained
Primitive and Point Edit model. Polygon has `center_x`, `center_y`, `radius`,
`points` and `rotation`. Star has `outer_radius` and `inner_radius` instead of
`radius`. Coordinates/radii use local du, rotation uses degrees, and points uses
dimensionless integral values (Polygon 3–256; Star 2–256). All are ordinary Scalar
properties, including point count bindings. Radii are nonnegative; inner radius
larger than outer intentionally creates an inverted Star. Defaults use rotation
−90°, so the first outer point faces up. These generators make straight edges
with zero initial handle lengths. Fractional point counts and source roundness
are not implemented; direct Point Edit handles remain available.

Generated identities follow outer/inner role plus reduced rational angular phase
around the source: `<source>-outer-1-5` is the same role in a five- or ten-point
source. Star inner phases are odd half-steps. Angle zero is canonically `0-1`.
Rotation/radius/center edits retain roles. IDs are not list indexes: count changes
that remove corrected vertices (including bypassed corrections) reject with
`UNRESOLVED_POINT_EDIT`; references to removed roles reject `MISSING_REFERENCE`.
The entire candidate fails atomically. Existing Circle/cardinal and Rectangle/
corner IDs are unchanged. No point identity is silently reassigned.

`clear_point_edit {object}` explicitly removes all overrides and their bindings,
retaining the source and paint stack, as one undoable operation. The GUI reviews
that loss before Reset point edits. Clearing and changing count can be one API
batch; external users of a disappearing point must still be detached explicitly.
Convert to Path freezes current active topology with the existing reference
blockers and correction-binding preservation rules. Disabled-only cycles keep
their earlier bypass behavior; enabling an actual cycle fails.

Evaluation resolves source count dependencies and generated references with the
same cycle detection and units. It memoizes each object's resolved topology,
then enumerates active generated properties. Canvas/tree/shape evaluation use
the same evaluated snapshot; they never infer a linked count from its fallback
literal. `path_contours` requires that snapshot for a linked count and reports
`EVALUATION_REQUIRED` if absent. No generated contour is persisted as a competing
authored copy. API/MCP `primitive_types` supplies exact source templates.

Native 0.8 is an additive operator-type change; earlier authored data migrates
without geometric conversion. Historical native 0.7 named-color poster bytes
remain a fixture. Polygon/Star source types are refused in older version envelopes.
`schemas/native-v0.8.schema.json` preserves that version's structural schema.

## Native 0.9: Anchor and Transform Parent

Multi-selection remains desktop view state, not native data. Batch scalar commands
`edit_properties {targets:[Ref],value,relative}`, `link_properties {targets:[Ref],source,relative}`
and `unlink_properties {targets:[Ref]}` take1..1000 unique scalar targets with one
compatible unit. Each command resolves its initial values once. Absolute edits
assign each target; relative edits add to each target's own starting value.
Relative links retain each starting difference as an explicit offset; unlink
freezes all starting evaluated values. Edits reject driven targets until explicit
unlink. Duplicate aliases, units, cycles, ranges and unresolved topology fail the
entire Session transaction. Sequential commands in one transaction still run in
order; the snapshot boundary is each batch command.

`translate_objects {objects:[id],dx,dy}` uses world-space displacement on1..1000
unique objects in one Composition. A selected descendant/follower already moving
through a selected effective ancestor keeps its local transform unchanged;
selected effective roots solve their local translation. The final evaluated
world matrices must equal the snapshot worlds plus the requested displacement.
Changed driven fields, necessary singular inverses and binding-induced failure
reject atomically. Unselected followers continue to follow normally.

The desktop selects objects or points using Shift-click on Canvas and extended
tree selection; these are distinct editing contexts. Common scalar rows show
Mixed rather than an average. Matching stack rows use the same operation type at
the same authored slot, resolving each actual instance to a stable Ref. Multiple
point drags use each object's initial world inverse; generated points retain
their source and receive normal Point Edit overrides. Selecting a source freezes
the whole target set and Session revision; accept/cancel restores the selection.
Typing a single value assigns all targets, `+=`/`-=` adjusts once, and a negative
literal remains absolute. A mixed row cannot be copied as a single Value/Reference.

Every Object stores `anchor:[Scalar x,Scalar y]` and `transform_parent:id|null`.
Old files migrate to Anchor(0,0), null parent, with their exact six affine Scalars,
references and placement retained. New GUI shapes explicitly Center Anchor once;
GroupContiguous initializes its new Group at current geometric bounds center.
Later child/source changes never implicitly recenter an authored Anchor.

`transform.anchor_x/y` are normal linkable local du properties. Changing Anchor
does not rewrite the matrix. As with any property, explicitly authored dependency
links may cause other properties to follow it. For local matrix M=[L,t], local
Anchor A and parent-space Position P, P=L*A+t. `set_position` solves t=P'-L*A.
`transform_around_anchor` applies L'=R(degrees)*L*diag(scale_x,scale_y) and
t'=P-L'*A: clockwise Y-down rotation in parent coordinates, scale along local
axes, zero/negative scale allowed. These are one-shot semantic edits, not stored
TRS/expression authorities. Changed driven fields reject; unchanged driven fields
remain linked. Re-evaluation verifies the requested matrix/anchor position;
coupled bindings that invalidate the direct solve reject TRANSFORM_PRESERVATION.

Effective parent is the explicit Transform Parent when present, otherwise the
structural parent, otherwise Composition identity. World=parent.world*local.
Explicit following replaces structural transform inheritance; it never applies
both. Structure continues to own order, membership, selection and future effect
scope. Parents must exist in the same Composition. The actual effective graph is
acyclic with depth<=128; surviving references protect a deleted parent. Finite
legacy accumulated matrices retain their prior range, while nonfinite output
rejects. All GUI/API/MCP callers use shared evaluate_transforms.

`set_transform_parent {object,parent:id|null,preserve_world:bool}` defaults to
preserve-world in the GUI. It solves local'=inverse(new_parent.world)*old_world,
then verifies the resulting world matrix. Detach(null) returns to the structural
parent. A singular new parent cannot be inverted and rejects SINGULAR_TRANSFORM;
a singular old object is otherwise allowed. Cycles and same-Composition checks
apply even when preserve_world is false. No implicit structural reparent occurs.

`center_anchor` uses target-local geometric bounds including exact cubic extrema,
final editable/repeated paths, earlier paint-captured geometry and Text layout
bounds; stroke width is excluded. Empty geometry rejects EMPTY_BOUNDS. Ordinary
effective descendants can be bounded locally even for a singular Group; external
followers that require inverse(Group.world) reject if that inverse is singular.
Center and Group creation are one undoable Session transaction.

Canvas Anchor mode (`Y`) displays an unexported crosshair and moves only Anchor
Scalars through the normal preview/commit/cancel path. Object drag uses effective
parent coordinates; points and gradients use actual object world coordinates.
`transforms` readback returns local/world matrices, effective parent, Anchor,
Position and world Anchor. Changed-ID reporting includes changed follower worlds.
SVG retains legacy nested matrices for purely structural scenes; when explicit
following exists, structure groups become ordering/name containers and each leaf
receives its shared world matrix, avoiding duplicate transforms or inverse solves.

Native0.9 is described by schemas/native-v0.9.schema.json. The0.8 documented Ref
field vocabulary was also corrected to include already-supported count/radius,
Text and named-color channels; emitted numeric properties are checked against it
in the current schema. This changes no authored file data.

## Desktop continuous protection (native format unchanged)

Host captures only `Session::document()` committed snapshots. One asynchronous
worker validates/encodes and writes; one newest pending snapshot replaces queued
intermediate edits. The one-second timer is not restarted by continuous input.
Completed storage receipts update only matching Session/path identities. An old
receipt may advance its own known durable revision but cannot claim the current
newer revision is saved. Storage completion updates the status label only, leaving
focused inputs and incomplete drafts intact.

`hello.persistence` reports live, pending, writing, native saved and recovery
revisions, independent destination errors, and recovery location. Null means no
verified revision. Recover explicitly flushes committed state; native failure
does not prevent recovery, and recovery failure does not prevent native saving.
Manual save/open/new/normal close wait for earlier jobs before replacing identities
or paths. Normal close/session replacement can proceed with a failed recovery
destination only when the exact current native bytes are independently reread and
verified. Explicit `recover` still reports that recovery failure. Opening recovery
uses `open_recovery`, creates an unnamed Session and never edits its source file.

Native writes compare SHA256 of the actual loaded bytes, including old-format
spelling, under a canonical-path QLockFile. Age-based stale expiry is disabled;
live slow writers must not lose their lock. A second comparison before commit
catches external changes during preparation. QSaveFile stages the replacement
with direct-write fallback disabled, then content is reread and hashed. Failure
before commit preserves the prior file. `IO_VERIFY_FAILED` after replacement is
explicitly ambiguous: replacement may have succeeded but cannot be labelled
verified. External edits/deletion reject as `FILE_CHANGED`; reopen or Save As to
another destination. Locks are cooperative; the final non-cooperating writer
race and hardware/power-loss guarantees remain outside this contract.

The first automatic replacement after open/manual save and subsequent automatic
replacements at least30 seconds apart retain exact previous bytes. Manual saves
request a generation whenever bytes differ. Owned timestamp+SHA256 backups are
deduplicated and target ten generations per native/recovery file; pruning follows
verified replacement only. Legacy names, modified bytes and unreadable/locked
generations are preserved, so cleanup is best effort rather than a hard disk cap.

Recovery data has a separately atomic `nect-recovery-1` receipt containing source
path, document/Session ID, revision, timestamp and content hash. Data success with
receipt failure is not reported as completed protection. A live Session holds an
active lock; eligible closed recoveries target twenty sessions and128 MiB including
their owned backup sizes, newest first. Legacy/foreign/mismatched receipts, modified
files and active sessions are excluded from pruning. A crash between the data and
receipt writes can leave a valid recoverable file with stale metadata; it is kept
and can be opened manually, without trusting the stale receipt. Abrupt exit may
leave a temporary staging file; unknown files are not automatically deleted.

The normal recovery-loss window is the one-second scheduling cadence plus queue
and storage latency. Slow or failed storage can extend it without a fixed bound;
the UI and API report the last verified revisions. Uncommitted drafts/gestures,
OS/device cache loss, external linked asset history, and cross-restart operation
History are not covered by a native save. All native read/write payloads are
bounded to8 MiB. No document migration or second mutable document model is added.

## Next contracts

Extend Text only with demonstrated shaping/layout requirements.
Add Raster only with immutable source/provenance and explicit color semantics.
Add Operator/Instance only with identity and regeneration contracts.
Extend compositing only with explicit alpha/group/color-space and interop semantics.

## Native0.10 property expressions

Scalar adds optional `expression:{source:string,version:1}`. It is mutually
exclusive with a non-null Binding. Literal remains authored as the inactive
fallback, just as with Binding. Source text (including whitespace/newlines) is
preserved exactly; compiled programs/caches and draft text are never serialized.
Readers0.1–0.9 remain strict and reject the new field. Migration preserves every
old value/ID/reference; that checkpoint writer emits0.10 (current writer0.12).

The pure language accepts finite decimal/scientific literals, parentheses,
unary +/-, binary + - * /, and `ref("object-id","point-id-or-empty","field")`.
References use the normal stable property IDs, never names/array positions.
Functions: abs, floor, ceil, round, sqrt, sin, cos (one argument); min/max (two);
clamp(value,lower,upper). Trigonometry uses degrees. Round halves away from zero.
No variables, assignment, loops, time, random, JS, filesystem, network or processes.

Addition/subtraction/min/max/clamp require compatible units. Multiplication needs
one dimensionless factor; division needs a dimensionless divisor or equal units
(the latter yields a dimensionless ratio). sqrt requires dimensionless input;
sin/cos require degree properties or literal angles. Literal-only subexpressions
may adopt their surrounding/target unit. A dimensionless property/result does
not implicitly turn into a distance; use a distance property for amplitude,
e.g. `sin(ref("phase","","generator.rotation")) * ref("phase","","generator.outer_radius")`.
Compound units are unsupported. Division by zero, negative sqrt, reversed clamp,
non-finite intermediates and existing property-range violations reject atomically.

Bounds:4096 source bytes,256 AST nodes,32 AST/parser depth,64 reference occurrences;
property dependency traversal retains its128-depth bound. Parsing is locale
independent. Each evaluation locally reuses compiled identical sources; no global
mutable cache or parallel expression evaluator is introduced. References enter
the same dependency visitor, including generated topology and transform requests.
All authored formulas (also bypassed corrections/operations) receive static syntax,
unit and reference validation. Only active properties execute, preserving existing
dormant-cycle/domain behavior. Deletion/topology change cannot silently retarget a
reference. Convert to Path retains active point formulas; references to discarded
generator properties block conversion.

`set_expression {targets:[Ref],expression:{source,version},replace_binding:bool}`
applies to1..1000 unique compatible Scalars in one candidate/Undo. Formula edits
replace earlier formulas; replacing a Binding requires explicit true. Set,
EditProperties and SetColor reject driven channels. Link/LinkProperties/LinkColor
explicitly install a Binding and clear the formula; Unlink variants freeze the
initial evaluated result and clear either source. History accounts for source
strings. API `expression_language` describes the language/limits. `get` and
`properties` retain authored source beside evaluated output. SVG exports current
values; `export_plan` discloses `property_policy:evaluated_values` and no formula
preservation. Native source is untouched by export.

GUI numeric fields accept `=expression`. Their compact state displays evaluated
numbers, with an fx control/source tooltip. fx or a multiline paste opens an
inline draft, with searchable stable-reference insertion and a separate draft
result. Apply/Ctrl+Enter commits; Cancel/Esc discards. Invalid drafts keep the
committed Canvas value. Drafts survive same-session Inspector refresh; a changed
Session revision rejects Apply until the draft is explicitly cancelled/reopened.
A visible checkbox authorizes replacement of an existing link. Drafts are view
state, excluded from save/recovery and Undo. Numeric +/-= remains a one-shot edit.

## Native0.11 visibility, geometry masks and common compositing

All Objects add `visible:bool` and `compositing:{version:1,opacity:Scalar,
blend:string,isolated:bool,mask:GeometryMask|null}`. `composite.opacity` is an
ordinary dimensionless property in [0,1], with the same links, formulas, batches
and Unlink behavior. Readers0.1–0.10 migrate to visible/opacity1/normal/nonisolated/
no-mask defaults; earlier schemas reject the new fields. Writer0.11 preserves
all authored geometry, appearance, mask identities and hidden sources.

A GeometryMask has document-unique `id`, same-Composition `source` Object ID,
`version:1`, `enabled` and `fill_rule:nonzero|evenodd`. Sources are Path or Text,
never the target itself or a Group. It uses the source's final evaluated geometry,
including Repeaters, in Composition/world coordinates. Each open contour closes
for mask filling without rewriting its source. Source paint, stroke width, normal
visibility, opacity and its own appearance mask do not affect this geometry mask.
No alpha/luma/invert mode or appearance-derived silhouette is implied. A bypassed
mask retains its source reference; deleting a referenced source rejects unless
all owners are deleted in the same transaction. Removing the mask preserves its
source and current visibility. Transform Parent determines following independently.

`set_visibility`, `set_compositing` and `set_mask` use the normal Session boundary.
`mask_objects` accepts at least two ordered contiguous siblings and an explicit
Top/Bottom choice. It wraps them in one Group, initializes its Anchor to the
current geometric center, masks by the last/first painted leaf and hides that
source's normal artwork. One Undo restores the complete structure and visibility.
`put_inside` moves an ordered contiguous block immediately below the destination
Group into its first child positions, preserving world placement and order.
Explicit Transform Parents remain unchanged; structural followers solve the new
local affine. Driven transforms, singular inverses or a dependency-induced world
mismatch reject atomically. Destination Group effects intentionally apply afterward.

`evaluate_scene` is a transient structural scene tree over one shared evaluated
shape per leaf. Qt and SVG consume its resolved masks/world transforms/isolation;
there is no second authored renderer document. The Composition artwork starts
transparent. The viewport's white Artboard and grey workspace are UI surfaces,
not blend backdrops. Add an ordinary filled object for authored paper/background.
A neutral Group (opacity1, normal, not explicitly isolated, no enabled mask)
passes its children into the current backdrop. Any other boundary aggregates
children on transparent, clips geometry, then applies opacity and blend once.
Opacity of overlapping children therefore does not compound within that Group.
Structure owns this scope; Transform Parent does not.

Supported blend IDs: normal, multiply, screen, overlay, darken, lighten,
color-dodge, color-burn, hard-light, soft-light, difference, exclusion. Unsupported
IDs reject rather than silently aliasing Add or another AE mode. Working space is
sRGB with premultiplied source-over alpha during raster compositing; authored
colors remain straight alpha. The Qt viewport currently uses 8-bit premultiplied
ARGB surfaces, bounded to16,384 physical pixels/axis,128 MiB simultaneous surfaces
and16 isolation levels. Isolated surfaces use pixel-aligned viewport-clipped
mask bounds or actual paint support (including transformed stroke joins), with
an antialias margin; authored object bounds are not raster allocation bounds.
Exceeding a renderer bound gives a persistent visible
error; it does not silently omit an effect or rewrite the document. This is a
bounded compositing subset, not full AE/HDR/linear-light/ICC production parity.

SVG exports userSpaceOnUse vector clipPaths in Composition coordinates and
world-transformed leaf artwork inside structural wrappers. This avoids inverse
matrices and double transforms for external followers or singular Group bases.
Group opacity and CSS `mix-blend-mode` / `isolation` carry the compositing contract;
readers must support these SVG/CSS features. `export_plan` discloses that reader
requirement. Native is unchanged; no raster bake or editable Text/formula source
is claimed in SVG. Neutral older scenes retain the earlier SVG projection.
`compositing_types` discovers the supported subset and `compositing_plan` reports
the resolved tree, masks, isolation and transparent backdrop.

GUI source visibility is independent from Show mask outline (viewport-only).
The selected target's hidden mask can show a faint dashed outline; Edit source
selects the actual retained object and its ordinary point controls. Normal hit
selection excludes hidden ancestors, zero opacity and clipped-out regions,
including Text/bounds fallbacks. A directly selected hidden object still exposes
its editing controls. Context-menu choices freeze Session/revision while open.

Semantics references: [W3C compositing and blending](https://www.w3.org/TR/compositing-1/)
(group invariance, isolated transparent backdrop and separable blend functions),
[Qt QPainter composition modes](https://doc.qt.io/qt-6/qpainter.html#CompositionMode-enum),
and the [Adobe blend-mode target vocabulary](https://helpx.adobe.com/after-effects/desktop/work-with-layers/work-with-layer-blending-modes/blending-modes-layer-styles.html).

## Native0.12 retained Offset Paths

`nect.shape.offset` version1 is an ordered local path operator. Amount defaults
to10du, is signed and bounded to magnitude1,000,000; Miter Limit defaults to4 and
is a dimensionless Scalar in[1,1000]. Both use ordinary links/expressions and batch
property commands. `line_join` is `miter`, `round` or `bevel`; fill_rule chooses
nonzero/evenodd region interpretation. Composite remains `below` because Offset
has no paint-order option. OperationOptions may supply line_join; omission keeps
its existing value. Unsupported options reject, including nondefault joins on
other operation types. Native0.1–0.11 readers remain strict and cannot carry Offset;
their authored state migrates unchanged. The writer emits0.12.

Positive amounts expand filled regions; negative amounts erode them. Complete
erosion is valid empty evaluated geometry. Zero Amount is an exact no-op, as is
bypass. Neither changes primitive generators, source IDs, point corrections or
authored contour topology. Convert to Path still freezes only the source and keeps
the operator stack. Generated Offset vertices are transient output, not new stable
point identities that a reference may target.

Offset evaluates preceding geometry in object-local distance after its PathInstance
transform, before the object's authored/world transform. It also modifies the
geometry of earlier paint layers: each layer keeps its transform, gradient basis,
stroke metrics, color and alpha. Resulting contours are pulled back into that
paint basis, with explicit rejection when no stable inverse exists. Reordering
Offset across Fill/Stroke therefore does not leave earlier paint on the old path;
reordering across a nonuniform Repeater can change distance and appearance.
Separate repeated instances remain separate regions and are not implicitly unioned.

Version1 accepts closed simple contours and nested/disjoint compound boundaries.
Open paths, zero-area regions, self-intersections and touching/crossing contour
boundaries reject atomically. Region topology is checked after adaptive cubic
subdivision into segments: control-hull distance to each segment is at most0.1du,
depth limited to24. This is a bounded polygon approximation, not an exact analytic
curve intersection/offset solver. Round joins also choose segments from0.1du
chord tolerance; miter joins exceeding the limit use a true bevel fallback.
Boost Geometry1.85 buffer supplies region expansion/erosion, using the already
required Boost headers; its additional amount-dependent input simplification is
disabled. Authored curves and their handles remain untouched.

Per projected instance: at most256 contours and32,768 flattened input vertices.
Each Offset application admits at most1,000,000 conservative estimated join
vertices over unique geometry/transform projections. Final shape budgets count
actual evaluated topology, including Offset expansion:4,096 path instances,
8,192 paint layers,250,000 anchors separately across geometry and paint outputs.
Finite evaluated coordinates must remain within magnitude1e12. Exceeded work,
topology, range or output bounds produce explicit errors; none silently skip the
operator. Per-application reuse of identical contours and transforms is transient.

Qt Canvas, mask sources and SVG consume the same evaluated paths/paint layers.
SVG contains evaluated vector contours; source operations stay in native, as
disclosed by export_plan. Add / Shape stack exposes Offset Paths, Amount, joins,
fill rule, bypass and ordering through the shared Session. New operations scroll
into view; the Inspector's Shape stack menu jumps directly to existing operations.
Operator discovery returns exact templates and supported geometry.

Behavioral references: Adobe's [shape render order](https://helpx.adobe.com/after-effects/desktop/drawing-painting-and-paths/vector-graphics-and-raster-images/overview-shape-layers-paths-vector.html)
and [Offset Paths controls](https://helpx.adobe.com/after-effects/desktop/drawing-painting-and-paths/shapes-and-shape-attributes/use-offset-paths.html).
This subset does not claim full AE Offset Copies, open-path or self-intersection parity.
