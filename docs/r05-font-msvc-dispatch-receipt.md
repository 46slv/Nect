# R05 font authoring — bounded MSVC dispatch repair

Initial isolated candidate on `dot/font-authoring-01`, based on
`4db79c74f02dcc42792b9bbe310889dbbc59eb1c`, tree
`a07427213ad4444b2ba563289c4445dffc19abc1`. No commit, push, main integration,
Windows execution, backend application or REQ-15 completion was claimed for
that initial Linux-only validation. The subsequent Windows replay is below.

## Observed Windows blocker

The authorized Windows canary `01a0fc59-be12-723a-8c1c-b30ca58dc086` tested
`b7c479b1697cb1ab312dfe4aa659e27e9813f2f9`, tree
`d627ec03de28a4d9a5d5899d13828de73950a124`, with VS 2019 x64 MSVC
19.29.30137, SDK 10.0.16299, Release, C++20 and `NECT_DESKTOP=OFF`.
It reported exactly two fatal C1061 block-nesting errors: `src/core.cpp:8340`
(`edited::<lambda_5>`) and `src/history.cpp:388`
(`Session::history_label::<lambda_8>`). No executables linked; all eight selected
tests were NOT_RUN. Build-log SHA-256:
`8cf3dbd0ac08fe6beb66d14ff2b18d234f822e9806abb3e125af18e2c4405a6d`;
report SHA-256:
`68914e6dc1485c44ec7cf6cbda3c60dafed6612ebea6b497d668673ade17107d`.

## Minimal repair

[Microsoft's C1061 documentation](https://learn.microsoft.com/en-us/cpp/error-messages/compiler-errors-1/fatal-error-c1061)
identifies chained else-if scopes as contributors to the compiler's hard
128-level nesting limit. Split core's 85-branch exact-type chain into chains of
36/30/19, and History's 87-branch chain into 29/28/30. This removes four `else`
connections; command predicates, bodies, order, labels and accounting do not
change. Core's predicates cover 109 distinct exact types with no overlap; the
other two Command alternatives retain their preceding dispatch. Every matching
History branch returns, retaining the same final generic fallback.

A source audit verifies identical ordered predicates, all 111 Command
alternatives, and exact full-file equality after reversing only the four splits
and explanatory comments. No header, variant, Window, MCP, schema, DirectWrite,
test-source, build-configuration or native-version edits are included.

## Linux verification, 2026-10-02

Own Release directory: `build-font-authoring`; CMake
`/tmp/nect-d0-tools/cmake/data/bin/cmake`; read-only Boost from
`../nect-d0/build/deps`. Build PASS for `nect`, `core_tests`, `history_tests`,
`text_tests`, `text_authoring_tests`, `font_authoring_contract_tests` and
`font_shaping_tests`.

- PASS: core 50, portable font authoring 52, portable text 3, and History
  budget/gesture subset 34 checks.
- PASS: 33 baseline/candidate JSON-lines responses are exactly equal across
  all five font commands, all three split groups, labels, History counts/bytes,
  invalid-batch atomicity and Undo/Redo. Each binary independently cold-reopens
  the final native document in a separate process with exact authored equality.
- SKIPPED: `font_shaping_contract`, code 77, explicitly unsupported on Linux.
- FAILED / incomplete: full `text_authoring_contract` and `history_contract`
  reach unsupported Windows DirectWrite projection. They are not full passes.
- FAILED / incomplete: unchanged `process_contract` reaches the existing radial
  ornament SVG golden mismatch. Both baseline and candidate complete 281 checks
  before that assertion (the suite prints its earlier 225-check checkpoint).
  Both produce identical SVG SHA-256
  `d26b3bcaa8ce14456cf47080005a5a713a5160728fb5c7f0c44a43cb018715ea`;
  the unchanged golden is
  `007036a75da5de6b23f7b852379d4b33f7dfcc51aa84a6069da8e3c053c6302c`.
- PASS: `git diff --check`.

Logs and the additional read-only parity harness are under
`build-font-authoring/evidence/msvc-dispatch/`: `build.log`, `source-audit.log`,
`font-contracts.log`, `core-history-native-contracts.log`,
`process-comparison.log`, `dispatch-parity.py`, `dispatch-parity.log`, and
`sha256.txt`.

The source/build owner releases this candidate for review and exact Windows
replay. Linux evidence and source reasoning do not establish that MSVC now
compiles, or that guarded DirectWrite code compiles or applies font intent.

## Windows replay and shaping-oracle conversion repair, 2026-10-02

The exact replay of `fdcae91505e646a4c50cdd8908f54afd2924dee1`, tree
`f42dac9e64027cc41913b34a727bed18733518f8`, resolved both C1061 errors and
built core, IO, `nect` and the portable font contract. Seven of eight selected
Windows tests passed: core 50, text 17078, text authoring 2035, portable font 50
(platform-conditional count), History 408, History budget/gesture 34, and
process 223 plus 1154 migration checks. Optional process JSON Schema checks were
NOT_RUN because that package was unavailable; inline checks ran. No executed
test failed or skipped. The eighth test, shaping, was NOT_RUN because its binary
was not produced. Desktop/MCP was not attempted while this gate was incomplete.
Replay report SHA-256:
`DD7489E9719229D050093AB3402B285828EBA5CC8DF69C43E84C616BC453A2AD`.

The next actual compiler error is C2397 at `tests/font_shaping_tests.cpp:55`:
`DWRITE_FONT_SIMULATIONS` implicitly list-initializes the oracle's `UINT32`
simulation-mask field. The bounded repair explicitly casts that one returned
mask to `UINT32`. [Documented simulation flags](https://learn.microsoft.com/en-us/windows/win32/api/dwrite/ne-dwrite-dwrite_font_simulations)
are 0, 1 and 2, with bitwise combinations; all fit without losing bits.
The destination and adjacent index/weight/style types, captured values and test
assertions remain unchanged. Inspection found no second instance of this
enum-to-`UINT32` aggregate-initialization pattern in the test source. Reversing
the single cast yields exact full-file equality with the replayed revision.

Linux rebuilt `font_shaping_tests`, `font_authoring_contract_tests`,
`text_tests`, `core_tests` and `history_tests` successfully. The focused run
passed core 50, portable font 52, portable text 3 and budget/gesture 34 checks;
shaping explicitly SKIPPED with code 77. `git diff --check` passed. Evidence:
`build-font-authoring/evidence/msvc-simulations/{build.log,contracts.log,source-audit.log,sha256.txt}`.

This one-line test-source repair remains uncompiled under Windows until its
next exact replay. The guarded shaping assertions have not run; no feature or
axis effect is established by these results. Source/build ownership is released
for review and replay, without commit, push or integration by this worker.
