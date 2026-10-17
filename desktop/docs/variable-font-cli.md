# Variable-font JSON export from Node

Run `node cli/export.js project.json artwork.svg` from `desktop/` using the same Node installation as the project tooling. The output must not already exist. Font files are embedded in project JSON; export does not download fonts or require a browser.

## Preparation and parity

The CLI first deserializes and structurally validates the whole project. For a project containing a `variable-glyf-v1` asset, it creates the shared `createVariableDocumentPreparer` with the Node-only transport in `cli/variable-font-node.js`. The transport runs `cli/variable-font-node-worker.js` in a real `node:worker_threads` worker. Only that worker imports the fontkit backend. Neither Node entry is imported by renderer code.

The CLI awaits preparation of the exact document, retrieves its immutable prepared document, activates the preparation ticket, and passes those prepared fonts plus the bundled default font to the existing core SVG exporter. There is no main-thread fontkit fallback and no rendering of an unprepared variable font. All variable runs, including hidden or disconnected Text nodes, must pass the shared document preflight. Embedded static fonts continue to use the existing resolver.

Static-only JSON keeps the original synchronous export path and byte-for-byte SVG behavior. It does not instantiate or load the Node worker transport.

## Bounds and failure behavior

The shared variable-font contract controls font formats, axis ranges, supported text and layout features, and resource limits. The current preparation timeout is at most 5 seconds per distinct worker request, with at most 8 distinct runs per document. Unsupported formats/layouts/text, unknown or out-of-range axes, invalid assets, missing preparations, worker failures and timeouts fail closed with a nonzero exit status. The CLI writes only after successful validation, preparation and SVG evaluation; it opens the destination with `wx`, preserving existing files even on successful rendering. This is an exclusive-create guarantee, not a claim that filesystem write failures cannot leave a newly created partial file.

The preparation bridge is disposed in `finally`, on both success and error. The Node transport also awaits all outstanding worker terminations before the CLI exits; the CLI does not force an early process exit after a preparation failure. Worker errors, message decoding errors and unexpected exits all reject pending work. No worker/cache persists between CLI processes.

## Verification

Run `node --test tests/variable-font-cli.test.js` for focused coverage:

- Fresh Node CLI processes export the licensed Roboto fixture at weight/width coordinates `(100, 75)` and `(900, 100)`, matching shared prepared-core SVG exactly.
- Repeated exports are deterministic; the two coordinate pairs produce distinct SVG.
- Static Text output matches the existing default-font SVG oracle exactly.
- Invalid axes, unsupported mapping and unsupported font bytes exit unsuccessfully, leave existing outputs unchanged and do not create missing outputs.
- Successful rendering still refuses to overwrite an existing file.
- A real-worker timeout releases the worker; disposal is idempotent and unprepared synchronous export rejects.

The fixture and its license/provenance are already in `tests/fixtures/variable-font/`. These focused Node checks do not establish browser, native desktop or full-suite acceptance.
