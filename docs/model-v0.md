# Native Document Schema v0

## M0 vocabulary

Implemented: Document, Composition, Artboard, Group, Path, Text, Contour, Point,
Scalar, Binding, Expression, Collection, Named Color, retained Circle/Ellipse/Rectangle/Polygon/Star sources,
Point Edit, gradients, local Fill/Stroke/Repeater stacks, document-local PresetDefinitions,
same-document Definitions/Instances and Macros, geometry masks and common compositing.

The current native writer is 0.75.

## Point Edit enabled expression v1

Native 0.70 adds optional `enabled_expression` to an installed procedural Path
Point Edit, beside its authored enabled literal and existing exact link. Link
and expression are mutually exclusive. The closed version-1 source is `true`,
`false`, a same-Composition `Ref{Object ID,"","point_edit.<correction ID>.enabled"}`
on a distinct installed correction, or its negation. Evaluation uses the source
correction's evaluated enabled result and is independent of visibility and
generated geometry. The literal, correction ID, overrides and exact expression
text remain authored. The qualified Ref exposes the expression source; the
unqualified owner slot `point_edit.enabled` remains literal-only. Linking with
explicit replacement clears an expression, while unlink freezes the evaluated
boolean in one Session command. Clearing, converting or deleting a referenced
correction is refused while a dependent survives, including a same-batch
clear-and-recreate that would reuse the deterministic correction ID. Native
0.1–0.69 remain readable; an expression under an older version or beside a
link is rejected. The closed wire shape is `schemas/native-v0.70.schema.json`.

## Retained Gradient enabled expression v1

Native 0.69 adds optional `enabled_expression` to a retained Fill/Stroke Gradient,
beside its authored enabled literal and existing exact link. Link and expression
are mutually exclusive. The closed version-1 source is `true`, `false`, a
same-Composition `Ref{Object ID,"","op.<paint operation ID>.gradient.<gradient ID>.enabled"}`
on a distinct retained Gradient, or its negation. Pure evaluation drives the
Gradient's own bypass independently of Object visibility and owning operation
enablement. The literal, nested IDs and exact expression text remain authored.
The revisioned Session command sets or explicitly replaces the source; unlink
freezes the evaluated boolean. Same-ID `SetGradient` edits preserve that source
and refuse direct driven literal or source changes. Native 0.1–0.68 remain
readable; an expression under an older version or beside a link is rejected.
The closed wire shape is `schemas/native-v0.69.schema.json`.

## Geometry mask enabled expression v1

Native 0.68 adds optional `enabled_expression` to an installed GeometryMask,
beside its authored enabled literal and existing exact link. Link and expression
are mutually exclusive. The closed version-1 source is `true`, `false`, an exact
`Ref{Object ID,"","mask.<stable mask ID>.enabled"}` on a distinct installed mask
in the same Composition, or its negation. Evaluation drives mask bypass in the
scene, Canvas and SVG while preserving the authored literal, mask identity,
geometry source and exact expression text. The legacy owner-slot `mask.enabled`
is literal-only. The dedicated Session command sets an expression with explicit
replacement; linking clears an expression and unlink freezes the evaluated bool.
Same-ID `SetMask` edits preserve the source and reject direct driven changes.
Native 0.1–0.67 remain readable; an expression under an older version or beside
a link is rejected. The closed wire shape is `schemas/native-v0.68.schema.json`.

## Operation enabled expression v1

Native 0.67 adds an optional `enabled_expression` beside the authored enabled
literal on built-in ShapeOperation entries. It is mutually exclusive with the
existing `enabled_driver`. The closed version-1 source is `true`, `false`, an
exact same-Composition built-in `op.<stable ID>.enabled` Ref, or its negation.
The literal and exact expression text remain authored; pure evaluation drives
shape, paint, compositing and export bypass. Unlink freezes the evaluated bool
in one Session command. Tagged Macro instances and Macro graph nodes cannot
own this expression. Native 0.1–0.66 remain readable; an expression carried
under an older version or alongside a link is rejected. The closed wire shape
is `schemas/native-v0.67.schema.json`.

## PresetDefinition v1

Native 0.63 adds a document-owned map of stable-ID `PresetDefinition`s. The bounded
v1 schema accepts exactly one `nect.shape.offset@1` entry followed by one
`nect.shape.repeater@1` entry for the `local_paths_and_paint` target domain. It
stores enabled literals, every built-in Scalar parameter, and the supported literal
options; it stores no object references, expressions, gradients or executable code.
`schemas/native-v0.63.schema.json` describes the serialized form. Native 0.62 and
older documents remain readable with an empty PresetDefinition map. Native 0.64
extends that format with same-document Definitions and Instances.

Create-from-stack selects these two built-ins in their source stack order and
reports the exact captured source operation IDs. Missing, reversed, or repeated
Offset/Repeater instances reject. Other paint operations remain in the source
object but are not copied into the definition. A driven enabled bit, Scalar, or
applicable option on a captured operation rejects with `PRESET_NONPORTABLE_SOURCE`
and exact operation Refs. Applying a definition validates the target and stack
limit, appends fresh operation IDs in one Session commit, and is one Undo. Rename,
update and delete participate in native serialization and history. Re-editing a
definition does not alter operation snapshots already applied to Objects. This Preset v1 does not include Action graphs or a cross-document preset library. Macro definitions are a separate native v0.65 feature.

## Macro v1

Native 0.65 adds document-local Macro definitions with append-only pinned revisions and tagged Macro entries in the same ordered Path/Text processing stack as built-in operations. `schemas/native-v0.65.schema.json` describes the serialized form. The bounded first graph is Offset@1 -> Repeater@1 over `local_paths_and_paint`; graph validation resolves stable node IDs and typed edges independent of display/storage order. Group stacks and arbitrary executable graphs are unsupported.

Revision 1 publishes the stable public parameter ID `macro.offset.amount`, mapped to Offset.amount in document units. An instance pins a definition revision and stores optional numeric overrides by that exact public ID. `properties` and `get` expose the ordinary stable Ref `{object, point: instance ID, field: "macro.offset.amount"}`. Editing an override, toggling or reordering an instance, updating its pinned revision, and detaching it use atomic Session commands with Undo. Revision migration preflights all existing overrides against the target revision; unresolved or incompatible values reject without changing authored state. Detach replaces the Macro at the same stack position with fresh ordinary operation IDs and copies published values.

Native v0.65 requires the `macros` root array and tagged Path/Text stack entries. Native v0.64 and older continue to decode with no Macro definitions, and legacy untagged stack operations retain their ordinary operation meaning. A Macro-bearing payload that claims an earlier native version is rejected. PresetDefinition v1 does not flatten or capture mixed Macro stacks.

## PresetDefinition v2 — ordered built-in and Macro entries

Native 0.66 adds PresetDefinition schema v2 while retaining the v1 representation
and its Offset@1 → Repeater@1 semantics. A v1 Preset read into memory is still
written as v1 with the same stable Preset ID and literal values. Schema v2 stores
one ordered tagged sequence: supported built-in operation payloads or a Macro
Definition ID, pinned revision, enabled literal, and literal overrides keyed by
stable PublicParamID. The Macro graph and its internal node IDs are not copied.

Capture records the complete supported Path/Text processing sequence in its
authored order. Built-in enabled/options/parameter fields must be literal, and
gradient payloads or unsupported stack entries refuse capture. JSON-lines/formal
MCP `create_preset_from_stack` defaults to v1 for existing callers; pass
`schema_version: 2` to capture the complete tagged sequence. The Desktop Preset
action requests v2 explicitly. Applying first preflights the entire sequence,
target domain, pinned Macro revisions and public
parameter contracts, then appends fresh processing-entry IDs and Macro instance
IDs in one Session revision. Failure leaves the target, revision and history
unchanged. Re-editing changes later applications only. Changing a Macro pin
requires every captured PublicParamID to remain present with compatible type,
unit and domain.

An unavailable or incompatible Macro pin can be retained when reading a native
document; application reports the exact unavailable definition, revision or
parameter. Deleting a Macro Definition referenced by a Preset is refused. Native
0.65 and older reject Preset v2 as a version lie. The closed native 0.66 shape is
defined in `schemas/native-v0.66.schema.json`.

## Workspace Preset Library v1

The workspace Preset Library copies canonical PresetDefinition v1/v2 payloads
containing supported built-in literal entries. It refuses Macro entries and
other source-dependent or executable content. Publication assigns a workspace
AssetID distinct from the source document DefinitionID and writes one bounded
payload file; roots, Favorite AssetIDs and shared Quick Access slots remain in
`library/v1/state`. Native 0.75 is unchanged.

Reading an asset validates the exact requested AssetID, envelope version,
accepted revision, schema, canonical payload and SHA-256. Unknown versions and
corrupt or missing assets remain unavailable under their exact identity; the
Library does not substitute by label. Publishing, updating, deleting, adding a
Favorite and assigning a slot are separate explicit operations. An update keeps
AssetID/FavoriteID, advances the accepted revision and affects future imports
only. Import and apply allocates a fresh document DefinitionID and appends fresh
operation IDs in one Session commit, so one Undo removes both the definition and
application. No source document or payload code is needed when applying an asset.

## Workspace Macro Library v1

The workspace Macro Library stores the canonical typed `MacroDefinition` JSON
payload, including every retained graph revision, stable graph-local IDs,
`latest_revision`, and PublicParamID mappings. Its sibling `.macro.json` envelope
uses schema 1, a distinct workspace AssetID, accepted asset revision and SHA-256.
The source MacroDefinitionID remains in the copied payload; the Document import
requires a fresh DefinitionID and processing instance ID. Asset revision is
caller-supplied receipt context and remains distinct from the explicitly selected
retained Macro pin.

