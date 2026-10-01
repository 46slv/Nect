# R10-TEMPLATE-DUPLICATE-01 / first core vertical

Status: CORE/JSON FIRST VERTICAL QUALIFIED / DESKTOP ADAPTER NOT_RUN. 2026-10-01 UTC.
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

No new Window button wiring until the desktop can be built/tested. Core/API success
is not GUI/Host/MCP acceptance or whole REQ-205 closure. Keep existing unsupported
Window guard until a separately verified adapter consumes the canonical command.
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
