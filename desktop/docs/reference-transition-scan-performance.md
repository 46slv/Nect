# Dense reference-transition scan

## Measured obligation and scope

Current production source still takes over a second to edit the terminal point
of a 10,000-point Curve whose X component is referenced by another Composition.
The adopted Mission requires measured responsiveness. This packet removes one
demonstrated allocation/parser hot path; it does not claim complete interactive
performance or native acceptance.

Application baseline: main 04628a4f7185ed26490b5e3ee14b0c5a94b704bb, identical
desktop source under documentation main bb134c063f83739d53ea53e10dd32620321a62ff.
The old graphParameter index improvement is already included and is not repeated.

## Change and protected semantics

Only allRefs inside assertArtworkReferenceTransition changes. It still visits
every layer, node and parameter descriptor in the same order, including Curve
points and Remap edits. It collects each parser's references in the same order,
preserving duplicates and schema 64/65 gates. Both previous and candidate
snapshots are scanned. Exact empty strings yield no references, so they no longer
invoke four parsers or allocate several intermediate arrays per descriptor.
Whitespace and every other nonempty string retain the parser path.

No persistent cache, validation omission, graph/identity rule, schema, expression
language, Session envelope, effective-value check or commit/history path changes.
The existing missing-reference retarget guards remain after collection. The
independent review found no supported-data difference and passed 136 differential
collector cases across schemas 1/59-65, mixed qualifiers, empty/whitespace and
malformed text, duplicates, multiple layers/nodes, Curve and Remap descriptors.

## Reproducible measurement

Run from desktop:

node scripts/bench-cross-composition-edit.mjs

This creates synthetic stable-ID fixtures of 100, 1,000 and 10,000 points. A Scalar
in the target Composition references the source Curve's last point X component.
It measures five synchronous common Session dispatches per fixture. Correctness
checks are outside timing: live value, one-step Undo/Redo values, complete project
serialization equality after Redo and reopened value. No timing threshold is a CI gate.

On shiro-WS, Node 24.14.1, win32/x64, three sequential baseline/candidate process
pairs provided 15 samples per side per size. No owned build, browser or test
workload ran concurrently with these final pairs. Earlier exploratory pairs
1-3 are retained but excluded because pair 1 overlapped local tests/build.

Pooled medians:

- 100 points: 19.94 ms before, 15.54 ms after; small-scene measurements are noisy.
- 1,000 points: 157.64 ms before, 124.15 ms after.
- 10,000 points: 1,609.07 ms before, 1,333.92 ms after, about 17.1% lower.
- 10,000-point ranges: 1,476.43-1,794.62 ms before,
  1,289.71-1,490.19 ms after.
- Individual 10,000-point pair medians were 1,650.04 -> 1,296.22 ms,
  1,609.07 -> 1,368.79 ms and 1,535.73 -> 1,323.24 ms.

These are measurements, not a universal guarantee. They exclude Canvas rendering,
DOM interaction, displayed-frame timing and native WebView2. Setup, switching,
serialization and reopening remain outside the measured dispatch boundary.

A separate identical-fixture CPU profile covering three edits attributed about
801.7 ms to the old collector and 20.7 ms to the new collector. Remaining
foreignAffectedVertices cost was about 985.7 ms in that profile. Full validation
and project candidate work still dominate; 10,000-point editing remains slow.

Ignored evidence:

- prior checkout desktop/test-results/cross-comp-audit-receipt.json and
  cross-comp-profile.json contain the baseline audit;
- this checkout desktop/test-results/reference-scan-performance-receipt.json,
  reference-bench-after-4/5/6.json and cross-comp-profile.json contain final
  comparison/profile evidence;
- baseline raw reference-bench-before-4/5/6.json lives in the prior checkout.
  Both checkouts and the original histories remain preserved.

## Verification

The five new density tests cover the empty-heavy fixture and bounded measurement,
terminal-point liveness/history/reopen, invalid inactive dependent rejection with
project/state/Redo unchanged, and dangling-point identity reuse refusal plus Undo.
The original reference, schema opacity, clipboard, Color, mixed-cycle, Boolean
reactivation and connected/inactive tests remain in place.

Ten focused files passed 201 tests with zero failures/skips/cancellations.
Production build and native-CSP startup/eval blocking passed. Browser checks and
exact candidate CI are recorded with the publication receipt; local focused
success does not substitute for those checks.

The release uses fixed panel positions; no panel/docking code changes. Native
WebView2/physical IME and subjective acceptance remain separate LOCAL_WAIT.
This optimization completes only this bounded hot-path reduction, not the
performance Mission or full Nect product.

