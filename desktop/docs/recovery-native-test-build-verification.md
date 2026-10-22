# Recovery native acceptance verification

Completed 2026-10-22 on shiro-WS, continuing the explicitly authorized PR74
acceptance scope. Branch `codex/recovery-native-test-build` is based on
`codex/recovery-failed-document-retry` at
`71be9026ae1365e1b07fe33672d68dc52bb01b66`. It does not include or modify the
separate PR77 integration lane. The underlying Recovery writer, Session,
controller and renderer are unchanged in this branch.

## Result

The genuine Windows Tauri/WebView2 runtime passed the fixed test-build scenario:

- A baseline revision 0 wrote through the real native writer. Only synthetic A
  revisions 1 and 2 failed through the temporary, instance-local writer wrapper.
- A revision 2 completed its simulated failure after switching to B. B wrote
  successfully through the real writer in the same new isolated store.
- The visible failure control identified A revision 2 while B remained current.
  DOM selection and click retried that exact target; A revision 2 then wrote
  natively exactly once. Old revision, old generation and resolved-target
  requests were rejected without another write.
- B's project, file state, command identity, Undo/Redo availability and SVG were
  exactly equal before and after retry. Native SVG matched the shared exporter.
- The original writer was restored in `finally`. CLI fixture backups were read
  back exactly before one normal `finish_close` request. No OS dialogs opened.
- After exit, only the three exact new native records were read: current A equals
  the latest A project, previous A equals its baseline, and B equals the original
  B project. Their timestamps match successful native writes.

Normal runtime exclusion also passed using the separately compiled normal EXE:
no native marker, result getter or test scenario was present; its initial
revision remained 0. Normal/test JavaScript assets and native binaries passed the
exclusion scan. Both mismatched frontend/native build pairings were rejected
before creating a window. No new native command was registered.

## Executables and source

| Build | Compiled source | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| Normal | `b4591173ca9064a1ed24b521c64534e8e0dc9d3c` | 10025984 | `cbc21c4f661b478ba3812319cc20a96a76f63d822c679d92d83847fe07258154` |
| Explicit test v2 | `83116e903215c010179f2a207fa5bbff51ca6709` | 10036224 | `f2a6b606c30bbc529747ab6cd852e7e8e44e9caf9fc9e45b57e63cd646b49b9d` |

The resumed run did not rebuild the completed normal candidate. The only later
compiled-source change resolves live controls in the excluded test module;
normal compiled sources and frontend hashes were unchanged. Test v2 alone was
rebuilt after the concrete harness failure below. Later commits contain verifier
and receipt changes, not a different compiled product.

## Preserved failure and correction

The first test candidate (`69309eac1b85a686657d27e0b08a45ed4810abfc4dafa39497439d8d0843c155`)
stopped at `Retry UI not visible`; its writer was restored, current fixture was
backed up and its owned process exited normally. Its failure evidence is retained.
The corrected test harness re-resolves the current details/select/button after
startup rendering instead of retaining a detached element across animation
frames. The successful native result records the first captured element as
replaced/disconnected, then the current control as connected/open. Existing
visibility and exact target assertions remain. No product UI assertion was
removed and no production UI code was changed.

The external verifier attached after the fixed four-second failure display
interval, so the successful run has captured failure HTML, identity and positive
control bounds, but no failure-stage screenshot. Its final native screenshot was
visually inspected. This is not claimed as screenshot evidence of the failure
menu or a physical click.

## Focused checks and shutdown

Previously completed checks were collected, not rerun as a full audit: 2 Node
build/scenario tests, 4 Rust frontend-profile tests and 2 Rust isolation tests;
normal/test frontend builds, native configuration check, two negative Cargo
pairings and both initial native builds passed. The test-only live-control
change passed the same 2 focused Node checks and the necessary test build.
PR74's existing 27 tests and CSP browser acceptance were not repeated.

Normal PID 101208 (start 19:03:07.0378371 UTC), failed initial test PID 125664
(start 19:04:27.3200768 UTC), and successful test PID 99080 (start
19:19:56.4359606 UTC) each received one normal close after readable backups.
Their exact executable/start/profile identities were recorded first. Owned
processes, recorded WebView browser PIDs, direct children and listening ports
9495/9497 were absent after exit. No force termination or repeated close was used.

## Evidence and boundaries

Work directory:
`D:/Nect-codex-output-20261022-task13-f336c94f2b714cd5bc73d6ca646c8f2e/recovery-native-test-build-20261022`.
The successful stage is `test-native-v2`; use its `native-cases/scenario.json`,
`receipt.json`, `failure-ui.html`, `final-runtime.png`, and
`evidence/native-store-readback.json` / `exit.json`. Normal exclusion evidence is
under `normal-native`; the failed first scenario remains under `test-native`.
Build and exclusion receipts are under the work directory's `evidence` folder.
The sealed sibling is `recovery-native-acceptance-artifact-83116e9`.

This proves native runtime behavior with fixed DOM synthetic input and simulated
write rejection, not physical/OS input or a real disk failure. CLI save/readback
is not ordinary OS Save/Open or cold reopen. No OS dialog, normal document,
existing Recovery, full disk, permission change, bridge override or security
change was used. The test feature adds its own gate/marker/fixture/wrapper; byte
or performance equivalence to the normal executable is not claimed. The normal
executable excludes that module and API surface. Failed contexts remain in-memory
until a successful write; no new crash/exit durability is introduced.

PR77 integration must separately reconcile this branch's small main.js footer,
Cargo build/profile gate and Vite configuration. No merge, release or Library
upload was performed. Both previous candidates and test evidence are preserved.
