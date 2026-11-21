# Windows live-editor acceptance (development only)

This test runs the actual Windows editor in a fresh, hidden instance using the
bundled Blend Distance example and, for the positive Japanese scenario, a bundled
font fixture. It does not connect to an already running
Nect, launch Hermes, or install a service. The parent process owns the standard
streams; there is no listener. Normal Nect does not enable this mode.

1. In the development shell set `VITE_NECT_LIVE_EDITOR_ACCEPTANCE=1`, build
   the frontend, and build the Tauri binary with the explicit
   `custom-protocol,live-editor-acceptance-test` features. Use a writable
   disposable build directory for the EXE and test outputs.
2. Set `NECT_ACCEPTANCE_EXE` to the absolute path to that exact EXE,
   `NECT_ACCEPTANCE_SHA256` to its reviewed SHA-256, and
   `NECT_ACCEPTANCE_RECEIPT` to a **new** JSON path beside the EXE.
3. Run `python -B desktop/scripts/verify-live-editor-acceptance-mcp.py` from
   the repository. It verifies the binary checksum before spawning the runner.
   Do not use the adapter as a product MCP server.

The fixture exercises MCP initialization and tool discovery, Blend editing,
Undo and Redo, stale target rejection, Save As, request receipt readback,
separate copy save and reopen, and SVG/PNG. The Save As destination is fixed
inside the run directory; the native file writer and live document binding are
used without a picker. After dropping the first Save As reply, the test reads
the request receipt and verifies that replay reuses the outcome, with one file
and one committed save epoch. Copy, SVG, and PNG are compared to known Windows
fixture hashes; a deliberately updated fixture or renderer needs an independently
reviewed baseline rather than simply replacing the expected hashes. The run does
not cover restart persistence, the Windows Save As picker, or a Hermes Desktop
model session. It never touches existing projects.

For a normal renderer build, explicitly unset the acceptance flag or set it to
`0`. The generated JavaScript must contain none of
`__NECT_LIVE_EDITOR_TEST_INSTANCE__`, `testLiveEditorProtocol`,
`live_acceptance_claim`, or `live_acceptance_save_as`.

### Choose one scenario per run

Use a fresh receipt for each scenario. Clear inherited flags that are off in
the table before starting the verifier, including when switching back to the
baseline. All four variables are optional; on means exactly `1`.

| Scenario | `NECT_ACCEPTANCE_ARTWORK` | `NECT_ACCEPTANCE_TEXT` | `NECT_ACCEPTANCE_TEXT_JP_DEFAULT_PROBE` | `NECT_SVG_ACCEPTANCE_JP_FIXTURE` | Tool count |
| --- | --- | --- | --- | --- | --- |
| Baseline | off | off | off | off | 11 |
| Artwork | on | off | off | off | 12 |
| ASCII text | on | on | off | off | 13 |
| Japanese text, missing glyph | on | on | on | off | 13 |
| Japanese text, embedded font | on | on | off | on | 13 |

### Artwork creation scenario

Keep the baseline run with no artwork flag; it still checks the same 11 tools and
expected artifact digests. For a separate run, set `NECT_ACCEPTANCE_ARTWORK=1`
and a fresh `NECT_ACCEPTANCE_RECEIPT`, then run the same verifier with a
hash-pinned test build that implements `artwork.create`. It discovers 12 tools
and checks target-bound Rectangle and default Text creation, invalid coordinates,
stale requests, Undo/Redo, Save As reconciliation, and reopening the saved
composition. It checks the returned layer/node IDs and coordinates in the saved
JSON, then verifies valid SVG and 960×540 PNG outputs distinct from the baseline.

This exercises neither arbitrary text input nor a normal-product MCP connection,
Hermes Desktop, or the native save picker.

### Text editing scenarios

With an artwork-compatible test executable matching this revision, enable
both NECT_ACCEPTANCE_ARTWORK and NECT_ACCEPTANCE_TEXT. This discovers 13 tools
and exercises targeted ASCII text editing through reopening and SVG/PNG output.
For a separate negative case set NECT_ACCEPTANCE_TEXT_JP_DEFAULT_PROBE=1 with a
fresh receipt. Mixed Unicode text must round-trip intact as editable Text, but
DejaVu Sans has no U+63DB. Both SVG and PNG must return unsupported-glyph,
with no output file. Unknown transport or writer errors remain a separate
outcome-unknown status; an unknown result must be checked before retry.
For the Japanese positive run with a test build at 4b0b093 or newer, also set
NECT_SVG_ACCEPTANCE_JP_FIXTURE=1, leave the negative flag off, and use a fresh
receipt. The native fixture attaches one existing redistributable font and
one pre-authored text layer to the Blend fixture. The run edits that Text
node, verifies its exact Unicode and font asset after reopening, and checks
SVG and PNG output. No OS font installation or lookup occurs. Same-build GUI
and native installed-face parity and normal-product MCP remain separate.