Only the validated Offset@1 -> Repeater@1 graph in the
`local_paths_and_paint` domain is portable. Unsupported graph nodes, versions,
schemas, malformed/corrupt bytes and missing identities remain unavailable under
their exact AssetID; reads do not rewrite those files. Preset and Macro files
share the bounded 256-immediate-asset store and existing Favorite/Quick Access
authority. Explicit asset update advances only the workspace accepted revision;
already imported Document definitions and pinned instances remain unchanged.
`import_apply_macro` is an ordinary serializable Macro command inside generic
`apply.commands`, so fresh definition, instance and stable numeric overrides
commit atomically with one Undo and roll back with any later failing batch command.
The command preserves all retained graph revisions while pinning one explicit
revision for the new instance. JSON-lines and MCP use this same Session command;
the API receipt reports caller metadata without attesting which Library file was
read. Native 0.75 and its unsupported native-node behavior are unchanged.

## Definition / Instance v1

Native 0.64 extends the 0.63 native schema with stable-ID `Definition`s and
`Instances` in the same Document. `schemas/native-v0.64.schema.json` describes the
combined serialized form.
A Definition points at an existing authored Object subtree; its source Items keep
their existing stable Object IDs and remain the only authored source Objects.
Creating, renaming and deleting a Definition and placing, overriding, resetting or
detaching an Instance use the shared Session command boundary and one atomic history
entry. Deleting a Definition with a live Instance returns `DEFINITION_IN_USE`.

The first vertical accepts one source subtree and one Composition. Definition
dependencies must stay inside that subtree; dangling dependencies, escaping links,
expressions, masks or transform parents, nested Instances and cross-Composition
placement reject. Instance identity is stable and separate from source identity.
Each Instance keeps its own authored placement transform, visibility, order and
compositing. Source edits to geometry, text content, descendant transforms,
descendant visibility and root compositing/content flow into every Instance.

Definition evaluation ignores the source root's affine transform, anchor, transform
parent and visibility source. The source root's content and compositing—including
its opacity, blend, mask and isolation—remain part of the evaluated Definition.
Descendant authored transforms and visibility remain in the source subtree. Scene
projection expands unique transient proxy IDs for render/export/Canvas lookup;
these proxy Objects are evaluated state and are never added to the native Document.
Each Composition render's combined authored and transient projected Object count
must stay within 10,000. Exceeding that render bound returns
`INSTANCE_RENDER_LIMIT`. A Document supports at most 10,000 Definition records.

Scalar Instance overrides are limited to source `composite.opacity` and Text
`text.font_size`. Keys use stable source Object IDs and empty point IDs. Other scalar,
typed, structured and nested overrides are unsupported. Reset removes one local key
so the current source value flows through again. Detach copies the current source
subtree and overrides into ordinary Objects under the Instance's existing layer ID,
resets copied source-root affine/anchor/transform-parent/visibility, and preserves
source-root content/compositing. The materialized copy no longer changes with source
edits. One Undo restores the live Instance and one Redo restores the exact copy.

Layer is the UI presentation of an Object; there is no duplicate Layer state model.

Raster, Resource, general node graphs, addressable procedural generated Instances
and full compositing are not stubbed ahead of real callers.

## Identity / ownership

IDs are opaque, document-unique strings. Names and array indices are not identity.

Rename and point reorder do not retarget references.

Objects are owned exactly once by a Composition root or Group child. Ownership is derived from roots/children, not duplicated in a parent field.

Collections are non-owning ordered sets. Collection membership does not alter the source object's parent or transform inheritance.
`CollectionCommand` creates, renames, replaces ordered members and deletes a
Collection through the ordinary atomic Session revision/Undo path. A member is
an exact existing Object ID; membership is unique and limited to 10,000 IDs.
Groups and descendants are independent membership choices. One Object may be in
multiple Collections, and deleting an Object prunes all memberships. The Desktop
Collections menu and JSON-lines/formal MCP commands use this same authority.
Collection-only edits do not change draw order, transforms, visibility or Canvas
pixels. Native 0.64 already stores Collection IDs, names and member order, so this
command surface does not change the save format.

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

### Independent object duplication (native 0.13 unchanged)

`DuplicateObjects{objects,prefix}` / `duplicate_objects` copies 1..1000 selected
objects in one Composition, including each selected Group's structural closure
once. Selection order does not change paint order. Each copied sibling run is
inserted just above its original run under the same structural owner. Copies
start in place; `TranslateObjects` is an explicit subsequent placement edit.
This is independent authored state, not an instance or clipboard format.

The caller supplies a fresh ASCII ID prefix (1..48 characters). The command
allocates fresh object/source/contour/point/operation/gradient/stop/mask IDs;
collisions and all normal validation/history limits reject the entire batch.
Generated point roles and Point Edit identity follow the new retained source.
Bindings, parsed expression references (including disabled authored sources),
masks and Transform Parents within the closure target the copies. Expression
text outside changed reference arguments retains its formatting. Outgoing
references stay on their original targets. Named Colors and accepted image
assets stay shared. Existing inbound references and Collection memberships
continue to address only originals. Text content and retained procedures remain
editable, including disabled corrections/operations. No native migration occurs.

GUI Edit/context menus and Ctrl+D use this command and select the copied roots.
`apply` responses include `created_ids` for new objects; `inspect` exposes their
hierarchy/nested IDs. The pure core `duplicated_roots` projection identifies the
new root selection before a successful commit. One Undo removes the copy and
Redo restores exactly the same IDs. Cross-document/Composition copying, linked
instances, implicit displacement and partial point duplication are unsupported.

Native JSON stores authored data only, not evaluated caches, Qt widgets, or session revision.

Unknown fields/versions/kinds, duplicate JSON keys, invalid references, non-finite values, unsupported color/unit claims are rejected explicitly.

Current M0 limits are safety bounds, not product performance targets.

## Native 0.2: retained primitives and point corrections

The primitive slice introduced 0.2; current native readers accept strict 0.1 through
0.73. Migration of 0.1
preserves authored values, IDs and bindings, with no geometry conversion. The
historical linked fixture in `tests/fixtures/native-v0.1-linked.nect` is loaded,
edited, saved and reopened in separate processes. Unknown fields and behavior
versions remain errors. See `schemas/native-v0.2.schema.json`.

A Path owns either authored contours or one retained primitive source, never
both. `nect.shape.circle`, `nect.shape.ellipse` and `nect.shape.rectangle`
version 1 map typed local distance parameters to a closed cubic path. Source instance IDs are stable;
derived contour and point IDs append fixed semantic roles (east/south/west/north
or corner names). Source IDs have a 64-character bound; generated IDs fit the
normal 96-character bound. The correction instance namespace is reserved.

Circle has center_x, center_y and radius; Ellipse and Rectangle have center_x,
center_y, width and height. `generator.*` parameters are ordinary linkable distance properties.
Derived point/handle fields join the same dependency evaluator; generator/point
cycles reject atomically. Circle uses four cubic arcs, with handle length
`radius * 0.5522847498307936` (a cubic approximation, not an exact rational circle).
Ellipse uses stable east/south/west/north anchors at its half-width and half-height
extents. Its east/west handles use `height/2 * 0.5522847498307936`; north/south
handles use `width/2 * 0.5522847498307936` with the same tangent convention.

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
- Fill supports nonzero/evenodd. Stroke v1 uses butt caps and miter joins (limit 4).
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

`transform_objects {objects:[id],rotation,scale_x,scale_y,pivot:null/[x,y]}`
applies one common world-space edit to1..1000 unique objects in one Composition.
The initial snapshot supplies all selected world matrices and, for null pivot,
the center of their union of evaluated geometric bounds (stroke width excluded).
An explicit pivot permits geometry-free objects. Scale along Composition axes,
then rotate clockwise in Y-down coordinates: W'=T(p)*R*diag(sx,sy)*T(-p)*W.
Zero/negative scales are deliberate collapse/reflection; finite command magnitudes
for rotation/scales are limited to1e9. This differs from single-object local-axis
`transform_around_anchor`; authored Anchors, sources, IDs and hierarchy stay intact.

A selected effective descendant/follower inherits the same edit once and retains
its exact local matrix. Other selected objects solve against their external
parent's initial world matrix. Necessary singular inverses, changed driven fields,
nonfinite output, cross-Composition selection and final world-target mismatch
reject atomically. Unselected followers and explicitly authored dependencies keep
normal behavior; this is a matrix edit, not an appearance freeze/bake. Native0.13
is unchanged. Bounds-center pivot requires geometry for every selected object.

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
references and placement retained. Fresh Primitive, Path and Text creation initializes
Anchor once from canonical local geometry in the shared Session transaction;
GUI, API and MCP callers need no additional Center Anchor command. Initial atomic
batches resolve forward references before initialization, and explicitly authored
Anchor axes take precedence. Geometry depending on another fresh Anchor initializes
that source first; a circular initial-Anchor dependency rejects atomically with
CREATION_ANCHOR_CYCLE. Anchor-dependent position/transform commands in the
same batch consume the initialized center. Empty geometry retains a neutral Anchor;
the explicit Center Anchor command still rejects EMPTY_BOUNDS.
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

## Native 0.71 Alpha mask v1

Native 0.71 extends the same `Object::compositing.mask` record with required
`mode:geometry|alpha` and `invert:bool`. Readers of native 0.1–0.70 migrate an
existing mask to `geometry` and `invert=false`; a document claiming an older
version while carrying these fields is rejected. Luma is unsupported in v1.
Invert is valid only for Alpha mode. Geometry retains its previous path-fill
coverage and fill-rule behavior.

Alpha renders the retained source subtree to a bounded transparent RGBA surface
in Composition space, then uses that rendered alpha at the existing target mask
stage. It includes source paints/effects, Group children and internal masks,
image alpha, and source opacity. It ignores only the source root's ordinary
visibility and blend; descendant visibility and child blends remain normal.
The target still follows content, Group postchildren effects, mask, opacity and
blend. Inversion is `1 - source_alpha`, including coverage outside the source
within the target content bounds. Alpha source cycles follow appearance
dependencies through Group children and Alpha mask-source edges; Transform
Parent edges remain separate geometry dependencies.

