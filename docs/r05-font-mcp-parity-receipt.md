# R05 font authoring — formal MCP discovery/parity candidate

Isolated, unintegrated candidate based on
`b7c479b1697cb1ab312dfe4aa659e27e9813f2f9`, tree
`d627ec03de28a4d9a5d5899d13828de73950a124`, branch
`dot/font-authoring-01`. Native remains 0.78. No commit/push, main integration,
Windows operation, GUI acceptance or REQ-15 closure is claimed here.

## Bounded changes

- `scripts/mcp_server.py`: document all five typed font commands, exact tag and
  uint32/double domains, `wght`/`ital` ownership, unchanged-collection
  `update_text` restriction, native 0.78 and authored/request/actual-run
  distinctions. Expose the existing `text_defaults`, `text_fonts`, and
  `text_layout` requests in capability enumeration. Dispatch remains unchanged.
- `tests/font_mcp_parity.py`: actual stdio MCP initialization, tools and resources;
  live desktop-owned API/MCP replay parity for feature add/update/remove and axis
  set/replace/remove; ordered/padded tags, max uint32 and precise finite doubles;
  preserved source fields/drivers; rejected invalid, reserved, missing, non-Text,
  stale-revision and stale-session actions; unchanged state/history on rejection;
  atomic failed batch; Undo/Redo, Save As and separate desktop-process native
  reopen. A pre-authored native fixture avoids pretending Linux CreateText can
  shape. Linux font discovery/layout must explicitly refuse with
  `TEXT_PLATFORM_UNSUPPORTED`; Windows receipt transport alone claims no effect.
- `CMakeLists.txt`: `font_mcp_discovery` (15-second limit) and desktop-only
  `font_mcp_parity` (60-second limit). No host/core/backend/schema edits.

## Verification on Linux, 2026-10-02

Own Release build: `build-font-mcp`; CMake
`/tmp/nect-d0-tools/cmake/data/bin/cmake`; read-only Boost/Qt from
`../nect-d0/build/deps`. Initial OpenGL discovery failed; configuring with the
existing GL headers and system `libGL.so.1`, `libOpenGL.so.0`, `libGLX.so.0`
resolved it. No dependency installation or shared build reconfiguration occurred.

- Build PASS: `nect_desktop`, `font_authoring_contract_tests`, `text_tests`,
  `font_shaping_tests`.
- Python syntax and `git diff --check`: PASS.
- Actual MCP discovery: PASS, 36 assertions, including advertised font commands,
  three request operations and explicit unsupported status.
- `font_authoring_portable_contract`: PASS, 52 checks.
- `text_contract`: PASS, 3 portable checks; its existing DirectWrite-labelled
  banner is not Windows execution evidence.
- `font_shaping_contract`: SKIPPED, code 77, as expected on Linux.
- Native test-fixture parser/normalization: PASS using existing core binary;
  this is fixture validation, not live MCP mutation proof.
- Full `font_mcp_parity`: **NOT_RUN past desktop startup**. Both ordinary and
  approved escalated attempts stopped before readiness at
  `QLocalServer::listen: Unknown error 1`. A direct socket-creation probe produced
  `PermissionError: [Errno 1] Operation not permitted` for `AF_UNIX`.
  Consequently live mutation parity, Qt-bridge numeric preservation, live
  Undo/Redo and desktop cold reopen are prepared assertions, not proven results.
  Transport assertions remain strict; no fake host or alternate transport was
  substituted. Resume on an authorized host that permits its local Session IPC.

Logs under `build-font-mcp/evidence/`: `configure-final.log`,
`configure-registration.log`, `build.log`, `contracts-final.log`,
`fixture-validation.log`, `live-run.log`, `live-escalated.log`,
`live-registered.log`, `source-checks.log`, and `sha256.txt`.

Source and isolated build ownership are released for integrator review. This
receipt does not replace the separate Windows shaping or later GUI/G11 gates.
