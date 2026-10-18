# Grapheme editing: isolated reproduction checkpoint

## Scope and state

Owner: the font/text-editing lane in `task-4/Nect`, branch
`codex/grapheme-editing-20261018`. Base is
`9e4cbf9040c6e080710ca509765c3340fec53989` (includes font-discovery source
`66211d47a3875e5b412dd7159f7a37a5ea27c212`).

Production code is unchanged. This checkpoint contains only authored Unicode
fixtures, pure geometry/Session tests and a disposable browser probe. It does
not implement the requested editing correction.

The source tree was read only. Its `grapheme-probe-checkpoint.md` states that
the previous owner made no font implementation before taking per-operation
recovery. Observed dirty production files were `core.js`, `main.js` and
`recovery-autosave.js`. The clone contains committed base files, not that WIP.
No source checkout/reset/stash, private font/data, user window/profile/recovery
operation, EXE build or release was performed.

## Observations

`node tests/grapheme-native-probe.e2e.js` used headless Windows Edge
`155.0.4283.45` with a fresh disposable profile. Its 13 authored fixtures give
52 real keyboard operations. Browser and Node `Intl.Segmenter` agree with
explicit UTF-16 boundary expectations for all 13 fixtures. The native gaps are:

| Fixture                            | Native operation that enters/removes part of the grapheme |
| ---------------------------------- | --------------------------------------------------------- |
| `x + U+0301`                       | Backspace                                                 |
| `x + U+0301 + U+0323`              | Backspace                                                 |
| KA + VIRAMA + ZWJ + SSA            | ArrowLeft, ArrowRight, Backspace, Delete                  |
| Hangul leading/vowel/trailing jamo | Backspace                                                 |

Surrogate pairs, family ZWJ emoji, flag pairs, airplane variation selectors,
skin-tone emoji and keycaps are safe for these tested native operations.
Every tested native deletion also passed real browser Undo and Redo.
This is an input-control probe, not a claim about all Unicode or all browsers.

`canvas-text-edit.js` currently exposes scalar boundaries. A synthetic scalar
layout with independently hittable component boxes demonstrates nine composite
fixture cases whose caret list includes internal grapheme positions. The
surrogate-pair and ASCII/Japanese cases pass. `ffi` deliberately keeps three
editing units even when a font could shape it into one glyph. A real application
control/evaluated-font probe remains necessary; synthetic geometry is evidence
of the boundary algorithm, not production UI acceptance.

## Verification and commands

From `desktop/`:

- `node --test tests/text-session.test.js tests/canvas-text-edit.test.js tests/grapheme-session.test.js`:
  **33 passed**, including 20 new whole-grapheme deletion/composition tests.
  Native draft history restores character IDs/offsets; Session Undo/Redo and
  exact JSON encode/decode round trips hold. Uncommitted composition does not
  enter the authored snapshot, and commit/Undo are refused until composition
  ends. These tests use the existing Session and do not inject physical IME.
- `node --test tests/grapheme-editing.test.js`: **17 passed, 9 TODO**.
  TODO means missing implementation, not success. Set
  `NECT_GRAPHEME_ACCEPTANCE=1` to make the same boundary assertions strict:
  **17 passed, 9 failed**, exit 1, on the unchanged base.
- `node tests/grapheme-native-probe.e2e.js`: **52 operations**, seven native
  operation gaps above; native Undo/Redo and all segmentation assertions passed.
- New files pass Prettier; `git diff --check` passes.

Detailed local output is under ignored `desktop/test-results/grapheme-editing/`:
`native-probe.json`, `strict-before.txt`, `session-checks.txt`. It contains only
the authored fixtures. No native app or WebView2 EXE was built.

## Next bounded implementation, after parent handoff

1. Confirm the current font WIP/ownership handoff, then probe the actual Text
   controls at the exact integration base. Keep shared `main.js` connection
   edits as a separate small patch from the dedicated editing module/tests.
2. Use raw textarea UTF-16 grapheme boundaries for editing, independently of
   NFC character identity and OpenType shaping clusters. Filter pointer carets
   to legal grapheme boundaries without changing per-scalar authored identity.
3. Test the smallest native selection correction before deletions that would
   split a grapheme. Preserve the native edit/input/history path; do not replace
   native-safe arrows/deletions or emulate text with value writes. Any selection
   correction must occur before the existing draft range capture.
4. Bypass correction during composition; validate Shift selection, interior
   pointer placement, native Undo/Redo, draft commit and saved JSON at that base.
   Remove TODO markers once their actual implementation passes.

No OpenType follow-on is selected: concrete grapheme editing gaps remain.

## Pending boundary

The delegated request explicitly permits independent fixtures/pure tests while
overlapping implementation awaits parent handoff. The parent source thread
`01a11861-608b-7537-9fec-1ca440fa10ef` could not be resolved by app messaging
("thread not found" with both default and durable host). Neither message was
delivered. The shared-code handoff is **pending**, not denied, and no production
mutation was attempted. Resume on the parent's ownership/handoff reply.

External-review gate: local evidence is sufficient for this reproduction
checkpoint; status `not-required`, external submissions 0, local review cycle 1.
The initial sandbox read/browser launch restrictions were resolved through
approved isolated operations; there is no unresolved approval denial.
