# P03-SAVE-AS-01 — typed source preservation at the native Save As boundary

**Entry:** `codex/practical-alpha@81ef64149ae46f3be691a6fc0450e78f68e8ad59`, native 0.20, clean local/tracking/fresh remote parity before work. DEC-71 Mission Owner holds the single writer after the ignored `build/manual-recipes/p03-save-as-takeover-ack-20260927.json` receipt and predecessor RELEASE. The live Completion Route R02/P03 requires authored property storage and per-type acceptance; the P03 packet collection §6 and U02 retain the broader type/UI/API/native matrix. This packet covers one native Save As boundary only.

## Observed owner path

`Window::save(true)` selects a native path and calls `Host::save`. `Host::save` encodes the committed Session, drains a running protection job, calls `store_native`, then changes `file_path`, stamp and saved revision only after verified storage. `store_native` uses a sidecar lock, atomic replacement and disk hash readback. Saving to the current source requires its prior stamp; explicit Save As to another path does not. Recovery tracks the active source path. Existing `live_save_contract` covers external conflict and basic Save As, but does not assert a typed driver/literal and stable ID across the source/destination boundary.

## Frozen positive and negative oracle

Use an owned temporary directory. Create two Text objects with stable IDs. The target owns `text.layout` literal `frame`, `frame_width=96`, `frame_height=48`, and a same-field Ref to a source whose `text.layout` literal is `auto`; its evaluated layout is `auto` while its own frame Scalars stay authored. Save the source, induce an external source-file conflict, and preserve the exact external source bytes/hash. Explicit Save As to a distinct destination must persist native 0.20 with both IDs, the target's literal, frame Scalars and exact driver Ref; a disk cold reopen must evaluate `auto`. The destination readback must match the committed revision and the active native binding, while the external original remains byte-identical. Recovery must follow the destination after explicit protection.

For a failed Save As to an invalid owned path, expect a storage error with no destination file, no new native binding or saved revision, no Session revision or authored change, and no loss of the already protected committed state. A later valid Save As must still work. Do not use user files or assume a successful method return alone proves durability.

## Exit and residuals

Review the focused contract and independent disk/hash/readback evidence; repair any concrete failure before a coherent checkpoint. This slice does not close P03, R02, GUI Save As interaction, all typed properties, long-term recovery guarantees, or any Confirmed Requirement. GUI reachability stays a separate `LOCAL_WAIT` without new supported owned-window evidence; R11-I remains a distinct format DESIGN_GATE.

## Local result — 2026-09-27

The new `typed_source_save_as` case in `tests/live_save_tests.cpp` exercised the frozen positive and negative oracles in an owned `QTemporaryDir`. It verified external original bytes and SHA-256 through a native conflict, unchanged binding/revision/authored Document and protected recovery on an invalid destination, exact native 0.20 destination bytes and stable Text IDs/driver/literals, recovery provenance after a successful Save As, and a separate `Host` cold reopen from the destination. No production source or native schema changed.

`cmake --build build --config Release --target live_save_tests` passed; `ctest --test-dir build -C Release -R "^live_save_contract$" --output-on-failure` passed **1/1**. `git diff --check` passed. A default-config build attempted before Release failed in existing `src/text.cpp` (`OutlineSink`/`OutlineRenderer` `final` through the older Windows SDK WRL `RemoveIUnknownBase`, MSVC C3246); that attempt did not test this packet. The Release result is the verified contract. Host-level cold reopen is not a distinct CLI process, a desktop MCP route, or hands-on GUI evidence. Those surfaces remain open for follow-on acceptance.

## P03-SAVE-AS-02 — desktop MCP Session parity packet

**Freeze:** Start from synchronized `codex/practical-alpha@cede61660f1c04aa4a5fc3b63ee509b30c29decb`. Use the existing offscreen desktop + formal MCP client test harness and owned temporary paths. The MCP `nect_file` `save` operation must forward to the live Host Session with exact session/document/revision checks. Do not introduce a second save implementation, native version change, or GUI completion claim.

**Positive oracle:** In one live desktop Session, create linked Text source/target with `text.layout` literal/driver and target-owned frame Scalars through Session commands. Save an original native 0.20 file through `nect_file`, then explicitly Save As to a distinct owned destination. Read `nect_session` after each operation; only the destination becomes the active `file`, its `saved_revision` matches the committed revision, and recovery follows it after `recover`. Compare original and destination bytes/hashes and inspect the authored source/target IDs and literal/driver. Restart the desktop on the destination and use the formal MCP client to read the same exact authored and evaluated values from a new Session. Keep any automatic original-source save timing explicit; do not claim unchanged original bytes after later live edits that legitimately autosaved there.

**Negative oracle:** A stale session/document or expected revision and an invalid owned destination must reject without changing the live binding, revision, destination existence, or authored Document. Read back the current native/recovery receipt after failure. This tests the API/MCP surface; actual GUI Save As remains `LOCAL_WAIT` until a supported owned-window interaction is available. Full R02/P03 and Confirmed Requirement closure remain open.

### P03-SAVE-AS-02 local result — 2026-09-27

The reviewed formal MCP scenario in `tests/mcp_desktop_tests.py` creates a linked Text layout source/target through Session commands, saves the original and explicit destination through `nect_file`, and verifies `nect_session` identity, active path, saved/recovery revisions and recovery provenance. Stale session/document/revision and invalid owned destination reject with no file, Document, binding or receipt mutation. Original and destination native 0.20 bytes/SHA-256 match, including the stable Text IDs, target `frame` literal, exact layout Ref and `96 × 48` target frame Scalars. A newly started desktop Session cold opens the destination; the same formal MCP client reads the retained authored and evaluated values, and the original/destination bytes remain unchanged.

Python compilation, Release `nect_desktop` build, `git diff --check` and the focused `mcp_desktop_contract` **1/1 PASS**. This is offscreen desktop/API/MCP and distinct-process native evidence. The `gui_save_as_acceptance` receipt is false; it does not establish hands-on GUI Save As, all typed property kinds or full R02/P03/Confirmed Requirement completion. R03 GUI Snap remains a separate `LOCAL_WAIT`; R11-I remains a format DESIGN_GATE.