## Issue #222 Stage 0: synchronous dispatch attribution (2026-11-08 UTC)

This is a bounded measurement, not an optimization or a GUI/Worker experiment.
Exact base and freshly checked remote main:
`8281ae7ea992b8824e03eeb2b5abe2c66f30008e`.
The existing benchmark and its semantic assertions were reused unchanged on disk.
The probe in [profile-reference-stage0.mjs](../scripts/profile-reference-stage0.mjs)
transforms three modules only in memory through Node load hooks. No product
source, Session ownership, validation path or history behavior was changed.

### Conditions and reproducibility

- shiro-WS, Windows x64, Node v24.14.1. The raw receipt records the CPU model,
  timestamp and SHA-256 of each original module.
- Isolated detached worktree at the exact base. Primary checkout was clean.
  The parent confirmed the former writer stopped/released; no matching benchmark
  process or competing Stage 0 worktree was found before entry. No shared GUI
  or native app was launched or controlled.
- One instrumented process, then one uninstrumented control process, each with
  exactly 5 dispatches at 100, 1,000 and 10,000 points. No repeated timing loop.
  Owned focused tests ran before/after, not during either measurement process.
  Unrelated machine activity was not controlled, so this is not a quiet-host
  performance guarantee.
- Setup, expression binding, Composition switches, value checks, Undo/Redo,
  serialization equality and reopened-value checks stay outside the timed
  dispatch. All logging and report serialization are also outside timing.
- Timer/wrapper overhead remains in the instrumented samples. This single
  sequential control pair cannot distinguish instrumentation overhead from
  process warmup, GC or machine noise. No warmup dispatch was added.

From `desktop`:

```sh
node scripts/profile-reference-stage0.mjs > stage0.json
node scripts/bench-cross-composition-edit.mjs > stage0-control.json
node --import ./scripts/profile-reference-stage0.mjs --test --test-concurrency=1 tests/reference-transition-density.test.js tests/artwork-parameter-reference.test.js tests/artwork-parameter-reference-regressions.test.js tests/composition-property-reference.test.js tests/composition-reference-owner.test.js tests/cross-composition-vector-reference.test.js tests/cross-composition-vector-review-regressions.test.js tests/qualified-color-reference.test.js tests/artwork-reference-clipboard.test.js tests/curve-point-reference.test.js
```

The load-hook probe requires a Node runtime supporting `module.registerHooks`;
this receipt used Node 24.14.1. Anchors fail closed if source structure drifts.
The source hashes and exact base are part of the receipt, not a promise that
future main gives the same measurement.

### Results

| Points | Instrumented median | Uninstrumented control median |        Control range |
| ------ | ------------------: | ----------------------------: | -------------------: |
| 100    |            17.30 ms |                      14.71 ms |       13.18–16.98 ms |
| 1,000  |           110.31 ms |                     110.86 ms |     104.77–119.30 ms |
| 10,000 |         1,348.15 ms |                   1,316.15 ms | 1,275.68–1,438.25 ms |

Selected stage medians in the instrumented run:

| Stage                                             | 100 points inclusive/exclusive ms | 1,000 points inclusive/exclusive ms | 10,000 points inclusive/exclusive ms |
| ------------------------------------------------- | --------------------------------: | ----------------------------------: | -----------------------------------: |
| cloneDocument, 9 calls/dispatch                   |                       4.58 / 4.58 |                       40.17 / 40.17 |                      463.31 / 463.31 |
| Foreign graph enumeration and reverse-index build |                       3.03 / 2.99 |                       22.65 / 22.63 |                      314.48 / 314.45 |
| Project candidate, 2 calls/dispatch               |                       2.89 / 2.89 |                       15.40 / 15.40 |                      162.78 / 162.78 |
| Changed-node detection                            |                       0.23 / 0.23 |                         2.41 / 2.41 |                        29.89 / 29.89 |
| Changed-vertex seeding                            |                       0.43 / 0.43 |                         5.17 / 5.17 |                        59.56 / 59.56 |
| foreignAffectedVertices total                     |                       3.77 / 0.02 |                        29.65 / 0.06 |                        400.25 / 0.13 |
| Effective projection/evaluation                   |                       1.57 / 0.79 |                         7.48 / 1.79 |                        89.89 / 19.89 |
| Effective domain validation                       |                       0.49 / 0.02 |                         2.09 / 0.02 |                         27.63 / 0.02 |
| validate, 7 calls/dispatch                        |                       1.85 / 1.85 |                         6.69 / 6.69 |                        89.81 / 89.81 |
| Session command candidate                         |                       1.27 / 0.13 |                         8.01 / 0.47 |                         93.37 / 4.97 |

