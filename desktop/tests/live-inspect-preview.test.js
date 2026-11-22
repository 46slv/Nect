import assert from 'node:assert/strict';
import test from 'node:test';
import { mountLiveInspectPreview } from '../src/live-inspect-preview.js';

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
    readTarget: () => null,
    canInspect: () => true,
  });
  const [, enable, check, disable, status] = dom.body.children[0].children;
  return { enable, check, disable, status };
}

test('preview starts off, ignores synthetic clicks and drops a reply after disable', async () => {
  const calls = [];
  let finish;
  const view = fixture(async (name, args) => {
    calls.push([name, args.epoch]);
    if (name === 'live_preview_attach') return 71;
    if (name === 'live_preview_read')
      return new Promise((resolve) => {
        finish = resolve;
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
  finish(true);
  await pending;
  assert.match(view.status.textContent, /^Off/);
  assert.equal(view.check.disabled, true);
  assert.deepEqual(calls, [
    ['live_preview_attach', undefined],
    ['live_preview_read', 71],
    ['live_preview_detach', 71],
  ]);
});
