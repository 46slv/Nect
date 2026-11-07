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
