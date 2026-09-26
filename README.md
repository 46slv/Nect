# Nect

> **Fresh implementation agent:** start with [START_HERE.md](START_HERE.md).  
> A repo link plus that file is intended to be enough to resume current work without old chat history.

Nect is an experimental 2D graphics authoring engine/editor focused on:

- fully addressable Bézier points and handles
- non-destructive property-driven editing
- expressions and cross-object references
- vector + raster + compositing in one document model
- Illustrator/Photoshop interoperability
- first-class API/MCP automation
- local-AI-friendly structured document semantics

This repository is the technical source of truth for implementation.
Product requirements, decisions, and research evidence live in the Notion specification hub:
https://app.notion.com/p/3dffd279a6f381cca8c7c4dec111b131

## Current status

**Practical-alpha workflow acceptance demonstrated (Windows, 2026-09-22)**

The Windows Qt desktop now creates paths from an empty document, selects and edits
points/handles, provides a property-source picker, and shares atomic commands and
Undo with a live local API and formal MCP stdio adapter. Native save, previous-file
backups and recovery snapshots are implemented. The Windows 30 fps viewport
baseline and integrated authoring/recovery/SVG acceptance are in
[docs/first-usable.md](docs/first-usable.md#integrated-practical-alpha-acceptance--2026-09-22).
The scoped Mission completion conditions are demonstrated; this is a development
checkpoint, not a release. Known limits include intermittent image-chooser teardown
delay, a 34.75 ms dense-scene point-release measurement, and bounded output subsets.
`CURRENT_GOAL.md` records the active daily-output Mission and continuous-development authority.

Circle, Rectangle, Polygon and Star retain their generators after direct point edits, with
visible Point Edit overrides/bypass and explicit Convert to Path. Ordered local
Fill/Stroke/Repeater stacks share one core evaluation for Canvas and SVG. Native
0.13 saves Linked/Embedded PNG/JPEG assets and editable Image placements, retained Offset Paths, geometry masks, common compositing, expression source, procedural state, editable linear/radial gradients, ordered Artboards
with parent-size inheritance, editable Text, named colors, retained Polygon/Star,
authored Anchors and explicit Transform Parents. It migrates 0.1–0.12 without
reference loss. Windows Text uses installed fonts and supports Japanese horizontal
and vertical writing, automatic size, fixed-frame wrapping and overflow diagnostics.

View → History (`Ctrl+Shift+H`) lists retained operations and explicitly returns to
an earlier or later state. The default limit is 1,024 edits / 64 MiB estimated
change storage; oldest operations are pruned. A new edit replaces the redo branch.
History belongs to the open Session, while native files and backups survive restarts.

Implemented in M0:

1. create/load a small native document
2. address every Bézier point and handle property by stable ID
3. bind compatible properties across objects
4. reject cycles, invalid references, invalid units, unknown fields and partial batches
5. save/reload authored state
6. undo/redo through one Session owner
7. export a declared SVG subset
8. expose a local JSON-lines command adapter for black-box testing

File → **Export PNG…** exports the active Artboard at an explicit number of pixels
per document unit, with transparent or white background. One settings dialog shows
the resulting pixel dimensions before export and disables output above the limits.
Successful scale/background/destination settings are remembered within the Window;
cancel leaves them unchanged. Settings freeze the source revision; concurrent API
edits reject the stale export. Existing output replacement asks once. Output uses the Canvas
artwork/compositing renderer, excludes paper/selection/guides, and declares 8-bit
sRGB. Dimensions round up; limits are 8192 pixels per axis / 16,777,216 pixels,
scale >0 through16, with the existing128MiB compositing surface budget. White is
applied behind the completed transparent composition, preserving blend semantics.
Native data/history are unchanged. Active gestures and native/Linked source
image destinations reject. File replacement is atomic without direct-write fallback.
The desktop API `export_png` and formal MCP `nect_export_png` take the same
session/document identity, expected_revision, absolute .png path, composition,
artboard, scale and background (`transparent` or `white`). The core-only CLI does
not provide a Qt renderer; its unsupported PNG requests remain explicit.

Not implemented yet:

- AI/PSD codecs
- OpenFX hosting
- full typography
- advanced raster formats, pixel painting and complete production compositing

The PS/AI parity backlog is **not** implementation authorization.

## Build

Requirements:

- C++20 compiler
- CMake 3.24+
- Boost.JSON headers
- Python 3 for black-box tests

Example:

```sh
cmake -S . -B build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
python3 scripts/smoke.py --exe build/nect
```

Windows desktop, with the installed/local Qt and Boost SDK directories:

```powershell
./scripts/build-windows.ps1 -BoostRoot <boost-header-root> -QtRoot <qt-msvc2019_64-root>
./build/Release/nect_desktop.exe
```

On the current development machine both SDKs are under ignored `build/deps`, so
the build script defaults work. It deploys Qt DLLs/plugins only into the local
build output. Close the development executable before rebuilding it. No SDK is
downloaded by configure or the script.

Add a Circle/Rectangle and adjust its parameters, create a Curve, or choose Draw
Path and click anchors (Enter finishes).
Drag anchors/handles; Alt-drag an anchor to create handles. Escape cancels a drag.
Shift-click adds/removes objects or points; Ctrl/Shift extended selection in the
tree uses the same selection. Dragging multiple objects or points is one Undo.
Space-drag pans; wheel zooms; Fit frames the artboard.
Snap ON/OFF beside Fit (also in View) gently aligns object-body drags to the active
Artboard and visible objects' geometric edges/centers within 6 screen pixels.
Dashed guides appear while aligned. Snap starts ON and is a per-window view
preference; exact numeric/API edits and point/handle edits stay unchanged.
Groups select as a unit; double-click enters them and the breadcrumb returns.
**Ctrl+D** (Edit or context menu → Duplicate objects in place) makes independent
copies and selects them for placement. Group children and internal property,
expression, mask and Transform Parent references copy together. External links,
Named Colors and image assets keep their shared sources; existing links into the
originals stay there. Duplication is one Undo and preserves retained sources.

Inspector fields accept
numbers and one-shot `+=`/`-=` adjustments. Right-click provides Copy Value,
Copy Reference, Paste Value, Paste Link and explicit Unlink. Drag ↗ to a source
field (hover Objects to inspect another source); click ↗ to search.
Expressions use the same row (see below). With multiple targets, common
properties show Mixed; a number sets every target and `+=`/`-=` preserves each
target's differences. Source picking freezes all targets and returns to them.

Select adjacent objects and use the Canvas/Objects context menu → Mask With
Top / Bottom. Labels identify the source by actual paint order. The operation
creates one Group, hides the source artwork and retains its editable geometry.
Properties → Edit source selects it without making its paint visible; Show mask
outline controls a faint viewport overlay. Put Inside moves immediately preceding
siblings into the top selected Group, preserving world placement and order.
Every object/Group has visibility, ordinary linkable/expression opacity, twelve
blend modes and explicit isolation. Neutral Groups pass through; masks, opacity
and blend aggregate the children. Geometry masks use final Path/Text contours;
alpha/luma masks and full AE blend parity remain unsupported. SVG retains vector
clips, Group opacity and CSS blend/isolation, so the reader must support those
SVG/CSS features. `examples/colour-cut.nect` demonstrates the retained workflow.

**Offset Paths** in Add or Shape stack expands/contracts a retained closed outline.
Amount supports links and expressions; Miter/Round/Bevel and the fill rule are
editable. Reorder it around Repeater to change local distance behavior. Earlier
Fill/Stroke geometry follows Offset while its paint coordinates remain intact.
Open, self-intersecting or touching compound outlines reject visibly. Curves use
bounded0.1du polygon approximation in evaluated output; native source points stay
editable. Zero Amount and bypass preserve input exactly.

Add Text creates an editable source. Use Edit text to compose Japanese or other
Unicode content, then Apply for one undo step. Select font, writing direction,
alignment and sizing in Properties; numeric text fields also support links.
Frame overflow and missing-font fallback are visible in the Inspector. Text is
outlined in SVG exports; the native document retains editable content and font
references. Fonts are not embedded.

Colors opens three separate views: document-authored named colors, actual enabled
paint inputs grouped by exact RGBA, and colors explicitly copied through Nect's
Color menu during this Window session. Each paint and gradient stop has Copy
Value/Reference, Paste Value/Link and explicit Unlink actions. Value copies remain
independent; linked colors follow the stable source even after renaming. A named
color cannot be removed while referenced. Clipboard values preserve sRGB profile,
straight alpha and full numeric precision; unsupported richer structured colors
reject instead of silently becoming HEX. Pinned user palettes and copied history
across application restarts are not implemented in this slice.

The semantic `stroke_style` command explicitly promotes a Stroke to behavior v2
with butt/round/square caps, miter/round/bevel joins and a linkable miter limit
(1–1000). Canvas/PNG/SVG output share this style; native0.14 and Undo preserve it.
Default Stroke v1 remains butt/miter/4. Inspector controls and SVG style import
are pending the next checkpoint; see [the contract](docs/model-v0.md#stroke-behavior-v2-within-native-013).

The Shape stack supports multiple solid Fill/Stroke entries, HEX RGBA/color
editing, enable/reorder/remove and Repeater. Add a radial repeater for a fixed-step
12 × 30° starting point. Repeater before paint creates a compound path; after
paint it repeats separately painted copies. Source points remain directly editable.
Open `examples/radial-ornament.nect` for an original procedural sample, or reproduce
it in an empty live desktop with `scripts/create_radial_demo.py --endpoint nect-demo
--output build/radial.nect`. The script uses the production Session API and refuses
to replace existing artwork.

Add → Polygon / Star exposes center, point count, rotation and radii. Count is a
normal linkable integer property. Point edits follow stable angular roles; a
count change that would remove an edited/referenced vertex rejects atomically.
Reset point edits explicitly removes corrections in one undoable command.
`examples/polystar-field.nect` combines linked counts, a retained point edit,
Repeater, gradient, named colors and editable Text; recreate it with
`scripts/create_polystar_demo.py --endpoint <name> --output <file.nect>`.

Transform & Anchor exposes the anchor's Position, editable local Anchor, Center
Anchor and one-shot rotation/scale about that pivot. Edit → Edit Anchor (`Y`)
drags its Canvas crosshair without moving the artwork. The original affine
properties remain available under Affine matrix. Rotation/scale actions do not
create separately linkable TRS properties.

Transform Parent chooses a same-Composition object to follow, with Keep artwork
in place enabled by default. Detach returns to structural inheritance. Structure
still owns ordering and groups; explicit following replaces its transform to
avoid applying it twice. Cycles, singular inverse requirements and changed driven
matrix fields reject atomically. `examples/pivot-follow.nect` and its SVG are
reproducible with `scripts/create_transform_demo.py` through the live API.

To expose the desktop-owned document to a local automation client:

```powershell
./build/Release/nect_desktop.exe --automation-endpoint nect-local
python scripts/mcp_server.py --endpoint nect-local
```

The second process is a formal MCP 2025-06-18 stdio server. Configure it as a
stdio command in your MCP client, initialize and list tools, then use
`nect_session`, `nect_command` and `nect_file`. Mutations require the returned
session/document identity and current revision. Opening/new rotates the session
identity; a stale client cannot edit the replacement document. The local endpoint
is opt-in and restricted to the current user; there is no TCP listener.

`scripts/create_radial_demo.py --endpoint <name> --output <file.nect> --gradients`
authors the linked linear/radial gradient variant from an empty live document.
The editable result and SVG are checked in as `examples/gradient-ornament.*`.
Select a paint's Linear/Radial mode, edit coordinates/stops numerically, or enable
Edit gradient handles in its Inspector. Solid bypass retains its stops and links.

The Artboards list selects a frame and its Composition. Add/Duplicate places a
frame to the right; up/down changes export order without moving artwork. Edit
active frame exposes crop coordinates and dimensions, with independent width/
height overrides of a same-Composition parent. Detach keeps the current size.
Fit focuses the active frame; View > Fit all artboards shows that Composition.
SVG export uses the active frame. CLI callers can use
`nect --svg <composition-id> <artboard-id>` with native JSON on stdin.
`scripts/create_artboard_demo.py --endpoint <name> --output <file.nect>` builds
the three-frame `examples/artboard-studies.nect` fixture and numbered SVG crops.

`scripts/session_client.py` calls that same desktop API directly. The original
`nect --serve` remains a separate headless JSON-lines lane, **not MCP**.
`tests/mcp_desktop_tests.py` demonstrates seeded creation, edits, linking,
reordering, failure readback, Undo, native save/restart and crash recovery.

Committed edits live-save to the named file and recovery storage in the background
at a one-second cadence. Incomplete fields and gesture previews remain separate.
One running snapshot plus the newest pending snapshot bounds the queue. Status and
API `hello.persistence` distinguish pending, writing, verified native/recovery
revisions and destination failures. A slow or failed disk extends the loss window;
pending work is not yet protected. Manual Save and normal open/new/close explicitly
drain earlier writes and protect the latest committed state before changing targets.

Atomic replacement has direct-write fallback disabled and verifies written bytes.
Content fingerprints plus cooperative locks reject external native changes; use
Save As to another path or reopen. Recovery continues through native conflicts.
File > Open Recovery (API/MCP `open_recovery`) opens an unnamed copy. Recovery lives
in local Nect app-data, or `--recovery-dir`.

Manual saves and the first live replacement after opening/saving retain the prior
file, then automatic generations are sampled about every30 seconds. Native and
recovery `<file>.backups` target ten owned generations. Inactive managed recovery
sessions target20 /128 MiB, newest first; active sessions and legacy, modified or
unrecognized files are excluded. Cleanup is best effort and never invalidates a
verified save. History is separate from these backups. Native files are limited
to64 MiB; unsupported fields/versions reject without altering the source. This
does not promise survival of all hardware failures or non-cooperating external
writes in the final rename race. See the persistence contract in `docs/model-v0.md`.

## Property expressions

Type `=expression` in a numeric field, or click **fx**. Use **Insert reference…**
for searchable stable property references. Arithmetic, min/max/clamp, rounding,
sqrt and degree-based sin/cos are supported; units and normal property ranges
still apply. Multiline paste grows into an inline draft. Apply/Ctrl+Enter commits;
Cancel/Esc discards. The number remains the evaluated result, separate from source.
An invalid or stale draft cannot replace valid artwork, and typing a number cannot
silently unlink a formula. Formula drafts are not saved until Apply.

API/MCP `expression_language` reports the exact subset. `set_expression` authors
it through the same Session. Native0.10 retains source; SVG exports evaluated
geometry/appearance. See [model contract](docs/model-v0.md#native010-property-expressions).

## Entry points

- `START_HERE.md` — single entrypoint for a fresh Astra/Sol/Codex session
- `CURRENT_GOAL.md` — current implementation scope
- `AGENTS.md` — protected implementation boundaries
- `ARCHITECTURE.md` — ownership and data-flow boundaries
- `docs/model-v0.md` — native model semantics
- `docs/quality.md` — anti-slop engineering contract
- `docs/first-usable.md` — M1 acceptance flow
- `schemas/native-v0.16.schema.json` — current native JSON shape (0.1–0.15 readers retained)

## Project rules

- One canonical authored document state.
- GUI, API, and MCP must share the same core commands.
- Stable IDs are not array indices.
- Unsupported interoperability must be explicit; never silently flatten or discard.
- Prefer the smallest correct implementation boundary over speculative managers/services/frameworks.
- Candidate requirements remain backlog until explicitly selected by the current milestone.
- No distribution license has been selected yet.

## Linked and Embedded images

Add (or File) → **Linked Image… / Embedded Image…** imports an explicit local
PNG/JPEG as an Image. Width/Height are ordinary properties, with links and
expressions; Canvas body dragging uses the existing transform commands. Images
support existing geometry masks, opacity, blend modes and Group composition.
Image sources cannot themselves be geometry masks or use vector Shape stacks.

Both modes keep accepted original source bytes in the native document. A linked
asset also remembers an absolute local drive path. **Check link** (or File →
Image Assets → Check links) compares the current file without changing artwork.
There is no automatic filesystem scan or pixel replacement. **Reload** accepts
changed bytes; **Relink…** accepts a new file and locator; **Embed accepted image**
keeps cached bytes and removes the link, including when the source is missing.
All placements share asset identity, so Reload/Relink updates them together while
preserving each placement's dimensions/transforms. Every change is one Undo.
The Image Assets dialog reuses accepted sources and removes unused assets;
deleting a placement alone keeps its asset. Reopening starts link status at
`unchecked` and never fetches external files. Missing/unreadable files leave
accepted pixels available for editing, export and recovery.

Windows WIC uses only its built-in PNG/JPEG codecs from bounded memory. Eight-bit
RGB/gray/palette sources, alpha, JPEG EXIF orientations1–8 and usable bounded
RGB/gray ICC profiles are supported. Accepted data includes its color interpretation:
unprofiled input assumes sRGB; embedded ICC is projected to sRGB. CMYK, HDR/high
bit depth, animation, PNG eXIf, unsupported profiles/color metadata and URLs/UNC
paths reject explicitly. Original bytes and metadata stay in native; SVG embeds
lossless normalized oriented sRGB PNGs, shared once per asset.

Limits:8MiB original bytes and16,777,216 pixels per image,8192 per dimension;
24MiB and33,554,432 pixels per document,128 assets. Native/local API limit64MiB;
asset import/replacement preflights serialized admission. SVG normalized PNG limit
16MiB per asset /32MiB total. ICC profile limit1MiB. Import/Reload and SVG export
are synchronous; gestures reuse derived image projections.

MCP adds `nect_image` for explicit file import and `status/check/reload/relink/embed`.
`nect_command` → `assets` returns source metadata, mode, locator and placements
without the base64 payload. `create_image`, asset add/replace/delete and ordinary
Image properties use the same Session commands as GUI. See
[material-study.nect](examples/material-study.nect) and its [SVG](examples/material-study.svg)
for a masked Linked JPEG shared across four placements plus an Embedded transparent
PNG, Multiply, named colors and editable Text. Its procedural source files are
original fixture artwork; another checkout may need Relink, while cached artwork
remains intact.


## Exact object alignment

Select whole objects and use Inspector **Align · geometric bounds**, or Edit →
Align objects. Align left/center/right or top/center/bottom to the initial
selection envelope or the active Artboard. A single object can align to an
Artboard. Point selections are excluded. Each action is one Undo and preserves
retained sources. Bounds include evaluated geometry, text and image rectangles;
stroke width and mask cropping are excluded. This is a one-time placement, not
a persistent constraint. Structural ancestor/descendant selections and driven
transforms that cannot represent the result reject without partial changes.

API/MCP `apply` accepts `{"type":"align_objects","objects":["a","b"],
"axis":"x","alignment":"min","artboard":null}`. Axis is x/y; alignment is
min/center/max; artboard is null for selection bounds or an Artboard ID.

Inspector **Equal H gaps / Equal V gaps** (also in Edit → Align objects) spaces
3–1000 non-overlapping whole objects by their geometric bounds. Spatial order is
independent of selection order; the outer two stay fixed. This ignores the
alignment-target chooser. Overlap on the chosen axis rejects explicitly. API/MCP
command: `{"type":"distribute_objects","objects":["a","b","c"],"axis":"x"}`.


## Rotate, scale and reflect a selection

Select whole objects or Groups, then **Ctrl+Shift+T**, Edit → **Rotate / scale
selection…**, or the same Inspector/context action. Rotation uses clockwise
degrees; scales use percentages. Link X/Y for uniform scaling, or use **Flip X /
Flip Y** for reflection. The shared pivot is the selection's geometric bounds
center (excluding strokes) or explicit canvas coordinates. Scaling uses canvas
axes before rotation. Authored object Anchors stay unchanged.

Apply is one Undo; cancel/default no-op does not edit the document. Selected
parents/followers move once, retained sources remain editable, and a changed
driven transform or necessary singular inverse refuses atomically. The dialog
rejects stale artwork if another API edit occurs while it is open. Existing
single-object Anchor rotation/scale controls remain available separately.
API/MCP: `{"type":"transform_objects","objects":["a","b"],"rotation":30,
"scale_x":0.8,"scale_y":0.8,"pivot":null}`. Factors (not percentages) in the API;
explicit pivot is `[x,y]`. Negative/zero factors reflect/collapse an axis.

## Selection and close inspection

On the Canvas, **Ctrl+A** selects visible artwork in the current Composition or
entered Group, treating child Groups as whole objects. In point-edit context it
selects all anchors of the currently selected objects. Edit → Select all in
editing context offers the same operation. Text fields keep their normal Ctrl+A.

**Ctrl+2** / View → Fit selection frames selected geometric bounds (stroke width
excluded). Canvas **Shift+F** does the same; **F** still fits the active Artboard.
Point selection frames anchor positions. Empty selection leaves the view alone;
a single point uses bounded zoom. View/selection operations never author history.

Canvas **arrow keys** move selected whole objects or points by1 world du;
**Shift+arrow** moves10du, independent of zoom and Snap. Each key event (including
auto-repeat) is one undoable Session transaction. Rotated/scaled point coordinates
are inverse-mapped; retained shapes gain ordinary Point Edit overrides. Driven
changes reject the whole transaction. Text/tree keys remain their usual editing
keys; draw/drag, Anchor Edit and gradient-handle modes do not nudge artwork.

Drag from empty Canvas to select objects fully contained by the rectangle;
**Shift-drag** adds them. In point context, only anchors in the current target
objects are considered. Scope-level Groups stay whole, hidden artwork is not a
normal target, and geometric bounds exclude stroke width/mask cropping. Selection
commits on release; **Escape** preserves the previous selection. Empty click
still clears, Shift-empty-click preserves it.


## Editable SVG artwork intake

**File → Import SVG artwork… (Ctrl+I)** takes an explicit local SVG and appends
one editable Group at the active Artboard origin. The source file stays intact;
Undo removes the entire import. Supported paths, Groups, affine transforms and
solid Fill/Stroke become normal native authoring, ready for point edits, layout,
references and native save. [wayfinding-mark.svg](examples/wayfinding-mark.svg)
is an original supported fixture.

The static subset supports M/L/H/V/C/S/Q/T/A/Z, relative and short forms, plus
rect/circle/ellipse/line/polyline/polygon. Quadratics lower exactly; elliptical
arcs and rounded shapes use disclosed cubic approximation. Original
[shape badge](examples/shape-badge.svg) and [arc mark](examples/arc-mark.svg)
exercise these paths. Text/images, gradients, CSS stylesheets, masks/filters and external content reject
the whole import. SVG viewport maps coordinates but does not create a crop or
Artboard; off-viewport artwork remains editable. See [exact contract and limits](docs/svg-import.md).

Host `import_svg` and formal MCP `nect_import_svg` require current identity/revision,
absolute local path, Composition, fresh identifier prefix, name and x/y. The
result reports root, paths, points, viewport dimensions and conversion boundary.
Core-only CLI does not include the Qt reader or claim SVG import.


### Stacking order

Edit → Arrange stacking order and the selection context menu expose Bring forward
(Ctrl+]), Send backward (Ctrl+[), Bring to front (Ctrl+Shift+]) and Send to back
(Ctrl+Shift+[). Select objects/Groups sharing one structural parent. Selected
siblings retain their relative order; one-step moves cross one unselected neighbor.
Operations remain inside that parent, preserve coordinates/references, and use the
same `reorder_objects` Session command available to API/MCP. A boundary no-op does
not add history. Point selections and mixed parents are refused. Layering changes
naturally affect overlaps, blending and masks; Undo restores the exact ordering.


**Ungroup selected Groups (Ctrl+Shift+G)** in Edit or the selection context menu
removes ordinary neutral static containers and selects their children. Child
shape, order and world placement stay intact; one Undo restores the Group.
Opacity/masks/blends, dynamic Group transforms, explicit Group Transform Parents,
references to the removed Group and unpreservable child dependencies are refused
with an explanation. This is especially useful for static imported SVG nesting.
The same `ungroup {composition,parent,group}` command works through API/MCP;
[the precise preservation contract](docs/model-v0.md) applies to all callers.
