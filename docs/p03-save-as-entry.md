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
