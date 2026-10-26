# Offline preview update check

The Build menu and offline fixture client share a DOM-free manual-check controller and a small view. The production Build menu retains its private `provider = null`, so it displays only the unconfigured status and never calls a source or preparation adapter. Native channel and package authority stay unknown. `preview-update-check.js` remains a compatibility adapter outside the production entry graph, with no production fixture setting, source or preparation implementation. This increment does not enable updates.

## Contract

`validatePreviewDiscovery({identity, provider, request, response, now})` reuses `validateDiscovery` for native eligibility, all seven request/audience fields, closed schema1 metadata, strict arbitrary-precision SemVer, and literal byte-bounded notes/build labels. Only a native-authoritative preview channel qualifies. Unknown values never qualify by matching other unknown values. Package/commit provenance cannot establish installation eligibility or version order.

The fixture response is a closed envelope:

```js
{
  schema: 2,
  expiresAt: 2000,
  discovery: { /* unchanged schema1 candidate or no-update response */ }
}
```

`expiresAt` is a positive safe-integer Unix epoch time in milliseconds. `now` is an explicitly supplied nonnegative safe integer in the same units. No clock is read by the validator. At or after `expiresAt`, both positive advisories and negative reports return unavailable/SOURCE_EXPIRED. Missing, fractional, unsafe, nonfinite or coerced expiry values fail MALFORMED_SOURCE; invalid or throwing clocks fail CLOCK_INVALID. These fixture rules do not establish authenticated freshness, a signed feed or an online clock policy.

First-match order preserves null-provider dominance, provider/identity/request admission, then requires preview channel. Local `{kind:'transport-error'}` and a missing response remain distinct. Next come closed envelope validation, existing inner schema/audience/candidate/version checks, clock validation, then expiry. Inner transport-error is not metadata and cannot be wrapped. Same/older versions remain unavailable even if commit/build metadata differs. Output is only the existing advisory projection: version, literal notes and optional untrusted build label. No URL or apply action is accepted or returned.

## Fixture UI

`createPreviewFixtureCheck` is an explicit test client with an injected `readFixture`, `nextRequest` and `clock`. Mounting it does not read a source. A click clears any old advisory, disables the stable button while pending and reads one fixture with a frozen request. The clock is read after completion so metadata can expire in flight. Failure exposes Retry preview check; each click requests a fresh audience. Overlapping checks are ignored, disposed clients ignore late results, and successful later checks replace errors. There is no timer, background refresh, automatic retry or implicit apply.

Rendering reuses `renderDiscovery` and its cautious source wording. Notes/build labels stay literal; there are no download/install controls. This fixture client's identity is synthetic test data, not renderer authority supplied to native code. There is no app entry point or switch that activates it in production.

## Manual review and confirmation

`createManualUpdateCheck` binds each review to frozen copies of the admitted provider/configuration, native identity, request and validated response. `readContext` supplies the current provider/identity to revalidate at completion, refresh and confirmation. Only the exact current `reviewId` can consume consent. Changed context, expired metadata, cancellation, disposal, a new check or a reused request ID cannot authorize preparation. Cancelling a pending source or preparation does not abort external work, but generation checks prevent its late result from restoring a review or consent. Cancellation/disposal during a synchronous state notification also stops adapter invocation.

An injected **test-only** `prepare` adapter exposes the explicit confirmation boundary. It is invoked at most once per review, with immutable validated metadata, and its result is revalidated. Completion means only that this adapter returned; it is not a downloaded package, verified publisher, installation eligibility or ready-to-install claim. Missing/failed preparation consumes consent; retry requires another successful check and explicit confirmation. Production supplies neither adapter and gains no renderer-configurable source seam or native updater command.

## Verification and remaining dependency

Run from desktop:

```sh
node --test tests/manual-update-check.test.js tests/preview-update-check.test.js tests/update-discovery.test.js tests/build-info.test.js tests/build-version.test.js
node --test tests/*.test.js
node node_modules/prettier/bin/prettier.cjs --check .
node scripts/check-native-build-config.mjs
node node_modules/vite/bin/vite.js build
```

The focused tests cover frozen inputs, expiry boundaries for both report kinds, invalid clocks/expiry, guard precedence, every audience field, wrong/unknown channels, same/older/numerically newer previews, malformed schemas/URLs, literal text, failure/retry, in-flight expiry, duplicate clicks, context/consent invalidation, single-use confirmation, preparation failure and disposed/stale results. Three regressions first reproduced adapter calls after synchronous cancellation, re-entry during disposal, and cancellation being overwritten during final validation. They pass with terminal/generation guards. Existing aggregate frontend/DOM tests pick up the new test file without a workflow change. Synthetic DOM fixtures are not native WebView2 or visual acceptance; native UI for this increment remains NOT_RUN. Existing unchanged artifact verifier ZIP tests need no local repeat for this JS-only change.

Before live delivery, choose monotonically ordered preview versions above the current0.1.0, an explicit native preview build contract, durable immutable distribution, metadata/signature trust and key custody, package/bootstrap strategy, and save/cancel/apply/rollback/data compatibility rules. One-day Actions artifacts are not a live feed. No version bump, keys, feed, download, installation, restart, permissions/CSP, lifecycle/preferences or user data changes belong to this increment.
