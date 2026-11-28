---
name: nect-agent-workflow
description: Use the scoped Nect Desktop headless workflow to create, navigate, edit, save, reopen, or export a disposable project over local stdio. Not a live-editor connector.
---

# Choose the Nect surface

- Use the **native Nect Desktop editor** for an already open/live project, interactive inspection, native file/resource preparation, or tools outside the list below. Follow the editor owner's authorized route. This skill does not attach to, pair with, or replace a native editor. Legacy Qt or existing G19 API/MCP surfaces are separate.
- Use this **headless stdio MCP process** for an independent synthetic/disposable workspace and the operations below. It uses the Desktop Composition Session, command history, project codec and export renderer. Keep the editable `.json` and rendered `.svg`/`.png` as separate deliverables.

## Launch from the checkout

With the checkout's existing Node dependencies available, create a disposable workspace under a dedicated parent you own. Supply its **existing, canonical, absolute local path**. Example on Windows (substitute your workspace path):

```powershell
node .\desktop\cli\agent-workflow.js --workspace 'E:\scratch\owned-parent\workspace' --chromium-path 'C:\Program Files\Google\Chrome\Application\chrome.exe'
```

In a local stdio MCP client's process configuration use `node` as the command and pass the script path and each flag/value as a separate argument. Use the checkout as the working directory, or supply the absolute script path. Set `--chromium-path` to a working local Chromium executable for PNG export and embedded-PNG validation (using the checkout's existing `playwright-core`). Image-free Open and SVG do not need a browser. There is no fallback renderer, decoder, or browser download. Do not register this as an installed/public endpoint or use it to open production or personal files.

The process speaks MCP `2025-06-18`, newline-delimited UTF-8 JSON-RPC on stdin/stdout (16 KiB per request). Initialize, send `notifications/initialized`, then use `tools/list` and `tools/call`. For example, these are real initial frames, one line each:

```jsonl
{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"manual-check","version":"1"}}}
{"jsonrpc":"2.0","method":"notifications/initialized"}
{"jsonrpc":"2.0","id":2,"method":"tools/list"}
{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"nect_new","arguments":{"target":null,"width":377.5,"height":253.125}}}
```

Tool replies carry JSON in a text item: `{ok:true,result}` or `{ok:false,error:{code,message}}`, with MCP `isError` on tool failure. Transport errors are JSON-RPC errors. Close stdin to end the process; EOF does **not** implicitly save.

## Author and verify

1. `nect_inspect` shows no project at launch. Pass `target:null` to `nect_new` or `nect_open` when there is no document. New requires explicit numeric `width` and `height`: positive finite CSS px, at most `100000` each, fractions allowed. Open takes `fileName`, not dimensions. It uses the first stored Composition and resets history.
2. After New/Open, echo the returned `target` unchanged in every non-inspect call. It covers session identity, project revision, and the binding revision (changed by first Save or Save As). Do not retry a stale edit blindly: inspect the current document and reconfirm intent. `nect_create_rectangle` takes `x`, `y` (artboard center, px), and optional `name`. Inspect for the Rectangle → Fill → Output graph, or use `nect_move_artwork` with its stable `layerId` and `dx`/`dy`. Locked or unsupported targets are refused. Inspect returns a graph summary (up to 256 nodes total; `nodeCount` can exceed the displayed nodes). `nect_undo` / `nect_redo` step the real project History.
3. `nect_save` needs `fileName` for an unbound project; on a bound project it updates only that binding. `nect_save_as` requires a fresh name, binds the result, and never writes the old file. `dirty` tracks the authored content relative to the last save, so undoing back to saved content can be clean while the revision increases. `savedRevision` uses `lifecycleRevision`, not the binding revision. New/Open refuse replacement of a dirty project unless you supply `discard:true`; an invalid replacement still preserves current work. On `BINDING_CHANGED`, inspect and use a fresh Save As rather than overwriting the changed file.
4. `nect_export_svg` and `nect_export_png` take a fresh `fileName` and export current committed artwork even before Save. PNG uses the existing headless SVG-to-PNG raster adapter at 1×, subject to its size and resource limits. Verify actual bytes independently: inspect the saved project dimensions/IDs, parse SVG dimensions and paths, and check PNG signature, image dimensions and nonempty pixels. For restart proof, stop the process, start a new one, Open without dimensions, confirm reset history and compare the rendering. Do not claim GUI, native pairing, font/resource preparation, or full editing parity from these checks.

### Find and select a Composition

