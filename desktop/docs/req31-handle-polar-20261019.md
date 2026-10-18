# REQ-31: owned handle Angle° / Length

Base: `f35a4659d6569b13a5a5448b1526ea5fac7ab6d9` on
`codex/req210-text-tool-20261019`; development branch:
`codex/req31-handle-polar-20261019`.

Confirmed/Must source: [REQ-31](https://app.notion.com/p/3e0fd279a6f38157b288ea8e88d56052).
Its direction/length acceptance is implemented for Curve and Multi Path owned
points. This is a bounded gap closure, not full Inspector acceptance or release.

The shared point-row renderer exposes incoming/outgoing polar controls in both
the graph and Inspector. Multi Path now also uses that existing point panel in
the Inspector. `handle-polar.js` converts the edited component to a relative
Vector2 and uses the existing curve-point gesture, including core coupling and
one atomic History entry. No core command, schema or saved-angle state was added.
[Input semantics and zero-length policy](selected-point-panel.md#owned-handle-angle-and-length-req-31-2026-10-19).

Verified on SS-Note-ROG:

- Focused tests: **31/31 PASS** with `node --test tests/handle-polar.test.js
  tests/canvas-gesture-session.test.js tests/gesture-session.test.js
  tests/direct-selection.test.js`.
- `npm run build`: **PASS**, 126 modules. Existing large-chunk warning remains.
- Actual Chrome **155.0.8059.80**, disposable temporary profile, generated fixture
  documents only, localhost **5205**: **PASS**. Both node types, both handles,
  corner/smooth/symmetric; numeric → SVG overlay and pointer drag → polar values;
  one Undo/Redo per edit; zero/regrowth; negative/empty/non-finite rejection;
  safe arithmetic; Escape; JSON reload; lock/expression/connection protection;
  focus and nested scroll; graph/Inspector synchronization; retained X/Y edits;
  untouched rounded-field precision and dirty revision.
- Browser page errors: **0**. [Portable receipt](evidence/req31-handle-polar-chrome.json).
  Local screenshot: `test-results/handle-polar/inspector.png` (visually inspected).
- `git diff --check`: **PASS**.

Two additional existing synthetic DOM suites (`authored-value-dom.test.js` and
`text-outline-path-ui-dom.test.js`) fail at startup with
`ReferenceError: MutationObserver is not defined`. The same failures were reproduced
against the unmodified base main.js using a read-only test loader. Their harness
and unrelated full-suite failures were left unchanged.

Native Tauri/EXE and physical IME are **unverified**. Path Remap, multi-point bulk
polar editing and broader Inspector work are excluded. Old 5199/5201/5203,
user artwork, Notion, merge/release/distribution and auth settings were untouched.

Local review found no unresolved material design decision. External consultation
gate: **not-required**, zero submissions; requirements, core tests and actual
browser evidence were sufficient. Learning closeout: **PASS / NO_REUSABLE_DELTA**;
project-specific semantics and guards are recorded here and in focused tests.