**Inclusive includes all nested measured stages. Exclusive subtracts only nested
instrumented spans; it is not V8 CPU self-time.** Uninstrumented callees remain
in their containing stage. Project candidate therefore includes its underlying
project validation/copy routines. cloneDocument aggregates calls across several
parents, including candidate construction and effective projection. Do not add
inclusive rows together, or add their independently computed medians.

At 10,000 points the three reverse traversals are respectively 0.011 ms
(foreign closure), 0.014 ms (reachable closure) and 0.012 ms (affected closure).
Effective-input activation is 0.032 ms in this fixture. This does not establish
their cost for a graph with many foreign edges or dynamic Boolean activation.

The `documentDependencyGraph` constructor itself totals only 0.034 ms across
3 calls/dispatch. Its returned graph is lazy: full vertex enumeration, parameter
indexing, dependency edges and reverse-index assembly execute later in the
314.48 ms stage. Timing only the constructor would misidentify the hotspot.

Using **sums of exclusive times divided by the sum of dispatch times** across
all five 10k samples, rather than ratios of independent medians:

- cloneDocument: 34.26%.
- Foreign graph enumeration/reverse-index build: 23.43%.
- Project candidate: 11.95%.
- Changed-vertex seeding: 4.70%; changed-node detection: 2.08%.
- The three reverse traversals combined: 0.0034%.
- All exclusive stages together account for 99.9975% of the dispatch window.
  This is a nesting/accounting check, not accuracy at sub-microsecond scale.

### Existing evidence reconciliation and next candidate

The saved CPU profile was inspected before adding instrumentation, and its
sample totals were re-derived rather than recollecting that profile.
SHA-256: `e86231ea55ec3a103626cd1fac05f452a3461cf36b7123d3e01cbeb29922581d`.
It covers a different 3-edit run lasting 4,562.57 ms:
`foreignAffectedVertices` inclusive 985.744 ms, `allRefs` 20.712 ms,
`cloneDocument` 1,413.612 ms and project-candidate path 435.590 ms.
These sampled inclusive sums are not directly comparable with this run's
per-dispatch wall-clock medians. They do independently support the same broad
cloning/graph-work cost centers.

**Finding:** cloning/candidate materialization is the largest aggregate cost.
Within foreign-reference processing, all-vertex enumeration and reverse-index
construction dominate; propagation itself does not. A changed-node-only
shortcut targets roughly 2% of this fixture and should not be sold as the
primary repair.

For one bounded follow-on investigation, prioritize the enumeration/index-build
stage and investigate reuse of dependency/index data for the **same immutable
snapshot within one transition**, including candidate acyclicity work. Do not
confuse caching the cheap constructor with avoiding its lazy traversal, and do
not reuse before/candidate data merely because their node IDs match. This is a
candidate to qualify against existing semantics, not an implemented change or
a claim that reuse will remove all 314 ms. Broad clone/history ownership changes
are a separate, higher-risk candidate.

### Verification and limits

The instrumented modules passed the 10 focused files above: **176 tests passed,
zero failures, skips or cancellations**. Their existing cases include dense
terminal-point liveness, Undo/Redo/reopen, inactive invalid dependents and atomic
rejection, missing-point identity reuse, qualified references/Color/clipboard,
mixed cycles, and dynamic/reactivated dependencies. Both timing processes also
passed the original benchmark's semantic assertions at every size.

No product source changed. GUI/native input-to-paint, Worker startup/IPC, new
optimization implementation, full aggregate tests, production build and CI were
not run for this measurement-only packet. No frame-rate, native responsiveness,
broad requirement completion or performance improvement is claimed. Five samples
are descriptive and do not establish p95 or a universal regression threshold.

Raw portable evidence is under [stage0-reference-dispatch-20261108](evidence/stage0-reference-dispatch-20261108/):
instrumented per-dispatch spans/source hashes, uninstrumented control,
historical-profile summary, pooled exclusive accounting and focused-test log.

## Same-snapshot graph reuse trial: not adopted (2026-11-08 UTC)

A follow-on to issue #222 Stage 0 implemented and measured one bounded reuse
candidate. **It did not demonstrate a performance benefit and was removed.**
The source in this checkpoint is byte-for-byte equal to the Stage 0 baseline.
No main merge, release, GUI/Worker work or broader optimization was performed.

### Exact candidate and protected scope

