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

For a separate read-only graph socket check, set `NECT_ACCEPTANCE_OBJECTS=1`,
`NECT_ACCEPTANCE_GRAPH_WIRES=1` and `NECT_ACCEPTANCE_GRAPH_PORTS=1` on a fresh
fixture root. The 14-tool verifier distinguishes stored input wires from
available GUI sockets. On the reopened Blend in Distance mode it checks
`a`, `b`, `distance` and the default output, cursor offsets, the stale
pre-reopen target, invalid fields, and the unchanged read revision. The
surrounding verifier still edits its own disposable Blend and exports it.
The socket check does not read values or change wiring; no ordinary-app
transport is enabled.

For same-layer Vector2 wiring, use those three graph socket flags plus
`NECT_ACCEPTANCE_VECTOR_WIRE=1`. The 15-tool run adds a synthetic layer with
two Text nodes. It detaches and reconnects the named Position output, checks
the revision and stored wire across Undo/Redo and reopen, and verifies the
literal fallback and final SVG/PNG. It does not enable production editing.

For same-layer Scalar wiring, set the three graph socket flags and
`NECT_ACCEPTANCE_SCALAR_WIRE=1` on a fresh root, leaving
`NECT_ACCEPTANCE_VECTOR_WIRE` off. The synthetic Separate Vector node's
named `x` socket feeds the Text node's `size`. The 15-tool verifier
checks port type, detach and reconnect, revision guards, Undo/Redo,
persistence including the original literal Size of 100, and preservation
of the unrelated Position wire. It validates final SVG and PNG export.

For a separate primitive shape run, use a fresh isolated root and set
`NECT_ACCEPTANCE_OBJECTS=1`, `NECT_ACCEPTANCE_ARTWORK=1`, and
`NECT_ACCEPTANCE_SHAPE_ROUNDTRIP=1`. Leave unrelated scenarios off.
The 13-tool verifier adds Rectangle, Circle, Ellipse, Polygon, and Star at
known centers; verifies that Undo removes only Star and Redo restores the
same IDs and order; then checks the ID order and centers after Save As and
reopen. It verifies a nonempty labeled SVG path for each shape type and a valid
PNG export with the fixture dimensions, not individual shape pixels.
No ordinary installed connection is involved.

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

### Last-layer deletion scenarios

In a separate disposable run, set `NECT_ACCEPTANCE_OBJECTS=1` and
`NECT_ACCEPTANCE_LAYER_DELETE=1`. The request requires the exact editor
target, layer ID, and explicit confirmation. After discarding the reply,
check that the old ID is gone and the new layer contains only an empty
Output, even though the layer count is still one. Replaying the old
request must fail. Undo restores the original graph; Redo restores the
same blank ID, which must persist after save and reopen.

In another fresh directory with the same executable, also set
`NECT_ACCEPTANCE_LAYER_DELETE_RESTORE=1`. This run saves after Undo
without Redo and requires the reopened project, SVG and PNG to match the
baseline byte for byte. Neither run tests Undo across reopen.

### Blank layer scenario

In a separate run, set `NECT_ACCEPTANCE_OBJECTS=1` and
`NECT_ACCEPTANCE_LAYER_CREATE_BLANK=1` on the matching fixture executable.
The client discards the create reply and rediscovers the single new layer and
its empty Output. Replaying the old request must fail. Undo/Redo and reopen
preserve the expected identities; baseline SVG and PNG must remain identical.

### Layer state readback

Add `NECT_ACCEPTANCE_LAYER_STATE=1` to a Guide or Solo scenario on the same
fixture build. The read tool takes only a captured editor target and a layer
ID, and returns the stored visibility, lock, Guide, and Solo flags. It does
not infer effective rendering visibility or return a name, nodes, or paths.
The test reads the flags before and after editing and reopening, rejects an
old target and an unknown layer, and checks that the read leaves the revision
unchanged.

### Guide layer scenario

In a separate run, enable `NECT_ACCEPTANCE_ARTWORK=1`,
`NECT_ACCEPTANCE_OBJECTS=1`, `NECT_ACCEPTANCE_LAYER_LOCK=1`, and
`NECT_ACCEPTANCE_LAYER_GUIDE=1` with a matching fixture executable.
The 15-tool run converts a Rectangle layer, reverses the conversion, tests
history and lock rejection, and saves that same layer as a locked Guide.
After reopen, its ID and artwork remain in the project, but SVG excludes it
while retaining the Blend and Text. PNG must export successfully.

