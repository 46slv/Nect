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
function fixture(
  t,
  { registerResult = { roots: [{ id: 'new', name: 'new' }], selectedRootId: 'new' } } = {},
) {
  const initial = deferred(),
    refresh = deferred(),
    calls = [];
  handler = (cmd, args) => {
    calls.push({ cmd, args });
    switch (cmd) {
      case 'list_asset_folders':
        return initial.promise;
      case 'register_asset_folder':
        return registerResult;
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

test('search while folder bootstrap is pending keeps the registered folders', async (t) => {
  const f = fixture(t);
  const search = f.panel.querySelector('[data-asset-search]');
  search.value = 'demo';
  search.dispatchEvent(new win.Event('input', { bubbles: true }));
  f.initial.resolve([{ id: 'new', name: 'new' }]);
  await tick();
  await tick();
  assert.deepEqual(
    [...f.root.options].map((option) => option.value),
    ['new'],
  );
  assert.equal(search.value, 'demo');
  await waitFor(() => f.calls.some((entry) => entry.cmd === 'refresh_asset_folder'));
  f.refresh.resolve({
    ...listing('new'),
    items: [
      { relativePath: 'demo.svg', bytes: 20 },
      { relativePath: 'other.svg', bytes: 20 },
    ],
  });
  await waitFor(() => f.status.textContent.includes('1 assets'));
  assert.deepEqual(
    [...f.panel.querySelectorAll('[data-path]')].map((item) => item.dataset.path),
    ['demo.svg'],
  );
});

for (const listFirst of [false, true])
  test(`cancelled folder picker preserves pending bootstrap (list first: ${listFirst})`, async (t) => {
    const registration = deferred();
    const f = fixture(t, { registerResult: registration.promise });
    f.add.click();
    await waitFor(() => f.calls.some((entry) => entry.cmd === 'register_asset_folder'));
    if (listFirst) {
      f.initial.resolve([{ id: 'new', name: 'new' }]);
      await tick();
    }
    registration.resolve(null);
    await tick();
    if (!listFirst) f.initial.resolve([{ id: 'new', name: 'new' }]);
    await tick();
    await tick();
    assert.deepEqual(
      [...f.root.options].map((option) => option.value),
      ['new'],
    );
    await waitFor(() => f.calls.some((entry) => entry.cmd === 'refresh_asset_folder'));
    f.refresh.resolve(listing('new'));
    await waitFor(() => f.status.textContent.includes('0 assets'));
    assert.equal(f.add.disabled, false);
  });

for (const listFirst of [false, true])
  test(`failed folder picker remains visible after bootstrap (list first: ${listFirst})`, async (t) => {
    const registration = deferred();
    const f = fixture(t, { registerResult: registration.promise });
    f.add.click();
    await waitFor(() => f.calls.some((entry) => entry.cmd === 'register_asset_folder'));
    if (listFirst) {
      f.initial.resolve([{ id: 'new', name: 'new' }]);
      await tick();
    }
    registration.reject(Error('Could not add folder'));
    await tick();
    if (!listFirst) f.initial.resolve([{ id: 'new', name: 'new' }]);
    await waitFor(() => f.calls.some((entry) => entry.cmd === 'refresh_asset_folder'));
    f.refresh.resolve(listing('new'));
    await waitFor(() => !f.add.disabled);
    assert.equal(f.root.value, 'new');
    assert.equal(f.status.textContent, 'Could not add folder');
    assert.equal(f.status.dataset.error, 'true');
    const refresh = [...f.panel.querySelectorAll('button')].find(
      (b) => b.textContent === 'Refresh',
    );
    refresh.click();
    await waitFor(() => f.status.textContent.includes('0 assets'));
    assert.equal(f.status.dataset.error, 'false');
  });

test('bootstrap received during a picker cannot overwrite a later successful folder choice', async (t) => {
  const registration = deferred();
  const f = fixture(t, { registerResult: registration.promise });
  f.add.click();
  await waitFor(() => f.calls.some((entry) => entry.cmd === 'register_asset_folder'));
  f.initial.resolve([{ id: 'old', name: 'old' }]);
  await tick();
  registration.resolve({ roots: [{ id: 'new', name: 'new' }], selectedRootId: 'new' });
  await waitFor(() => f.calls.some((entry) => entry.cmd === 'refresh_asset_folder'));
  f.refresh.resolve(listing('new'));
  await waitFor(() => f.status.textContent.includes('0 assets'));
  assert.deepEqual(
    [...f.root.options].map((option) => option.value),
    ['new'],
  );
  assert.equal(f.root.value, 'new');
});

test('closing during folder discovery prevents late bootstrap from refreshing', async (t) => {
  const f = fixture(t);
  closeAssetLibrary();
  f.initial.resolve([{ id: 'new', name: 'new' }]);
  await tick();
  await tick();
  assert.equal(f.panel.isConnected, false);
  assert.deepEqual(
    f.calls.map((entry) => entry.cmd),
    ['list_asset_folders'],
  );
});

test('invalid buffered folder response is reported after picker cancellation', async (t) => {
  const registration = deferred();
  const f = fixture(t, { registerResult: registration.promise });
  f.add.click();
  f.initial.resolve(null);
  await tick();
  registration.resolve(null);
  await waitFor(() => f.status.textContent === 'Invalid asset folder list');
  assert.equal(f.status.dataset.error, 'true');
  assert.equal(f.add.disabled, false);
  assert.equal(
    f.calls.some((entry) => entry.cmd === 'refresh_asset_folder'),
    false,
  );
});

test('bootstrap failure does not hide a concurrent folder registration failure', async (t) => {
  const registration = deferred();
  const f = fixture(t, { registerResult: registration.promise });
  f.add.click();
  f.initial.reject(Error('Could not read folders'));
  await tick();
  registration.reject(Error('Could not add folder'));
  await waitFor(() => f.status.textContent.includes('Could not read folders'));
  assert.match(f.status.textContent, /Could not add folder/);
  assert.equal(f.status.dataset.error, 'true');
});

test.after(() => {
  closeAssetLibrary();
  clearMocks();
  delete globalThis.isTauri;
  delete globalThis.window;
  delete globalThis.document;
  win.happyDOM.abort();
});
