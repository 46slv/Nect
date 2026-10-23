# Native Recovery failure-injection boundary

PR74 native follow-up, 2026-10-22. Product remains
`68021a7f7cd42401c0455896a4d29fbbb23534c5`; inspected receipt HEAD
`2e4b0d58cc3c60567d435f8fe702934676d667cd` was clean.
EXE10,025,984 bytes, SHA-256
`38e7abe872ea5f2f9d942fe8c7156e8a560e0d5dc9c922ffafadf4138bad3272`
matched its existing sealed artifact. No product change or rebuild.

## Result: native failure case not run

The requested safe native mock-write boundary is absent in this EXE:

- `src/main.js:999` creates module-private `const recovery` and passes the
  imported `writeRecovery` directly to its constructor. The class supports
  dependency injection at construction, but this native instance is not exposed.
- `src/main.js:13558` publishes a copied recovery state/failure summary through
  `window.nect`. Its remaining APIs load/read/dispatch documents; none supplies
  the controller, writer setter or acceptance-only mock adapter.
- `src/storage.js:5` invokes native `write_recovery`. The pinned Tauri2.9.0
  `scripts/core.js` declares `__TAURI_INTERNALS__.invoke` with
  `Object.defineProperty` and only a value, without writable/configurable flags.
  Replacing the actual bridge is not an existing narrow writer-test interface.
- `src-tauri/src/recovery_storage.rs:280` calls the actual Store.write from a
  blocking task. No native mock/failure selector is registered. Its cfg(test)
  failure tests are test binaries, not runtime entry points in this release EXE.
- `src-tauri/src/acceptance_storage.rs` and `main.rs` isolate profile/documents/
  Recovery paths. That opt-in does not change or inject a write function.
- `tests/recovery-failed-context.e2e.js` replaces a web-only IPC fixture before
  application import. This is the successful production-CSP browser acceptance,
  not a substitution available inside the genuine native bridge.

Per the current instruction to stop if a safe injection boundary is absent, no
new native app/profile or port was started. Native A failure/B switch/targeted
retry, stale native Retry rejection and native state/SVG invariance are NOT RUN.
CLI save/readback, finish_close and PID/port exit checks were not performed:
there was no new instance to save or close. No process/close request is pending
or uncertain. This is a missing capability, not an approval denial.

No bridge/property override, debugger-scope rewrite, bundle rewrite, fake native
token, invalid document, full disk, permissions change, native-file replacement
or existing Recovery manipulation was used to force a failure. No new hook or
storage engine was added. Normal documents and Recovery were not read or changed.

A future native acceptance run needs a deliberately scoped way to inject/restore
only this isolated instance's Recovery writer while preserving its real Session,
UI and native bridge. Adding that capability and rebuilding is a separate design/
implementation choice; this follow-up does not implement it automatically.

The product fix and27 focused tests, exact status DOM component, production-CSP
Edge before/after scenarios and D native compile remain the completed evidence;
none was rerun. Native/physical/OS acceptance is not promoted to passed. The
failed context still lives only in the running window until a successful write;
no new crash/exit durability or close-guard behavior is claimed.

Evidence: sibling `recovery-native-boundary-20261022/evidence/boundary.json`
records exact source/dependency hashes, descriptor inspection and stage status.
The earlier46-file EXE artifact remains sealed. This follow-up only updates
PR74 documentation/description. No main merge, release or Library upload.
