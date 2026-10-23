# Grapheme editing: module and application connection

## Integration checkpoint (2026-10-18)

The parallel integrator connected the module in `main.js` and applied the
Canvas caret proposal with an additional NFC-layout to original-UTF-16 mapping.
The adapter extends unsafe native deletion ranges before draft capture and
corrects native arrow movement after each default action, including repeats.
Native input and native Undo/Redo retain ownership of text changes. Graph Text,
Canvas Text and OS Text fields share the boundary adapter; composition bypasses
it. Missing `Intl.Segmenter` explicitly refuses editing. Graph Ctrl+A remains
separate from native text Ctrl+A. Failed layout preview does not tear down the
active native editor or introduce fallback shaping.

The original nine Canvas acceptance TODOs now run as ordinary tests. The 88
focused tests pass, including raw decomposed/NFC caret mapping. The production
CSP application test `tests/grapheme-app.e2e.js` covers 14 authored fixtures:
all use real native keys/deletion/Undo/Redo, draft commit, Session Undo/Redo and
JSON reopen. ASCII, `ffi` and decomposed `e + acute` also exercise actual Canvas
pointer and Shift selection. The other 11 use the real Graph Text field because
the existing layout rejects combining/control sequences or lacks the selected
font's glyphs. Those existing shaping/font-coverage limits remain explicit;
this integration does not establish rendering support for those scripts/emoji.
Actual browser Save download and Open file selection restore the edited JSON
exactly. A fresh profile with `Intl.Segmenter` removed refuses Canvas/Graph edits
and preserves authored state. Native desktop filesystem IO is not claimed here.
Physical IME is unverified; composition bypass is synthetic. Generated receipts
are under ignored `test-results/grapheme-app/` and `test-results/integration/`.

The following sections retain the original lane's module and experiment evidence.
Their pending-connection statements describe the pre-integration checkpoint.

## Ownership and source

Font/text-editing lane: `task-4/Nect`, branch
`codex/grapheme-editing-20261018`, base
`9e4cbf9040c6e080710ca509765c3340fec53989`, initial reproduction commit
`23042953df2c08c48361e79900cdc159b03ac3d4`.

The parent subsequently authorized the dedicated pure module and its tests.
Shared app/renderer connections remain pending handoff and are kept separate.
The source tree was read only: its dirty `core.js`/`main.js` changes concern
per-operation recovery, with no grapheme implementation or overlapping module.
No source checkout/reset/stash, private font/data, user window/profile/recovery
operation, EXE build, main merge or release was performed.

## Implemented dedicated API

`src/grapheme-editing.js` has no DOM/Session dependencies or text mutations.
It keeps the original UTF-16 string; NFC normalization, scalar character IDs
and OpenType shaping clusters remain separate. `ffi` retains three graphemes.

| Export                                        | Contract                                                                                    |
| --------------------------------------------- | ------------------------------------------------------------------------------------------- |
| `graphemeBoundaries(text)`                    | Extended-grapheme UTF-16 offsets, including 0 and the end                                   |
| `graphemeCaretIndex(text, index, bias)`       | Snap an interior position backward, forward or nearest                                      |
| `graphemeDeletionRange(text, intent)`         | Desired whole-grapheme range for native content Backspace/Delete; expand partial selections |
| `graphemeMovementCorrection(text, selection)` | Correct an interior native movement result; return null for legal native selections         |

Deletion/movement return null during composition. Deletion also ignores
history, word/line deletion, cut and insert intents. The adapter still owns
when correction is necessary; the module does not replace native input or Undo.
Movement requires the observed **logical** direction, not an assumption that
ArrowLeft always reduces UTF-16 indices. Shift extension retains legal anchors,
handles crossing them, and expands pre-existing interior anchors outward.
Invalid settled ranges fail without a partial result. `Intl.Segmenter` is
required, with an explicit unavailable error; do not add a scalar fallback that
claims grapheme safety. Target-runtime feature gating belongs to integration.

## Evidence

Headless Windows Edge `155.0.4283.45` used fresh disposable profiles.
Baseline probe: 13 authored fixtures / 52 real keyboard operations. Browser and
Node segmenters agree with explicit fixture offsets. Seven baseline native-control
gaps have no correction connected in the application yet:

| Fixture                            | Native operation that enters/removes part of the grapheme |
| ---------------------------------- | --------------------------------------------------------- |
| `x + U+0301`                       | Backspace                                                 |
| `x + U+0301 + U+0323`              | Backspace                                                 |
| KA + VIRAMA + ZWJ + SSA            | ArrowLeft, ArrowRight, Backspace, Delete                  |
| Hangul leading/vowel/trailing jamo | Backspace                                                 |

Native surrogate pairs, family ZWJ, flags, airplane variation selectors,
skin-tone emoji and keycaps were safe in these baseline operations.

From `desktop/`:

- `node --test tests/text-session.test.js tests/canvas-text-edit.test.js tests/grapheme-session.test.js tests/grapheme-boundaries.test.js`:
  **61 passed**, including 28 dedicated module cases. Planned deletion ranges
  feed the existing Session tests: character IDs/offsets, native draft history,
  Session Undo/Redo and exact JSON round trips hold. Uncommitted composition
  stays outside the authored snapshot and refuses commit/Undo until completed.
- `node tests/grapheme-native-adapter.e2e.js`: **98 passed operations**, plus
  composition bypass. This experiment loads the real module into an isolated
  textarea. It covers Backspace/Delete from boundaries and interior positions,
  arrows and Shift arrows. Actual deletion events remain trusted, uncancelled
  native `beforeinput`/`input`; real browser Undo/Redo succeeds. No replacement
  text writes or synthetic input are used to implement deletion. Single-scalar
  boundary deletions are left native. The IME bypass check is synthetic, not
  physical IME acceptance.
- `node --test tests/grapheme-editing.test.js`: original application-connection
  acceptance targets remain **17 passed, 9 TODO**. Strict mode
  `NECT_GRAPHEME_ACCEPTANCE=1` gives **17 passed, 9 failed** on unchanged shared
  code. These are not claimed fixed in the application.
- New files pass Prettier; `git diff --check` passes.

Detailed local output is under ignored `desktop/test-results/grapheme-editing/`:
`native-probe.json`, `native-adapter.json`, `module-session-checks.txt` and
`strict-before.txt`. It contains only authored fixtures.

## Separate connection patch and remaining work

`docs/patches/canvas-grapheme-carets.patch` is **unapplied**. It filters the
renderer caret list through native-text grapheme boundaries while retaining
per-scalar selection geometry and authored identity. `git apply --check`
passes at the lane base. Its candidate code passed all 13 synthetic layout
boundary cases. It is a proposed renderer connection, not real UI acceptance.

The isolated textarea adapter is evidence for native selection extension before
deletion and correction after native movement. It is intentionally not
registered in `main.js`. Production integration must coordinate the existing
draft range capture/composition guards, native-safe paths, repeated movement
and actual Text controls. It must feature-check the target runtime and validate
real Canvas pointer placement, Shift selection, native Undo/Redo, draft commit
and saved JSON at the integration base. Physical IME remains unverified.

Pending actions: parent handoff for `main.js` / renderer connection, integration
and actual app validation. Status **pending by instruction**, not denied or
uncertain. The parent requested no direct cross-thread messages; results return
through the final report. No other implementation blocker remains for the pure
module. No OpenType follow-on is selected while these editing gaps remain.

External-review gate: local implementation/experiment evidence resolves this
bounded module; status `not-required`, external submissions 0, local review
cycle 1. Initial sandbox read/browser restrictions were resolved through
approved isolated operations; no unresolved approval denial remains.
