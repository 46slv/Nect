# Verification record

## Safe bidirectional layer ordering, 2026-10-16

The unsafe Up-only row path has been replaced with stable-ID one-position Forward/Backward controls. Model and actual-handler synthetic DOM tests cover permutation-only record/asset preservation, boundaries/Redo, locked targets and locked-neighbor crossing, complex/hidden/guide/empty records, exact selection/history/view/focus retention and unchanged live Pen/gesture/rename/IME/dialog guards. The [ordering contract](artwork-layer-ordering.md) states the scope and disabled focus semantics. The registered Chrome acceptance checks actual three-color canvas overlap, saved JSON/SVG/PNG and reopen plus 1280/1600 layouts. Exact-commit Windows acceptance remains separately observed; local process-sandbox denial is not retried or treated as browser verification.

The duplication browser fixture now keeps all six negative-scale/rotated routes visibly inside the artboard and asserts nonzero source/copy alpha before exact SVG/RGBA comparisons for every route. Earlier all-transparent transformed pixel comparisons are not evidence of visible placement. Production duplication behavior and old artifacts are unchanged.

2026-10-13, initial operable vertical implementation.

## Passed

- `npm test`: 25 tests across core, strict SVG import and synthetic DOM integration.
- Real cubic Bézier Boolean four operations and hole area; Curve handles; downstream Path output.
- Typed socket connection, cycle rejection, fallback/expression restoration, locked-edge reload.
- Safe Remap suspension after changed Boolean geometry; stable identity through Transform.
- JSON roundtrip, malformed version/type/IDs/positions rejection, transaction rollback and undo/redo.
- Headless Text outline counter preservation with licensed DejaVu Sans.
- Synthetic DOM: initial render, parameter propagation, undo, socket connect/disconnect, layers, saved reload, invalid-load preservation.
- `npm run build`: production bundle succeeds.

## Local execution boundary

Real Chromium test script exists in `tests/editor.e2e.js`, but no browser assertions ran in this environment. Chromium exits before opening a page because its process singleton socket receives `Operation not permitted`; an escalation attempt produced the same runtime restriction. A separate available cloud browser could not reach this workspace's localhost (connection refused). No public tunnel or deployment was created to circumvent the boundary.

Windows build and Chrome results are recorded below. Native WebView2 rendering, dialogs and close/save interruption flows remain unverified. No Windows SDK was installed in the constrained shared workspace.

## Acceptance remaining

- Broader 1920×1080 and smaller-window browser layout review.
- Native Windows file durability and app interaction verification.
- Broader SVG compatibility and keyboard focus continuity review.
- V/R Converters fixture is implemented and headless-exported; interactive practical review remains.
- Incremental cache and depth bound are covered; cloud worker isolation remains.
- More geometry edge cases: tangencies, degenerate contours, extreme coordinates, text/font topology change rebind.

A passing synthetic DOM or headless test is not equivalent to a real browser or Windows pass. This initial implementation is not a release-ready product.

## Follow-on checks

The synthetic DOM pass also covers pen points/handles/closing, multi-generator movement, atomic lock rejection, Undo while drawing, rejected Output cycle and locked Output auto-routing. Own SVG export/import roundtrip, compound holes, unsafe/unsupported SVG rejection, cache invalidation and defensive isolation are tested.

`examples/vr-converters.json` is an editable practice fixture, not a final approved logo. Its headless SVG was rasterized with Inkscape and visually inspected for shapes, counters, layering and label placement. That is **output geometry proof only**, not an editor screenshot.

A bounded Windows-only GitHub Actions workflow is prepared: standard public-repository Windows runner, contents:read, 35-minute timeout, per-branch concurrency cancellation, 1-day development artifact retention, no release/deploy. Its actual build/Chrome results must be recorded after the run; configuration alone is not a pass.

## Windows CI attempt 1