The first Alpha slice accepts Path, Text, Image and Group sources. SVG explicitly
refuses a document containing an Alpha mask until it can project that mask
without loss. Native, Session/API, MCP and the desktop inspector expose the same
mode and invert fields.

## Native 0.72 sRGB Luma mask

Native 0.72 extends the same mask record with `mode:geometry|alpha|luma`,
`invert:bool`, and required `mask_color_space:"srgb"`. Readers of 0.1–0.70
migrate masks to Geometry, non-inverted, sRGB. Readers of 0.71 retain their
saved Geometry/Alpha mode and invert bit while defaulting the profile to sRGB.
Newer mask fields under an older version are rejected. Geometry remains the
existing Path/Text fill-rule clip. Invert is valid for Alpha and Luma only.

Luma uses the same isolated source RGBA projection as Alpha: retained Path,
Text, Image or Group appearance in Composition space, including source effects,
internal masks, child visibility and opacity, while ignoring only source-root
visibility and blend. From premultiplied 8-bit sRGB projection bytes, RGB is
unpremultiplied only when alpha is nonzero. For straight channels normalized to
`[0,1]`, coverage is
`(0.2125 R + 0.7154 G + 0.0721 B) * A`; zero-alpha pixels yield zero. Optional
inversion follows this calculation and fills outside-source pixels within the
target content bounds. The result is rounded to the nearest byte and applied at
the existing postchildren mask stage before target opacity and blend.

Enabled Luma masks are refused by SVG until an exact projection is available;
disabled masks may export their matching visible output. Native, Session/API,
MCP and the desktop Inspector expose mode, invert, source, enabled state and
the explicit sRGB profile. This is an 8-bit sRGB subset and makes no claim for
linearRGB, ICC-managed, high-dynamic-range, After Effects or hands-on GUI parity.

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

## Native0.13 — retained raster assets and Image placements

The new `raster_assets` table owns stable asset IDs, names, version1 mode
(`linked`/`embedded`), locator and immutable accepted original bytes. Persisted
metadata records SHA256, MIME, oriented pixel width/height, JPEG EXIF orientation,
color interpretation and interpretation_version1. Strict decode validates the
complete bounded memory image and compares every persisted metadata field; it
never trusts a declared hash/dimension. Canonical base64 rejects malformed padding,
nonzero padding bits, whitespace and oversized source data. Older0.1–0.12 documents
migrate with an empty table; old readers reject the new fields/type.

An `image` leaf owns `{asset,width:Scalar,height:Scalar}`. Its rectangle starts at
local(0,0); positive display dimensions up to1e7du use ordinary property addressing,
links, unit checks, expressions and multi-edit. Existing affine/Anchor/Transform
Parent, visibility, masks, opacity, isolation and blends apply unchanged. The
image domain has no contours, text/generator/PointEdit, children or Shape stack.
`evaluate_scene.images` projects accepted payload references and display dimensions;
Shape evaluation explicitly rejects Image. Only Path/Text can source a geometry
mask. Asset deletion rejects existing Image references; placement deletion does
not discard reusable assets. AddRasterAsset/CreateImage can be one atomic batch;
ReplaceRasterAsset updates every placement without touching object fields.

RasterPayload has a private validating factory and no mutable public data. Document
copies, gesture snapshots and history share its immutable original byte allocation.
Equality compares actual contents when pointers differ. Asset history deltas account
for retained encoded source bytes; derived RGBA buffers never enter authored/native
state. Canvas keeps bounded current-Composition image projections, dropping unused
entries and decoding only a newly accepted payload. Its QImage data is premultiplied
for Qt composition; the memory backend exposes oriented straight-sRGB RGBA8.
API changed_ids diffs authored structs directly instead of serializing/parsing all
base64 data during each semantic edit. Asset replacements also report placement IDs.

`Host::import_image` / `update_asset` read only explicitly named absolute local
Windows drive paths, bounded to8MiB, reject file changes during the read, then use
normal Session commands with serialized-size preflight. GUI, local API and formal
MCP share these methods. No native decoder, expression or core evaluator reads
locator paths. Embedded locator is empty. Linked locator is an absolute opaque
path, preserving meaning across Save As and recovery. Link observations are
transient, keyed to locator+accepted hash, with UTC check time and optional observed
hash. Explicit Check yields current/changed/missing/unreadable; no authored revision
changes. Open begins unchecked. Reload/Relink failure leaves the entire Session
unchanged; Embed uses cached data even for a missing source. All placements retain
size and transforms. Current status is an observation, not an ongoing file watch.

The backend instantiates only Microsoft WIC PNG/JPEG decoder CLSIDs, from memory;
COM/WIC/BCrypt are Windows components, not new downloaded dependencies. Original
source<=8MiB, each axis<=8192, pixels<=16,777,216. Document totals<=24MiB and
33,554,432 pixels across at most128 assets. Native/CLI/local-socket/protection limit
is64MiB. Pure core enforces authored budgets; IO/Host asset admission also serializes
the candidate before committing to prevent accepting an unsavable import. An
oversized history entry remains atomic under the existing64MiB history estimate.

Color/orientation interpretation is versioned: 8-bit RGB/gray and supported indexed
PNG, JPEG EXIF1–8, alpha preserved, unprofiled color explicitly assumed sRGB. Usable
v2/v4 RGB/gray ICC is converted via memory color contexts to sRGB, without altering
alpha. PNG iCCP is inflated through the existing Boost headers with full-stream,
Adler32 and1MiB output checks before WIC metadata allocation. JPEG ICC segments
are assembled with the same limit. Invalid profiles/metadata, non-sRGB standalone
PNG gamma/chromaticities without a usable profile, CMYK, >8-bit channels, animation,
PNG eXIf and other formats reject explicitly. No resizing, source rewrite or hidden
bit-depth reduction occurs. Non-Windows raster operations report backend unavailable.

SVG reuses the same decoder, emits lossless normalized metadata-free sRGB PNG data
once per asset in definitions, and places it through scale/world transforms under
existing mask/compositing wrappers. Source orientation is already baked into that
projection; original orientation/profile/bytes remain native. SVG admits<=16MiB
normalized PNG per asset /32MiB aggregate; exceeded encoding budgets are explicit
errors. export_plan discloses image conversion. Import/reload and export are bounded
synchronous operations; async decoding, automatic watching, relative locators,
vector/PDF/RAW assets, pixel painting, image alpha/luma masks and vector operators
on raster images remain unsupported.