`nect_inspect` returns `compositions` in project order (or `[]` before New/Open). Each entry contains the exact `id`, `name`, `width`, and `height`, with no layers, nodes, or assets. The separate `composition` field describes the active tab and its graph. The catalog is returned only by Inspect. Use IDs as returned, not labels: names can be duplicate, blank, or Unicode.

Call `nect_switch_composition` with the current `target` and an exact `compositionId`. The tool selects that tab for subsequent inspection, active-artwork edits, and export. A real switch returns `changed:true` and a new target even though project and binding revisions have not changed. All previous targets remain stale, including when switching back to the original tab. Selecting the current tab returns `changed:false` and preserves its target. A switch does not edit the project, change Undo/Redo or dirty state, write files, or persist the selected tab. Open still selects the first stored Composition.

The scoped workflow uses the owner's synchronous switch only. If a tab needs runtime preparation (for example, variable text needing prepared font outlines), it returns `PREPARATION_REQUIRED` without changing selection or using a substitute. Use the native Desktop editor to select that content; the stdio process does not prepare fonts or attach to a live editor. Open still requires a supported first stored Composition. `NO_DOCUMENT` means create or open a project first; `STALE_TARGET` means inspect again; `UNKNOWN_COMPOSITION` means no exact ID matched; malformed arguments return `INVALID_ARGUMENT`.

### Rename a Composition

Call `nect_rename_composition` with the current `target`, an exact `compositionId` from the Inspect catalog, and `name`. Rename does not switch tabs or change the file binding. A changed name is one Undo step and is only persisted by Save. Passing the exact stored name is a no-op, even if the stored name is blank; it preserves pending Redo. Blank replacements are rejected, and nonblank text is stored as supplied, including Unicode and surrounding whitespace.

`NO_DOCUMENT` means there is no open project; `STALE_TARGET` means inspect again before retrying; `UNKNOWN_COMPOSITION` means no exact ID matched; a blank replacement returns `INVALID_ARGUMENT`. Names exceeding the project model's limit (100,000 UTF-16 code units) return `EDIT_REJECTED`. Over stdio the entire UTF-8 JSON-RPC line before the newline must fit in 16 KiB, including the escaped name, target, and request envelope. An oversized line is rejected at the transport, not truncated.

### Embedded PNGs

Open checks every PNG in the project's global asset registry, including images in other Compositions and unused images, before replacing the current document or binding. SVG export checks the PNG bytes actually embedded in the rendered SVG, including derived images, before publishing the original SVG bytes. Both check the still-PNG image data (including a complete zlib stream and legal scanlines) before requiring a real Chromium decode and matching natural dimensions. Verify an image-bearing SVG by checking its `<image>` geometry and decoding the embedded data independently; the display size need not equal the PNG's natural size.

On `IMAGE_REJECTED`, repair or replace the source asset and retry; on `IMAGE_VALIDATION_UNAVAILABLE`, relaunch with a working `--chromium-path`. A failed Open (even with `discard:true`) or SVG check preserves the current document, binding, history and existing files; no new SVG is published.

## File boundary

Use simple names such as `original.json`, `copy.json`, `artwork.svg` and `artwork.png` (ASCII alphanumeric first, then ASCII alphanumeric/underscore/hyphen, one lowercase extension; up to 80 characters). No subdirectories, absolute filenames, traversal, ADS, device names, shell expansion, UNC or remote/mapped storage. The launcher alone sets the root; tool calls cannot change it. Only canonical regular single-link input/bound files are accepted, and the workspace, parent and files must have the launching UID where the OS exposes it. Outputs fail rather than clobber an occupied leaf. Save verifies the binding's identity, metadata and exact byte digest before replacing it. A cleanup warning means publication succeeded but staging cleanup failed; inspect the disposable workspace before retrying.

The workspace **and parent must be exclusively owned and nonconcurrent**. The checks catch obvious symlinks/junctions, canonical mismatches, replacements and external edits. They are not a sandbox or atomic defense against arbitrary same-user filesystem races or hidden reparse types, nor a guarantee of storage durability. Do not point them at shared, synced, personal or production data.

## Desktop and headless entry points

The headless API is `createAgentWorkflow({files}).dispatch` in `desktop/src/agent-workflow.js`; the stdio MCP and local workspace file adapters are in `desktop/cli/`. Desktop starts without a document. GUI New collects explicit width and height, then uses the shared `createExplicitNewProject` factory and the existing guarded `replaceDocument` path. Open uses the dimensions stored in the project. The headless workspace is not the live Desktop session: do not instantiate another headless owner as a proxy for it, and do not treat headless verification as native pairing acceptance.
