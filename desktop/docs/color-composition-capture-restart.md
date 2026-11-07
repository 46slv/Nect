# Existing-palette Composition capture and restart

## Scope and contract

This is bounded acceptance of the existing explicit Capture action, not a new
palette product or conversion default. Confirmed [REQ-287](https://app.notion.com/p/3fdfd279a6f38163b448d696f4fe70ea)
requires dependency/identity preservation and atomic import. [REQ-89](https://app.notion.com/p/3e0fd279a6f381889184efccbad78c5d)
requires stable shared-color references without linking equal independent colors.
The [schema59 contract](key-color-recipe-initialization.md#explicit-linked-trial-schema59)
already distinguishes linked import from explicit local capture when the receiving
project owns a key-color packet. No final coefficients, automatic migration or
generic cross-project link behavior is adopted here.

Panels ship at fixed positions. Development-time changes to their arrangement
must remain inexpensive; this is not user-configurable Adobe-style docking.
This acceptance uses the existing fixed Colors and Inspector surfaces unchanged.

## Recovered picker repair

PR91, head 830d64bdeb72a3a613b92969c778a0eb8d2089a7, had released ownership and
historical green checks but was not included in main47df6ea8. Its browser-only
picker cleanup was recovered by normal merge. No whole-function replacement:
current Session/project revision guards, checkbox selection, Capture behavior and
native picker route remain unchanged.

Replaying its eight-test DOM file against current unmodified main failed: the
first Cancel left the import Promise pending, with later cases also observing
retained controls. The recovered claim-once change passes all eight. It releases
the temporary input and both handlers for cancel/change/opening failure before
awaiting a selected file, and ignores stale callbacks. The test's old select-value
assignment is replaced by assertions against the current checked checkbox.

The older project-import browser test waited for a leftover body input after
opening a file chooser. Once Cancel correctly removes it, headless automatic
chooser cancellation exposes that harness dependency. It now registers the real
Playwright filechooser event before activation and selects through that chooser,
for valid and invalid files. This does not weaken import assertions.

## Permanent browser regression

composition-import-selection.e2e.js now closes its owned Chrome server process,
checks the PID is absent, launches a distinct process and opens the exact Capture
download through Project Open before editing the receiving live palette. Existing
full-state assertions, one-step Undo/Redo, local captured values, source
preservation and stale Capture rejection remain. The process receipt is written to
test-results/composition-import-selection/browser-processes.json.

An initial harness run did not dismiss the fresh blank editor's unsaved-work
prompt, so its filechooser wait timed out. The harness now handles that explicit
Discard prompt for its disposable fresh startup; production code was not changed.
The subsequent complete browser run passed.

## Actual artwork acceptance, 2026-11-07

The explicit linked-B ORBIT QA copy from the earlier production acceptance was
used as both source and receiving project, deliberately colliding palette, slot,
Composition and Artwork identities. It was read only; downloaded files went into
the new isolated checkout's ignored evidence directory.

- Normal import was disabled and the conflict explanation was visible.
- Explicit Capture added all 16 source Artwork layers in one Undo step. The
  receiving Composition, live packet and ten existing slots stayed exact. Five
  ordinary shared colors received fresh collision-remapped IDs instead of
  deduplication by equal HEX.
- The linked satellite Fill became its exact current local value, #7b7b7b alpha1.
  Other manual bindings retained ordinary remapped references. Imported PNG pixels
  matched the source over the full 960x540 image, not just sampled colors.
- Undo/Redo restored/reapplied the complete import. Save As downloaded exact JSON.
  Chrome PID3904 exited, distinct PID87168 opened that file, and full project state
  plus rendered imported pixels remained exact.
- The visible Colors editor then Previewed/Applied a receiving C1/delta-L edit.
  Preview stayed detached, the receiving live satellite changed to #bcbcbc, and
  captured Artwork stayed pixel-identical. One Undo/Redo restored both states.
  Final SVG, PNG and editable JSON were downloaded. Actual PNG was inspected.

Input SHA256: 8fd7519c3228dfbf67e903171e08693b61b83405edf8f2a3ba1500fe2a9ac5c9.
The original ORBIT SHA256 remained
0cd385f5c874a892ea4a64871e4fab9b3ff8e5fb6ff4de099c2dea318462e239.
Source/captured/reopened/final captured RGBA SHA256:
c8ac2f166e1363d52a009fbf8e9f02f5e935c4df61259ecc37b98a17eeda27bc.
Page errors: zero. Both owned browser processes exited successfully.

Evidence under the isolated checkout:
desktop/test-results/orbit-existing-palette-capture/receipt.json,
qa.mjs, input/output state snapshots, PNGs, SVG and screenshots. The runtime app
source was merge82debc83c15b6591963d53e3252720881c772eb5; later tracked changes
are regression/docs only. No input or authored user project was overwritten.

## Fresh checks and limits

From desktop/: nine focused import/color files, 93 tests passed with no failures,
skips or cancellations. Production build and native-CSP startup/eval-blocking
passed. Browser scripts composition-import-selection, project-composition-import,
browser-open-retry and key-color-artwork-status passed in sandboxed installed
Chrome. The first CSP invocation lacked CHROMIUM_PATH and could not launch
/usr/bin/chromium on Windows; setting it to installed Chrome passed, without
source changes. Historical PR91 checks are not treated as current candidate CI.

This is browser-process restart, not native WebView2 or physical IME acceptance.
Actual OS Cancel/Esc is not measured by controlled DOM Cancel events. Subjective
review, legacy palette migration, final experimental coefficients and full
REQ-211/Mission completion remain open. No native ownership inventory, release,
installed-app replacement, permission change or Codex execution was performed.
