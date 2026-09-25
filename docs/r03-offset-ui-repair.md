# R03-OFFSET-UI-01 — Stack Jump test fixture repair

**Packet:** Sol-approved bounded repair under the DEC-71 Mission on `codex/practical-alpha@4ad98659dd2cacbbddf6ca5628bff98f846a715b`. Source: the `offset_ui_interaction` failure carried by `CURRENT_GOAL.md`. Scope: `tests/offset_ui_tests.cpp` only; no product behavior or authored format change.

## Failure and repair

The test used `window.findChild<QScrollArea*>()` for Stack Jump navigation and field reveal. The Window has a Utility scroll area before the Inspector scroll area. The test therefore reset the wrong scrollbar and mapped the Offset Inspector group into an unrelated viewport. The crash occurred in `QWidget::mapTo` at `Qt6Widgets.dll+0x52a80`, with a null-offset access, when the test checked that mapping after the QAction and event drain. Delaying or omitting the production `ensureWidgetVisible` call did not remove the crash. Those diagnostic production edits were reversed.

Select the production Inspector by its stable object name, `inspector-scroll`, for both navigation and field reveal. Keep the Stack Jump visibility, unchanged revision, property edit, native roundtrip, Undo and atomic error assertions intact. A Release rebuild of `offset_ui_tests` passes the complete test after this correction.

## Acceptance

The focused Release `offset_ui_interaction` passed. Full serial Release CTest passed **41/41** in 63.83 s, including Offset UI, Window and Canvas interaction contracts. The production diff is empty. A disposable desktop process (owned PID 111276, session `a0dc93ee-0e37-4e63-a196-a9d2b76100bf`) reached revision 0; its ready receipt is under ignored `build/r03-offset-live-18b0bdf4115e/ready.json`. Computer Use returned an empty app inventory and lacked the documented window-list operation, so the process was closed after identity verification. Interactive GUI acceptance is `BLOCKED_ENV`; visible creation, navigation and edit were not run. The automated Qt test exercises the real Window/Inspector behavior offscreen. This packet repairs test targeting; it does not claim broader R03 or REQ-33 completion.
