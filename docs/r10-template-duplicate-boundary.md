# R10-TEMPLATE-DUPLICATE-01 / first core vertical

Status: CORE/JSON + QT OFFSCREEN WINDOW VERTICAL QUALIFIED / AGGREGATE OPEN. 2026-10-01 UTC.
Owner: dot, single writer on isolated dot/d0-cloud-canary.
Entry: D0 qualified at f171bd742d22a658a11e02a8c63105571403712e; current
checkpoint records the existing Definition-backed Template Duplicate residual.
Product authority: Confirmed REQ-205 / G5 Template lifecycle; no background policy
or Candidate promotion. Native 0.77 authored model remains unchanged.

## Bounded contract

Add canonical `duplicate_template_artboard` to the existing Template Session
command family and JSON-lines adapter. Input: composition, source artboard,
id_prefix (max 32 characters), x, y, index. Same Composition only. Source must
have a Template assignment and its ordinary owned Definition Instance.

Copy authored frame/layout/source references, Template identity, field overrides,
Guide overrides/suppression, and Definition/descendant override identity. Allocate
fresh IDs deterministically: prefix-artboard, prefix-grid, prefix-guide-N and
prefix-content-1. Do not copy or materialize the source Definition. Reuse ordinary
DuplicateObjects for the owned root Instance; translate only its literal tx/ty
by the explicit frame-origin delta. Preserve other appearance and source refs.
Reject a transform parent or driven tx/ty, rather than freezing placement. A
root affine scale/rotation is retained; changing literal world-plane translation
does not require inverting its matrix. No unrelated artwork moves or duplicates.

The copied Instance follows ordinary DuplicateObjects paint insertion immediately
after its source. Other roots keep exact identity/order relative to each other.
The frame inserts at explicit index. One atomic Session transaction / Undo / Redo.
The normal final validator rejects identity collisions and invalid authored state;
errors must preserve exact prior native bytes, revision and History.

## Independent acceptance

Geometry-only fixture (no DirectWrite/WIC): source Rectangle width60, x20 under
Definition; target A frame(100,100), owned Instance tx100, overridden logo x90.
Duplicate B frame(600,100) -> new owned Instance tx600, same Definition and x90
local override. Source width80 propagates to both; resetting B x restores20 while
A remains90. Local Guides use fresh IDs; inherited Guide source IDs/field overrides
and suppression remain unchanged. Frame/Grid sources remain references.

Check fresh identities, unrelated source bytes, exact native cold read, Undo/Redo,
JSON command success and explicit field validation. Negative missing source,
wrong Composition, duplicate generated IDs, bad index/coordinates, stale revision,
unsafe driven placement and later failing batch each leave state/history unchanged.

## Residuals and limits

The Window Duplicate control now consumes the canonical command after the own-cloud
Qt desktop build and focused offscreen interaction checks passed. This does not
establish hands-on visual, Windows Host/MCP acceptance or whole REQ-205 closure.
General artboard duplication and cross-Composition reuse are outside this slice.

## Verification receipt — 2026-10-01 UTC

Full core Release build PASS. The dedicated geometry-only fixture passes 63
checks, including native cold read, evaluated source propagation/reset, exact
Undo/Redo, frame/layout references, effective inherited-Guide suppression,
independent ID collisions, literal placement offset and unsafe-placement refusal.
Canonical JSON command success and invalid-index atomic rejection are exercised.
Independent source review found no production must-fix; review-driven fixture
coverage was added. Early fixture Margin/Grid dimensions were invalid and were
repaired, without weakening production validators.

Full core CTest: 20 real passes, 1 explicit WIC skip, 8 existing platform/golden
failures across 29 tests. The failing test names are unchanged from D0. Real-process
smoke still passes. Raw logs: ignored build/d0-evidence/duplicate-final-build.log,
duplicate-final-ctest.log and duplicate-final-smoke.log. This does not close
Windows/Qt/GUI/MCP or whole REQ-205. Capacity extremes and exhaustive malformed
JSON permutations are not newly qualified by this bounded fixture.

## Qt Window follow-on — 2026-10-01 UTC

Official Qt6.5.3 qtbase and ICU archives were downloaded/checksum-verified into
ignored build/deps. Debian GL development headers were extracted there too; no
system permissions or credentials changed. The aqt installer could not create its
local multiprocessing socket; archive listing and direct download/extraction did
not require that socket. The normal runtime socket restriction remains in place.

Full desktop all-target Release build PASS. The unchanged Template Window suite
passed baseline. The new public Duplicate expectation failed against the old
unsupported guard, then passed after routing to the canonical command. Final
focused four contracts (Template UI/core duplicate/JSON/Guide core) PASS; smoke PASS.
Tests include repeat clicks, exact Undo/Redo, rightmost-frame+40 placement and
insertion, and atomic refusal of driven content without changing active frame.
Independent read-only UI source review found no must-fix; suggested checks added.

First full Linux/Qt offscreen CTest: 34 passed, 1 explicit WIC skip, 26 failed out
of61. DirectWrite/WIC and local-socket failures remain explicit. effects_panel had
an exception escape followed by SEGFAULT; transform_ui has a creation-atomicity
assertion failure. These are being diagnosed and must not all be assumed platform
limitations. The aggregate suite is NOT qualified. Raw logs are under ignored
build/d0-evidence/desktop-all-ctest.log and template-ui-{red,final}-test.log.

### Aggregate failure classification follow-on

Test-only exception containment now closes the modal and rethrows callback failures
outside Qt. Re-run effects_panel exits as a normal failed test (not SEGFAULT),
showing INVALID_ASSET_LOCATOR / absolute local drive path in the Window status.
The transform creation assertion now identifies add-text / TEXT_PLATFORM_UNSUPPORTED.
Neither expectation is relaxed or skipped. The original crash was an escaping
assertion in the test callback after the platform path refusal, not evidence that
Template Duplicate crashed. No supported Windows success path changes.

Other observed blockers: nine explicit DirectWrite failures, four explicit WIC
failures, two Windows path-validation failures, seven local-server startup failures,
and missing nect-stroke-cp2-cases-r1.json for SVG import. The separate SVG process
golden one-ULP issue was already present before the desktop adapter change.
Keep all relevant rows failed/LOCAL_WAIT; no full-suite or platform-port completion.