Primary platform references: [WIC native pixel formats](https://learn.microsoft.com/en-us/windows/win32/wic/-wic-codec-native-pixel-formats),
[WIC metadata](https://learn.microsoft.com/en-us/windows/win32/wic/-wic-about-metadata),
[IWICColorContext](https://learn.microsoft.com/en-us/windows/win32/api/wincodec/nn-wincodec-iwiccolorcontext).


## One-shot geometric alignment

`AlignObjects` / `align_objects` uses evaluated world-space geometric bounds
(exact transformed cubic extrema, evaluated paint instances, text contours and
image rectangles). Group bounds include descendant geometry; visibility, stroke
width and masks do not redefine these bounds. It moves transforms, preserving
source geometry, stable IDs, linear transforms and native0.13.

Required JSON fields are objects, axis (x/y), alignment (min/center/max), and
artboard (null or ID). Selection-envelope targets require2–1000 unique objects;
an Artboard target allows1–1000 in its Composition. Structural ancestor plus
descendant selection rejects as OVERLAPPING_SELECTION. Empty bounds, cross-
Composition targets, driven translations and unrepresentable transforms reject.

Displacements are solved together across effective Transform Parents. A selected
follower can move independently of its selected parent. Results are reevaluated
and must equal every original bound translated by its requested displacement;
geometry/reference side effects reject as ALIGNMENT_PRESERVATION. Session applies
the command atomically with ordinary history. No persistent layout constraint or
new saved format is introduced.

`DistributeObjects` / `distribute_objects` shares the alignment bounds and
simultaneous translation solver. It requires3–1000 unique objects in one
Composition, axis x/y, no structural overlapping selection. Sort by starting
minimum on that axis (stable ID breaks ties), preserve both outer objects, and
divide the sum of original adjacent gaps equally among all gaps. Initial axis
overlap rejects as OVERLAPPING_BOUNDS, including nested intervals. Zero gaps and
zero-size geometry are allowed. All other preservation/atomicity rules apply;
dependent geometry failure is DISTRIBUTION_PRESERVATION. No persistent layout
constraint is authored.


### Neutral static Group ungrouping

`ungroup {composition,parent,group}` replaces one Group at its sibling position
with its ordered children. Surviving object/point/operator IDs and geometry are
retained. Inheriting children compose the removed Group's static local affine
into their own; explicitly external child Transform Parents remain unchanged.
No inverse is needed, including singular parent/group transforms. Group membership
in Collections is pruned; child membership stays intact. One Session Undo restores
all authored state. Native format remains0.13.

The Group must be visible, normal/pass-through, literal opacity1, with no mask or
shape stack. Driven Group affine values and explicit Group Transform Parent are
refused (`UNGROUP_APPEARANCE`/`UNGROUP_DYNAMIC`). Surviving references to the removed
Group, including explicit child Transform Parents, remain invalid; they are never
silently retargeted or frozen. Driven child matrix changes refuse normally.
All surviving world matrices and evaluated non-matrix values must stay equal;
transform-dependent geometry changing through the reorganization rejects
`UNGROUP_DEPENDENCY`. Group Anchor is removed with its container; child Anchors
and local geometry remain authored unchanged. This is bounded source-preserving
reorganization, not a flatten-compositing conversion.

## Stroke behavior v2 within native 0.13

`stroke_style {object, operation, line_cap, line_join, miter_limit}` explicitly
promotes one existing Stroke to behavior version 2. Caps are `butt`, `round`,
`square`; joins are `miter`, `round`, `bevel`. Miter limit is a Scalar in [1,1000]
addressable as `op.OP.miter_limit`. The command preserves an unchanged driven
limit and rejects changing it with `DRIVEN_PROPERTY`. Invalid styles/ranges reject
the entire command batch. Source geometry, IDs and stack order are unchanged.
Undo restores the exact prior version; native save/reopen retains version and style.

Default/add-operation Stroke remains v1: its encoded fields and butt/miter/4
behavior do not change. Version 2 requires retained `line_cap`, `line_join` and
`parameters.miter_limit`; native 0.13 schema accepts both versions. Older behavior
readers reject v2 rather than silently discarding its appearance. Native <=0.12
cannot contain v2. This is an operator behavior extension, not a global migration.

Shared evaluated paint carries cap/join/limit to Canvas, PNG and SVG. Miter ratio
uses SVG semantics (`Qt::SvgMiterJoin`), falling back to bevel beyond the limit.
Round/square caps cover wholly zero-length subpaths with at least one segment;
an open move-only subpath has no ink. Degeneracy checks include used cubic handles
and path-instance transforms after stack evaluation. Canvas supplements Qt's
omitted zero-length segments with a single combined stroke outline so translucent
paint is not applied twice at overlaps. Paint transforms/gradients/compositing
retain their existing coordinate space. Width zero remains invisible.

References: [SVG stroke semantics](https://www.w3.org/TR/svg-strokes/),
[SVG zero-length segments](https://www.w3.org/TR/SVG2/paths.html#Zero-length-path-segments),
[Qt 6.5 QPen](https://doc.qt.io/qt-6.5/qpen.html), and
[Qt stroker implementation](https://raw.githubusercontent.com/qt/qtbase/6.5/src/gui/painting/qstroker.cpp).
Inspector style controls and SVG intake of these styles are the next checkpoint;
this contract does not claim those adapters yet. Dashes, pressure, brushes and
variable width remain unsupported.

## Native 0.14 — authored Guides, Grid and Margin

Composition `guides` is a required array in 0.14; each Guide has a document-unique
ID, axis (`x` for a vertical line, `y` for a horizontal line), name and position
in Composition coordinates. At most 10,000 Guides are allowed document-wide.
An Artboard may own one optional `layout` with independently authored Margin
insets and Grid. Grid has its own document-unique ID, Artboard-local bounds,
column/row counts and gutters. Moving an Artboard carries the Grid in world
space without changing its local numbers. Resizing an Artboard or an inherited
parent size revalidates the layout and rejects the whole edit if it no longer
fits; definitions are never automatically scaled or clamped.

`AddGuide`, `UpdateGuide`, `DeleteGuide` and `SetArtboardLayout` use the shared
Session command path and revision gate. `SetArtboardLayout` replaces one
Artboard's complete optional layout; callers editing only Margin or Grid must
carry the other authored component forward. The P02-B “Set Grid to margin
box” control will copy the current evaluated Margin rectangle into Grid bounds
once, without linking later edits. Undo, Redo and recovery use the accepted native
state. Artboard duplication copies layout values with a fresh Grid ID; adding
a blank Artboard starts without layout.

The 0.14 reader migrates 0.1–0.13 with empty Guides and absent layouts, keeping
existing IDs and geometry. The writer emits 0.14; the strict schema is
`schemas/native-v0.14.schema.json`. Native save retains definitions, while
SVG/PNG artwork export omits layout overlays. Canvas editing and visual
acceptance are separate P02-B/C checkpoints; this section describes the
authored model and codec.

## Native 0.15 — typed Text italic driver

`TextSource.italic` remains the authored boolean literal. A Text source may also
store one optional `italic_driver`: `{ "link": Ref }` or
`{ "expression": Expression }`. The only valid link source and target is the
stable object Ref `{object, point:"", field:"text.italic"}`. An expression has
version 1 and accepts `true`, `false`, `ref("object-id","","text.italic")`,
or `!ref("object-id","","text.italic")`. These are boolean expressions;
numeric Scalar links and expressions do not coerce to or from this property.

Evaluation is pure and detects missing or non-Text references, self-reference,
cycles and excessive depth. The evaluated value drives Text layout and render;
the literal and driver remain authored. `link_text_italic`,
`set_text_italic_expression` and `unlink_text_italic` are Session commands with
the normal revision and Undo semantics. Unlink freezes the current evaluated
value as the literal. Editing the literal while driven requires an explicit
unlink, and replacing a driver requires `replace_driver:true`. Rename and
reorder do not retarget a stable Ref; duplication remaps references within the
duplicated set. A source cannot be deleted while a surviving driver refers to
it. Invalid edits reject the entire command batch.

The 0.15 reader accepts 0.1–0.14 Text without a driver and retains its literal.
The writer emits 0.15 and includes `italic_driver` only when authored. The
strict shape is `schemas/native-v0.15.schema.json`; semantic reference and
cycle checks belong to the core. Other boolean fields and non-Scalar types do
not gain a generic link or expression contract in this version.

## Native 0.16 — typed Text weight link

`TextSource.weight` remains the authored integer literal in [1,999]. A Text
source may also store one optional `weight_driver:{"link":Ref}`, where the
source and target refs are stable Object IDs with empty point and field
`text.weight`. Only Text weight may link to Text weight. The driver has no
offset, arithmetic, expression or implicit conversion to a Scalar double.

Evaluation follows the integer link without changing either authored literal.
Text layout and render use the evaluated weight. `link_text_weight` requires an
explicit replacement flag when another driver exists; `unlink_text_weight`
freezes the current evaluated value into the literal. Rename and reorder do not
retarget the link. Copying both Text objects remaps the copied link to the
copied source. Missing, wrong-type, cyclic and out-of-range values reject the
whole Session command batch, and a surviving dependent prevents source deletion
until it is unlinked.

The 0.16 reader accepts 0.1–0.15 Text as literal-only weight and rejects a
`weight_driver` in those earlier versions. The writer emits 0.16 and omits the
driver when absent. `schemas/native-v0.16.schema.json` constrains the wire
shape; the core validates semantic references and cycles. Integer expressions,
generic multi-target links and other integer fields remain unsupported.

## Native 0.17 — typed Text content link

`TextSource.content` remains the authored UTF-8 literal (at most 32,768 bytes).
A Text source may also store one optional `content_driver:{"link":Ref}`, where
the source and target are stable Object IDs with empty point and field
`text.content`. Only Text content may link to Text content. The driver has no
offset, expression, interpolation or implicit Scalar conversion.

Evaluation follows the stable link with a 128-edge depth bound, detects missing,
wrong-type, self and cyclic references, and checks the evaluated UTF-8 content
limit. Layout, Canvas, shape outlines, bounds, SVG and Inspector preview consume
the evaluated string; inspection and native save retain the literal and driver.
`link_text_content` requires `replace_driver:true` to replace a driver, while
`unlink_text_content` freezes the current evaluated string into the literal.
`UpdateText` may edit other fields while preserving a driver, but cannot change
the driven literal. Rename and reorder do not retarget a link; duplication
remaps links whose source is also copied. A surviving dependent prevents source
deletion until it is unlinked. Failed commands preserve the Session and revision.

The 0.17 reader accepts 0.1–0.16 Text as literal-only content and rejects a
`content_driver` in those earlier versions. The writer emits 0.17 and omits the
driver when absent. `schemas/native-v0.17.schema.json` constrains the wire
shape; the core validates semantic references and cycles. Generic Scalar links,
expressions, multi-target content edits and links for other Text strings or enums
remain unsupported.

## Native 0.18 — typed Text font-family link

`TextSource.family` retains its authored nonempty UTF-8 literal (at most 1,024
bytes) and may store one optional `family_driver:{"link":Ref}`. The link source
and target are stable Text Object IDs with an empty point and field
`text.family`; cross-field, cross-kind and Scalar coercion are unsupported.
Evaluation follows same-type links with a 128-edge bound and validates the
evaluated family against the existing font-family constraints. The shared Text
projection supplies layout, Canvas, shape, bounds, SVG and Inspector measurement;
native inspection retains the literal and driver. Font fallback and warning
behavior remain the existing platform behavior.

`link_text_family` requires `replace_driver:true` to replace a driver.
`unlink_text_family` freezes the current evaluated family. `UpdateText` may edit
other Text fields while preserving the driver, but cannot change its driven
literal. Rename/reorder preserve the stable Ref; duplication remaps it when
both ends are copied. A surviving dependent prevents source deletion. All
commands use Session revision, atomic validation and Undo.

The 0.18 writer adds only optional `family_driver`, omitting it for literal-only
Text. Readers accept 0.1–0.17 family literals and reject this driver in older
versions. `schemas/native-v0.18.schema.json` constrains the closed wire shape;
the core verifies existence, same-field/type, cycles, UTF-8 and byte limits.
Generic Scalar commands, expressions, offsets, name rebinding and links for
other strings or enums remain unsupported.

## Native 0.19 — typed Text direction link

`TextSource.direction` keeps its authored `horizontal` or `vertical` literal.
It may also carry an optional `direction_driver:{"link":Ref}` to another Text
object's `text.direction` field. Source and target use stable object IDs and an
empty point ID. Evaluation follows the link without changing either literal,
with the existing 128-edge depth, missing-source and cycle checks. The shared
evaluated Text projection supplies direction to layout, Canvas, shape, bounds,
SVG and Inspector measurement; native inspection retains authored values.

`link_text_direction` uses the Session's revision and Undo path and requires
`replace_driver:true` to replace an existing driver. `unlink_text_direction`
freezes the evaluated choice into the target literal. `UpdateText` can change
unrelated fields while preserving a direction link, but cannot directly edit
its driven literal. Rename and reorder preserve the source ID, duplication
remaps copied links, and deleting a still-referenced source is rejected.

The 0.19 writer omits `direction_driver` for literal-only Text. Earlier native
versions retain their literal direction on read and reject this field if it is
present. `schemas/native-v0.19.schema.json` constrains the closed wire shape;
the core checks semantic references and the `horizontal`/`vertical` domain.
Other Text enums, enum expressions and generic typed batch links remain open.

## Native 0.20 — typed Text layout link

`TextSource.layout` retains its authored `auto` or `frame` choice and may store
one optional `layout_driver:{"link":Ref}` to another Text object's
`text.layout` field. Stable object IDs and an empty point ID identify the same
enum domain. The shared pure Text projection substitutes only the evaluated
layout choice; the target's `text.frame_width` and `text.frame_height` Scalars
remain its own authored values. Auto sizing ignores those dimensions, while
fixed-frame sizing uses them for wrapping and overflow. A link does not copy
the source's frame, font or content.

`link_text_layout` and `unlink_text_layout` use Session revision, validation
and Undo. Replacing a driver is explicit, unlink freezes the evaluated choice,
and direct edits to a driven literal reject. Rename/reorder preserve stable
references, duplication remaps copied endpoints, and a surviving dependent
prevents source deletion. The 128-edge depth, missing-source and cycle checks
apply to this enum lane.

The 0.20 writer omits `layout_driver` for literal-only Text. Older native
versions retain their layout literals but reject a present 0.20 driver field.
`schemas/native-v0.20.schema.json` constrains the closed wire shape; the core
checks semantic references and the `auto`/`frame` domain. Other enum links,
enum expressions and generic typed batch links remain open.

## Native 0.21 — typed Text alignment link

`TextSource.alignment` retains its authored `start`, `center` or `end` literal
and may store one optional `alignment_driver:{"link":Ref}` to another Text
object's same `text.alignment` field. The pure evaluated Text projection uses
the source's current alignment while preserving the target's authored literal,
frame dimensions, locale and other properties. Fixed-frame text exposes the
alignment change in its layout; the source's frame is never copied.

`link_text_alignment` and `unlink_text_alignment` follow the Session revision,
validation and Undo path. Replacing a link is explicit, unlink freezes the
current evaluated alignment, and `UpdateText` preserves a driver on unrelated
edits while rejecting an implicit driven-literal change. Stable IDs survive
rename and reorder. Missing sources, wrong Text fields, self-links, cycles and
dependency chains beyond 128 reject atomically.

The 0.21 writer omits `alignment_driver` when no link exists. Native 0.20 and
earlier files retain their alignment literal and reject a present 0.21 driver.
`schemas/native-v0.21.schema.json` constrains the closed same-field Ref shape;
the core checks source existence and the closed `start`/`center`/`end` domain.
Enum expressions and generic typed batches remain separate work.

## Native 0.22 — typed Text locale link

`TextSource.locale` retains its authored nonempty UTF-8 literal, limited to
128 bytes, and may store one optional `locale_driver:{"link":Ref}` to another
Text object's same `text.locale` field. The source and target use stable Object
IDs and an empty point ID. Evaluation follows the link with the existing
128-edge dependency bound and rejects missing, non-Text, self and cyclic
references. It preserves the literal and driver while the pure evaluated Text
projection substitutes only the locale used by the existing DirectWrite layout
and font-name fallback path.

`link_text_locale` and `unlink_text_locale` use Session revision, validation and
Undo. Replacing a link requires `replace_driver:true`; unlink freezes the current
evaluated locale into the target literal. `UpdateText` may edit unrelated Text
fields while preserving a locale driver, but it cannot change the driven locale
literal or replace/remove its driver. Rename and reorder preserve the stable
Ref, duplication remaps it when both endpoints are copied, and a surviving
dependent prevents deletion of its source.

The 0.22 writer omits `locale_driver` for literal-only Text. Native 0.21 and
earlier files retain their locale literals and reject this driver field.
`schemas/native-v0.22.schema.json` constrains the closed same-field Ref shape;
the core applies the existing nonempty, 128-byte and UTF-8 checks without
canonicalizing tags or changing platform shaping, warning or fallback behavior.
BCP 47 policy, generic string links and locale expressions remain unsupported.

## Native 0.24 — retained editable Text on Path

`TextSource` may carry one optional `path_attachment` with the stable `path`
Object ID, authored `contour` ID, `start_mode` (`distance` in du96 or
`normalized` fraction), finite `start`, nonnegative finite `spacing`, and
`reversed` flag. The source and Text must share a Composition. The attachment
does not replace Text content or outlines with authored geometry. It retains
the original Text source, object ID, properties and transform; detaching removes
only this optional field.

The first consumer accepts automatic horizontal one-line Text and DirectWrite
left-to-right shaped runs. It keeps shaped glyph IDs, advances, per-glyph
baseline offsets and monochrome outlines as ephemeral projection data. Glyph
advance midpoints sample the selected contour; the intact glyph outline follows
the forward tangent and is mapped through the Text world's inverse transform.
Text alignment anchors the shaped span; whitespace advances participate even
when they have no outline. Open contours reject a span outside `[0,L]`; closed
contours wrap finite starts and reject spans longer than one lap. Unsupported
vertical, frame, multiline, RTL/bidi, generated-contour and singular-inverse
cases fail explicitly.

The shared sampler evaluates authored Path point values and the Path world
transform before any Shape stack, then adaptively subdivides cubic controls in
Composition space with a 0.05 du96 arc-length error budget, maximum depth 20
and 4096 leaves per contour. Samples invert arc length back to cubic parameter
and return the cubic point and analytic one-sided tangent. Interior authored
knots use the outgoing nonzero tangent; an open endpoint uses its incoming
nonzero tangent. Reversed traversal reverses sample order and tangent while
preserving authored point order and IDs. Canvas, geometric bounds, SVG and
`text_layout` all consume the same projected shape; attached API layout bounds
describe projected glyph geometry, not the detached Text rectangle.

`CreateText` and `UpdateText` carry attachment changes through the same Session
revision, validation, atomic commit and Undo path as other authored edits.
Deleting a referenced Path, changing it so the attachment no longer evaluates,
or creating a transform cycle rejects without changing the document or history.
Deleting the Path and all dependent Text together is valid. Duplication remaps
both Path and Contour IDs only when both are copied; Text-only copies retain the
original Path/Contour reference. Rename and reorder never retarget the link.

The 0.24 writer omits `path_attachment` for detached Text. Native 0.1–0.23
documents migrate to detached Text without changing authored IDs, text or
geometry; a 0.23 document containing `path_attachment` is rejected. The strict
`schemas/native-v0.24.schema.json` describes the wire shape, while decode and
Session validation check the actual Path, Contour, Composition and projection.
Native save/reopen preserves the exact attachment values and editable source.
Multi-line/frame, vertical, RTL/bidi, multi-contour traversal, deform mode,
variable font/features, clipping/repeat overflow policies and Group/part
consumers remain separate work.

## Native 0.25 — Group postchildren Posterize

A Group may own an ordered operation stack containing `nect.group.posterize`
behavior v1. Each instance has a stable ID, enabled flag and ordinary Scalar
parameter `levels`, an integer from 2 through 16 with default 2. Its input is
the premultiplied 8-bit sRGB RGBA image after the Group's children have each
evaluated their own operations and composited in child order. For nonzero-alpha
pixels, Posterize unpremultiplies each sRGB channel, computes
`floor(channel * (levels - 1) + 0.5) / (levels - 1)`, and premultiplies by the
unchanged alpha. Zero-alpha pixels become transparent black. Enabled instances
run in stack order; disabled instances preserve the input. The Group mask,
opacity and blend are applied after its pixel operations. Groups reject
Path-local Fill, Stroke, Offset and Repeater operations; Path, Text and Image
reject Group Posterize.

The 0.25 writer stores the Group stack using the existing operation IDs,
parameter Scalars, property refs, revision and Undo machinery. Native 0.1–0.24
documents migrate with an empty Group stack, and 0.24 documents reject a
smuggled Group `stack` field. `schemas/native-v0.25.schema.json` strictly
describes the Group-only wire shape; core validation enforces the integer range
and target domain. Canvas and PNG evaluate the actual postchildren pixels. SVG
export refuses an enabled Group Posterize and its API export plan lists the
unsupported effect; a bypassed instance allows ordinary SVG projection. This
operator uses only the existing 8-bit sRGB pixel path and makes no HDR,
linear-light or exact vector-export claim.

## Native 0.26 — linked Fill rule

Path and Text Fill operations expose their `nonzero` or `evenodd` rule as an
authored enum property at `op.<operation-id>.fill_rule`. The operation keeps its
literal choice when a same-field stable `Ref` link drives its evaluated choice.
`link_fill_rule` requires an explicit `replace_driver` flag; `unlink_fill_rule`
freezes the current evaluated choice as the literal. Session validation rejects
cycles, invalid domains and failed multi-command edits atomically. `get` and
`properties` expose literal, driver and evaluated choice separately. Canvas,
render plans and SVG use the evaluated Fill choice.

The 0.26 writer omits `fill_rule_driver` when absent. Native 0.1–0.25 documents
remain readable with literal-only Fill rules; an older version claiming this
driver is rejected. `schemas/native-v0.26.schema.json` defines the optional
driver wire shape, while Session validation checks that its source is an
existing Fill rule on a Path or Text object. Offset and mask fill rules remain
literal. This slice does not add expression evaluation to the Fill enum or
generalize links to every typed property.

## Native 0.30 — linked compositing isolation

`Compositing.isolated` remains the authored boolean literal. An optional
`isolated_driver: {link: Ref}` addresses only `Ref{Object ID,"","composite.isolated"}`
in the same Composition. Dedicated `link_composite_isolated` and
`unlink_composite_isolated` Session commands preserve the target literal while
linked; replacement requires `replace_driver:true`, and unlink freezes the
current evaluated authored value. `get` and `properties` expose the literal,
stable driver and evaluated authored value separately. `SetCompositing` may
change blend while preserving a driver and its unchanged literal, but changing
the driven literal requires unlinking first.

Scene isolation remains an aggregate: evaluated authored isolation OR any
non-neutral opacity, blend, enabled mask or enabled Group postchildren effect.
A false linked value never cancels another isolation requirement. Canvas and
compositing/export projections consume that effective scene value. Put Inside,
Ungroup and Move Out use evaluated authored isolation when requiring a neutral
Group. Duplicate operations remap the driver when both objects are copied;
removing a referenced source is rejected unless the dependent is removed or
unlinked in the same atomic batch.

The 0.30 writer omits `isolated_driver` when absent. Native 0.1–0.29 retain
literal-only isolation; older versions carrying this field are rejected.
`schemas/native-v0.30.schema.json` constrains the optional closed same-field Ref,
while Session validation enforces source existence, Composition identity,
self/cycle and 128-edge depth. This adds no boolean expressions or cross-field
coercion.

## Native 0.31 — instance-qualified geometry mask bypass link

`GeometryMask.enabled` remains its authored boolean literal. An optional
`enabled_driver: {link: Ref}` addresses only the exact installed
`Ref{Object ID,"","mask.<GeometryMask ID>.enabled"}` in the same Composition.
The legacy `Ref{Object ID,"","mask.enabled"}` remains a literal-only owner-slot
read and is never a link endpoint. Dedicated `link_mask_enabled` and
`unlink_mask_enabled` Session commands retain the target literal while linked;
replacement requires `replace_driver:true`, and unlink freezes the evaluated
bypass bit. `get` and `properties` expose literal, stable driver and evaluated
value for the qualified Ref. Scalar commands reject both mask Ref forms.

`SetMask` preserves an existing driver for same-ID source/fill-rule edits and
rejects a driven literal edit or driver injection/replacement. Removing or
replacing that mask instance requires unlinking its outgoing driver first;
surviving links to a removed/replaced mask ID reject atomically. Duplicating
both owners remaps the copied object and mask IDs in the driver Ref; duplicating
only the target retains its external source. Rename, reorder and same-ID edits
preserve identity. Evaluation reads only the source mask's authored/evaluated
bypass value: object visibility, geometry-source visibility and parent effects
do not alter it. Scene, Canvas and SVG use the evaluated bit while preserving
the target mask and its geometry source.

The 0.31 writer omits `enabled_driver` when absent. Native 0.1–0.30 retain
literal-only mask enable; older versions carrying this field are rejected.
`schemas/native-v0.31.schema.json` constrains the optional closed Ref wrapper,
while Session validation enforces installed mask identity, same-Composition
ownership, self/cycle and 128-edge depth. This adds no boolean expressions,
cross-field coercion or generic property picker.

## Native 0.33 — authored Artboard size sources

Each Artboard retains its literal width and height. An optional `width_driver`
or `height_driver` stores either `{link: Ref}` or `{expression: Expression}` for
that dimension. The existing `parent_size` flags remain the only inheritance
source. A dimension uses exactly one of its literal, its active parent flag,
its typed link, or its typed expression. Explicit typed replacement disables
only that dimension's parent flag and preserves the other dimension, parent
identity and local fallback. An Artboard expression may reference only Artboard
width/height properties in the same Composition; it uses `du` and the bounded
numeric expression language. Scalar commands do not acquire Artboard targets.

Dedicated `link_artboard_size`, `set_artboard_size_expression` and
`unlink_artboard_size` commands use stable Artboard IDs and revisioned Session
edits. Replacing an active parent or typed source requires `replace_driver`;
unlink freezes the evaluated dimension into its literal. `update_artboard`
preserves unchanged or omitted typed drivers during unrelated edits and refuses
driver injection, silent clearing, or a direct literal edit of a typed-driven
dimension. `detach_artboard_parent` freezes inherited dimensions and retains
independent typed sources. Missing sources, cross-Composition refs, cycles,
nonpositive/out-of-range results, invalid expressions and source deletion
reject atomically.

Native 0.1–0.32 Artboards reopen without typed size drivers; older versions
carrying the new fields reject. The 0.33 schema closes each optional driver
wrapper, while Session validation checks source identity and evaluation.
Typed `get`/`properties` expose the retained literal, source kind, exact link
or expression, and evaluated dimension. This is Artboard-size authoring, not
content inheritance or a general mixed-type property graph.

## Native 0.34 — authored Guide position expressions

Each Guide retains its literal `position`, stable ID and axis. Native 0.34 adds
an optional `position_expression: {source,version}` beside the existing
`position_driver: {link: Ref}`. At most one may be present. The link representation
and literal-only records remain unchanged; native 0.1–0.33 reject the new field.
The 0.34 schema closes both optional fields and rejects their simultaneous use.

`set_guide_position_expression` uses the bounded version-1 numeric expression
language in `du`. Its references may identify only same-Composition,
same-axis `guide.position` properties. Link and expression dependencies share
the pure Guide evaluator, cycle and depth checks, and finite `[-1e9,1e9]` du
range. A typed source replaces another only with `replace_driver`; unlink
freezes the evaluated coordinate into the literal. Generic Scalar and Artboard
dependencies remain separate. Native Save As retains expression source text
and stable Guide Refs; Canvas overlay, hit, snap and Align-to-Guide use the
evaluated coordinate.

## Native 0.34 — typed layout reads without a format change

Grid and Margin retain their 0.14 authored layout representation. A present
Margin exposes four `margin.<side>` typed reads through its owning Artboard ID;
a present Grid exposes bounds, counts and gutters through its own stable ID.
The eight Grid fields and four Margin fields are Artboard-local. Counts are
unitless integers; all other layout values use `du`. `get` and `properties`
return the stored literal as the evaluated local value with no link or
expression capability. Artboard movement affects Canvas world placement but
does not rewrite those literals. `SetArtboardLayout` remains the sole full
layout mutation owner, and native Save As retains the original Grid ID and
layout values without a format change in that slice.

## Native 0.35 — Margin left Artboard size link

A present Margin may retain an optional `left_driver: {link: Ref}` beside its
authored `left` literal. The Ref identifies another Artboard's width or height
in the same Composition, with an empty point ID and `du` units. The evaluated
source dimension supplies the target's local left inset; the literal and exact
Ref remain authored. Other Margin sides and all Grid fields stay literal-only.
The 0.35 writer omits `left_driver` when absent. Native 0.1–0.34 reject a
version-lied driver, and `schemas/native-v0.35.schema.json` closes its shape.

Dedicated `link_margin_left` and `unlink_margin_left` commands use the revisioned
Session. Replacing a different active source requires `replace_driver`; unlink
freezes the evaluated inset into the literal. Full layout and Artboard updates
preserve a link while the authored left literal is unchanged and refuse direct
driven edits or driver injection. Validation checks the evaluated inset against
the evaluated target width before commit. Typed reads disclose literal, exact
source and evaluated value; Canvas and the Inspector consume the evaluated
layout. Artboard size remains independent of layout, so this one-way edge does
not add a mixed dependency cycle.

## Native 0.36 — Grid bounds x Artboard size link

A present Grid retains its stable ID and authored `bounds.x` literal. Native
0.36 adds an optional `bounds_x_driver: {link: Ref}` on the Grid; the writer
omits it for a literal Grid. The Ref identifies a distinct Artboard width or
height in the Grid owner's Composition, with an empty point ID and `du` units.
Earlier supported versions decode as before; a 0.35 record with a version-lied
driver is rejected. `schemas/native-v0.36.schema.json` closes the optional field.

Dedicated `link_grid_bounds_x` and `unlink_grid_bounds_x` commands own source
changes. An explicit replacement changes an active source; unlink freezes the
evaluated local x into the literal. Full layout and Artboard updates preserve
the source with the same Grid ID and x literal and refuse an implicit clear,
ID change or driven x edit. Validation uses the evaluated x for Grid containment
against the evaluated target Artboard width. Typed reads disclose the literal,
exact source and evaluated x. Canvas Grid overlay, snapping and Grid-reference
alignment/distribution use evaluated bounds; native Save As retains the authored source.
Artboard sizes do not read Grid layout, so this edge remains one-way.

## Native 0.37 — Grid bounds x expression

A present Grid may now retain `bounds_x_expression: {source,version:1}` beside
its authored `bounds.x` literal. It is mutually exclusive with the existing
`bounds_x_driver` link. The expression uses the bounded numeric language in
`du`; its `ref()` calls may address only width or height on distinct Artboards
in the Grid owner's Composition. Those dimensions use the existing Artboard
size evaluator, including parent-size, link and expression sources. Artboard
sizes never depend on layout, so this adds a one-way source edge. Constants
are allowed. The evaluated x must still satisfy Grid containment and cell
constraints against the evaluated target Artboard size.

`set_grid_bounds_x_expression` uses the revisioned Session path. Switching
between a link and an expression requires explicit `replace_driver`; unlink
freezes the evaluated x into its literal. Full layout and Artboard updates
preserve an unchanged source for the same stable Grid ID and authored x,
while rejecting implicit source edits and injected fields. Typed reads expose
the literal, source kind, exact expression and evaluated x; Canvas, Snap and
Grid-reference Align consume the evaluated layout. Native Save As retains the
authored expression, and native 0.1–0.36 reject a version-lied expression
field. `schemas/native-v0.37.schema.json` closes the optional field and
rejects simultaneous Grid link and expression sources.

## Native 0.38 — Margin left expression

A present Margin may retain `left_expression: {source,version:1}` beside its
authored `left` literal. It is mutually exclusive with the existing
`left_driver` link. The bounded numeric language evaluates to Artboard-local
`du`; each `ref()` may address only width or height of a distinct Artboard in
the Margin owner's Composition. Referenced dimensions use the existing
Artboard-size graph, including parent inheritance and expressions. Artboard
sizes do not depend on Margin layout. The evaluated left must be finite and
nonnegative and leave positive content width with right inset and evaluated
target width.

`set_margin_left_expression` uses the revisioned Session command path.
Replacing a link or different expression requires `replace_driver`; unlink
freezes the evaluated inset into the authored literal. Full layout and
Artboard updates retain an unchanged source for the same owning Artboard and
literal while rejecting implicit source edits. Typed reads expose literal,
source kind, exact expression and evaluated inset. Canvas Margin overlay,
Inspector and Grid-to-Margin copy consume the evaluated value. Native Save As
retains the authored expression; native 0.1–0.37 reject a version-lied field.
`schemas/native-v0.38.schema.json` closes the optional field and rejects
simultaneous Margin link and expression sources.

## Native 0.39 — Grid bounds y Artboard size link

A present Grid retains its stable ID and authored `bounds.y` literal. Native
0.39 adds an optional `bounds_y_driver: {link: Ref}` on that Grid; the writer
omits it when y is literal. The Ref identifies a distinct Artboard width or
height in the Grid owner's Composition, with an empty point ID and `du` units.
Earlier supported versions decode as before; a 0.38 record carrying a
version-lied y driver is rejected. `schemas/native-v0.39.schema.json` closes
the optional field. Grid x sources remain independent.

Dedicated `link_grid_bounds_y` and `unlink_grid_bounds_y` Session commands own
source changes. An active source requires explicit replacement; unlink freezes
the evaluated local y into the authored literal. Full layout and Artboard
updates preserve an unchanged source with the same Grid ID and y literal, and
reject an implicit clear, ID change or driven y edit. Evaluation uses the
existing Artboard-size graph; Artboard sizes do not read Grid layout. The
evaluated y and authored Grid height must fit within the evaluated target
Artboard height. Typed reads expose the literal, exact source, evaluated y and
`du`; Canvas Grid overlay, snapping and Grid-reference alignment/distribution
consume the evaluated bounds. Native Save As preserves the authored source.

## Native 0.40 — Grid bounds y expression

A present Grid may retain `bounds_y_expression: {source,version:1}` beside its
authored `bounds.y` literal and optional `bounds_y_driver` link. At most one
source may be present. The bounded numeric expression language evaluates to
Artboard-local `du`; each `ref()` may address only width or height on a distinct
Artboard in the Grid owner's Composition. Referenced dimensions use the existing
Artboard-size graph, including parent inheritance, links and expressions.
Artboard sizes do not depend on Grid layout, so the source edge stays one-way.
The evaluated y and authored Grid height must fit within the evaluated target
Artboard height.

Dedicated `set_grid_bounds_y_expression` and existing link/unlink commands own
source changes. Replacing a link or a different expression requires
`replace_driver`; unlink freezes the evaluated y into the authored literal.
Full layout and Artboard updates preserve an unchanged source for the same
stable Grid ID and literal while rejecting implicit source edits. Typed reads
expose the literal, exact source and evaluated y. Canvas Grid overlay, vertical
Snap and Grid-reference Align consume the evaluated layout. Native Save As
retains the authored expression; native 0.1–0.39 reject a version-lied y
expression. `schemas/native-v0.40.schema.json` closes the optional field and
rejects simultaneous link and expression sources.

## Native 0.41 — Margin top Artboard size link

A present Margin retains its authored `top` literal and may add an optional
`top_driver: {link: Ref}`. The Ref identifies the width or height of a distinct
Artboard in the owner's Composition, with an empty point ID and `du` units.
The existing Artboard-size evaluator resolves parent, link and expression
sources without a second layout property store. Evaluated top and authored
bottom must leave positive content height on the evaluated owning Artboard;
invalid upstream edits reject atomically.

Dedicated revisioned `link_margin_top` and `unlink_margin_top` commands own
source changes. Replacing an active source requires `replace_driver`; unlink
freezes evaluated top into the literal. Full layout and Artboard updates keep
the source when the Margin and top literal survive, while rejecting direct
driven edits and source injection. Duplication replays the stable source Ref;
deleting a source Artboard used by a surviving Margin is refused. Typed reads
show literal, exact link and evaluated top, with expression support false.
Canvas Margin overlay, Inspector and one-shot Grid-to-Margin copy use evaluated
top. Native Save As retains the literal and source; versions 0.1–0.40 remain
readable and reject a version-lied top driver.
`schemas/native-v0.41.schema.json` closes the optional field.

## Native 0.42 — Margin top expression

A present Margin may retain `top_expression: {source,version:1}` beside its
authored `top` literal, mutually exclusive with `top_driver`. The bounded
numeric expression evaluates to Artboard-local `du`; each `ref()` may address
only width or height on a distinct Artboard in the owner's Composition. The
existing Artboard-size evaluator resolves parent, link and expression sources.
Evaluated top plus authored bottom must leave positive content height.

Dedicated revisioned `set_margin_top_expression` authors the expression.
Replacing a link or different expression requires `replace_driver`; the
existing unlink command freezes evaluated top into the literal. Full layout
and Artboard updates preserve an unchanged source, while rejecting direct
driven edits and source injection. Duplication replays the exact expression;
deleting a referenced source Artboard is refused. Typed reads expose literal,
exact expression and evaluated top. Canvas Margin overlay, Inspector and
Grid-to-Margin copy consume evaluated top. Native Save As retains source text
and literal; versions 0.1–0.41 remain readable and reject a version-lied top
expression. `schemas/native-v0.42.schema.json` closes the optional field and
rejects simultaneous top link and expression sources.

## Native 0.43 — Margin right Artboard size link

A present Margin retains its authored `right` literal and may add an optional
`right_driver: {link: Ref}`. The Ref identifies the width or height of a
different Artboard in the owning Composition. The existing Artboard-size
evaluator resolves the source, including its existing parent, link or
expression chain. Evaluated left plus evaluated right must leave positive
content width on the evaluated owner Artboard.

Dedicated revisioned `link_margin_right` and `unlink_margin_right` commands own
source changes. Replacing a different source requires `replace_driver`; unlink
freezes evaluated right into the literal. Full layout and Artboard updates
preserve an unchanged source while rejecting direct driven edits, Margin clear
and source injection. Duplicate Artboard replays the exact stable source Ref;
deleting a referenced source Artboard is refused. Typed reads expose literal,
source and evaluated right, with expression support false. Canvas Margin overlay
and one-shot Grid-to-Margin copy use evaluated right. Native Save As retains the
literal and source; versions 0.1–0.42 remain readable and reject a version-lied
right driver. `schemas/native-v0.43.schema.json` closes the optional field.

## Native 0.44 — Margin right expression

A present Margin may retain `right_expression: {source,version:1}` beside its
authored `right` literal, mutually exclusive with `right_driver`. The bounded
numeric expression evaluates to Artboard-local `du`; each `ref()` may address
only width or height on a distinct Artboard in the Margin owner's Composition,
with an empty point ID. The existing Artboard-size evaluator resolves referenced
dimensions, including parent, link and expression sources. Evaluated right must
be finite and nonnegative and leave positive content width with evaluated left
and the owner Artboard's evaluated width.

The revisioned `set_margin_right_expression` command owns expression changes.
Replacing any active link or different expression requires `replace_driver`;
reapplying the same expression is idempotent. Existing unlink freezes evaluated
right into the literal. Full layout and Artboard updates preserve an unchanged
source for the same Margin and literal while rejecting direct driven edits,
source injection and Margin clear. Duplicate Artboard replays the exact
expression text and stable Ref; deleting a referenced source Artboard is
refused. Typed reads expose the literal, exact expression and evaluated right.
Canvas Margin overlay, Inspector and one-shot Grid-to-Margin copy consume the
evaluated value. Native Save As retains the exact expression text and literal;
versions 0.1–0.43 remain readable and reject a version-lied right expression.
`schemas/native-v0.44.schema.json` closes the optional field and rejects
simultaneous right link and expression sources.

## Native 0.45 — Margin bottom Artboard size link

A present Margin retains its authored `bottom` literal and may add an optional
`bottom_driver: {link: Ref}`. The Ref identifies the width or height of a
different Artboard in the owning Composition, with an empty point ID. The
existing Artboard-size evaluator resolves the source, including its parent,
link or expression chain. Evaluated top plus evaluated bottom must leave
positive content height on the owner Artboard.

Dedicated revisioned `link_margin_bottom` and `unlink_margin_bottom` commands
own source changes. Replacing a different source requires `replace_driver`;
unlink freezes evaluated bottom into the authored literal. Full layout and
Artboard updates preserve an unchanged source while rejecting direct driven
edits, Margin clear and source injection. Duplicate Artboard replays the exact
stable source Ref; deleting a referenced source Artboard is refused. Typed reads
expose the authored literal, exact link and evaluated bottom, with link support
and no expression support. Canvas Margin overlay, Inspector and one-shot
Grid-to-Margin copy consume evaluated bottom. Native Save As retains the literal
and source; versions 0.1–0.44 remain readable and reject a version-lied bottom
driver. `schemas/native-v0.45.schema.json` closes the optional field.

## Native 0.61 — relative Text weight links

`TextSource.weight` keeps its authored integer literal and optional stable
`weight_driver.link`. Native 0.61 adds an optional signed integer `offset` to
that driver; evaluation is the linked Text's evaluated weight plus the offset,
and the result must remain an integer in [1,999]. A missing offset means zero,
and the writer omits zero to preserve the established absolute-link shape.
Typed batch edits, links and unlinks snapshot every target before mutation so a
relative edit adds to each evaluated starting value and a relative link records
each target's difference from one common source. The source and target Refs are
same-Document Text `text.weight` Refs with stable Object IDs. Driven edits
reject unless their existing source is explicitly replaced or unlinked.

The reader accepts 0.1–0.60 unchanged, including weight links without offsets;
any `offset` under an older version is a version lie and rejects. Native 0.61
retains the exact source Ref, signed offset and authored literal, while
evaluation rejects results outside [1,999]. `schemas/native-v0.61.schema.json`
closes the optional offset field.

## Native 0.62 — Composite isolation expressions

`Object.compositing.isolated` retains its authored boolean literal and may add
one optional `isolated_expression: {source,version:1}`, mutually exclusive with
`isolated_driver`. The closed boolean grammar accepts `true`, `false`, or a
qualified same-Composition `ref("object-id","","composite.isolated")` with
optional negation. Evaluation reads the referenced authored isolation value;
it does not feed back from effective scene compositing. Self references,
cycles, missing or cross-Composition sources, and other fields are rejected.

The revisioned `set_composite_isolated_expression` command owns source changes.
Replacing an active link or different expression requires `replace_driver`;
reapplying the exact expression is idempotent. Existing unlink freezes the
evaluated authored isolation into the literal, and direct literal edits while
driven are rejected. Duplicate remaps an expression Ref only when its source
is duplicated in the same operation. Typed reads expose the authored literal,
exact expression and evaluated isolation. Scene rendering and SVG export use
the evaluated value while preserving the authored source. Native Save As keeps
the exact expression and literal; versions 0.1–0.61 remain readable and reject
a version-lied expression. `schemas/native-v0.62.schema.json` closes the
optional field and rejects simultaneous link and expression sources.

## Native 0.73 — retained Ellipse

Native 0.73 adds `nect.shape.ellipse@1` to the existing retained Primitive
source. It stores `center_x`, `center_y`, `width` and `height` as ordinary
linkable distance properties; width and height use the existing nonnegative
length rule and default to 220 and 140. Earlier native versions remain readable
but reject an Ellipse source, so changing an Ellipse file's version to 0.72 or
earlier cannot conceal the new behavior.

The source generates stable east/south/west/north anchors in the same
`<source-id>-<role>` namespace as Circle, with contour ID
`<source-id>-contour`. For `rx=width/2`, `ry=height/2`, anchors are
`(center_x+rx,center_y)`, `(center_x,center_y+ry)`,
`(center_x-rx,center_y)` and `(center_x,center_y-ry)`. Using
`k=0.5522847498307936`, east/west incoming and outgoing handle lengths are
`k*ry`; north/south lengths are `k*rx`. Point Edit remains the downstream
correction authority, and native files save the retained source plus authored
corrections rather than generated anchors. See
`schemas/native-v0.73.schema.json`.

## Native 0.74 — rigid Group Path Follow

Native 0.74 adds an optional `path_follow` relation owned by a Group. The
relation retains its own stable ID, an exact authored Path Object ID and
Contour ID, distance or normalized start, relation normal offset, and reversed
traversal. Its `items` object is keyed by the stable IDs of direct Group
children; each item retains a distance offset, normal offset, and
`follow_tangent` flag. The source must be an authored Path contour in the same
Composition and outside the Group subtree. Visibility does not affect sampling.
Generated geometry, Text and other Groups are not Path sources.

Evaluation samples the source in Composition space and computes an ephemeral
derived local frame for each attached child. Tangent following converts the
sampled world tangent frame through the inverse Group world matrix. With
`follow_tangent=false`, only the sampled point plus normal offset is converted
into Group-local space; the child's authored orientation remains Group-local.
In both cases, the child's authored local transform is composed once after the
derived frame. Path Follow does not rewrite geometry, Text source, transform
Scalars, hierarchy, stable IDs or item membership. Editing the Path or child
therefore reevaluates placement without baking.

Open contours accept a normalized start only in [0,1], or a distance start in
[0,length]; each child sample must remain in [0,length]. Closed contours wrap
sample distances. Zero-length contours reject. A following Group and each
attached child must use its structural transform parent, without an explicit
Transform Parent. Structural ancestors may still transform the Group. The
combined transform dependency graph rejects cycles, including a source Path
whose effective transform depends on a followed child. Clear removes only the
relation; Undo and Redo retain ordinary Session atomicity.

Session/API/MCP commands attach, update, clear, set and remove stable child
items. The Desktop inspector uses the same commands and reads the retained
relation back from Session state. Native 0.1–0.73 remain readable; a Path Follow
field under an older version rejects as a version lie.
`schemas/native-v0.74.schema.json` closes the optional Group relation and child
item shape. This release is the rigid Path-following slice; it does not deform
child geometry or provide general rigs.

## Native 0.75 — retained Group Path Deform

The same Group `path_follow` relation adds optional `mode: rigid | deform` and
`deform_axis: x | y`, defaulting to rigid and x when absent. Native 0.74 rigid
relations read unchanged; these new fields under 0.74 or earlier reject. Deform
retains relation/Object/Contour/Point IDs and authored source geometry exactly.
Switching mode is one revisioned Update command and one Undo; clear/detach removes
only the relation. Derived contours and frames are never serialized.

Deform items may identify a direct Path/retained primitive child or a Group whose
descendants are eligible Paths/primitives. Text, Image, Instance and nested Path
Follow relations reject explicitly. The established same-Composition, source
outside follower subtree, structural-parent, cycle and invertible follower-Group
rules remain. Singular child transforms are supported; only the follower Group
requires inversion.

Evaluated source anchors and control points first use the current leaf-to-Group
affine matrix. X mode takes x as longitudinal u and y as normal v; Y swaps them.
At `start + item.distance + u`, the Composition-space PathSampler produces a
tangent/normal frame converted by inverse Group world. Anchors add relation and
item normal offsets plus v. Incoming/outgoing vectors use the anchor's sampled
frame so each cubic knot keeps one consistent tangent basis. Final projection
uses the Group world once. Controls contribute to the source range check: open
overspan rejects, and the full relation's closed span cannot exceed one lap.
Projection limits each relation to 1000000 unique derived anchors, in addition
to the existing Shape output limits.

`evaluate_scene` owns transient contours, ordered paints, geometry world overrides
and stable source-point provenance. Canvas, mask sources, world bounds and SVG
consume that projection. Cubic path and paint instance transforms are consumed
once; stroke width remains the authored Group-local width after deformation.
World bounds use Composition space. A deformed leaf's evaluated-local bounds
use its Group-local projection plane, including singular child transforms.
Source pivot recentering and retained-vector intake request source-only bounds
so projected coordinates never replace authored local coordinates.
Gradient endpoints retain the source affine field in Group coordinates; v1 bends
geometry rather than the gradient field. SVG marks this evaluated derivative and
its conversion plan keeps native source preservation explicit.

`compositing_plan` exposes each projected leaf's relation, Group, source Object,
Path/Contour IDs, x/y axis and stable source-point projected anchors/controls in
Group-local space. The Inspector updates the same retained relation. Canvas
preserves selection/provenance but refuses direct projected anchor/handle drag
with `DEFORM_SOURCE_EDIT_REQUIRED`; Inspector/API source edits reevaluate normally.
Widget/pixel tests are offscreen correctness evidence, not hands-on GUI acceptance.
See `schemas/native-v0.75.schema.json`.

## Native 0.76 — Composition-owned Artboard Templates

Native 0.76 adds an ordered `templates` collection to each Composition and an
optional `template_assignment` to an Artboard. A Template retains a stable ID,
name, source Artboard ID, and optional existing R04 Definition ID. An assignment
retains the exact Template ID, a reserved target-local Grid ID, an optional
ordinary content Instance ID, independent width and height Scalars, and local
Margin/Grid family state. Source and target Artboards and an optional Definition
must resolve in the same Composition. Names and list positions never replace
stable identity.

Assigned frame width and height inherit independently from the source. Target
x, y, name, authored frame literals, and unrelated content remain target-owned.
An explicit `parent_size` axis or existing size link/expression remains a
separate local source; Template inheritance does not replace it. Margin and
Grid inherit as whole optional families. A local family may be explicitly
absent to disable inheritance until reset. The target's reserved Grid ID stays
local while its evaluated bounds, counts, and gutters follow the source.

Create, rename, delete, assign, set/reset, and detach are revisioned Session
commands shared by Desktop, API, JSON-lines, and formal MCP. Stable source or
target IDs are captured for each command. Missing identities, cross-Composition
references, unsupported fields, wrong types, invalid containment, source cycles,
and deletion of an in-use Template or source Artboard reject atomically. A
Definition-backed assignment may create one ordinary R04 content Instance
using a fresh caller-supplied ID, initially placed at the target Artboard origin.
Later frame movement changes the crop; it does not move or reparent that content.

The existing R04 Instance override map adds only descendant `transform.tx`,
`transform.ty`, and Rectangle `generator.width`/`generator.height` Scalar Refs.
Source Fill/Color edits continue to propagate. No local Fill/Color override is
serialized. The normal evaluator projects the current Definition source and
local Scalar overrides into Canvas, bounds, SVG, and native/API readback.

Reset removes only the selected axis or family and restores live inheritance.
Detach freezes inherited frame/layout values, preserves independent authored
sources and existing local overrides, materializes assigned content through the
ordinary R04 detach path with fresh descendant IDs, and removes only the
assignment. Undo restores the exact relation/content state; Redo restores the
materialized state. Save As writes authored Template, assignment and override
state; a fresh Host/process reopen evaluates it from the saved native document.
Native 0.1–0.75 remain readable, while Template fields under an earlier version
reject as a version lie. `schemas/native-v0.76.schema.json` closes the new
Composition, Artboard, Template, and assignment shapes.
