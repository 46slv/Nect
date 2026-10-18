# Node picker category expansion

## Scope

Independent copy and branch `codex/toggle-list-scroll-20261019` start at
`e8c9db5225f6e26e36b0d69d3acb3320829b9fb0` (Draft PR28). The previous
source worktrees, user artwork and existing Chrome localhost:5199 tab/server
were left untouched. This copy used localhost:5201 with disposable content.

The existing node picker had a category select and a flat list, with no
expand-to-top behavior. Its empty-query/all-categories view now has one category
level, with Create initially expanded. Clicking a category heading or pressing
Enter/Space/Right opens it and aligns its heading to the list viewport top.
Left closes it. Up/Down on headings moves between headings; Up/Down in the
search input selects visible candidates in displayed order. No deeper hierarchy
was added. The existing explicit category filter remains available.

Only explicit expansion aligns a heading. Candidate clicks preserve the list
position; closing categories preserves it with trailing space when necessary.
Scrolling uses the list's scrollTop, never scrollIntoView. Short final groups
get enough internal trailing space even when the entire list was initially
shorter than its viewport. Search ignores expansion state and returns a flat
alphabetic list; exact/prefix ranking no longer precedes alphabetic ordering.
Browsing does not issue a Session command or change the document/graph selection.
No layer visibility, renaming, serialization or Undo implementation was changed.

## Verification

Focused regression: **18/18**, using:

```text
node --test tests/node-catalog.test.js tests/node-picker-dom.test.js tests/command-session.test.js tests/context-menu-session.test.js tests/font-preview-session.test.js tests/readonly-session.test.js
npm run build
```

The build passes (124 modules), with the existing large-chunk warning. Tests cover
category scope, closed-category search, alphabetic order independent of match
rank, NFKC/Japanese aliases, repeated expansion/collapse, synthetic composition
and keyCode-229 guards, repeat keys, ordinary selection preserving scrollTop,
cancel/stale elements, font filters and captured-target/one-Undo behavior.
Repeated browsing preserves the exact document, dirty flag, graph selection,
history depth and Viewer state. The existing standalone picker DOM harness also
needed its window's MutationObserver exposed for the context-menu integration.

Actual SS-Note-ROG Chrome observations:

| Operation | Result |
| --- | --- |
| Expand final built-in Value category | Heading delta from list top 0.09 px; list scrollTop 718; page and dialog scrollTop 0 |
| Search `transform` while both relevant categories are closed | Transform, Vector Transform in alphabetic order; ArrowDown selects the latter |
| Search Japanese alias `角丸`, Enter, Undo | Corner Round found; graph node counts 1 -> 2 -> 1; one Undo restores the initial graph |
| Expand categories after Undo | Graph remains one node; Undo remains disabled |
| Short-tail fixture, initially no overflow | Last heading delta -0.41 px; list scrollTop 108; 268.203 px of trailing space |
| Select Gamma in short-tail fixture | list scrollTop remains 108; page scrollTop 0 |
| Native Left/Right after opening Last | list scrollTop 108 -> 108 -> 108; page and dialog stay 0 |
| Chrome warn/error query on final app tab | Empty list |

The short-tail check exposed a real initial defect: scrollHeight is at least
clientHeight, so it cannot measure the actual content end of an underfilled list.
The final code measures the trailing spacer's position instead. Collapsing also
retains sufficient trailing room to avoid clamping the current viewport.

Reproduce the disposable short-tail page from the current production function:

```text
node tests/node-picker-scroll-fixture.js
npm run dev -- --port 5201 --strictPort
```

Open `/test-results/toggle-list/short-tail.html`, click Last, select Gamma,
reopen/focus Last, then press Left/Right without refocusing it through a browser
locator. Browser locator focus can itself scroll an element before sending a key;
the recorded final keyboard measurements use native current-focus keys.

Local ignored evidence: `desktop/test-results/toggle-list/regression.log`,
`build.log`, `final-chrome.jpg`, `short-tail.jpg` and generated `short-tail.html`.
Screenshots were captured from the actual browser, not a simulated renderer.

## Boundaries

Physical Japanese IME, native WebView2 and actual Ctrl+C/V were not tested. The
composition guards are synthetic DOM coverage; typing Japanese text through the
browser is not physical IME acceptance. No full suite, release or merge claim.
No authentication or permission settings were changed. No denied/pending action
remains. A requested parent progress message returned `thread not found` and did
not deliver; the delegated final response is the reliable handoff route.

Local review followed the shared coding review/Git owners and project UI skill.
External consultation gate: **not-required**, zero submissions; local tests and
browser measurements resolved the decisions (one local repair/verification cycle
for the short-list geometry). Learning gate: PASS, `NO_REUSABLE_DELTA`; the
project-specific geometry finding and reproduction live here and in the fixture.
