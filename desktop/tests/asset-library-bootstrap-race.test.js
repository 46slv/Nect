import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { mockIPC, clearMocks } from '@tauri-apps/api/mocks';

const win = new Window();
globalThis.window = win;
globalThis.document = win.document;
globalThis.isTauri = true;
let handler = () => {
  throw Error('Unexpected IPC before test');
};
mockIPC((command, payload) => handler(command, payload));
const { desktop } = await import('../src/storage.js');
assert.equal(desktop, true);
const { mountAssetLibrary, closeAssetLibrary } = await import('../src/asset-library.js');
const deferred = () => {
  let resolve, reject;
  const promise = new Promise((yes, no) => {
    resolve = yes;
    reject = no;
  });
  return { promise, resolve, reject };
};
const tick = () => new Promise((resolve) => setImmediate(resolve));
async function waitFor(predicate) {
  for (let i = 0; i < 30 && !predicate(); i++) await tick();
  assert.ok(predicate(), 'expected IPC/UI transition');
}
const listing = (id) => ({ root: { id, name: id }, generation: 'test', items: [], warnings: [] });
function fixture(t) {
  const initial = deferred(),
    refresh = deferred(),
    calls = [];
  handler = (cmd, args) => {
    calls.push({ cmd, args });
    switch (cmd) {
      case 'list_asset_folders':
        return initial.promise;
      case 'register_asset_folder':
        return { roots: [{ id: 'new', name: 'new' }], selectedRootId: 'new' };
      case 'refresh_asset_folder':
        if (args.rootId === 'new') return refresh.promise;
        throw Error('Unexpected refresh ' + args.rootId);
      default:
        throw Error('Unexpected IPC ' + cmd);
    }
  };
  const panel = mountAssetLibrary({
    capture() {
      throw Error('unexpected capture');
    },
    insert() {
      throw Error('unexpected insert');
    },
  });
  t.after(() => closeAssetLibrary());
  const add = [...panel.querySelectorAll('button')].find((b) => b.textContent === 'Add folder');
  const root = panel.querySelector('[aria-label="Asset folder"]'),
    status = panel.querySelector('[data-asset-status]');
  return { initial, refresh, calls, panel, add, root, status };
}
for (const kind of ['old-success', 'old-failure'])
  test(`stale bootstrap ${kind} cannot replace a newer folder`, async (t) => {
    const f = fixture(t);
    assert.equal(f.add.disabled, false);
    f.add.click();
    await waitFor(() => f.calls.some((x) => x.cmd === 'refresh_asset_folder'));
    assert.equal(f.root.value, 'new');
    f.refresh.resolve(listing('new'));
    await waitFor(() => f.status.textContent.includes('0 assets'));
    const before = f.status.textContent,
      count = f.calls.length;
    if (kind === 'old-success') f.initial.resolve([{ id: 'old', name: 'old' }]);
    else f.initial.reject(Error('stale initial failure'));
    await tick();
    await tick();
    assert.equal(f.root.value, 'new');
    assert.deepEqual(
      [...f.root.options].map((x) => x.value),
      ['new'],
    );
    assert.equal(f.status.textContent, before);
    assert.equal(f.calls.length, count);
  });
test('stale bootstrap cannot interrupt a newer refresh still in flight', async (t) => {
  const f = fixture(t);
  f.add.click();
  await waitFor(() => f.calls.some((x) => x.cmd === 'refresh_asset_folder'));
  assert.equal(f.root.value, 'new');
  const before = f.status.textContent,
    count = f.calls.length;
  assert.equal(f.add.disabled, true);
  f.initial.resolve([{ id: 'old', name: 'old' }]);
  await tick();
  await tick();
  assert.equal(f.root.value, 'new');
  assert.equal(f.status.textContent, before);
  assert.equal(f.calls.length, count);
  assert.equal(f.add.disabled, true);
  f.refresh.resolve(listing('new'));
  await waitFor(() => f.status.textContent.includes('0 assets'));
  assert.equal(f.root.value, 'new');
  assert.equal(f.add.disabled, false);
});
test('normal empty initial catalog succeeds and leaves Add available', async (t) => {
  const f = fixture(t);
  f.initial.resolve([]);
  await waitFor(() => f.status.textContent === 'Add an asset folder');
  assert.equal(f.add.disabled, false);
  assert.deepEqual(
    f.calls.map((x) => x.cmd),
    ['list_asset_folders'],
  );
});
test('normal nonempty initial catalog can be selected and refreshed', async (t) => {
  const f = fixture(t);
  f.initial.resolve([{ id: 'new', name: 'new' }]);
  await waitFor(() => [...f.root.options].some((option) => option.value === 'new'));
  const manualSelection = !f.calls.some((x) => x.cmd === 'refresh_asset_folder');
  t.diagnostic(`initial refresh required synthetic change: ${manualSelection}`);
  if (manualSelection) {
    assert.equal(f.root.disabled, false);
    f.root.value = 'new';
    f.root.dispatchEvent(new win.Event('change', { bubbles: true }));
  }
  await waitFor(() => f.calls.some((x) => x.cmd === 'refresh_asset_folder'));
  f.refresh.resolve(listing('new'));
  await waitFor(() => f.status.textContent.includes('0 assets'));
  assert.equal(f.root.value, 'new');
});

test.after(() => {
  closeAssetLibrary();
  clearMocks();
  delete globalThis.isTauri;
  delete globalThis.window;
  delete globalThis.document;
  win.happyDOM.abort();
});