Baseline: Stage 0 checkpoint
`26a3bf394760dae622f7903059c96f0eeff36b03`, with product source identical to
main `8281ae7ea992b8824e03eeb2b5abe2c66f30008e`.
The candidate touched only `desktop/src/core.js` (64 added / 26 removed lines,
including formatting). It retained graph-local parameter enumeration and
optionally memoized dependency edges. The same normalized candidate graph was
passed from project acyclicity validation to foreign-impact validation. Reuse
required exact Document object identity, never node-ID equality or a persistent
cross-dispatch cache; the before snapshot kept a separate graph.

Before entry, live primary/remote main were still 8281ae7e and clean; the Stage 0
writer had released, with no matching benchmark process or graph-reuse worktree.
The trial used a new isolated worktree and branch. The rejected candidate patch,
source SHA-256 pair, full raw samples, runner and focused-test output are preserved
in [graph-snapshot-reuse-20261108](evidence/graph-snapshot-reuse-20261108/).
The patch is evidence, not an enabled feature or a recommendation to apply it.

### Same-condition finite comparison

Windows x64, Node v24.14.1, shiro-WS, the unchanged
`bench-cross-composition-edit.mjs` fixture. Six separate Node processes ran
sequentially in **AB / BA / AB** order. Each process measured exactly 5 dispatches
per size, yielding 15 samples per side per size. No timer instrumentation was
loaded. Owned tests ran before/after, not during the timed comparisons.
Unrelated machine activity remained uncontrolled; no p95 or universal latency
claim follows from this finite sample.

All processes exited successfully, including the benchmark's live-value,
one-step Undo/Redo, serialization equality and reopened-value assertions.
Setup and correctness checks were outside timing as in Stage 0.

| Points | Baseline pooled median ms | Candidate pooled median ms | Observed change |
| ------ | ------------------------: | -------------------------: | --------------: |
| 100    |                     14.58 |                      15.42 |    5.76% slower |
| 1,000  |                    108.33 |                     110.16 |    1.69% slower |
| 10,000 |                   1305.62 |                    1349.21 |    3.34% slower |

The 10k ranges overlap: baseline 1241.05–
1386.50 ms, candidate
1170.75–1541.72 ms.
Individual 10k process-pair medians:

- Pair 1: 1305.62 -> 1339.99 ms.
- Pair 2: 1297.63 -> 1259.37 ms.
- Pair 3: 1308.86 -> 1363.91 ms.

This is evidence of **no demonstrated benefit**, not proof that every use of
same-snapshot graph reuse is intrinsically slower. Extra cache lookup/retention,
allocation and GC are possible offsets, but this trial did not isolate their
causal contributions. Do not select the one faster pair and discard the others.

### Semantic checks, disposition and next step

The candidate passed the same 10 focused reference files used in Stage 0:
**176 passed, zero failures/skips/cancellations**, plus the original benchmark
assertions in all six processes. The preliminary three-file smoke passed 52.
These are focused semantics, not a full aggregate/build/native/CI acceptance.

After preserving the patch and evidence, the trial restored core.js from the
exact baseline and verified its SHA-256 equals the baseline measurement hash.
No trial implementation remains active in the checkpoint.

Next useful investigation: attribute the nine cloneDocument calls and the two
project-candidate materializations by call site and lifetime before proposing
another bounded change. Stage 0 measured those disjoint exclusive buckets at
34.26% and 11.95% of the 10k dispatch window. Do not weaken validation, canonical
Session/history ownership, detached effective-value projection, or atomic
rejection merely to reduce copies. Reworking the full graph traversal or adding
a persistent cache is not authorized by this failed trial's evidence.

## Copy lifetimes and one discarded inner backup (2026-11-08 UTC)

The follow-on decomposed Stage 0's nine cloneDocument calls and two
project-candidate stages. It found one bounded copy that can be removed without
sharing the public snapshot or canonical project History.

### Callers, multiplicity and lifetime

For the unchanged two-Composition dense fixture, the nine calls are:

| Call site                                              | Calls/dispatch | Data lifetime / reason                                                                                                                              |
| ------------------------------------------------------ | -------------: | --------------------------------------------------------------------------------------------------------------------------------------------------- |
| CommandSession command candidate                       |              1 | Detached authored edit; becomes the admitted inner snapshot. Keep isolation from the old document.                                                  |
| normalizedProjectSource via projectAsReferenceDocument |              4 | Two Compositions in each of before/candidate bindings. Validation can normalize defaults/IDs, so keep separate detached projections.                |
| assertArtworkReferenceTransition effective projection  |              1 | Effective values are written only into this detached copy; authored fallback values must not change.                                                |
| Inner History.apply backup                             |              1 | Generic rollback copy. For the selected no-history full replacement only, no code mutates the old snapshot and the backup is immediately discarded. |
| CommandSession.snapshot after inner commit             |              1 | Detached adapter/public boundary used to build the outer project candidate. Keep this ownership boundary.                                           |
| Outer project History.apply backup                     |              1 | Canonical project Undo/Redo and rollback; replaceRecord mutates the outer target. Keep this copy.                                                   |

