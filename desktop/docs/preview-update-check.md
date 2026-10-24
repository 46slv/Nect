# Offline preview update check

This increment is an offline fixture client and pure metadata validation. It does not enable updates. The production Build menu still has its private `provider = null`; native channel and package authority stay unknown. `preview-update-check.js` is outside the production entry graph. It has no default provider, source, native identity, request factory or clock, and no runtime fixture setting. PR84's separate Build commit display is not a dependency.

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

## Verification and remaining dependency

Run from desktop:

```sh
node --test tests/preview-update-check.test.js tests/update-discovery.test.js tests/build-info.test.js tests/build-version.test.js
node --test tests/*.test.js
node node_modules/prettier/bin/prettier.cjs --check .
node scripts/check-native-build-config.mjs
node node_modules/vite/bin/vite.js build
```

The focused tests cover frozen inputs, expiry boundaries for both report kinds, invalid clocks/expiry, guard precedence, every audience field, wrong/unknown channels, same/older/numerically newer previews, malformed schemas/URLs, literal text, failure/retry, in-flight expiry, duplicate clicks and disposed/stale results. Existing aggregate frontend/DOM tests pick up the new test file without a workflow change. Synthetic DOM fixtures are not native WebView2 or visual acceptance; native UI remains NOT_RUN. Existing unchanged artifact verifier ZIP tests need no local repeat for this JS-only change.

Before live delivery, choose monotonically ordered preview versions above the current0.1.0, an explicit native preview build contract, durable immutable distribution, metadata/signature trust and key custody, package/bootstrap strategy, and save/cancel/apply/rollback/data compatibility rules. One-day Actions artifacts are not a live feed. No version bump, keys, feed, download, installation, restart, permissions/CSP, lifecycle/preferences or user data changes belong to this increment.
