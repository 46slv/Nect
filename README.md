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

Circle and Rectangle retain their generators after direct point edits, with
visible Point Edit overrides/bypass and explicit Convert to Path. Ordered local
Fill/Stroke/Repeater stacks share one core evaluation for Canvas and SVG. Native
0.5 saves procedural state, editable linear/radial gradients and ordered Artboards
with parent-size inheritance. It migrates 0.1–0.4 without reference loss.

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
- raster/compositing production model

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
Space-drag pans; wheel zooms; Fit frames the artboard. Groups select as a unit;
double-click enters them and the breadcrumb returns. Inspector fields accept
numbers and one-shot `+=`/`-=` adjustments. Right-click provides Copy Value,
Copy Reference, Paste Value, Paste Link and explicit Unlink. Drag ↗ to a source
field (hover Objects to inspect another source); click ↗ to search. General
expressions, Polygon/Star, text, gradients and masks are subsequent slices.

The Shape stack supports multiple solid Fill/Stroke entries, HEX RGBA/color
editing, enable/reorder/remove and Repeater. Add a radial repeater for a fixed-step
12 × 30° starting point. Repeater before paint creates a compound path; after
paint it repeats separately painted copies. Source points remain directly editable.
Open `examples/radial-ornament.nect` for an original procedural sample, or reproduce
it in an empty live desktop with `scripts/create_radial_demo.py --endpoint nect-demo
--output build/radial.nect`. The script uses the production Session API and refuses
to replace existing artwork.

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

Recovery protects committed revisions at a one-second timer cadence (synchronous
IO in this first small-document slice); drafts remain separate. Manual Save uses
atomic replacement with direct-write fallback disabled and keeps ten previous
native files in `<file>.backups`. Recovery files are in the local Nect app-data
folder, or `--recovery-dir`. File > Open Recovery opens one as an unnamed document.
This is not yet continuous saving to the named source file, asynchronous IO, or
bounded recovery-directory retention. Unsupported native fields/versions are
rejected without altering the source.

## Entry points

- `START_HERE.md` — single entrypoint for a fresh Astra/Sol/Codex session
- `CURRENT_GOAL.md` — current implementation scope
- `AGENTS.md` — protected implementation boundaries
- `ARCHITECTURE.md` — ownership and data-flow boundaries
- `docs/model-v0.md` — native model semantics
- `docs/quality.md` — anti-slop engineering contract
- `docs/first-usable.md` — M1 acceptance flow
- `schemas/native-v0.5.schema.json` — current native JSON shape (0.1–0.4 readers retained)

## Project rules

- One canonical authored document state.
- GUI, API, and MCP must share the same core commands.
- Stable IDs are not array indices.
- Unsupported interoperability must be explicit; never silently flatten or discard.
- Prefer the smallest correct implementation boundary over speculative managers/services/frameworks.
- Candidate requirements remain backlog until explicitly selected by the current milestone.
- No distribution license has been selected yet.