Run [38426140626](https://github.com/46slv/Nect-SVG/actions/runs/38426140626) at commit `c793a278492baf06a321269629e9c7654ef4f214` failed at Prettier: Windows checkout converted text to CRLF while the repository format is LF. Installation passed. Tests, Chrome interaction and Rust/NSIS stages were **skipped**, not failed or passed. `.gitattributes` now specifies LF for text and marks binary assets explicitly. Dependency advisory status is tracked in [dependency-security.md](dependency-security.md).

## Windows CI observed success

Run [38427148348](https://github.com/46slv/Nect-SVG/actions/runs/38427148348), commit `47d7aa056993c36e255e333bba301677cc910bc5`, passed formatting, 25 tests, production build, actual Windows Chrome interactions, Tauri Rust build and NSIS packaging. The 1600×900 browser screenshot was visually inspected. The unsigned installer is 2,370,367 bytes, retained as a one-day development artifact. This proves packaging, not native WebView2 interaction. Screenshot review exposed card overlap and cramped Vector2 controls; the subsequent changes address these locally.

The follow-on tests cover typed Remap position/handle expressions, connections and fallback restoration, source-change suspension, explicit rebind and undo/redo/JSON preservation, Toggle closure, and measured card placement. Extended Chrome coverage verifies those interactions and captures initial, Remap and tall-Curve scroll layouts. Until its CI run succeeds, this extended suite is prepared rather than observed.

Run [38432755637](https://github.com/46slv/Nect-SVG/actions/runs/38432755637), commit `8acee2b10a397d5f8528c394fd028d6fa8a636f6`, passed all stages: 30 tests, build, extended actual Chrome interactions and NSIS. Initial, Remap and tall-Curve screenshots were inspected: Vector2 controls are aligned and adequately wide, new cards do not overlap, and the Curve footer is reachable. The installer is 2,370,227 bytes. Remap fields continue below the viewport and remain scrollable. The subsequent focus/unsaved-document tests require their own observed run.

Run [38434322299](https://github.com/46slv/Nect-SVG/actions/runs/38434322299), commit `a198f98933cb4f23d30592e9b883931f74d573bc`, passed 31 tests, actual Chrome focus/draft/Tab/undo and replacement-cancel checks, and NSIS. The 2,370,132-byte installer and screenshot archive were retrieved; ZIP SHA256 matched GitHub's `5d590bc6bf844d1a7379fa063b10d1075708ebd5b4dcd1155f9295be7f9a182a`. The 1600×900 screenshot showed no layout regression. Native WebView2 interaction remains unobserved.

The next acceptance extension covers atomic multi-point Remap editing plus browser opening/editing/reloading/exporting the V/R practice fixture at 1920×1080. Its core regressions check all-or-nothing refusal and one-step history; browser results must be recorded after observation.

Run [38435675197](https://github.com/46slv/Nect-SVG/actions/runs/38435675197), commit `e535b7119535796a40143be47866e950bfcb081a`, passed 33 tests, the actual Chrome multi-point/V/R acceptance and NSIS. The installer is 2,372,235 bytes; artifact ZIP SHA256 is `8f12725cd744cd5902b32f5b6e396308dc2f479299934c1a3dde47aa8b7a7417` and matched after download. The V/R 1920×1080 image was visually checked for editable V outline, R counter, text and four compositions. It exposed retained graph scroll from the prior document: document/composition replacement now resets the graph viewport and old focus, while ordinary edits/Undo retain them. A browser regression asserts the newly opened first card is visible.

The current Node CLI also exported the unmodified four-composition V/R fixture without UI: 5,821 bytes, SHA256 `d8380931f15d239783b8b3ad7116744e87d4df39c9efc532338f9cb8bd4890e3`. The [native Windows checklist](windows-acceptance.md) is the remaining platform acceptance handoff.

## Latest accepted development checkpoint

Run [38436901526](https://github.com/46slv/Nect-SVG/actions/runs/38436901526), code commit `e703332ca79b70da0c2077e04f0e6cc68a41a827`, passed all stages: 33 tests, format/build, actual Windows Chrome interaction and Rust/NSIS. The corrected 1920×1080 V/R screenshot visibly starts with the Curve card/header and Output in view; the prior stale-scroll issue is resolved. The unsigned installer is 2,372,212 bytes. Downloaded artifact ZIP SHA256 matched `1abdca73b170d01d50a705e35af1c944d268b66e19df4a515f10591c9cec72f4`.

M0–M5 **primary development flows** are operational: UI-independent evaluation and persistence, typed graph editing, Curve/Pen/canvas synchronization, Text outlines, safe Remap and batch edits, JSON/SVG interoperability, and the editable V/R browser exercise. This is not a declaration that every edge case, deferred gesture or native Windows acceptance has passed. The V/R fixture remains a practice example rather than approved final branding.

Resume conditions:

- An authorized available Windows native environment: follow [windows-acceptance.md](windows-acceptance.md), especially close/save interruption and dialog durability.
- Explicit resolution of the cancelled dependency-network update before retrying the documented dev patches. No alternate install route was used.
- Future scope may add user fonts/shaping, broader SVG fidelity, richer graph navigation and cloud isolation; none is claimed complete or required to operate this first usable editor.

No protected-main merge, public release, deployment, signing or permission/billing change was performed. Draft PR #1 is the review/handoff point. Documentation-only result updates do not alter the tested application code.

## Border ports and cancelled gestures / 2026-10-14

Code commit `c0cb41cfdce1c7ae68df276017e36a2c876a5532`, [Windows CI 38501443561](https://github.com/46slv/Nect-SVG/actions/runs/38501443561), passed formatting, all 33 tests, production build, actual Chrome interaction and Tauri/NSIS packaging.

- Synthetic DOM regressions confirm cancelled generator, graph-card, Pen and vertex gestures restore the exact document; undo/redo history is retained and later pointer movement/release does not resume the cancelled operation. Cancelled wiring clears compatible-target highlights without connecting on a later release. Ordinary Pen gestures still undo in one step.
- Actual Chrome verifies pointer cancellation, socket centers on left/right card borders, external-half input hit testing, dynamic connection visibility, matching wire endpoints, horizontal resize/undo and socket positions after long-Curve graph scrolling. A connected card placed at graph origin retains its complete input circles.
- Initial, connecting, connected, graph-origin and long-Curve screenshots were visually inspected. White/silver styling, border-centered circles and reachable tall-card controls are retained. Screenshots are browser evidence, not native WebView2 evidence.
- Unsigned NSIS installer: 2,371,437 bytes. The 2,979,162-byte artifact ZIP was downloaded and its SHA256 matched `9cf44b30ede427f10ed55cba4887fc45516d94dfc4bd8a10b30ce8fab0ec8557`. The archive and a representative screenshot were separately retained beyond the one-day CI retention.

The restored local workspace had no installed project dependencies. Local syntax checks, diff checks and the dependency-free layout test passed; the full local check stopped at missing Prettier, and the DOM test could not load Happy DOM. Those local runs are not claimed passed. The complete tests and build passed in the existing isolated Windows workflow using the unchanged lockfile. No dependency patch was installed or retried. Native Windows interaction and the previously cancelled dependency update remain separate open gates.

## Collapsed-node acceptance / 2026-10-14

Tested commit `020047d5da5119cee8c9ee09c16c89f798259916`, [Windows CI 38504090197](https://github.com/46slv/Nect-SVG/actions/runs/38504090197), passed formatting, 34 tests, production build, actual Chrome interaction and Tauri/NSIS packaging.

- Nodes collapse through their title triangle while retaining the name, output and connected inputs. Input centers are 24 px apart; the two-input Boolean card is below 100 px high. Compatible empty inputs appear during wiring, allowing replacement, disconnection and reconnection without expanding the card.
- Core tests verify persistence, legacy files without the optional flag, invalid flag rejection and no geometry-cache invalidation. DOM and Chrome verify undo/redo, unchanged width/position/selection, saved reload, keyboard focus through repeated Enter toggles, cancelled temporary wiring and resumed parameter editing after expansion. The previous border-port, pointer-cancellation and long-Curve regressions still pass.
- The 1920×1080 collapsed, active-wiring and expanded-node screenshots were visually inspected. The complete archive and a representative collapsed-node screenshot were retained separately beyond CI expiry.
- Unsigned NSIS installer: 2,371,968 bytes. Downloaded artifact ZIP: 3,188,086 bytes. SHA256 matched `04847b43d5bed401091acff0567e03ab8e2e8545340c5cfa565791df9bd643d7`.

Earlier attempt `c3cbad1` stopped at test-file formatting before tests or browser execution. Attempt `0f3767f` passed all 34 tests and build, then stopped because the browser assertion treated a newly introduced temporary input socket as an output. The assertion now distinguishes outputs explicitly; inspection confirmed the temporary input was on the correct left border. The final run above passed that check and all remaining interactions. These earlier stopped stages are not counted as passes.

Local dependencies remained absent; only syntax/diff checks and the dependency-free layout test ran locally. No dependency update, main merge, release, signing or native Windows interaction was performed. PR-body editing remains paused after its separate cancelled tool call; the repository documents contain these current results.

## Japanese fonts and vertical text (2026-10-14)

The implementation adds embedded static TTF/OTF assets, Japanese outline text and mixed vertical layout; see [typography](typography.md) for exact limits. Local `npm run check` passes formatting, all 46 unit/synthetic-DOM tests and the Vite build. The font regressions include real CFF OTF and quadratic TTF fixtures, vertical alternates/metrics/orientation, missing glyph/unsupported format and embedding restrictions, stale/cancel/invalid import preservation, immutable asset history, JSON editing after reload and byte-identical headless CLI export.

The local Chromium executable could not start because this execution environment denies its process-singleton socket. An approved/escalated launch encountered the same OS restriction; no browser pass or screenshot is claimed from that attempt. `tests/text.e2e.js` adds actual file-picker, cancellation, stale read, OTF/TTF, save/reopen, new glyph editing, shared font selection, spacing and browser/headless SVG equality coverage to the existing Windows Chrome CI workflow. The final Windows result below passed that complete suite. Native WebView2 file-picker/close behavior remains a separate Windows acceptance item.

Tested implementation commit `3faa5d5f82a62742bdc31e2e9f2bf50a955e9ea6`, [Windows CI 38507757994](https://github.com/46slv/Nect-SVG/actions/runs/38507757994), passed formatting, all 46 tests, production build, both real Chrome interaction scripts and Rust/Tauri/NSIS packaging. The logs independently record the existing editor regression pass and the new Japanese font/vertical-text regression pass. No browser page errors occurred.

The 1600×1000 Japanese horizontal/vertical editor screenshot was visually inspected. Japanese characters remain upright; punctuation, brackets, small kana and the long vowel mark use vertical forms; Latin is clockwise and columns advance right to left. The visible dashed overlay is the selected generator's existing outline guide. An independent Inkscape rendering of headless SVG was also inspected; it is not a browser screenshot.

Downloaded artifact `11868791616` is 3,325,659 bytes. Its SHA256 matches GitHub's `3a896f4f1b5ec7eff800c8b9759f22dd44cd6ac5194d5af71a89e7f2feee109d`. It contains the unsigned NSIS installer, all screenshots, saved editable Japanese project JSON, outline SVG and the test font's OFL/provenance. The complete ZIP and representative screenshot were retained separately beyond GitHub's one-day expiry. No release or main merge occurred.

The cancelled dependency-version update was not retried. Existing lockfile dependencies were installed locally using a writable temporary npm cache; package versions and lockfile remain unchanged. PR-body editing and Notion updates were not attempted. Native Windows interaction remains NOT RUN; successful Chrome and packaging are not a substitute for native file-dialog/close acceptance.

## Closed Path Offset integration (2026-10-15)

The independently qualified 31-test kernel is integrated as a bounded Path → Path node with an exact `clipper2-ts@2.0.1-18` pin and source-hash guards. Graph/DOM tests cover typed parameters, paint ordering, signed amount and joins, zero/empty erosion, provenance-safe Remap suspension, strict interactive work limits and failed/cancelled gesture history. The [contract](closed-path-offset.md) gives numerical and interaction boundaries.

Local aggregate checks are recorded by the implementation lane. Browser assertions were not observed locally: Chromium exited before opening a page because the singleton socket was denied. The new real-Chrome script and production-CSP sample run are part of the existing Windows CI suite; until that exact commit succeeds, they are prepared coverage, not a passed browser/Windows claim. No main merge or release is implied.

## Wrap width · auto height (2026-10-15)

The schema8 horizontal composition slice has dedicated unit, synthetic DOM and real-Chrome coverage described in [the contract](text-wrap.md). Local `npm run check` passes formatting, all 385 unit/synthetic-DOM tests and the production build. The 16 focused Wrap tests include exact pre-wrap geometry hashes, a 10000-unit single-shaping stress case and core/CLI SVG equality. Local Chromium was attempted but stopped before page startup because the process-singleton socket is denied by this environment. No local browser pass or screenshot is claimed. The new E2E script is in the existing Windows workflow; exact-commit aggregate/Chrome/CSP/Rust/NSIS results must be observed separately.

Native WebView2/IME, high DPI and Adobe/DirectWrite visual parity remain unverified. No main merge, release, signing or access change is implied.

## Manual pair kerning verification

The schema9 slice has focused core, actual-handler DOM and real-pointer Chrome tests in `tests/pair-kerning*`, covering the [acceptance contract](pair-kerning.md). Local Chrome launch was attempted but blocked before page startup by the environment’s process-singleton socket restriction. No local Chrome or native WebView2/IME pass is claimed. The E2E script is part of the existing Windows workflow; observe the exact published commit’s Chrome/CSP/Rust/NSIS result separately.

## Names, visibility and bypass / 2026-10-15

The cumulative slice adds inline stable-ID naming, explicit layer Hide/Show and schema10 typed bypass. Local checks cover the [contract](names-visibility-bypass.md), including every supported identity, ignored-operation errors, stored cycles/types, provenance, paint/character metadata, locks/history and Type on Path duplication. Actual Chromium interaction coverage is included in the full existing Windows workflow, after pair-kerning. The prior wrap synthetic-IME test now waits for the committed leading render and focuses its textarea before composition, preserving its original text/identity assertions. Exact-commit Windows Chrome/CSP/Rust/NSIS results remain a separate required observation; no native WebView2/IME claim is made.

## View-navigation implementation, 2026-10-15

- Local `npm run check`: formatting, 441 pure/synthetic-DOM tests and production build passed.
- Independent review found and verified fixes for pointer-toolbar Space continuity, sub-1% artboard zoom direction, busy graph wheel guards and retained graph centering extent.
- Added real-Chrome navigation assertions and 1280×800 / 1600×1000 screenshots to the full `npm run test:e2e` chain. Exact Windows CI execution remains pending at this source checkpoint; do not infer a browser pass from local DOM tests.
- Local Chromium still fails before page creation because its process-singleton socket is restricted. Native WebView2/high-DPI/physical pinch/real IME remain unverified.

## Multi-Composition session integration / 2026-10-19

The active Composition projection is now wired to the production command call sites and project-level Undo/Redo. Native/browser Open, Save, recovery, and public window.nect.load read or write the complete project envelope; old single-Document JSON uses the existing core deserializer/upgrader before migration. SVG and PNG export continue through the active Composition projection, preserving its independent dimensions. Project-shared fonts and images remain at project scope. The Add Composition action and selector connect creation and switching to the owner. Existing Composition Reference nodes retain their Layer-graph meaning.

- Focused model/owner tests: **19 PASS**, including legacy upgrader routing, stable history projections, prepared edit and project Undo.
- node --check for the changed JavaScript modules: **PASS**.
- git diff --check: **PASS**.
- Production core-factory execution and browser/native UI lifecycle: **NOT RUN**; required external packages are absent. No npm or network retry was made.
- Whole-project Composition import is connected in the follow-up below; per-Composition selection/view-state retention remains outside this slice.

Next: run the core factory and multi-Composition lifecycle acceptance using an authorized local dependency/runtime, then exercise browser/native Open, Save, recovery, active SVG/PNG export, shared assets and Undo/Redo. These checks are separate from the passed dependency-free owner/model tests.

Follow-up local validation / 2026-10-19:

- One authorized npm ci completed with 43 packages installed and 0 vulnerabilities.
- Focused Core, Composition model/session/import/view-state, import-action, and document-lifecycle suites: 64 PASS. A direct production Core factory smoke also passed cross-Composition Undo/Redo and active-size SVG projection.
- npm run build: PASS (Vite reports a 1.3 MB JavaScript chunk-size warning).
- Real Chrome: all-Composition import and repeated-import ID remapping PASS. The run then timed out clicking Undo because the Tools nav intercepted the hit target at 900×620; Undo/Redo, Open/Save, and the remaining browser assertions are not claimed passed. Receipt: desktop/test-results/project-composition-import-ui.json.
- Aggregate npm test: FAIL. Representative synthetic-DOM tests hit Invalid URL while resolving relative modules from their data-URL loader. A temporary broad loader change was reverted; it is not part of the Composition implementation.
- A subsequent local-browser navigation returned ERR_NETWORK_ACCESS_DENIED; no bypass was attempted. Browser Open/Save and native WebView2 Open/Save/recovery are NOT_RUN.

## Whole-project Composition import / 2026-10-19

The existing Import action reads the project envelope and offers All Compositions as the default or one selected source Composition. All adds preserve each source frame, ordered Layer stack, Layer-graph references and required shared font/image assets. Entity and conflicting asset IDs are remapped; the validated batch commits as one project-history operation. Open still replaces the project, and Add SVG remains a separate action. Native source-file bindings are released after reading; stale edits during selection reject the import without mutation.

- Focused model/import/owner/action-contract tests: **25 PASS**.
- JavaScript syntax checks and git diff --check: **PASS**.
- Production core factory and browser/native import acceptance: **NOT RUN**; required external packages are absent. No npm/network retry was made. The real-browser E2E assertions are prepared coverage, not a runtime pass.

Next: when an authorized local dependency/runtime is available, run the core factory and exercise all-set/single Composition import, repeated collision remapping, atomic failure, one-step Undo/Redo, Open/Save/recovery/public load and active SVG/PNG output. Per-Composition selection and view-state retention remains separate work.

## Per-Composition selection and view state / 2026-10-19

The Composition selector now saves/restores in-memory UI state by stable Composition ID: active Layer, node/artwork/point selection, selected Guide, Canvas zoom/fit/pan, graph zoom/scroll, tool, and node-viewer bindings. Restore filters Layers, Nodes, Guides, Viewer references, and authored point IDs against the target Composition. Project replacement clears these snapshots and other ID-keyed details state; newly added/imported IDs start with defaults. Switching itself does not dispatch a document command or add History/dirty state. The current header control is a selector, not the planned keyboard-accessible tablist.

- Focused model/import/owner/view-state/action-contract tests: **33 PASS**.
- Changed JavaScript syntax checks and git diff --check: **PASS**.
- Production core-factory and browser/native UI acceptance: **NOT RUN**; `desktop/node_modules/pako/browser/deflate` is absent. No npm/network retry was made.

Next: when an authorized local dependency/runtime is available, exercise Composition switching with node/artwork/point selections and distinct Canvas/graph views, undo-driven active-ID changes, Open/New/recovery resets, and verify no IDs from another Composition survive. Implement the separate accessible Composition tablist only when that UI connection is in scope.

## Responsive Composition header / 2026-10-19

The 900×620 E2E receipt recorded the Tools nav intercepting a normal click on Undo. Source inspection identified the cause: the flex-wrapped header retained a fixed 48px height while the workspace began immediately below it, placing later header rows over the first workspace column.

The app now uses header/workspace/footer grid rows. The header and workspace use content-driven heights while retaining a 500px minimum workspace; wrapped controls therefore consume space before the workspace starts. The 900×620 E2E keeps its viewport and now checks pointer Undo/Redo plus keyboard Ctrl+Z and Ctrl+Shift+Z. It was not run after this fix because the browser returned ERR_NETWORK_ACCESS_DENIED; no bypass was attempted.

- Structural layout regression: PASS (node --test tests/responsive-header-layout.test.js).
- JavaScript syntax checks: PASS.
- Prettier check for the new structural test: PASS.
- Production build: PASS, with the existing 1.3 MB JavaScript chunk warning.
- Real browser after the fix: NOT_RUN due to localhost navigation denial.
- Aggregate npm test remains FAIL for a separate data-URL loader issue. Reproduced at tests/artboard-pan-dom.test.js: after converting selected imports in src/main.js to file URLs, the loader leaves the first ./text-wrap.js import unchanged, then evaluates the source as data:text/javascript;base64,...; Node reports ERR_UNSUPPORTED_RESOLVE_REQUEST for that relative specifier. At that checkpoint, no broad harness change had been made.

## Composition keyboard tablist / 2026-10-19

Replaced the multi-Composition header selector with a manual-activation tablist. Left/Right and Home/End move focus only; Enter/Space and click activate through the existing `switchActiveComposition` path. `aria-selected` follows the active Composition, roving `tabindex` follows keyboard focus, and the Workspace is labelled as the associated tab panel. Long names truncate visually with the full title retained; the bounded tab strip scrolls horizontally for many Compositions. The existing header/workspace grid remains content-driven for wrapped rows at the 900px target.

- Main happy-dom fixture with long names and 18 tabs, keyboard activation, wraparound, focus/selection state, click selection, and unchanged revision/dirty/history: **PASS**.
- Related DOM/contract/layout tests: **17/17 PASS** (`blank-artwork-dom.test.js`, `composition-view-state-contract.test.js`, `responsive-header-layout.test.js`).
- Production build: **PASS**; Vite reports the existing large JavaScript chunk warning.
- Real browser viewport/keyboard acceptance: **NOT RUN**. Per instruction, the previously observed localhost navigation denial was not retried or bypassed.

## Main data-URL test loader / 2026-10-19

Synthetic DOM tests load `src/main.js` from a `data:` URL. Node cannot resolve any relative imports in that module against the `data:` origin, including `./text-wrap.js` which appeared before imports that older fixtures already rewrote. Added `tests/helpers/main-data-url.js` to resolve static and dynamic same-root imports against the real `src/main.js` URL, then applied it to the 37 data-URL test files that lacked a complete rewrite. Its focused test checks both import forms.

`artboard-pan-dom.test.js` now imports its core constructors explicitly, exposes happy-dom's `MutationObserver`, and loads a real Circle through `window.nect.load()` so the existing artwork-preservation assertions act on actual art. The original pan, pointer-ownership, artboard and history assertions remain in place.

- Focused tests: **2 PASS** (`main-data-url-loader.test.js`, `artboard-pan-dom.test.js`).
- Syntax checks, focused Prettier check and `git diff --check`: **PASS**.
- Full `npm test` (run once): **1,559 tests; 1,447 PASS; 108 FAIL; 4 skipped**. The Artboard Pan case passed.
- Failure classification: 77 synthetic-DOM cases fail before exercising their assertions because their fixtures do not expose global `MutationObserver`; 3 Windows subprocess cases fail to launch Node using the configured path. The other 28 report assertion/schema/API mismatches, duplicate `evaluate` declaration, or missing `beginRecovery`. They are unresolved current test/code contract failures outside this loader patch; no clean baseline run is available to attribute them conclusively.
- Browser acceptance: **NOT RUN**. The existing localhost navigation denial was not retried or bypassed. No dependency install or network retry occurred.

## Happy DOM MutationObserver fixture alignment / 2026-10-19

The 82 synthetic-DOM test files that installed `window`, `document`, and other browser globals but omitted `MutationObserver` now bind the real `happy-dom` implementation. Two focused fixtures were corrected to focus the artboard before simulating artboard Delete, and to browse all Remap points and await the scheduled batch-control render before asserting Undo/Redo behavior. No application source or assertions were weakened.

- Focused serial run: deletion DOM, ordering DOM, Auto Handles, Bend, and Smooth: **22 PASS**.
- `git diff --check`: **PASS**.
- The single full `npm test` run exited 1. Its log has **0 `MutationObserver` errors** and 70 `test at` failure reports; the default spec reporter emitted no root aggregate. Previous summarized baseline: 1,559 tests, 1,447 PASS, 108 FAIL, 4 skipped (77 observer bindings, 3 Windows child-process errors, 28 assertion/schema/API mismatches). The report format prevents a direct aggregate comparison.
- Three current Windows child-process failures are environment/path errors, not approval-flow denials: `image-luminance.test.js` cannot launch `C:\Program Files\nodejs\node.exe` (`ENOENT`); two `variable-font-cli.test.js` cases pass a `/C:/...` CLI argument that Node resolves as `C:\C:\...` (`MODULE_NOT_FOUND`). No `EPERM`/`EACCES` appears in the run.
- Other current failures are assertion/API/schema expectations, duplicate `evaluate` declaration, missing test helpers (`serialize`, `beginRecovery`), and nested isolated-DOM scenario mismatches. No evidence ties these to the observer bridge; they remain open for scoped triage.
- Full-run log was retained only in the local temporary directory as `%TEMP%\nect-npm-test-mutationobserver.log`.

No dependency or network retry, no process-setting change, no localhost retry, and no push occurred. The follow-up requested a local commit only.

## Multi-input Boolean / 2026-10-19

The existing Boolean node now folds ordered inputs while retaining Path output. Union and Intersect fold every connected input; Subtract computes A minus the union of later inputs; Difference folds the existing symmetric-difference operation. A and B remain required and open contours are rejected transactionally. Legacy A/B projects retain their existing schema and parameters until the virtual `input_pending` socket is used or locked; that action materializes stable ordered keys and advances the document to schema 40.

- Focused core, Boolean UI, Join, and source-navigation tests: **57/57 PASS** (`core.test.js`, `expandable-boolean.test.js`, `expandable-boolean-dom.test.js`, `expandable-join.test.js`, `expandable-join-dom.test.js`, `source-navigation.test.js`, `source-navigation-dom.test.js`).
- JavaScript syntax checks, selected Prettier checks for core/source-navigation, the new tests, and architecture/verification docs, and `git diff --check`: **PASS**.
- Production build: **PASS**. Vite reports the existing large JavaScript chunk warning (about 1.33 MB minified).
- Native/WebView visual acceptance: **NOT_RUN**; the synthetic DOM test exercises socket rendering, UI wiring, and Undo/Redo.
- No dependency install, network retry, push, or remote mutation was performed.

## PNG PPI density metadata / 2026-10-19

The Desktop export already had mutually exclusive scale and physical-PPI sizing modes. PPI mode now writes the selected density into PNG `pHYs` as rounded pixels per metre; for 300 PPI the stored value is 11811 px/m (about 299.9994 PPI). Existing density is replaced, while scale mode returns the canvas PNG unchanged and every non-`pHYs` chunk is preserved.

- Focused sizing and density tests: **9 PASS** (`png-density.test.js`, `png-export-size.test.js`). The density test checks chunk CRCs, replacement, unchanged raster chunks, scale-mode behavior, and the export adapter call.
- `node --check` for `raster.js` / `main.js`: **PASS**. `npm run build`: **PASS**, with the existing large minified-chunk warning.
- Full `npm test` and actual WebView2/canvas file inspection: **NOT_RUN**. No dependency install or network retry occurred.

## Fixed/responsive SVG output sizing / 2026-10-19

SVG export now offers fixed px/mm/cm root dimensions or responsive `100%` root width/height while retaining the same canonical pixel `viewBox`. The option applies only to the generated SVG string; it does not write to the editable project. The focused live-Text `CommandSession` test confirms serialization, History, and one-step Undo/Redo remain unchanged and the Text node stays editable.

- Focused SVG export, root-unit, and purity tests: **13 PASS** (`svg-responsive-export.test.js`, `svg-export-units.test.js`, `export-purity.test.js`).
- JavaScript syntax checks, selected Prettier checks, and `npm run build`: **PASS**; Vite reports the existing large minified-chunk warning.
- Full `npm test` and actual WebView2 SVG-file inspection: **NOT_RUN**. The prior PNG pHYs file-level check remains **NOT_RUN**. No dependency install or network retry occurred.
