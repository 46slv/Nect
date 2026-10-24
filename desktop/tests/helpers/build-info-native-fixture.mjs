import assert from 'node:assert/strict';
import { Window } from 'happy-dom';

const window = new Window();
globalThis.window = window;
globalThis.isTauri = true;
const calls = [];
const mode = process.argv[2];
// Same public checkout value used by the CI receipt, including PR merge checkouts.
const receiptCommit =
  process.env.GITHUB_ACTIONS === 'true'
    ? process.env.GITHUB_SHA
    : 'd0d9c9265a1628983f727715bc205a44aaa5c0a6';
assert.match(receiptCommit, /^[0-9a-f]{40}$/);
assert.equal(receiptCommit.length, 40);
const identity = Object.freeze({
  authority: 'native',
  integrity: 'verified',
  appVersion: '0.1.0',
  buildFlavor: 'normal',
  assetMode: 'static',
  runtimeIsolation: 'ordinary',
  channel: null,
  commit:
    mode === 'missing-commit'
      ? null
      : mode === 'invalid-commit'
        ? '<script>invalid</script>'
        : receiptCommit,
  buildId: receiptCommit,
  ...(mode === 'newline-commit' ? { commit: `${receiptCommit}\n` } : {}),
  ...(mode === 'invalid-identity' ? { integrity: 'invalid' } : {}),
  ...(mode === 'browser-identity' ? { authority: 'browser' } : {}),
  ...(mode === 'dev-server' ? { assetMode: 'dev-server' } : {}),
});
window.__TAURI_INTERNALS__ = {
  invoke: async (command, args) => {
    calls.push({ command, args });
    if (mode === 'failure') throw Error('Query failed');
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
  const runtime =
    mode === 'failure'
      ? 'Native identity query failed'
      : mode === 'invalid-identity'
        ? 'Native identity invalid'
        : mode === 'browser-identity'
          ? 'Browser preview'
          : mode === 'dev-server'
            ? 'Native development build'
            : 'Native build';
  assert.ok(menu.textContent.includes(runtime));
  const trusted = mode === 'success' || mode === 'dev-server';
  const commit = menu.querySelector('[data-build-commit]');
  assert.equal(
    commit.textContent,
    trusted ? `Commit ${receiptCommit.slice(0, 8)}` : 'Commit unknown',
  );
  assert.equal(commit.getAttribute('title'), trusted ? receiptCommit : null);
  assert.match(menu.textContent, /Update checks not configured/);
  assert.equal(app.querySelector('[data-document]').textContent, 'unchanged');
  assert.equal(menu.querySelectorAll('input,button,a').length, 0);
}
assert.equal(calls.length, 1);
assert.equal(calls[0].command, 'query_build_identity');
assert.deepEqual(calls[0].args, {});
await window.happyDOM.abort();
if (mode === 'success')
  console.log(`receipt checkout ${receiptCommit}; rendered Commit ${receiptCommit.slice(0, 8)}`);
console.log('one read-only query; cached across rerender; no provider or authored changes');
