# Selection suite maintenance

Base: verified PR47 `ba8815117ba74aa486c7b90a08cc7280066424ae`,
`codex/canvas-artwork-marquee`, remote `https://github.com/46slv/Nect.git`.
Scope is only the nine failures already reproduced on PR46. Reuse prior receipts
in `test-results/canvas-artwork-marquee/baseline-probe.log` and
`baseline-multi-move.log`; do not repeat those whole suites or add features.

Classification from current source/spec evidence: category 1 (confirmed product
bug) none; category 2 (test not following current contract) nine; category 3
(unrelated/undetermined expectation) none after bounded investigation. This is a
test-only candidate; no product/native package or authored schema change.

| #   | Existing failing scenario                                    | Category / cause                                                                                                                                                                                             | Bounded correction                                                                                                                                                                         |
| --- | ------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| 1   | Toolbar appends five independent artworks                    | 2: Text no longer authors on toolbar click. REQ-210 separates Tool activation and Canvas release creation.                                                                                                   | Assert Tool activation is neutral, then create at the same intended center via a primary Canvas click. Retain five layers, identities, one Undo/Redo, JSON/SVG assertions.                 |
| 2   | Collective snapping / Alt / live Shift constraint            | 2: old H/V-only expectation predates integrated H/V/45 behavior. Delta [20,15] projects exactly to [17.5,17.5].                                                                                              | Assert that exact displacement; retain original free-drag, Alt, unchanged committed data and cancellation checks.                                                                          |
| 3   | Center distribution / selection / views / no-op Redo         | 2: queries controls before opening current Canvas Align panel.                                                                                                                                               | Open via the real toolbar control, reopen after outside-click dismissal; retain exact Undo/Redo/no-op/view checks.                                                                         |
| 4   | Distribution stale / busy / invalid members                  | 2: same missing-panel assumption.                                                                                                                                                                            | Use current panel route, retain forced stale/invalid command checks.                                                                                                                       |
| 5   | Six alignment controls / set / views / Undo                  | 2: same missing-panel assumption.                                                                                                                                                                            | Use current panel route; retain every action and exact assertions.                                                                                                                         |
| 6   | Gap controls vs centers / views / Undo                       | 2: same missing-panel assumption.                                                                                                                                                                            | Use current panel route, retain separation, counts and no-op/Redo checks.                                                                                                                  |
| 7   | Gap stale / unsupported / interrupted guards                 | 2: same missing-panel assumption.                                                                                                                                                                            | Use current panel route and retain forced invalid actions.                                                                                                                                 |
| 8   | Alignment unsupported / locks / IME / modal / Pen / gestures | 2: same missing-panel assumption.                                                                                                                                                                            | Open the panel explicitly even for disabled states; retain rejection assertions.                                                                                                           |
| 9   | Graph Overview measured/cancellable navigation               | 2: initial Canvas Align disclosure is open. Clicking outside dismisses it and structurally renders; per-element Happy DOM geometry mocks then refer to detached elements. Native layout has real dimensions. | Close the unrelated Align panel through its real control before measuring the Graph fixture. Retain exact card count/size, 2,000 cards, view-only state and all cancellation/resize tests. |

Evidence: current `main.js` alignmentPanel/align-panel/closeTransientPanels and
outside-click policy; `docs/req210-text-tool-20261019.md` and `src/text-tool.js`
(`f1e4866`); `docs/snap-45.md`, integrated `artworkDelta` and
`src/direction-snapping.js` (`3ab1d3b`). The older `shift-axis-movement.md` and
Inspector-era alignment prose are historical, not the current interaction owner.
Graph Overview's existing `updateGraphOverview` behavior remains untouched.

No tests are removed/skipped and no numeric tolerances loosened. The alignment
test helper must execute the actual panel toggle and verify reopening is authored
document/history-neutral. Run these nine scenarios only; if another expectation
fails, classify it before changing it. Product fixes would require corresponding
native checks; a test-only result must explicitly avoid claiming a fresh native
run or build. Preserve all prior candidate EXEs and receipts.

## Final receipt

All nine targeted scenarios pass. The first run passed cases 2 through 9; Text's
updated Canvas route additionally needed a finite stage rectangle in Happy DOM
for its existing release-inside-viewport guard. The final Text-only rerun passes,
including new assertions that Tool activation and the pending press are neutral.
No passing case was rerun without a change affecting it. No full/baseline suite
was repeated. Logs and `final-receipt.json` are in
`test-results/selection-suite-maintenance`; toolbar probes select only that real
tracked callback, and the seven multi-artwork cases use the existing
`NECT_DOM_SCENARIO` isolation route. Graph Overview runs its complete existing
scenario, including 2,000 cards and cancellation/resize/draft-state assertions.

Category totals: 1 = 0, 2 = 9 corrected, 3 = 0. No confirmed product defect or
unsupported expectation remains among these nine. All original validation and
Undo/Redo/no-op/selection/view checks remain; only the obsolete exact Shift
expectation changes to the specified 45-degree result. The group of six Align
cases now exercises the real panel opener and verifies that opening/reopening is
document/history/active-layer/selection-neutral. The original toolbar case is
renamed to distinguish immediate shapes from Canvas Text creation.

`git diff ba8815117ba74aa486c7b90a08cc7280066424ae -- src src-tauri` is empty.
Scoped formatting and diff checks pass. This follow-up closes the inherited test
drift noted in PR47; its original receipts are retained as historical evidence.

No new product EXE, frontend/native build or native UI run was made. The unchanged
PR47 candidate remains source `8236aebdfecd4ec8dd5346d9d1ed9bcfab64b942`,
10,000,896 bytes, SHA256
`09aa0d9170fd1a0817b52380b983198460744ce08e43b64cabdb2bef933c3d2e`.
Prior PR40 through PR47 candidates and receipts remain preserved. Physical input,
OS dialogs, cold-process file reopen and unrelated audits were not run here.
