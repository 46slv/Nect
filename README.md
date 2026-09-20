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

**M1 desktop loop delivered; practical alpha in progress**

The Windows Qt desktop now creates paths from an empty document, selects and edits
points/handles, provides a property-source picker, and shares atomic commands and
Undo with a live local API and formal MCP stdio adapter. Native save, previous-file
backups and recovery snapshots are implemented. The Windows 30 fps viewport
baseline and M1 evidence are in `docs/first-usable.md`. Next work is selected in
`CURRENT_GOAL.md`; this is not yet the completed practical alpha.

Circle, Rectangle, Polygon and Star retain their generators after direct point edits, with
visible Point Edit overrides/bypass and explicit Convert to Path. Ordered local
Fill/Stroke/Repeater stacks share one core evaluation for Canvas and SVG. Native
0.12 saves retained Offset Paths, geometry masks, common compositing, expression source, procedural state, editable linear/radial gradients, ordered Artboards
with parent-size inheritance, editable Text, named colors, retained Polygon/Star,
authored Anchors and explicit Transform Parents. It migrates 0.1–0.11 without
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

Not implemented yet:

- AI/PSD codecs
- OpenFX hosting
- full typography
- raster assets and complete production compositing

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
Space-drag pans; wheel zooms; Fit frames the artboard. Groups select as a unit;
double-click enters them and the breadcrumb returns. Inspector fields accept
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
to8 MiB; unsupported fields/versions reject without altering the source. This
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
- `schemas/native-v0.12.schema.json` — current native JSON shape (0.1–0.11 readers retained)

## Project rules

- One canonical authored document state.
- GUI, API, and MCP must share the same core commands.
- Stable IDs are not array indices.
- Unsupported interoperability must be explicit; never silently flatten or discard.
- Prefer the smallest correct implementation boundary over speculative managers/services/frameworks.
- Candidate requirements remain backlog until explicitly selected by the current milestone.
- No distribution license has been selected yet.