The preserved historical three-edit CPU profile independently attributes
1,413.612 ms of sampled inclusive clone time: normalized source projections
419.101 ms, command candidate 209.313 ms, snapshot 202.712 ms, inner backup
197.754 ms, outer backup 196.952 ms and effective projection 187.780 ms.
These are historical sampled sums, not current per-dispatch timings.

**Correction to the broad candidate label:** the two projectWithDocument
stages are shallow project projection plus validateCompositionProject
(assertJsonTree and schema checks), not two additional deep cloneDocument
calls. The first runs during preflight, the second after the committed inner
snapshot is copied. Reusing across that boundary would require a separate
ownership, reentrancy and stale-authority contract; this patch does not do it.

### Bounded implementation

Only the ordinary CommandSession candidate-commit path changes. With
retainHistory=true it uses the original History.apply. With retainHistory=false,
all existing candidate/preflight/authority checks still run, then the already
detached candidate replaces the whole private document. The existing second
sameDocument check and History.record preserve no-op, revision and notification
behavior. No callback mutates the prior document in this replacement span, so
the old rollback clone is unnecessary. Generic mutable History.apply is
unchanged, as are prepared/gesture paths, public snapshots, effective-value
copies, project validation and outer project History.

A separate instrumented diagnostic used the existing Stage 0 probe and confirms
**8 cloneDocument calls instead of 9**, with **2 project candidates retained**
on every measured dispatch at all three sizes. This diagnostic is not mixed
into the uninstrumented performance comparison.

### Finite same-condition performance comparison

Baseline product source is main
`8281ae7ea992b8824e03eeb2b5abe2c66f30008e`; the worktree starts from the
documentation-only checkpoint aff149190d4b3c47dc1a7c3867858337d5c1aa25.
Remote main and ownership were fresh-checked before creating the isolated
history-copy worktree. No competing measurement/source writer was observed.

Windows x64, Node v24.14.1, shiro-WS. The original benchmark ran in six
sequential processes ordered AB / BA / AB, five synchronous dispatches per
size per process, 15 samples per side per size. No probe instrumentation was
loaded in these six processes. Setup, value/Undo/Redo/serialization/reopen
checks and output logging stayed outside timing. Owned tests/builds ran
afterwards. Unrelated host activity was not controlled.

| Points | Baseline pooled median ms | Candidate pooled median ms | Median reduction |
| ------ | ------------------------: | -------------------------: | ---------------: |
| 100    |                     14.53 |                      14.54 |           -0.06% |
| 1,000  |                    110.03 |                     110.19 |           -0.15% |
| 10,000 |                   1346.82 |                    1258.51 |            6.56% |

The small sizes are effectively flat, not a demonstrated regression or speedup.
Individual 10k pair medians (including the pair that got slower):

- Pair 1: 1387.04 -> 1238.27 ms.
- Pair 2: 1358.86 -> 1407.94 ms.
- Pair 3: 1273.04 -> 1248.58 ms.

The 10k pooled median is 6.56% lower in this run. This is limited evidence,
not a universal latency guarantee or p95 claim. The structural reduction from
nine copies to eight supports the measured direction, but 10k edits still
take about 1.26 seconds and remain slow. No native/frame-rate improvement is
claimed.

### Verification, evidence and residuals

Six new regression cases cover frozen old-view and snapshot isolation,
no-op/revision/notification parity, preflight rejection, ordinary history-on/off
result parity and retained Undo/Redo, generic mutable History rollback,
preflight normalization back to a no-op and stale-envelope rejection.
Independent static review found no blocking issue and requested the latter
two explicit cases; both were added before final checks.

The exact focused/aggregate/build results and source hash are saved in
[inner-history-copy-20261108](evidence/inner-history-copy-20261108/verification.json).
That directory also contains all comparison samples, diagnostic spans,
historical clone caller stacks and the comparison runner. The six timing
processes all passed their semantic assertions.

No main merge, release, native app operation or Worker work was performed.
The two project-candidate validation passes and eight remaining dense/other
copies are not silently removed. A possible next investigation is the
preflight-to-outer-project handoff, but only with exact session/project
identity, revision, reentrancy and failed-adapter rollback proofs. Public
snapshot sharing or weakening effective validation is not a safe shortcut.
