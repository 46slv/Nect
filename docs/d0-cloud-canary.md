# D0 cloud execution qualification

2026-10-01 UTC. Authority: user-requested Dot-first migration and the D0 section of
[Nect Parallel Control](https://app.notion.com/p/3eafd279a6f381b9a740e217473c8562).

## Result

Cloud clone, isolated branch, useful build/test, bounded repair, commit, remote
candidate and exact readback are qualified. This is not whole-suite or release
acceptance. No Codex/Work task was created for this migration.

- Base: `01a909097ba222db0340e1c15cdeba6018f445ff` on `codex/practical-alpha`.
- Candidate: `dot/d0-cloud-canary`, remote `f171bd742d22a658a11e02a8c63105571403712e`.
- Tested/remote tree: `876da0e87e05677cc85d7acf6c9f0f5ba59877ee`.
- Local evidence commit: `76b64f18461d7439291f17be9c264a35559fb737`, preserved on
  local `dot/d0-local-evidence`. Remote API commit has the identical tree/parent
  and a different timestamp. Local active branch was advanced to the fetched
  remote candidate after tree comparison; working tree was clean.
- Candidate review: [Draft PR 13](https://github.com/46slv/Nect/pull/13).
- Primary remote branch remained at the exact base. No merge/release/publication.

## Environment and repair

Debian 13, GCC 14.2.0, Python 3.12.14, CMake 4.4.3, Boost 1.85.0.
CMake/Ninja were installed into temporary tooling from PyPI. Boost was unpacked
from its official archive into ignored `build/deps`. No SDK or credentials are
committed. Core-only Release, `NECT_DESKTOP=OFF`, `BUILD_TESTING=ON`.

The baseline build failed at two `std::find` calls in primitive_tests.cpp. Adding
its direct `<algorithm>` include repaired the build without changing assertions
or product behavior. Independent read-only review approved that exact diff.

## Verification

- Full core Release build: PASS after repair.
- primitive_contract: PASS.
- scripts/smoke.py: PASS; binding/edit/Undo/Redo, native cold reopen, SVG anchor
  checks and original source preservation. GUI/MCP were not tested.
- Full core CTest: 20 reported passes / 8 failures out of 28. One reported pass
  (assets_contract) is an existing non-Windows no-op and is NOT image acceptance.
- Seven failures: alignment, duplication, transform, definition, template,
  text_authoring and history encounter unsupported Windows DirectWrite/WIC paths.
- process_contract: golden SVG comparison encounters eight one-ULP sine(45deg)
  coefficients on Linux; a read-only diagnostic also found the equivalent
  gradient golden difference and a later DirectWrite dependency. No assertion
  was weakened and these tests remain failed.
- The historical 0.3 load and current-native migrated reload emit identical SVG
  on this host. Do not infer Windows or cross-platform bitwise render parity.

Raw local logs are retained under ignored `build/d0-evidence/` (configure,
baseline-build, repaired-build, repaired-ctest, smoke). Existing Windows evidence
remains historical evidence at its exact source SHA, not a result from this host.

## Remote write route

Shell `git push --dry-run` failed due to absent cloud Git credentials. The connected
GitHub plugin successfully created blobs/tree/commit and updated the branch with
`force=false`. Git fetch and ls-remote verified the remote commit, exact tree and
unchanged primary checkpoint. Remote author/committer both use the verified GitHub
noreply identity. No token was copied/created. This qualifies API-based remote
synchronization, not authenticated shell Git push. CI filters do not include this
branch/PR target; absent CI is not a pass.

## Continuation

Dot is the sole cloud writer on the isolated branch. Checkpoints are continuation
points. Background rendering semantics remain a local product clarification;
independent accepted Template residuals and truthful Linux test evidence can
proceed. No requirement is promoted solely because the execution environment moved.

### Follow-on evidence repair

The non-Windows asset fixture now returns 77 and CTest declares that code skipped.
Focused assets/raster/text/primitive run: 3 real passes, 1 explicit skip. Windows
asset assertions and their normal 0/1 result path are unchanged. This does not
hide the separate mixed-platform suite failures listed above.
