# Composition import file-picker cancellation

Issue: [#90](https://github.com/46slv/Nect/issues/90).
Base main: `6652b91cca5c138c4c929b689a9b484db4bfd52c`.
Branch: `codex/composition-import-picker-cancel-90`.

The browser branch of `openCompositionImportDialog()` previously settled its
Promise and removed the temporary file input only after `change`. A file-picker
`cancel` left both pending. A synchronous exception from `input.click()` also
left the appended input behind.

The first change, cancel or picker-opening exception now claims the input,
detaches both handlers and removes it once. Selection retains the File while its
contents are read; a late callback cannot read again or cancel that selection.
Cancel resolves without opening the composition selector. Reading/size errors
still reject, and invalid project contents use the existing rejection path.
The native `openFile()` branch and import command are unchanged.

## Evidence

The regression imports the actual `main.js` with the existing relative-import
test helper. It adds test-only exports in memory to observe the production async
function and existing selection/history. Happy DOM supplies the DOM; file-picker
activation and file contents are controlled by the test.

Against exact base `main.js`, the cancel regression fails because the import
Promise does not settle. The same test against the fix passes. For the final
baseline replay, a temporary copy of the test reads `git show` output saved under
ignored `test-results/import-picker-90/`; the working product file is not replaced.

Coverage includes five consecutive cancels with zero remaining temporary inputs,
exactly one removal and null handlers; empty change; retained/stale callbacks;
one file read despite late change/cancel; successful composition selection and
one-step import Undo/Redo; later composition-dialog Cancel; read failure,
oversized input, invalid JSON and synchronous picker-opening failure. Cancellation
and rejection compare the complete project, command/file/gesture state, selection
and both history stacks, including pending Redo.

Commands run from `desktop/`:

```sh
node --test tests/composition-import-picker-dom.test.js
node --test tests/composition-import-picker-dom.test.js tests/import-action-contract.test.js tests/composition-project-import.test.js tests/project-composition-import.test.js tests/project-import-expression-refs.test.js tests/project-import-validation.test.js tests/browser-open-retry-dom.test.js
node --check src/main.js
node node_modules/prettier/bin/prettier.cjs --check src/main.js tests/composition-import-picker-dom.test.js docs/composition-import-picker-cancel.md
node node_modules/vite/bin/vite.js build
git diff --check
```

The focused regression passes 8 tests including its parent; the related group
passes 33, with no failures or skips. Raw local logs are in the ignored
`desktop/test-results/import-picker-90/` directory.

## Remaining runtime acceptance

These are DOM regression results, not real browser or operating-system picker
acceptance. Actual Cancel/Esc on the repaired browser build remains **NOT RUN**.
The existing browser-connection refusal was not retried or bypassed; no security
settings were changed. Native Tauri/UI acceptance is separate and was not run.

On an authorized browser connection, repeat Import > Project composition and
Cancel/Esc five times, checking zero `body > input[type="file"]` after each.
Then select a valid project, cancel the composition dialog, select again and
import. Confirm existing work is preserved and one Undo/Redo removes/restores
only the imported composition set. GitHub CI results must be bound to the final
PR head; a passing DOM run does not close this real-picker acceptance row.
