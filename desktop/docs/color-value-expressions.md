# Color value and property references

## Brief

Source: [Nect Home](https://app.notion.com/p/3dffd279a6f381cca8c7c4dec111b131),
adopted all-inputs/value request and [research09](https://app.notion.com/p/3fcfd279a6f381e78204fa7e1053818b),
sections 3, 7, 8 and 10B. The delegated first slice narrows 10B to Color `value`
and existing Color-property references, excluding new palette storage and color math.

Verified remote `https://github.com/46slv/Nect.git`, PR39 head/base for this work
`codex/image-dither-output` at `c3703654b1e6354067e72cc7651656d27c8b7477`.
The independent clone is the only mutation target. PR39 product source is
`a195cd48d3e0b687ef964c14e4ba5fbe9bcc63c4`. Existing Pen continuation
`de2b5893580b8ba945c865b30b18b28c606d5ef9` is an ancestor (141 commits behind)
and remains untouched; the earlier missing-feature report was stale.

At this base `core.js` rejects Color expressions in validation and evaluation,
the expression resolver accepts numeric inputs only, and the property editor
offers expression controls only for Scalar/Vector2. No document palette/token
store exists in Desktop source. Its floating typography palette is unrelated.
Use the existing stable node ID + parameter key reference syntax. References
remain within the current composition, as with existing numeric references.

## Contract

- `value` reads the saved pre-expression `{hex, alpha}` base, including legacy
  Fill's live layer-fill fallback. Evaluation never writes the base or recurses.
- `ref("node-id", "color-key")` reads that Color input's effective RGBA value.
  Source sockets retain priority. Missing targets and wrong types are errors;
  self/downstream cycles remain forbidden. Names and array positions are not IDs.
- Ordinary and gradient-stop Color inputs share the same command and editor.
  Driven inputs show editable Base, saved Expression and read-only Result.
  One committed base edit is one Undo, preserving the expression.
- New Color expressions require schema 42; plain Color documents keep their
  existing version. Existing hex/alpha serialization and numeric grammar remain.
- Commands validate private candidates before history admission. Rejection,
  stale requests, Cancel and unchanged edits preserve authored state/history.
  Deleting an already-referenced source retains the existing diagnosed dangling
  reference contract; Undo restores it. No implicit bake or substitute color.

UI contract: Operate surface; primary action is editing the selected Color;
reuse property rows and the reference picker; show base/expression/result and
invalid/locked/connected states; preserve panel positions and existing density.

Non-goals: palette editor/token storage, cross-composition links, mix/alpha or
other color operators, Boolean/Text/enums, general structured expressions,
panel redesign, Pen changes, Image/Shape Dither changes. Document palette
references depend on future authoritative palette storage and stable token IDs.

## Verification plan

Focused core and actual property-editor tests cover Color and both gradient
types, repeat evaluation, typed references, dependency/cache updates, missing and
invalid targets, cycles, lock/stale/atomic failures, one Undo, rename/reorder,
duplication and serialize/reopen. Run existing numeric/reference/completion and
paint/Dither-focused regression, production build and native configuration.
Then build a separate native custom-protocol candidate and exercise the Color
editor plus file save/reopen in its WebView2 UI. Record exact evidence and limits;
do not rerun unrelated full audits or claim physical-input acceptance.

## Verification receipt — 2026-10-21 UTC

- Focused Node/actual-property-DOM regression: **104/104 PASS**. Includes eight
  Color fields (Color, Fill, Stroke, Keyer, both stops of both gradient types),
  numeric value/ref/completion compatibility, stable rename/order/duplicate and
  preset references, schema gating, source priority, cache reuse, atomic
  missing/type/cycle/invalid/lock/stale failures, Undo/Redo and JSON roundtrip.
  Existing Image/Shape Dither and preset tests passed without Dither source edits.
- Production browser/CSP: **PASS** for Linear and Radial Gradient Color editing,
  typed reference completion, Cancel, one-Undo base edit, invalid draft rejection,
  disk JSON reload and identical SVG. A real pointer Apply failure exposed
  completion dismissal moving the button during mouse down/up; preserving its
  focus fixed the original production pointer path. The same fix covers Cancel with live suggestions; the final browser/native runs verify that case. The two affected DOM integration tests also passed after this final UI correction.
- Web build: **PASS**, 138 modules; existing large-chunk warning. Native config,
  scoped formatting, JS syntax and `git diff --check`: **PASS**.
- Separate Windows release/custom-protocol build: **PASS**. Final executable
  `C:/Users/shiro/Documents/Codex/2026-10-21/task-13/native-target/release/nect-svg-desktop.exe`,
  9,994,240 bytes, SHA256
  `28f40e2d43464caf169cd1dc6d7ce00f815e18332aae5b46053fb76610c9459a`.
  Embedded build input `dist/assets/index-Do8JoPF6.js` SHA256
  `32287a654916d4899e58f5cea8d1a8112cfa9973ede89e775d8633ec7817471d`.
- Actual native WebView2 renderer at `http://tauri.localhost/`: **PASS**, both
  gradient types through the same production UI steps above, zero page errors.
  Synthetic CDP input, disposable `nect-svg-acceptance-color-r3` profile, owned
  PID122124. The first native probe read a queued frame too early after the model
  had changed; waiting for the actual output element resolved that test timing
  issue without a product change. The final run deferred the preceding task-owned
  synthetic recovery prompt with Later; no copy was restored or deleted.
  Candidate processes were closed after testing.
- Additional legacy probes: **3 pre-existing failures**, independently reproduced
  on unchanged base `c3703654` (21/24 pass): Color DOM fixture assumes a starter
  Fill at `color-dom.test.js:85`; Linear/Radial pipeline tests still expect Join
  Geometry to reject PaintedPath at lines 89/174. These files remain unchanged.
- **NOT RUN**: full suite, physical input/IME, native Save/Open OS dialogs. The
  Windows Computer Use skill requires `node_repl`, which is absent from this
  task's callable tools; the CUA surface explicitly disables native APIs. This is
  a missing controller dependency, not an approval denial. Disk JSON was written
  and reread by the runner, then loaded in the real renderer; that does not prove
  the native filesystem-dialog or cold-process reopen path.

Local evidence: ignored `desktop/test-results/color-value/focused.log`,
`browser/receipt.json`, `native/receipt.json`, their Linear/Radial screenshots and
JSON fixtures, `native-build.log`; independent base log at
`C:/Users/shiro/Documents/Codex/2026-10-21/task-13/Nect-color-base/base-results/legacy-probe.log`.
No binary, generated dependency cache or test output is committed.
