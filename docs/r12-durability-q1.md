# R12 early durability qualification (Q1)

Scope: G14 `R12-DURABILITY-FAULT-01`, early architecture qualification on base
`f24e33c4006bda894101d5276e5f68a14e9aeca0`, native **0.77**. This is not Q2 or
whole-REQ-206 verification. The complete matrix must be rerun on the exact final
L1/L2/L3 candidate, including its target-platform evidence.

Authority: [G14](https://app.notion.com/p/3eafd279a6f381d89d30ebaf3c4aeaab),
[confirmed REQ-206](https://app.notion.com/p/3e1fd279a6f381c08ad6cbc39530c384).

## Existing owner and safety boundary

`store_native` remains the only native replacement path. An explicitly enabled,
OFF-by-default `NECT_DURABILITY_FAULT_TESTS` CMake option, available only with
`BUILD_TESTING` and `NECT_DESKTOP`, compiles a scoped thread-local callback into
that writer. Ordinary production compilation has no callback API, state, or
calls. There is no environment variable, application setting, CLI switch in
Nect itself, alternate storage owner, or core/schema change.

The callback observes canonical destination and six checkpoints: before open,
before write, after write, before commit, after commit, and before readback.
Permission/capacity refusal returns explicit `IO_ERROR` with its injected class.
Short-write injection writes exactly half the small native fixture to the real
QSaveFile temporary staging file, cancels it, and reports written/expected bytes.
A post-commit readback refusal produces the existing `IO_VERIFY_FAILED` uncertain
outcome. No real disk is filled and no ACL, permission, system setting, production
file, or existing recovery root is changed.

Hooks are installed inside the actual `ProtectionWriter` callback, including
its worker thread. Nested scopes restore their predecessor and unrelated threads
do not inherit them. Hit/phase/PID checks prevent vacuous negative passes.

## Acceptance families

- Six permission/no-space/short-write cases use actual asynchronous Host persistence
  and its one-second cadence. Start with ten native generations; all existing
  generations and the last-good authoritative hash survive refusal. A failed
  attempt may add the current baseline as an eleventh generation, which retries
  deduplicate. Pruning resumes only after verified replacement. Saved revision
  remains 0 while committed/recovery revision is 1. Active preview 999 is excluded
  from persisted committed value 42. Explicit Save refusal, Save As, recovery
  provenance rebinding, and normal retry are checked.
- Owned children commit revision 1 and report pending 1, then writing 1 / saved 0
  at four actual-writer gates: before open, after write, before commit, after
  commit. Only the QProcess handle created by that test is terminated and reaped.
  The authoritative native is exact old-valid bytes before commit or exact
  new-valid bytes after commit. A fresh process inspects native, recovery receipt,
  hashes, revisions and source identity before choosing the valid recovery.
  Recovery Save As preserves both source candidates; a later native retry reclaims
  the killed writer's stale cooperative lock.
- Corrupt only a copied backup. Its malformed native is rejected before changing
  the current Host/Session/binding/receipt. A separately retained valid generation
  restores as an unnamed document, excludes live gesture preview, preserves
  document/composition/artboard/object/contour/point/dependency identities, and can
  be saved to a new destination without changing the original or any generation.

The on-disk recovery receipt may already say revision 1 while the blocked live
Host still reports recovery 0: the worker has not returned its combined result.
Neither is mislabeled native saved. Reopening creates a new Session at revision 0;
the selected recovery receipt preserves the predecessor's revision and source.

## Reproduction and evidence

Configure an isolated build with the repository's documented Qt/Boost dependencies,
`-DNECT_DESKTOP=ON -DBUILD_TESTING=ON -DNECT_DURABILITY_FAULT_TESTS=ON
-DCMAKE_BUILD_TYPE=Release`, then build `durability_fault_tests`, `storage_tests`,
`protection_tests`, and `live_save_tests` with one job. Run
`ctest --test-dir <build> -R '^(durability_fault|storage|protection|live_save)_contract$'
--output-on-failure`. The new test prints JSON-lines receipts with source base,
build/toolchain/native version, scratch root, SHA-256s, fault/phase, saved/live/
recovery/pending/writing revisions, errors, selected provenance, generations and
cleanup. Record the exact modified-source manifest beside the base SHA.

Local build logs and machine-readable receipts are under ignored
`build-durability-q1/evidence/`; the separate production-default configure and
symbol checks are under ignored `build-durability-default/evidence/`.
The qualification receipt below binds these results to the exact source and build.

## Limits

- Linux process-crash qualification is not Windows sharing/ACL evidence, physical
  power-loss testing, filesystem fault injection, hardware durability, or an
  fsync/power-loss guarantee. The after-write hook makes no flush/fsync claim;
  QSaveFile owns commit/rename internals.
- Deterministic error classes exercise application failure handling at the actual
  replacement boundary, not the operating system's true ENOSPC or permission
  implementation. Fixture disk use is small and bounded.
- The crash fixture measures a one-revision committed-versus-native-acknowledged
  gap at the reported configured cadence. It is not a global loss bound or a
  guarantee about external linked source history.
- All fixtures are geometry-only, with owned temporary recovery roots. No
  DirectWrite/font, GUI, subjective, release, or whole-product claim is made.


## Qualified receipt — 2026-10-02, own-cloud Linux

Environment: Linux 6.18.44, GNU C++ 14.2.0, CMake 4.4.3, Qt 6.5.3,
Boost 1.85.0, Release, one build job. Native version remains 0.77.
The tested candidate is the base SHA above plus the following exact source patch,
not an implied clean HEAD or a final product candidate:

- Code patch SHA-256: `517ce5d540d6ce87f0b1ae6ee02b0d4803f622ba867e7a1592621309dcbd7356`
- New durability executable SHA-256: `f854c9fd0e909e3c4bbf01b9f10c1230b4279a1e593bcb8a56b42cbe473e2ce7`
- `durability_fault_contract`: **PASS, 848 checks**, then three additional
  CTest passes (11.30 / 10.99 / 11.00 seconds)
- Existing `storage_contract`: **PASS, 71 checks**; existing
  `protection_contract`: **PASS, 180 checks**
- Native/SVG CLI smoke: **PASS**, including fresh-process native validation and
  SVG anchor checks; the smoke makes no MCP or GUI claim
- Existing `live_save_contract`: **NOT PASSED on Linux**. It reaches the existing
  Text projection requirement for Windows DirectWrite and fails there. No test
  was removed, skipped, weakened, or reclassified as a pass
- Separate default-OFF Release build: **production `nect_desktop` target builds**;
  existing storage/protection contracts pass **2/2** (1.60 / 0.06 seconds).
  `NECT_DURABILITY_FAULT_TESTS=OFF` in its cache, no durability test is registered,
  and compiled storage object plus complete desktop executable have **no hook
  symbols**. Desktop SHA-256:
  `5e8852d14d7996433ad891e6ecde8b171175607a72a0442b877f3641e05ec764`
- Read-only independent review checked exact source/binary manifests and every
  JSON receipt family, including phase hits, revision distinctions, cleanup and
  restored provenance. No production storage semantic failure was found

All six injected refusal cases preserve native SHA-256
`41ee89a240851e77174c1c04ef5e485abe3027c0423e8c271c1097f8c70aaa10`,
with saved revision 0 and independently protected revision 1. They preserve all
existing generations, deduplicate failed retries, permit Save As while the source
fault remains active, and permit same-bound-Host Save retry after removing that
fault (with the same scoped callback still installed).

For the four process-crash cases, old-native SHA-256 is
`edf51c57ec176be214edfd66aeb19ac67d8c259aa1acd615afb2c99c044a44ae`;
new committed/recovery SHA-256 is
`d0c91ad69d4fd9a427e07523c972abf872d3ae7a5f9ec8cc978d1134d7fa3d44`.
Before-open, after-write and before-commit interruption leave the former;
after-commit interruption leaves the latter. All gates report committed/writing 1,
saved 0. The independently verified on-disk recovery receipt is revision 1 while
the blocked Host still reports recovery 0. The initial complete run observed
958–978 ms from the edit to gate under the configured 1000 ms coarse timer, with
maximum observed committed-to-acknowledged-native gap **one revision**. This is
fixture measurement, not a worst-case production bound.

Corrupt copied backup rejects with `INVALID_JSON`. Another retained generation
restores with SHA-256
`11253c3a8e606d3e9459f63e8502d62b63e5be464ef2d464f115c76ca6adf392`,
while original native SHA-256
`844583fdc74e9449a35fb5621af748eeea3f9c7a5382de4ff5b5da2717aa2247`
and both generations remain unchanged. Every successful run reports all child
processes reaped and explicit scratch removal; cleanup failure fails the test.

### Exact tested source manifest

| File | SHA-256 |
|---|---|
| `CMakeLists.txt` | `340092a67456a03d7648efb96b04bff886251de0ce8a9f51bbe2000374c10fac` |
| `src/desktop/storage.hpp` | `a0abc49880979e90c726edd85e96290d6bb2748785f3b59d638411f1ba4fedb7` |
| `src/desktop/storage.cpp` | `1de86f7203cd1f6b3933baec17ad92c4854fa09a3f0324a6e51a496d6dafad52` |
| `tests/storage_tests.cpp` | `13bf3ae819102d302078bd172746a1097c55f8a4597f2f9bf1621a460f2f7242` |
| `tests/durability_fault_tests.cpp` | `0efa62a3d22df8b439da0a61ded02dee1927eaef9c961a1bc59f1199c67a4405` |

### Preserved red evidence and oracle repair

The test was authored first and failed to compile without the new hook API.
The initial local Qt configure/link also failed because the GLVND hints did not
provide `OpenGL::GL`; the isolated build was corrected to the existing system
`libGL.so.1` and read-only GL development includes. No dependency or main-worktree
content was changed.

The first runtime failed only at the after-commit expected-byte oracle. The
parent incorrectly reconstructed a new fixture at value 42: CreatePath initializes
its authored anchor to 66.5. The actual child correctly edited the baseline with
Set, preserving its authored anchor 5.0. An independent small diagnostic proved
that sole JSON difference; the corrected parent independently replays the same
canonical command on the same baseline *before spawning the child*. The child's
committed hash is checked before termination, and actual/expected hashes are
recorded before equality assertions. The expected bytes are never read from the
file under test. Independent review confirmed this is an oracle repair, not a
storage change or weakening. The failed run and diagnostic remain in the ignored
evidence directory.

Final evidence: `durability-second.jsonl` (empty matching stderr),
`durability-repeat.log`, `storage-protection-verbose.log`,
`existing-contracts.log` (includes the DirectWrite non-pass), `smoke.log`,
`source-manifest.sha256`, and `build-manifest.sha256` under the Q1 evidence root;
`ctest.log`, `desktop-build.log`, `desktop-hook-symbols.txt` (empty), and the
separate `build-manifest.sha256` under the default-OFF evidence root.
