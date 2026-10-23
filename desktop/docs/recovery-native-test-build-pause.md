# Recovery native test build: archived checkpoint

This is a historical checkpoint from 2026-10-22, not the current execution or
verification status. No native GUI or acceptance profile had been launched at
this checkpoint. Later source changes and native evidence must be assessed
separately.

## Historical implementation and checks

- Branch: `codex/recovery-native-test-build`.
- Source: `b4591173ca9064a1ed24b521c64534e8e0dc9d3c`.
- PR74 base: `71be9026ae1365e1b07fe33672d68dc52bb01b66`.
- Implemented compile-time Cargo/Vite gates, a fixed one-shot A/B scenario with
  copied readonly results and finally-restored writer, external native verifier
  and build-exclusion verifier. No runtime failure endpoint or new IPC command.
- Normal/test frontend builds and both invalid frontend/native pairings were
  verified. The 2 focused Node tests, 4 Rust profile tests and 2 Rust isolation
  tests passed; the earlier 27 checks were not rerun.
- An initial Rust declaration-order compile error was corrected. Rust formatting
  was not run because the formatter was unavailable in that environment.
- A normal native build receipt existed. The test native build and final
  exclusion receipts were not yet present. This is not a claim that either is
  still pending today.

## Generic artifact/resume guidance

Keep evidence in an owned artifact directory rather than recording private host
paths or process/session identifiers in public documentation. Inspect existing
receipts, build outputs and current process state before repeating work with an
uncertain outcome. Reconcile normal/test executable hashes and exclusion checks
before any fresh native launch.

Use a new isolated stage for each authorized launch. The native verifier must
back up readable fixtures and confirm writer restoration before one normal close
request. Collect identity-specific process, browser-child and port exit evidence
before reading only that run's exact current A, previous A and current B records.
Keep synthetic input and simulated write failure distinct from physical input,
OS dialogs, cold reopen and real disk failure. See the current generic build and
readback contract in [Recovery native acceptance build](recovery-native-test-build.md).
