# Recovery failure retry verification

Base PR73: 5c9f5f0192f967aa9d5572ede4d49c8af1b60e24.
Repair branch: codex/recovery-failed-document-retry.

The original full DOM experiment discarded animation frames. Restoring them
completed its longer baseline scenario. A shortened HappyDOM whole-app fixture
still failed to deliver the New-document root MutationObserver despite no UI
error; adding task drains did not fix that host discrepancy. Its source/logs are
preserved as experimental evidence, not product failures or passed acceptance.
The full routing check now runs in actual Edge against production CSP. The DOM
unit test exercises the exact production status-render function separately.
No assertion in an existing suite was removed, skipped or relaxed.

Before: the sealed PR73 bundle, loaded read-only, reproduced the real browser
New-document flow. A revision1 write failed after B was created; A's durable
revision remained0, B reported saved and no failed-document controls existed.
After: same flow shows A in Recovery failed. Explicit Retry sends the exact
latest A project under its original token; B project, file state, command/history
and SVG remain byte-for-byte/deep-equal. Both A/B failures are individually
selectable, retries disable while pending, and current/other failures clear
independently. Full browser run has zero page errors and max one active write.
Screenshots were inspected; failure controls use a compact menu so the workspace
header does not need an expanded row.

27 focused tests passed, no skips: existing autosave, document lifecycle,
commit notifications and numeric-scrub contracts, plus10 failure-context cases
and1 exact status DOM component. Coverage includes failure before/after a switch,
both A/B failing, multiple switches, newer edit/success, absent/stale failure
revision, duplicate in-flight retry, older generation/revision observation,
repeated I/O failure without a retry loop, and failed native-token allocation.
After the final compact-menu adjustment the affected DOM component and
production-CSP UI fixture passed again. The native build-config check passed.

Frontend build and native release build use D-local source, dependencies,
TEMP/TMP, Cargo home/registry cache and target. Existing artifacts are preserved.
Native build receipt and candidate identity are recorded below. Final remote
identity is returned with the draft PR and sealed candidate metadata. No installer, full audit or unrelated test suite is requested.

Working evidence: sibling recovery-failed-retry-20261022/evidence, diagnostic,
browser-baseline and browser-final. All data is synthetic. Failure injection is
an explicit storage/IPC fixture, not a real native filesystem failure. Browser
input is synthetic, not physical/OS input. No user document or existing Recovery
was read, overwritten, deleted or injected. No save/open/PNG OS dialogs.

Product limits remain: outstanding failed contents are only held in the running
window until a successful disk write. Exit/crash before success can lose those
changes; no persistence-format, retention, restore or close-guard semantics were
changed. No main merge, release or Library upload.

## Completed D build

Product commit: 68021a7f7cd42401c0455896a4d29fbbb23534c5.
Offline locked release/custom-protocol build passed with2 jobs in142.5 seconds.
EXE10,025,984 bytes, SHA-256
`38e7abe872ea5f2f9d942fe8c7156e8a560e0d5dc9c922ffafadf4138bad3272`.
The EXE was compiled, not launched; no installer was generated. Native Rust
storage was unchanged. No real native I/O failure or OS input claim.

The dedicated sealed output is
`D:\Nect-codex-output-20261022-task13-f336c94f2b714cd5bc73d6ca646c8f2e\recovery-failed-retry-artifact-68021a7`.
It contains the EXE, matching frontend bundle, final browser screenshots/receipts,
baseline failure receipt, focused/build logs, source delta and hash manifest.
Profiles, TEMP, caches and normal Recovery data are excluded. Prior candidates
are preserved. No new native process or dialog remains from this task; the
headless browser/server runs exited normally via their existing finally paths.

## Native follow-up boundary

The requested native mock-write acceptance could not start: this EXE exposes no
acceptance-scoped Recovery writer injection interface. See
[the exact native boundary receipt](recovery-native-boundary.md). No app/profile/
port was started, no CLI save/close operation was pending, and no bridge or real
Recovery fault was forced. The product fix stays verified by its focused/browser
evidence; native failure/retry acceptance remains NOT RUN, not passed.
