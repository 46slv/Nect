import test from 'node:test';
import assert from 'node:assert/strict';
import { EventEmitter } from 'node:events';
import { spawn, spawnSync } from 'node:child_process';
import { mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { browserTestFiles, runBrowserSuite } from '../scripts/browser-suite-runner.mjs';

// Snapshot of every browser argument in the pre-aggregator test:e2e command.
const originalBrowserFiles = [
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
];

function fakeChild() {
  const child = new EventEmitter();
  child.kill = () => true;
  return child;
}

function suiteOptions(spawnChild) {
  const lines = [];
  const signalSource = new EventEmitter();
  return {
    files: ['first.e2e.js', 'second.e2e.js', 'third.e2e.js'],
    spawnChild,
    signalSource,
    log: (line) => lines.push(line),
    lines,
  };
}

function assertNoSuiteHandlers(signalSource) {
  for (const signal of ['SIGHUP', 'SIGINT', 'SIGTERM']) {
    assert.equal(signalSource.listenerCount(signal), 0);
  }
}

test('browser runner preserves all 55 original scripts, arguments, order, and CSP gate', async () => {
  assert.deepEqual(browserTestFiles.slice(0, 55), originalBrowserFiles);
  assert.deepEqual(browserTestFiles.slice(55), [
    'tests/reference-status-help.e2e.js',
    'tests/canvas-transient-panels.e2e.js',
    'tests/browser-open-retry.e2e.js',
    'tests/svg-to-png.e2e.js',
  ]);
  assert.equal(browserTestFiles.length, 59);
  assert.equal(new Set(browserTestFiles).size, 59);
  assert.ok(Object.isFrozen(browserTestFiles));
  const pkg = JSON.parse(await readFile(new URL('../package.json', import.meta.url), 'utf8'));
  assert.equal(
    pkg.scripts['test:e2e'],
    'npm run test:csp && node scripts/browser-suite-runner.mjs',
  );
  assert.equal(pkg.scripts['test:csp'], 'npm run build && node tests/production-csp.e2e.js');
  for (const file of browserTestFiles) {
    await readFile(new URL(`../${file}`, import.meta.url));
  }
});

test('runs fresh Node children sequentially with exact arguments and inherited environment/stdio', async () => {
  const calls = [];
  const children = [];
  let openChildren = 0;
  const options = suiteOptions((command, args, spawnOptions) => {
    assert.equal(openChildren, 0, 'never overlap children, even after exit before close');
    openChildren++;
    calls.push({ command, args, spawnOptions });
    const child = fakeChild();
    children.push(child);
    setImmediate(() => {
      child.emit('exit', 0, null);
      setImmediate(() => {
        openChildren--;
        child.emit('close', 0, null);
      });
    });
    return child;
  });
  const existingHandler = () => {};
  options.signalSource.on('SIGINT', existingHandler);
  const result = await runBrowserSuite(options);
  assert.equal(result.status, 'completed');
  assert.equal(result.exitCode, 0);
  assert.deepEqual(
    result.results.map(({ status }) => status),
    ['passed', 'passed', 'passed'],
  );
  assert.equal(new Set(children).size, 3);
  assert.deepEqual(
    calls.map(({ args }) => args),
    options.files.map((file) => [file]),
  );
  for (const { command, spawnOptions } of calls) {
    assert.equal(command, process.execPath);
    assert.equal(spawnOptions.cwd, fileURLToPath(new URL('..', import.meta.url)));
    assert.equal(spawnOptions.env, process.env);
    assert.equal(spawnOptions.stdio, 'inherit');
    assert.equal(spawnOptions.shell, undefined);
  }
  assert.deepEqual(options.signalSource.listeners('SIGINT'), [existingHandler]);
  options.signalSource.off('SIGINT', existingHandler);
  assertNoSuiteHandlers(options.signalSource);
  assert.match(
    options.lines.at(-1),
    /complete: 3 passed, 0 failed, 0 errors, 0 interrupted, 0 not run$/,
  );
});

test('ordinary failures do not prevent later scripts and aggregate to nonzero', async () => {
  const codes = [7, 0, 2];
  let count = 0;
  const options = suiteOptions(() => {
    const child = fakeChild();
    const code = codes[count++];
    setImmediate(() => child.emit('close', code, null));
    return child;
  });
  const result = await runBrowserSuite(options);
  assert.equal(count, 3);
  assert.equal(result.status, 'completed');
  assert.equal(result.exitCode, 1);
  assert.deepEqual(
    result.results.map(({ code }) => code),
    codes,
  );
  assert.deepEqual(
    result.results.map(({ status }) => status),
    ['failed', 'passed', 'failed'],
  );
  assert.match(options.lines[1], /FAILED first\.e2e\.js \(exit 7\)/);
  assert.match(
    options.lines.at(-1),
    /complete: 1 passed, 2 failed, 0 errors, 0 interrupted, 0 not run$/,
  );
  assertNoSuiteHandlers(options.signalSource);
});

for (const synchronous of [false, true]) {
  test(`${synchronous ? 'synchronous' : 'emitted'} spawn error stops with an incomplete result`, async () => {
    const error = new Error('synthetic ENOENT');
    let count = 0;
    const options = suiteOptions(() => {
      count++;
      if (synchronous) throw error;
      const child = fakeChild();
      setImmediate(() => {
        child.emit('error', error);
        child.emit('close', -2, null);
      });
      return child;
    });
    const result = await runBrowserSuite(options);
    assert.equal(count, 1);
    assert.equal(result.status, 'infrastructure-error');
    assert.equal(result.exitCode, 1);
    assert.equal(result.error, error);
    assert.equal(result.results[0].status, 'error');
    assert.match(options.lines[1], /ERROR first\.e2e\.js \(synthetic ENOENT\)/);
    assert.match(
      options.lines.at(-1),
      /incomplete: 0 passed, 0 failed, 1 errors, 0 interrupted, 2 not run$/,
    );
    assertNoSuiteHandlers(options.signalSource);
  });
}

test('unexpected child signal is incomplete infrastructure failure, not ordinary completion', async () => {
  let count = 0;
  const options = suiteOptions(() => {
    count++;
    const child = fakeChild();
    setImmediate(() => child.emit('close', null, 'SIGKILL'));
    return child;
  });
  const result = await runBrowserSuite(options);
  assert.equal(count, 1);
  assert.equal(result.status, 'infrastructure-error');
  assert.equal(result.exitCode, 1);
  assert.equal(result.results[0].signal, 'SIGKILL');
  assert.match(options.lines.at(-1), /incomplete:.*2 not run$/);
  assertNoSuiteHandlers(options.signalSource);
});

for (const [signal, exitCode] of [
  ['SIGHUP', 129],
  ['SIGINT', 130],
  ['SIGTERM', 143],
]) {
  test(`${signal} stops scheduling, signals only the active child, and exits ${exitCode}`, async () => {
    let count = 0;
    const kills = [];
    let options;
    options = suiteOptions(() => {
      count++;
      const child = fakeChild();
      child.kill = (received) => {
        kills.push(received);
        setImmediate(() => child.emit('close', null, received));
        return true;
      };
      setImmediate(() => options.signalSource.emit(signal));
      return child;
    });
    const result = await runBrowserSuite(options);
    assert.equal(count, 1);
    assert.deepEqual(kills, [signal]);
    assert.equal(result.status, 'interrupted');
    assert.equal(result.signal, signal);
    assert.equal(result.exitCode, exitCode);
    assert.equal(result.results[0].status, 'interrupted');
    assert.match(
      options.lines.at(-1),
      /incomplete: 0 passed, 0 failed, 0 errors, 1 interrupted, 2 not run/,
    );
    assertNoSuiteHandlers(options.signalSource);
  });
}

test('interruption between children starts no later script and kills no unrelated process', async () => {
  let count = 0;
  let kills = 0;
  const options = suiteOptions(() => {
    count++;
    const child = fakeChild();
    child.kill = () => {
      kills++;
    };
    setImmediate(() => child.emit('close', 0, null));
    return child;
  });
  const originalLog = options.log;
  options.log = (line) => {
    originalLog(line);
    if (line.includes('PASSED')) options.signalSource.emit('SIGINT');
  };
  const result = await runBrowserSuite(options);
  assert.equal(count, 1);
  assert.equal(kills, 0);
  assert.equal(result.status, 'interrupted');
  assert.equal(result.exitCode, 130);
  assert.match(
    options.lines.at(-1),
    /incomplete: 1 passed, 0 failed, 0 errors, 0 interrupted, 2 not run/,
  );
  assertNoSuiteHandlers(options.signalSource);
});

test('synthetic child scripts really run in distinct processes and preserve nonzero suite exit', async (t) => {
  const directory = await mkdtemp(join(tmpdir(), 'nect-browser-runner-'));
  t.after(() => rm(directory, { recursive: true, force: true }));
  const receipt = join(directory, 'receipt.jsonl');
  const files = [join(directory, 'first synthetic.mjs'), join(directory, 'second synthetic.mjs')];
  for (const [index, file] of files.entries()) {
    await writeFile(
      file,
      `
      import { appendFileSync } from 'node:fs';
      const record = (phase) => appendFileSync(${JSON.stringify(receipt)}, JSON.stringify({
        index: ${index}, phase, pid: process.pid, argv: process.argv.slice(2), path: process.env.PATH,
      }) + '\\n');
      record('start');
      await new Promise(resolve => setImmediate(resolve));
      record('end');
      process.exitCode = ${index === 0 ? 7 : 0};
    `,
    );
  }
  const runnerUrl = new URL('../scripts/browser-suite-runner.mjs', import.meta.url).href;
  const source = `
    import { runBrowserSuite } from ${JSON.stringify(runnerUrl)};
    process.exitCode = (await runBrowserSuite({ files: ${JSON.stringify(files)} })).exitCode;
  `;
  const completed = spawnSync(process.execPath, ['--input-type=module', '-e', source], {
    encoding: 'utf8',
  });
  assert.equal(completed.error, undefined);
  assert.equal(completed.signal, null);
  assert.equal(completed.status, 1, completed.stderr);
  const records = (await readFile(receipt, 'utf8'))
    .trim()
    .split('\n')
    .map((line) => JSON.parse(line));
  assert.deepEqual(
    records.map(({ index, phase }) => [index, phase]),
    [
      [0, 'start'],
      [0, 'end'],
      [1, 'start'],
      [1, 'end'],
    ],
  );
  assert.notEqual(records[0].pid, records[2].pid);
  for (const record of records) {
    assert.deepEqual(record.argv, []);
    assert.equal(record.path, process.env.PATH);
  }
  assert.match(
    completed.stdout,
    /complete: 1 passed, 1 failed, 0 errors, 0 interrupted, 0 not run/,
  );
});

test('real Node spawn errors stop without scheduling later synthetic children', async () => {
  const options = suiteOptions((command, args, spawnOptions) =>
    spawn(command, args, {
      ...spawnOptions,
      cwd: new URL('./does-not-exist-for-runner-test/', import.meta.url),
    }),
  );
  const result = await runBrowserSuite(options);
  assert.equal(result.status, 'infrastructure-error');
  assert.equal(result.exitCode, 1);
  assert.equal(result.results.length, 1);
  assert.equal(result.error.code, 'ENOENT');
  assert.match(options.lines.at(-1), /incomplete:.*2 not run$/);
  assertNoSuiteHandlers(options.signalSource);
});
