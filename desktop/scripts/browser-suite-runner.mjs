import { spawn } from 'node:child_process';
import { fileURLToPath, pathToFileURL } from 'node:url';

// Preserve the original test:e2e order. Each file still owns its browser/assertions.
export const browserTestFiles = Object.freeze([
  'tests/blank-artwork.e2e.js',
  'tests/pen-artwork.e2e.js',
  'tests/artwork-workflow.e2e.js',
  'tests/artwork-multi-move.e2e.js',
  'tests/artwork-alignment.e2e.js',
  'tests/node-output-alignment.e2e.js',
  'tests/artwork-distribution.e2e.js',
  'tests/artwork-gap-distribution.e2e.js',
  'tests/artwork-deletion.e2e.js',
  'tests/artwork-duplication.e2e.js',
  'tests/artwork-multi-duplication.e2e.js',
  'tests/artwork-ordering.e2e.js',
  'tests/editor.e2e.js',
  'tests/text.e2e.js',
  'tests/text-trial-feedback.e2e.js',
  'tests/canvas-in-place.e2e.js',
  'tests/system-font.e2e.js',
  'tests/guides.e2e.js',
  'tests/layout-guides.e2e.js',
  'tests/images.e2e.js',
  'tests/artboard-units.e2e.js',
  'tests/color.e2e.js',
  'tests/graph-navigation.e2e.js',
  'tests/graph-pan-joystick.e2e.js',
  'tests/graph-interaction-feedback.e2e.js',
  'tests/view-navigation.e2e.js',
  'tests/paint.e2e.js',
  'tests/linear-gradient.e2e.js',
  'tests/radial-gradient.e2e.js',
  'tests/closed-offset.e2e.js',
  'tests/text-path.e2e.js',
  'tests/math.e2e.js',
  'tests/wiring.e2e.js',
  'tests/creation.e2e.js',
  'tests/join-stroke.e2e.js',
  'tests/polygon-star.e2e.js',
  'tests/transform-anchor.e2e.js',
  'tests/position-reference.e2e.js',
  'tests/numeric-scrub.e2e.js',
  'tests/artboard-pan.e2e.js',
  'tests/text-position-output.e2e.js',
  'tests/character-position.e2e.js',
  'tests/null-position.e2e.js',
  'tests/vector-position.e2e.js',
  'tests/transform-handles.e2e.js',
  'tests/artwork-snap.e2e.js',
  'tests/vector-reference.e2e.js',
  'tests/character-transform.e2e.js',
  'tests/object-resize.e2e.js',
  'tests/rich-text.e2e.js',
  'tests/text-wrap.e2e.js',
  'tests/axis-drag.e2e.js',
  'tests/character-numeric-scrub.e2e.js',
  'tests/pair-kerning.e2e.js',
  'tests/editor-names-bypass.e2e.js',
  'tests/reference-status-help.e2e.js',
  'tests/canvas-transient-panels.e2e.js',
  'tests/browser-open-retry.e2e.js',
  'tests/angular-gradient.e2e.js',
  'tests/polar-transform.e2e.js',
  'tests/layer-duplicate-expression-refs.e2e.js',
  'tests/numeric-render-race.e2e.js',
  'tests/project-integrity-ui.e2e.js',
  'tests/text-inspector-layout.e2e.js',
  'tests/asset-library.e2e.js',
  'tests/asset-library-catalog.e2e.js',
  'tests/workspace-panels.e2e.js',
  'tests/asset-drag.e2e.js',
  'tests/color-coverage.e2e.js',
  'tests/shared-colors-panel.e2e.js',
  'tests/harmony-wheel-panel.e2e.js',
  'tests/ui-review-captures.e2e.js',
  'tests/raster-direct-png.e2e.js',
  'tests/composition-point-selection.e2e.js',
  'tests/svg-to-png.e2e.js',
  'tests/unified-font-picker.e2e.js',
  'tests/private-font-flow.e2e.js',
  'tests/ui-font.e2e.js',
]);

const signalExitCodes = { SIGHUP: 129, SIGINT: 130, SIGTERM: 143 };

export async function runBrowserSuite({
  files = browserTestFiles,
  spawnChild = spawn,
  signalSource = process,
  cwd = fileURLToPath(new URL('..', import.meta.url)),
  log = console.log,
} = {}) {
  const results = [];
  let activeChild = null;
  let interruptedBy = null;
  let infrastructureError = null;
  const handlers = new Map();

  for (const signal of Object.keys(signalExitCodes)) {
    const handler = () => {
      interruptedBy ??= signal;
      // Do not signal Vite, other suites, process groups, or unrelated browsers.
      // Browser descendants remain the individual test/Playwright owner's concern.
      try {
        activeChild?.kill(signal);
      } catch (error) {
        infrastructureError = error;
      }
    };
    handlers.set(signal, handler);
    signalSource.on(signal, handler);
  }

  try {
    for (const file of files) {
      if (interruptedBy || infrastructureError) break;
      log(`[${results.length + 1}/${files.length}] RUN ${file}`);
      const result = await new Promise((resolve) => {
        let child;
        try {
          child = spawnChild(process.execPath, [file], {
            cwd,
            env: process.env,
            stdio: 'inherit',
          });
          activeChild = child;
        } catch (error) {
          resolve({ file, status: 'error', error });
          return;
        }
        let error = null;
        child.once('error', (cause) => {
          error = cause;
        });
        // Wait for close, not just exit, before starting the next child.
        child.once('close', (code, signal) => {
          activeChild = null;
          resolve({
            file,
            code,
            signal,
            error,
            status: interruptedBy
              ? 'interrupted'
              : error || signal || code === null
                ? 'error'
                : code === 0
                  ? 'passed'
                  : 'failed',
          });
        });
      });
      results.push(result);
      const detail = result.error
        ? result.error.message
        : result.signal || (result.code === undefined ? '' : `exit ${result.code}`);
      log(`[${results.length}/${files.length}] ${result.status.toUpperCase()} ${file} (${detail})`);
      if (result.status === 'error') {
        infrastructureError = result.error ?? new Error(`Child ended unexpectedly: ${detail}`);
      }
    }
  } finally {
    for (const [signal, handler] of handlers) signalSource.off(signal, handler);
  }

  const status = interruptedBy
    ? 'interrupted'
    : infrastructureError
      ? 'infrastructure-error'
      : 'completed';
  const passed = results.filter((result) => result.status === 'passed').length;
  const failed = results.filter((result) => result.status === 'failed').length;
  const errors = results.filter((result) => result.status === 'error').length;
  const interrupted = results.filter((result) => result.status === 'interrupted').length;
  const notRun = files.length - results.length;
  log(
    `Browser suite ${status === 'completed' ? 'complete' : 'incomplete'}: ` +
      `${passed} passed, ${failed} failed, ${errors} errors, ` +
      `${interrupted} interrupted, ${notRun} not run` +
      (interruptedBy ? ` (${interruptedBy})` : ''),
  );
  return {
    status,
    exitCode: interruptedBy
      ? signalExitCodes[interruptedBy]
      : infrastructureError || failed
        ? 1
        : 0,
    results,
    signal: interruptedBy,
    error: infrastructureError,
  };
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  const result = await runBrowserSuite();
  process.exitCode = result.exitCode;
}
