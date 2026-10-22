import test from 'node:test';
import assert from 'node:assert/strict';
import { downloadCommand } from './command-download.js';

function deferred() {
  let resolve, reject;
  const promise = new Promise((yes, no) => {
    resolve = yes;
    reject = no;
  });
  return { promise, resolve, reject };
}

// Synthetic locator contract only. The separate command-menu-actions tests use
// the production menu hierarchy; real visibility and downloads remain E2E proof.
function fixture(action, { open = false, activate, pending = deferred() } = {}) {
  const label = action === 'save' ? 'Project' : 'Export',
    calls = [],
    file = { suggestedFilename: () => `artwork.${action}` },
    button = {
      async click() {
        assert.equal(open, true, 'the action must be revealed before activation');
        calls.push(action);
        if (activate) return activate();
        pending.resolve(file);
      },
    },
    menu = {
      async getAttribute(name) {
        assert.equal(name, 'open');
        return open ? '' : null;
      },
      locator(selector) {
        if (selector === 'summary')
          return {
            async click() {
              calls.push(label);
              open = !open;
            },
          };
        assert.equal(selector, `[data-action="${action}"]`);
        return button;
      },
    },
    page = {
      waitForEvent(name) {
        assert.equal(name, 'download');
        calls.push('wait:download');
        return pending.promise;
      },
      locator(selector, options) {
        if (selector === 'summary') {
          assert.ok(options.hasText.test(label));
          return { label };
        }
        if (selector === 'header details.command-menu')
          return {
            filter({ has }) {
              assert.equal(has.label, label);
              return menu;
            },
          };
        assert.equal(selector, `[data-action="${action}"]`);
        return button;
      },
    };
  return { page, calls, pending, file, label };
}

test('downloads listen before revealing and activating every Pen file action', async () => {
  for (const action of ['save', 'export', 'export-png']) {
    const { page, calls, file, label } = fixture(action);
    assert.equal(await downloadCommand(page, action), file);
    assert.deepEqual(calls, ['wait:download', label, action]);
  }
});

test('repeated downloads do not toggle an already-open menu closed', async () => {
  const { page, calls, file } = fixture('export-png', { open: true });
  assert.equal(await downloadCommand(page, 'export-png'), file);
  assert.deepEqual(calls, ['wait:download', 'export-png']);
});

test('a download does not hide a later command activation failure', async () => {
  const activation = deferred(),
    failure = Error('click failed after the download began'),
    { page, pending, file } = fixture('export', { activate: () => activation.promise });
  const result = downloadCommand(page, 'export');
  const rejected = assert.rejects(result, (reason) => reason === failure);
  pending.resolve(file);
  await new Promise(setImmediate);
  activation.reject(failure);
  await rejected;
});

test('a download timeout is reported while the command is still pending', async () => {
  const activation = deferred(),
    timeout = Error('download timeout'),
    { page, pending } = fixture('export', { activate: () => activation.promise });
  const result = downloadCommand(page, 'export');
  const rejected = assert.rejects(result, (reason) => reason === timeout);
  pending.reject(timeout);
  await rejected;
  // A later command failure is observed too, rather than becoming unhandled.
  activation.reject(Error('late click failure'));
  await new Promise(setImmediate);
});

test('a failed command is reported without an unhandled later download timeout', async () => {
  const failure = Error('command unavailable'),
    { page, pending } = fixture('save', {
      activate: () => {
        throw failure;
      },
    });
  await assert.rejects(downloadCommand(page, 'save'), (reason) => reason === failure);
  pending.reject(Error('late download timeout'));
  await new Promise(setImmediate);
});
