# Recovery native acceptance build

User-selected bounded continuation of PR74, base71be9026ae1365e1b07fe33672d68dc52bb01b66.
No generic runtime injection API or new native command. The single fixed
synthetic A/B scenario runs automatically only in an explicit test build.

Normal Vite mode defines the test branch false, emits only normal assets and
no test module/result/fault strings. Explicit recovery-acceptance-test mode
emits separate test assets. Cargo feature of the same name is not default.
Build.rs verifies a generated frontend profile marker against the native
feature, refusing mixed normal/test assets. Existing default frontendDist stays
unchanged. TAURI_CONFIG selects the test assets at build time only.

The feature requires the existing fresh acceptance root before Tauri startup.
After the existing path/identity checks the native test window receives a
read-only instance marker. No ordinary profile can enter the test scenario.
The JS module requires that marker, native bridge and fresh unbound revision0.
It wraps only this controller's writer, targets the synthetic A native token,
uses two bounded simulated failures, passes B and retry writes to the original
native writer, and restores that writer in finally. A read-only result getter
is the only test surface; no callable commands, paths or failure parameters.

The fixed scenario covers A r1 failure, newer A r2 failure across B switch,
stale failure revision/generation rejection, UI target selection, real native
retry success, and exact B project/file/history/SVG invariance. CLI backup and
one finish_close follow from the external verifier. Only newly created test
profiles/stores are inspected; no old Recovery, normal document or OS dialog.
Normal/test EXEs are separately named and hashed. The test build adds a guard,
marker, wrapper and automatic fixture, so it is not claimed byte/performance
identical to normal. The production writer/Session/renderer are unchanged.

## Build and isolated readback

Build the test frontend with `npx vite build --mode recovery-acceptance-test`,
then compile Cargo with `--release --locked --offline --features
custom-protocol,recovery-acceptance-test` and build-time `TAURI_CONFIG` selecting
`{"build":{"frontendDist":"../dist-recovery-acceptance-test"}}`. Normal builds
use the normal Vite output and `custom-protocol` only. Keep their executables
separately named and hashed. The test executable requires a new direct-child
`nect-svg-acceptance-*` profile under the existing `NECT_SVG_ACCEPTANCE_ROOT`
guard; never point it at an ordinary profile. That root opt-in does not enable
injection in a normal build. There is no general runtime failure selector.

For a fresh owned launch recorded by the acceptance launcher, run
`node scripts/verify-recovery-native-test.mjs <absolute-stage> normal|test` once.
Do not repeat against an existing stage. After identity-specific process,
browser-child and port exit proof, run
`node scripts/verify-recovery-native-store.mjs <absolute-test-stage>`. This reads
only the run's current A, previous A and current B records, compares projects and
native-success timestamps, and creates a non-overwriting readback receipt.

The store verifier rejects missing/mismatched launch and exit identities, failed
or unclosed runtime receipts, unexpected records/tokens, paths outside the stage
and symlink redirects in the checked paths. These are correlation and realpath
checks over an owned, quiescent stage, not protection against concurrent hostile
filesystem mutation. It validates the supplied exit evidence; it does not itself
observe or terminate processes. Do not use another run's receipts as proof.

## Selective follow-on verification boundary

The test-only live-target fix and external descriptor/store verifiers were
selectively ported from PR78 head
`2d07ad068e6438453b74b9170f0c9239d00a22c7`; its diverged history and private
operational evidence were not transplanted. Production writer, main module,
Rust host and build configuration are unchanged by this follow-on.

Local Node fixtures cover live DOM replacement, bounded failure, readonly test
surface assertions, malformed evidence, stage containment, symlink redirects,
record/timestamp equality and non-overwriting receipts. Normal and explicit test
frontend builds were checked for profile markers and test-string exclusion.
These fixture/build results are not a new native acceptance result. The earlier
native pass belongs to compiled source
`83116e903215c010179f2a207fa5bbff51ca6709`, using fixed synthetic DOM input and
simulated A write failures. Native compilation/binary exclusion, native runtime,
physical input, OS dialogs, cold reopen and real disk failure were not reverified
for this combined tree.
