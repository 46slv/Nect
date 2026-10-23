import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';

// Exercise the test-only helper without publishing another window API or runner.
const source = await fs.readFile(
  new URL('../src/recovery-acceptance-test.js', import.meta.url),
  'utf8',
);
const helper = source.slice(
  source.indexOf('  const liveRetryTarget = async'),
  source.indexOf('\n  try {'),
);
const failed = { id: 'run-fixture/d-1', failedRevision: 2 };

function target(document, { disabled = false, revision = 2, visible = true } = {}) {
  const group = document.createElement('details');
  group.dataset.recoveryFailures = '';
  group.innerHTML =
    '<summary>Failures</summary><select data-recovery-failed-target><option value="run-fixture/d-1">A</option></select><button data-recovery-retry-target>Retry</button>';
  const button = group.querySelector('button');
  button.dataset.recoveryRetryId = failed.id;
  button.dataset.recoveryRetryRevision = String(revision);
  button.disabled = disabled;
  button.getBoundingClientRect = () => ({ x: 0, y: 0, width: visible ? 100 : 0, height: 24 });
  return group;
}

function runHelper(win, result, frames = async () => {}) {
  return new Function(
    'document',
    'result',
    'assert',
    'frames',
    'delay',
    'Event',
    'getComputedStyle',
    helper + '; return liveRetryTarget;',
  )(
    win.document,
    result,
    (ok, message) => assert.ok(ok, message),
    frames,
    async () => {},
    win.Event,
    () => ({ visibility: 'visible' }),
  );
}

test('live target re-acquires controls after frame and selector replacements and before retry', async (t) => {
  const win = new Window();
  t.after(() => win.happyDOM.close());
  const { document } = win,
    result = { UIReadiness: [] };
  const first = target(document),
    second = target(document),
    third = target(document);
  document.body.append(first);
  let frames = 0;
  const liveRetryTarget = runHelper(win, result, async () => {
    if (frames++ === 0) first.replaceWith(second);
  });
  second
    .querySelector('select')
    .addEventListener('change', () => second.replaceWith(third), { once: true });
  const visible = await liveRetryTarget(failed, 'failure-visible');
  assert.equal(visible.group, third);
  assert.equal(visible.button.isConnected, true);
  assert.equal(result.UIReadiness[0].Replaced, true);
  assert.equal(result.UIReadiness[0].CapturedConnected, false);
  const latest = target(document);
  third.replaceWith(latest);
  const retry = await liveRetryTarget(failed, 'before-retry');
  assert.equal(retry.group, latest);
  assert.notEqual(retry.button, visible.button);
  assert.equal(retry.button.dataset.recoveryRetryId, failed.id);
  assert.equal(Number(retry.button.dataset.recoveryRetryRevision), failed.failedRevision);
});

for (const [label, options, message] of [
  ['stale revision', { revision: 1 }, /Explicit live A target missing/],
  ['disabled target', { disabled: true }, /Explicit live A target missing/],
  ['invisible target', { visible: false }, /Live Retry UI not visible/],
])
  test('live target fails closed for ' + label, async (t) => {
    const win = new Window();
    t.after(() => win.happyDOM.close());
    win.document.body.append(target(win.document, options));
    const result = { UIReadiness: [] };
    await assert.rejects(runHelper(win, result)(failed, label), message);
    assert.ok(result.UIReadiness.length <= 5);
  });

test('live target stops after bounded continual DOM replacement', async (t) => {
  const win = new Window();
  t.after(() => win.happyDOM.close());
  win.document.body.append(target(win.document));
  const result = { UIReadiness: [] };
  const liveRetryTarget = runHelper(win, result, async () => {
    win.document.querySelector('details').replaceWith(target(win.document));
  });
  await assert.rejects(liveRetryTarget(failed, 'unstable'), /Live Retry UI not visible/);
  assert.equal(result.UIReadiness.length, 5);
});
