import assert from 'node:assert/strict';
import test from 'node:test';
import {
  mountLiveInspectPreview,
  mountLiveInspectPreviewIfAvailable,
} from '../src/live-inspect-preview.js';

function fixture(call) {
  const el = () => ({
    style: {},
    dataset: {},
    children: [],
    setAttribute() {},
    append(...parts) {
      this.children.push(...parts);
    },
    addEventListener(_, fn) {
      this.action = fn;
    },
    async click(isTrusted = true) {
      if (!this.disabled) await this.action({ isTrusted });
    },
  });
  const dom = { body: el(), createElement: el };
  const host = el();
  mountLiveInspectPreview({
    dom,
    host,
    call,
    makeChannel: () => ({}),
    readTarget: () => ({
      documentGeneration: 1,
      compositionId: 'composition',
      sessionId: 'session',
      sessionRevision: 0,
      projectRevision: 0,
    }),
    canInspect: () => true,
  });
  const [, enable, check, disable, status] = dom.body.children[0].children;
  return { enable, check, disable, status };
}

test('a missing native feature does not mount the preview', async () => {
  const commands = [];
  await mountLiveInspectPreviewIfAvailable({
    call: async (name) => {
      commands.push(name);
      return false;
    },
  });
  assert.deepEqual(commands, ['live_preview_available']);
});

for (const reject of [false, true]) {
  test(
    'preview ignores synthetic clicks and a late ' + (reject ? 'rejection' : 'result'),
    async () => {
      const calls = [];
      let finish;
      const view = fixture(async (name, args) => {
        calls.push([name, args.epoch]);
        if (name === 'live_preview_attach') return 71;
        if (name === 'live_preview_read')
          return new Promise((resolve, deny) => {
            finish = () => (reject ? deny(new Error('Old read')) : resolve(true));
          });
        return true;
      });
      assert.match(view.status.textContent, /^Off/);
      assert.equal(view.check.disabled, true);
      await view.enable.click(false);
      assert.deepEqual(calls, []);
      await view.enable.click();
      assert.match(view.status.textContent, /^On/);
      const pending = view.check.click();
      await Promise.resolve();
      await view.disable.click();
      finish();
      await pending;
      assert.match(view.status.textContent, /^Off/);
      assert.equal(view.check.disabled, true);
      assert.deepEqual(calls, [
        ['live_preview_attach', undefined],
        ['live_preview_read', 71],
        ['live_preview_detach', 71],
      ]);
    },
  );
}

test('disabling during enable suppresses a rejected attach', async () => {
  let rejectAttach;
  const calls = [];
  const view = fixture((name) => {
    calls.push(name);
    return new Promise((_, reject) => {
      rejectAttach = reject;
    });
  });
  const enabling = view.enable.click();
  const disabling = view.disable.click();
  rejectAttach(new Error('Binding rejected'));
  await Promise.all([enabling, disabling]);
  assert.equal(view.status.textContent, 'Off. Document inspection disabled.');
  assert.equal(view.check.disabled, true);
  assert.equal(view.enable.disabled, false);
  assert.deepEqual(calls, ['live_preview_attach']);
});

test('disabling during enable detaches a late successful attach', async () => {
  let finishAttach;
  const calls = [];
  const view = fixture((name, args) => {
    calls.push([name, args.epoch]);
    if (name === 'live_preview_attach')
      return new Promise((resolve) => {
        finishAttach = resolve;
      });
    return Promise.resolve(true);
  });
  const enabling = view.enable.click();
  const disabling = view.disable.click();
  finishAttach(71);
  await Promise.all([enabling, disabling]);
  assert.equal(view.status.textContent, 'Off. Document inspection disabled.');
  assert.deepEqual(calls, [
    ['live_preview_attach', undefined],
    ['live_preview_detach', 71],
  ]);
});
