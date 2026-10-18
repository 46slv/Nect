# Nect regression validation

Run from `desktop/`:

```sh
node scripts/regression-runner.mjs
node scripts/regression-runner.mjs composition
node scripts/regression-runner.mjs graph layers numeric
```

The default `smoke` suite is intentionally short. The runner records `PASS` or
`FAIL`, exit code, duration, selected files, and captured output in
`output/regression-result.json`. A suite is not marked passed when it was not
selected; unsupported browser/native checks remain `NOT_RUN` in the checklist.

| Change area               | Suite       | Existing contract                               |
| ------------------------- | ----------- | ----------------------------------------------- |
| composition import / undo | composition | atomic import, dependencies, one undo           |
| node connections / copy   | graph       | command session, clipboard, duplication, wiring |
| layer order / duplicate   | layers      | ordering and duplication                        |
| numeric input             | numeric     | scrub, expressions, math                        |

## Luna/manual checklist (not scriptable)

Each check needs a fixture, exact operation, expected result, and failure evidence
(screenshot plus console/log excerpt). Stop the check on the first unexpected state
mutation and record `NOT_RUN`, `FAIL`, or `PASS` explicitly.

| Check               | Fixture / operation                                               | Expected                                                                                          |
| ------------------- | ----------------------------------------------------------------- | ------------------------------------------------------------------------------------------------- |
| Native file chooser | import a known composition through the OS chooser                 | import succeeds once; cancel leaves snapshot/revision unchanged                                   |
| IME                 | text node, Japanese IME composition start; attempt import/gesture | command rejected; snapshot/revision/UndoRedo unchanged; after composition end import is one undo  |
| Drag                | two-node graph; drag selected node through canvas                 | one committed edit; cancel restores; no retargeted IDs                                            |
| Context menu        | node selected; open right-click menu and copy/paste               | menu action uses same command path; copy/paste preserves IDs/contracts                            |
| Browser smoke       | launch dev server and load localhost                              | if network policy returns `ERR_NETWORK_ACCESS_DENIED`, mark `NOT_RUN` and retain browser evidence |

## Slop review

Run the existing `npm run format:check`, `npm test`, and `npm run build`. Review only
the diff for duplicate helpers, dead code, swallowed errors, and UI/API bypasses;
do not gate on raw length or complexity.
