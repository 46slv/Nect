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

In this build, a valid `editor.inspect` request travels over a dedicated
Tauri Channel to the fixture WebView and returns through the scoped reply
command; inspection cannot fall back to the edit mailbox. Native admission
validates the request ID, current synthetic grant and response shape. The
test does not cover production grants, an already-open app, concurrent GUI
tab changes, or Hermes.

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
`live_acceptance_claim`, `live_acceptance_save_as`, or `live_acceptance_inspect_attach`.

### Choose one scenario per run

Optionally set `NECT_ACCEPTANCE_OBJECTS=1` to discover one additional
read-only tool. This checks that the same target can list layers, discover
the Blend node address, and use that address for the edit. Layer entries include
the stored visibility flag, not evaluated visibility under Solo. Old list requests
are rejected after the edit. Keep the flag off with older executables.

Set `NECT_ACCEPTANCE_GRAPH_WIRES=1` to additionally read the Blend fixture's
stored input connections over the same MCP process. This checks exact input
and source IDs, missing named output represented by null, an empty end page,
and rejection of extra arguments and stale document revisions. The probe also
confirms that reading does not change the target, and a distance-only edit
does not change the returned wiring. The flag is for the acceptance build,
not the ordinary editor or the separate read-only preview.

Set `NECT_ACCEPTANCE_LAYER_VISIBILITY=1` to also exercise an explicit
hide and Undo on the fixture layer. The probe rejects stale revisions and
non-boolean values, verifies that selecting the current visibility changes
neither history nor revision, then runs the existing save/reopen and SVG/PNG
checks after restoring the visible layer. It cannot change lock or solo.

For rename persistence, set both NECT_ACCEPTANCE_LAYER_RENAME=1 and
NECT_ACCEPTANCE_RENAMED_ROUNDTRIP=1 with a new receipt. The renamed layer
must retain its Unicode name and stable ID after saving and reopening.
The saved JSON and exported SVG may differ only in the layer name and
data-layer labels respectively; the PNG must be byte-identical. Do not
combine with artwork, font fixtures, or hidden/locked roundtrip tests.

For a separate reorder run, set `NECT_ACCEPTANCE_LAYER_REORDER=1`,
`NECT_ACCEPTANCE_OBJECTS=1`, and `NECT_ACCEPTANCE_ARTWORK=1`. Leave other
edit scenarios and Japanese fixtures off. This uses the existing Blend plus
one test rectangle, verifies the stable-ID order and stale/no-op guards,
Undo/Redo, then saves, reopens, and compares persisted order with SVG paint
order. The standard PNG header and canvas size checks still run. The tool
is visible only when opted into this acceptance adapter, not in read-only
preview or the ordinary app.

For a separate duplicate run, set `NECT_ACCEPTANCE_LAYER_DUPLICATE=1` and
`NECT_ACCEPTANCE_OBJECTS=1`. Use a fresh receipt and leave other edit
scenarios off. The client discards the first duplicate reply, discovers one
new layer by ID, rejects exact replay against the old target, and verifies
Undo/Redo and save/reopen. It verifies independent node IDs and both layers
in the SVG. It does not retry against a new revision automatically.

For a separate layer-lock run with a fresh receipt, set both
`NECT_ACCEPTANCE_OBJECTS=1` and `NECT_ACCEPTANCE_LAYER_LOCK=1`. The test sets
one stored lock flag, confirms layer discovery reports that change, rejects
stale unlock and non-boolean input, checks no-op revision stability, and uses
Undo to restore the fixture before the standard save/reopen and export probes.
The lock operation is not registered by the ordinary app or read-only preview.
With `NECT_ACCEPTANCE_LOCKED_ROUNDTRIP=1`, the same scenario sets the lock again
immediately before saving. Reopening must preserve that flag and the authored
Blend; a stale unlock must fail even though the layer ID is unchanged. SVG and
PNG must still match the unlocked fixture. Do not combine this scenario with
hidden-layer, artwork, or Japanese text probes.

For a separate run with a fresh receipt, also set
`NECT_ACCEPTANCE_HIDDEN_ROUNDTRIP=1` and leave artwork/JP extensions off.
The smoke test then saves and reopens with the layer hidden. It verifies
that the original Blend node is still in the saved document, the hidden
state persists, and SVG export contains no path. PNG must remain a valid
960 × 540 image and differ from the visible baseline.

Use a fresh receipt for each scenario. Clear inherited flags that are off in
the table before starting the verifier, including when switching back to the
baseline. All four variables are optional; on means exactly `1`.

| Scenario                     | `NECT_ACCEPTANCE_ARTWORK` | `NECT_ACCEPTANCE_TEXT` | `NECT_ACCEPTANCE_TEXT_JP_DEFAULT_PROBE` | `NECT_SVG_ACCEPTANCE_JP_FIXTURE` | Tool count |
| ---------------------------- | ------------------------- | ---------------------- | --------------------------------------- | -------------------------------- | ---------- |
| Baseline                     | off                       | off                    | off                                     | off                              | 11         |
| Artwork                      | on                        | off                    | off                                     | off                              | 12         |
| ASCII text                   | on                        | on                     | off                                     | off                              | 13         |
| Japanese text, missing glyph | on                        | on                     | on                                      | off                              | 13         |
| Japanese text, embedded font | on                        | on                     | off                                     | on                               | 13         |

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

### Installed Windows text content parity

Use the same hash-pinned hidden Windows acceptance build, placed in a
dedicated disposable build directory outside the ordinary installation.
Run the Windows Rust test
`desktop_font_layout::tests::actual_os_japanese_basic_wrap_vertical_and_boundaries`
with `NECT_JP_WRAP_SMOKE_OUTPUT` pointing to a fresh scratch JSON file. This
produces only synthetic Meiryo outlines, not font files. Then run:

```text
node desktop/scripts/verify-live-installed-font-parity.mjs <acceptance-exe> <repo-root> <outline-json> <reviewed-exe-sha256>
```

Two independent editor runs load the same synthetic seed. One edits through
the Inspector's Apply Text control and the other through the independent MCP
client. Compare the entire resulting Text node and exact installed Meiryo
descriptor, reject a stale edit, and verify Undo/Redo and copy save/reopen.
The same isolated export route writes SVG and PNG after both edits; compare
their digests. The fixture profile and WebView debugger are disposable and
loopback-only. This is not a test of IME, the export file picker, Hermes,
or the product's external pairing and permissions. Do not commit the local
outline JSON, project copies, or exported artwork.
