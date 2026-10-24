import assert from 'node:assert/strict';
import { Window } from 'happy-dom';

const window = new Window();
globalThis.window = window;
globalThis.isTauri = true;
const calls = [];
const identity = {
  authority: 'native',
  integrity: 'verified',
  appVersion: '0.1.0',
  buildFlavor: 'normal',
  assetMode: 'static',
  runtimeIsolation: 'ordinary',
  channel: null,
};
window.__TAURI_INTERNALS__ = {
  invoke: async (command, args) => {
    calls.push({ command, args });
    if (process.argv[2] === 'failure') throw Error('Query failed');
    return identity;
  },
};
const { ensureBuildInfo } = await import('../../src/build-info.js');
const document = window.document,
  app = document.createElement('main');
document.body.append(app);
const wait = () => new Promise((resolve) => setTimeout(resolve, 0));
for (let i = 0; i < 2; i++) {
  app.innerHTML = '<header></header><section data-document>unchanged</section>';
  ensureBuildInfo(app, document);
  assert.equal(calls.length, i);
  const menu = app.querySelector('[data-build-info]');
  menu.open = true;
  menu.dispatchEvent(new window.Event('toggle'));
  await wait();
  assert.match(
    menu.textContent,
    process.argv[2] === 'failure' ? /Native identity query failed/ : /Native buildVersion 0\.1\.0/,
  );
  assert.match(menu.textContent, /Update checks not configured/);
  assert.equal(app.querySelector('[data-document]').textContent, 'unchanged');
  assert.equal(menu.querySelectorAll('input,button,a').length, 0);
}
assert.equal(calls.length, 1);
assert.equal(calls[0].command, 'query_build_identity');
assert.deepEqual(calls[0].args, {});
await window.happyDOM.abort();
console.log('one read-only query; cached across rerender; no provider or authored changes');
