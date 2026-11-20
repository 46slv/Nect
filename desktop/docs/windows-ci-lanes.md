# Windows CI lane split

## Bounded design (2026-11-19)

Move only the actual Chrome interaction step into an independent Windows 2022
job. Keep native compilation, Rust and frontend unit/DOM tests in their existing
order. Keep every browser file, sequential execution and failure aggregation.
Both jobs retain checkout, Node 24, npm ci and Japanese font provisioning; the
browser job builds the production CSP bundle through unchanged test:e2e.
No test filter, timeout reduction, permission increase, external action, or
concurrency-policy change is included. Existing `windows` becomes an always-run
fail-closed aggregate: both required lanes must report exactly success. Failed,
cancelled, skipped or absent results reject it. Other validation continues after
ordinary failures under the original conditions.

Native evidence retains its existing artifact name. Browser evidence uses a
separate `-browser-` name to avoid upload collisions; retain the original path
allowlist in each lane, so the union preserves all prior evidence. Missing files
are still ignored; upload errors still fail the corresponding lane.

There are two heavy Windows lanes plus a short Windows aggregate runner.
This trades additional runner setup and Actions total minutes for lower elapsed
time. It does not reduce work or promise a speedup when runners queue. No merge
until exact candidate CI, protection/check-name behavior and usage budget have
been reviewed.

## Measurement

Record queued/start/end timestamps separately for every job and step, and record
all test counts, failures, artifact contents and aggregate result. Compare the
previous serial run against candidate lanes on identical product source (verify
the tree outside workflow/document/test-only changes), not unrelated feature
revisions. Repeat only when authorized and without cancelling an active run.
An old workflow run and a new workflow run cannot honestly be described as an
exact-same-head A/B. Exact-head paired measurement would require an explicit
baseline workflow/strategy input, which this minimal patch deliberately does not
introduce. Report this limitation instead of attributing queue variance to code.

Accept only with Chrome suite unchanged (147 files), full frontend/Rust/native
checks retained, useful wall reduction, acceptable total runner minutes and
fail-closed aggregate behavior. Native UI acceptance remains separate.

## Follow-up trial (2026-11-20)

The first follow-up run split frontend work from native work and used two
Chrome shards. It revealed that the catalog test serves the production bundle
itself. Each Chrome shard now builds its own bundle. Shard 1 retains the
production CSP test; shard 2 runs the build without duplicating that test.
The aggregate requires native, frontend, and both Chrome shards to succeed.
