# Nect regression validation

Run from `desktop/`:

```sh
node scripts/regression-runner.mjs
node scripts/regression-runner.mjs composition
node scripts/regression-runner.mjs graph layers numeric
node tests/import-actions.e2e.js
```

The default `smoke` suite is intentionally short. The runner records `PASS` or
`FAIL`, exit code, duration, selected files, and captured output in
`output/regression-result.json`. A suite is not marked passed when it was not
selected; unsupported browser/native checks remain `NOT_RUN` in the checklist.

| Change area               | Suite       | Existing contract                                                     |
| ------------------------- | ----------- | --------------------------------------------------------------------- |
| composition import / undo | composition | atomic import, dependencies, one undo                                 |
| node connections / copy   | graph       | command session, clipboard, duplication, wiring                       |
| layer order / duplicate   | layers      | ordering and duplication                                              |
| numeric input             | numeric     | scrub, expressions, math                                              |
| Import menu and chooser   | browser     | DOM dropdown renders; SVG/image/composition clicks reach file chooser |

## Manual checklist (not scriptable)

Each check needs a fixture, exact operation, expected result, and failure evidence
(screenshot plus console/log excerpt). Stop the check on the first unexpected state
mutation and record `NOT_RUN`, `FAIL`, or `PASS` explicitly.

| Check               | Fixture / operation                                               | Expected                                                                                             |
| ------------------- | ----------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------- |
| Native file chooser | import a known composition through the OS chooser                 | import succeeds once; cancel leaves snapshot/revision unchanged                                      |
| IME                 | text node, Japanese IME composition start; attempt import/gesture | command rejected; snapshot/revision/UndoRedo unchanged; after composition end import is one undo     |
| Drag                | two-node graph; drag selected node through canvas                 | one committed edit; cancel restores; no retargeted IDs                                               |
| Context menu        | node selected; open right-click menu and copy/paste               | menu action uses same command path; copy/paste preserves IDs/contracts                               |
| Browser smoke       | launch dev server and load localhost                              | if network policy returns `ERR_NETWORK_ACCESS_DENIED`, mark `NOT_RUN` and retain browser evidence    |
| Import DOM flow     | click Import summary, then SVG/image/composition actions          | menu items are visible and each click opens the expected file chooser; `tests/import-actions.e2e.js` |

### Artboard units → SVG → PNG

Use a disposable new document. Set Artboard units to `mm`, set its width/height to
`100` / `50`, and save a baseline project JSON. Switch units to `cm` and back;
compare the project JSON to confirm display units alone did not edit it. Export
SVG with units `cm`, then export PNG in `Physical × PPI` mode at `300`. Capture the
unit fields and PNG preview; retain the baseline JSON, SVG and PNG. The SVG root
must say `width="10cm" height="5cm"` with
`viewBox="0 0 377.9527559055118 188.9763779527559"`; PNG preview and file must
both be `1181 × 591 px`. Compare project JSON again after both exports. **PASS**
only if dimensions match, unit switching/exports leave project JSON unchanged,
and no visible stretch or full-pixel border appears. Record the screenshot, three
files, and reviewer/runtime version. Any mismatch is **FAIL**; if the application
could not be run, mark **NOT_RUN** (not PASS).

## Slop review

Run the existing `npm run format:check`, `npm test`, and `npm run build`. Review only
the diff for duplicate helpers, dead code, swallowed errors, and UI/API bypasses;
do not gate on raw length or complexity.
