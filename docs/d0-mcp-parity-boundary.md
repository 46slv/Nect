# D0 MCP Template, Guide, and Grid parity boundary

This packet adds explicit wire-level regression coverage for D0 behavior already implemented in the
shared Session. It does not add product behavior, alter the transport architecture,
or require a third-party-host acceptance test.

## Formal MCP path

`tests/mcp_template_contract.py` starts the formal MCP stdio server as a subprocess,
performs MCP `initialize`, `notifications/initialized`, `tools/list`, and
`tools/call` JSON-RPC exchanges at protocol `2025-06-18`, and compares typed results
with the separate desktop-owned Host through the local Session API. The desktop Host
owns the live Session and exposes its own named-pipe/Unix-socket automation endpoint.
The CLI JSON-lines interface is used only to author a canonical starting fixture;
it is not treated as MCP.

The MCP discovery prose and parity test now cover:

- `duplicate_template_artboard`: exact placement, fresh Artboard/Content/Grid/local
  Guide IDs, retained source Template/Definition/Guide IDs and descendant overrides,
  and one-Undo/one-Redo restoration
- scoped `align_objects` Guide references: the same stable `GX` ID targets different
  fixed bounds on Artboards A and B, and a disabled target-local occurrence rejects
  without changing typed state, revision, History, or settled native bytes
- inherited Template Grid references for both Align and Distribute: target-local
  Grid IDs resolve to evaluated source bounds without writing local Artboard layout;
  fixed equal-gap coordinates are asserted for two targets, and a null-suppressed Grid
  rejects atomically

Fixed semantic values are independent of the MCP test implementation and are grounded
in the existing core oracles:

- `tests/template_duplicate_tests.cpp` covers fresh identity mapping, preserved source
  and overrides, exact x/y placement, one Undo/Redo, and atomic collision/stale cases
- `tests/artboard_guide_alignment_tests.cpp` covers the same `GX` Guide resolved
  against targets A and B, expected coordinates, scoped JSON semantics, and atomic
  disabled/suppressed/wrong-scope failures
- `tests/inherited_grid_alignment_tests.cpp` covers target-local inherited Grid IDs,
  fixed Align/Distribute bounds, preservation of absent local layout/source state, and
  `MISSING_GRID` for null suppression

The harness explicitly saves the initial fixture to an isolated working destination
before extended mutations, preserving the original source file. It then settles that
working file before atomic-refusal checks, performs a final Save As to a distinct
destination, and verifies exact saved bytes plus fresh-process cold reopen/readback.
The existing MCP Guide update/override lifecycle remains the pointer-free API
contract; UI guide arm/preview/cancel is validated separately.

## Validation boundary

Static Python compilation and standalone MCP metadata/initialize/discovery checks may
run without the desktop socket. The full desktop-owned socket parity test requires the
desktop runtime. In environments where that local-server startup is blocked, record
the runtime result as `NOT_RUN`; static checks are not a substitute for the Windows
candidate run. After integration, run the focused MCP parity test and relevant full
Windows verification against one exact candidate.

For this isolated packet, Python compilation, standalone MCP metadata checks, fixture
creation, and direct core CLI/JSON command-oracle canaries passed. The core CLI canary
verified the fixed Guide/Grid coordinates, duplicate IDs/index/placement/overrides,
Undo/Redo, and atomic refusal cases. It did not exercise the formal MCP stdio-to-
desktop-Host socket path; that Windows wire replay remains pending.

## Combined integration readback

After fan-in, the integrator reran all78 tests:51pass/1skip/26fail, identical failing names to the primitive baseline; no QLayout warning. Python compilation, actual MCP stdio initialize/discovery metadata and native/SVG smoke passed. The new full MCP Template wire test reaches the known desktop local-server startup failure on this Linux environment, so its new assertions remain unexecuted here. Exact Windows replay is still required.

- `repeater-mcp-integrated-tests.log` SHA256 `c5b7567e6c2f1e4a87600827ca0e78c9533d522ee7dee5049aa553d90740c1c4`
- `repeater-mcp-integrated-smoke.log` SHA256 `7cd237678e4001117718f685f4c612cbbbcb217b79273c1ad43af6dfc8986cb4`
- `repeater-mcp-integrated-metadata.log` SHA256 `c5685c4099d5969f17f110252e93f56b827c9d1b56e3b83ffa9f1c85ec9a4c6f`

## Subsequent exact Windows qualification

The previously pending formal wire cases passed on exact12b5863334c891efc302861f28b9285c87b4a174,
tree25b55029d6370de1a5c9ce13542bc9913d5c7a73. The Windows focused MCP Template contract
and full78 both passed this contract; full aggregate77pass/1known registry failure.
No parity test or product repair was required. See `platform-qualification-20261002.md`
for receipt hashes and remaining platform boundaries.