### Layer Solo scenario

In a separate run, use the matching fixture executable with
`NECT_ACCEPTANCE_ARTWORK=1`, `NECT_ACCEPTANCE_LAYER_VISIBILITY=1`, and
`NECT_ACCEPTANCE_LAYER_SOLO=1`. The 14-tool test sets Solo explicitly on a
Rectangle and a Text layer, hides the Text, and checks Undo/Redo and saved
flags. After reopen, SVG contains only the visible soloed Rectangle; PNG
must export successfully. Keep the other optional cases off for this run.

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

### Literal Fill readback

For a separate disposable fixture run, enable `NECT_ACCEPTANCE_OBJECTS=1`,
`NECT_ACCEPTANCE_ARTWORK=1`, `NECT_ACCEPTANCE_LAYER_LOCK=1`,
`NECT_ACCEPTANCE_SOLID_FILL=1`, and `NECT_ACCEPTANCE_SOLID_FILL_READ=1`.
Use a new receipt path. The verifier checks separate opt-in discovery of the
read tool, exact Fill selection, locked literal readback, stale-target refusal
and unchanged revision on read. It verifies the edited RGB and preserved
alpha after reopening the saved fixture. This does not exercise or expand
ordinary Windows pairing.

### One-document authoring round trip

To extend the literal Fill run to a single authoring workflow, also enable
`NECT_ACCEPTANCE_ARTWORK_TRANSLATE=1` and use a fresh fixture root containing
only the approved fixture build. Do not reuse a folder with earlier recovery
state. The verifier creates a Rectangle and Text, sets the Rectangle Fill,
moves both, saves, reopens and exports both formats. It checks exact authored
positions and Fill identity and value; SVG is checked for the named layer and
the chosen fill, and PNG for its signature and canvas dimensions. It does not
cover the normal app pairing, native dialogs or manual input.

With the same flags and a separate fresh root, add NECT_ACCEPTANCE_TEXT=1 to
edit the Text content as part of that workflow. The verifier checks the
authored phrase after reopening and a nonempty outline for the Text layer in
the SVG. This ASCII fixture does not cover IME or Japanese font fallback.

Add `NECT_ACCEPTANCE_TEXT_READ=1` to also test the opt-in read-only
`nect_test_read_text_content`. It returns the exact authored string for one
supported Text node, not font metadata or another node's content. A reply
that exceeds the fixture frame budget is rejected instead of truncated.
The verifier reads before and after the edit, rejects the old target, and
reads again after reopening without changing the revision.

For Japanese readback, enable `NECT_ACCEPTANCE_OBJECTS=1`,
`NECT_ACCEPTANCE_ARTWORK=1`, `NECT_ACCEPTANCE_TEXT=1`, and
`NECT_ACCEPTANCE_TEXT_READ=1` in a separate fresh root. Choose only one of
`NECT_SVG_ACCEPTANCE_JP_FIXTURE=1` (embedded fixture font; both exports
succeed) or `NECT_ACCEPTANCE_TEXT_JP_DEFAULT_PROBE=1` (missing glyph;
both exports refuse without creating a file). Both check that reading after
reopen returns the exact mixed Japanese/Latin Unicode string. These cases
do not exercise manual Japanese IME input.

### Literal Fill opacity

Add `NECT_ACCEPTANCE_SOLID_FILL_OPACITY=1` to the literal Fill readback
fixture. The read-back contract keeps RGB and alpha separate. The command
changes only an authored scalar from 0 to 1, preserving RGB and the existing
edit restrictions. The verifier checks a 0.75 opacity, including lock and
stale rejection, Undo/Redo, reopened state and the SVG fill-opacity attribute.
It also checks transparent and fully opaque literals without losing RGB.

For a separate fresh run, add `NECT_ACCEPTANCE_SOLID_FILL_ZERO_COPY=1`
to keep alpha zero in the saved copy. Arrange while the rectangle is
visible, then set alpha to zero before save. The stable Fill ID and
authored RGB survive reopen even though the rectangle is not painted.
Other artwork is still exported. This case does not change which shapes
are eligible for the existing visible-artwork placement tool.
