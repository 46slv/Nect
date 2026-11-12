import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { mockIPC, clearMocks } from '@tauri-apps/api/mocks';

const win = new Window({ url: 'http://localhost/' });
globalThis.window = win;
globalThis.document = win.document;
globalThis.isTauri = true;
let handler;
mockIPC((command, payload) => handler(command, payload));
const { mountAssetLibrary, closeAssetLibrary, ASSET_FOLDER_FAVORITES_KEY } = await import(
  '../src/asset-library.js'
);
const tick = () => new Promise((resolve) => setImmediate(resolve));
async function waitFor(predicate) {
  for (let i = 0; i < 50 && !predicate(); i++) await tick();
  assert.ok(predicate(), 'expected asset transition');
}
const deferred = () => {
  let resolve, reject;
  const promise = new Promise((yes, no) => {
    resolve = yes;
    reject = no;
  });
  return { promise, resolve, reject };
};
const a = { id: 'root-a', name: 'assets', path: 'C:\\A\\assets' };
const b = { id: 'root-b', name: 'assets', path: 'C:\\B\\assets' };
const replacement = { id: 'root-new', name: 'assets', path: 'C:\\A\\assets' };
const listing = (root) => ({ root, generation: 'test', items: [], warnings: [] });
function fixture(t, initial = []) {
  const data = new Map([[ASSET_FOLDER_FAVORITES_KEY, JSON.stringify(initial)]]);
  let failRead = false;
  let failWrite = false;
  const storage = {
    getItem(key) {
      if (failRead) throw Error('read denied');
      return data.get(key) ?? null;
    },
    setItem(key, value) {
      if (failWrite) throw Error('write denied');
      data.set(key, value);
    },
  };
  Object.defineProperty(win, 'localStorage', { value: storage, configurable: true });
  let roots = [a, b];
  let listRoots = () => roots;
  let refresh = (id) => listing(roots.find((root) => root.id === id));
  const calls = [];
  handler = (command, payload) => {
    calls.push({ command, payload });
    if (command === 'list_asset_folders') return listRoots();
    if (command === 'refresh_asset_folder') return refresh(payload.rootId);
    if (command === 'remove_asset_folder') {
      roots = roots.filter((root) => root.id !== payload.rootId);
      return roots;
    }
    throw Error(`Unexpected IPC ${command}`);
  };
  const options = {
    capture: () => assert.fail('folder preferences must not capture document state'),
    insert: () => assert.fail('folder preferences must not insert artwork'),
  };
  const mount = () => {
    const panel = mountAssetLibrary(options);
    return {
      panel,
      root: panel.querySelector('[aria-label="Asset folder"]'),
      status: panel.querySelector('[data-asset-status]'),
      pin: () =>
        [...panel.querySelectorAll('button')].find((button) =>
          /^(Unpin|Pin) folder$/.test(button.textContent),
        ),
      quick: (id) =>
        [...panel.querySelectorAll('[data-quick-open]')].find(
          (button) => button.dataset.quickOpen === id,
        ),
      removeFavorite: (id) =>
        [...panel.querySelectorAll('[data-quick-remove]')].find(
          (button) => button.dataset.quickRemove === id,
        ),
    };
  };
  t.after(() => closeAssetLibrary());
  return {
    mount,
    calls,
    data,
    setRoots(value) {
      roots = value;
    },
    setList(value) {
      listRoots = value;
    },
    setRefresh(value) {
      refresh = value;
    },
    failRead(value) {
      failRead = value;
    },
    failWrite(value) {
      failWrite = value;
    },
    saved() {
      return JSON.parse(data.get(ASSET_FOLDER_FAVORITES_KEY));
    },
  };
}
async function ready(view) {
  await waitFor(() => !view.root.disabled && view.root.options.length > 0);
}
function choose(view, id) {
  view.root.value = id;
  view.root.dispatchEvent(new win.Event('change', { bubbles: true }));
}

test('native favorites survive remount, use exact IDs, and remove stale IDs without retargeting', async (t) => {
  const f = fixture(t);
  let view = f.mount();
  await ready(view);
  view.pin().click();
  assert.deepEqual(f.saved(), ['root-a']);
  choose(view, 'root-b');
  await ready(view);
  view.pin().click();
  assert.deepEqual(f.saved(), ['root-a', 'root-b']);
  assert.equal(view.pin().textContent, 'Unpin folder');
  closeAssetLibrary();
  view = f.mount();
  await ready(view);
  assert.ok(view.quick('root-a'));
  assert.ok(view.quick('root-b'));
  view.quick('root-b').click();
  await ready(view);
  assert.equal(view.root.value, 'root-b');
  assert.equal(f.calls.at(-1).payload.rootId, 'root-b');
  closeAssetLibrary();
  // A same-name, same-path registration after removal is still a different ID.
  f.setRoots([replacement, b]);
  view = f.mount();
  await ready(view);
  assert.equal(view.quick('root-a'), undefined);
  assert.match(view.removeFavorite('root-a').textContent, /Unavailable folder root-a/);
  assert.equal(view.quick('root-new'), undefined);
  const before = f.calls.length;
  view.removeFavorite('root-a').click();
  assert.equal(f.calls.length, before);
  assert.deepEqual(f.saved(), ['root-b']);
  assert.equal(view.quick('root-a'), undefined);
  assert.ok(view.quick('root-b'));
  view.quick('root-b').click();
  await ready(view);
  view.pin().click();
  assert.deepEqual(f.saved(), []);
  assert.equal(view.quick('root-b'), undefined);
});

test('failed saves leave both stored and visible favorites unchanged, even after refresh', async (t) => {
  const f = fixture(t);
  const view = f.mount();
  await ready(view);
  f.failWrite(true);
  view.pin().click();
  assert.deepEqual(f.saved(), []);
  assert.equal(view.pin().textContent, 'Pin folder');
  assert.equal(view.quick('root-a'), undefined);
  assert.match(view.status.textContent, /Quick access could not be saved/);
  choose(view, 'root-b');
  await ready(view);
  assert.match(view.status.textContent, /Quick access could not be saved/);
  f.failWrite(false);
  view.pin().click();
  assert.deepEqual(f.saved(), ['root-b']);
  assert.equal(view.pin().textContent, 'Unpin folder');
  f.failWrite(true);
  view.pin().click();
  assert.deepEqual(f.saved(), ['root-b']);
  assert.equal(view.pin().textContent, 'Unpin folder');
  assert.ok(view.quick('root-b'));
  closeAssetLibrary();
  const reopened = f.mount();
  await ready(reopened);
  assert.ok(reopened.quick('root-b'));
});

test('unreadable or malformed settings cannot be overwritten by a pin', async (t) => {
  const f = fixture(t);
  f.data.set(ASSET_FOLDER_FAVORITES_KEY, '{invalid');
  let view = f.mount();
  await ready(view);
  assert.equal(view.pin().disabled, true);
  assert.match(view.status.textContent, /Quick access could not be loaded/);
  view.pin().click();
  assert.equal(f.data.get(ASSET_FOLDER_FAVORITES_KEY), '{invalid');
  closeAssetLibrary();
  f.data.set(ASSET_FOLDER_FAVORITES_KEY, '');
  view = f.mount();
  await ready(view);
  assert.equal(view.pin().disabled, true);
  assert.match(view.status.textContent, /Quick access could not be loaded/);
  assert.equal(f.data.get(ASSET_FOLDER_FAVORITES_KEY), '');
  closeAssetLibrary();
  f.failRead(true);
  view = f.mount();
  await ready(view);
  assert.equal(view.pin().disabled, true);
  assert.match(view.status.textContent, /Quick access could not be loaded/);
  assert.equal(f.data.get(ASSET_FOLDER_FAVORITES_KEY), '');
});

test('pending or failed registry discovery never prunes saved IDs', async (t) => {
  const f = fixture(t, ['root-a']);
  const bootstrap = deferred();
  f.setList(() => bootstrap.promise);
  const view = f.mount();
  assert.match(view.removeFavorite('root-a').textContent, /Checking folder root-a/);
  assert.deepEqual(f.saved(), ['root-a']);
  bootstrap.reject(Error('registry unavailable'));
  await waitFor(() => view.status.textContent.includes('registry unavailable'));
  assert.match(view.removeFavorite('root-a').textContent, /Unavailable folder root-a/);
  assert.deepEqual(f.saved(), ['root-a']);
  view.removeFavorite('root-a').click();
  assert.deepEqual(f.saved(), []);
});

test('unregister keeps the favorite unavailable until explicitly removed', async (t) => {
  const f = fixture(t, ['root-a', 'root-b']);
  const view = f.mount();
  await ready(view);
  [...view.panel.querySelectorAll('button')]
    .find((button) => button.textContent === 'Remove')
    .click();
  await waitFor(() => !view.root.disabled && view.root.options.length === 1);
  assert.equal(view.root.value, 'root-b');
  assert.deepEqual(f.saved(), ['root-a', 'root-b']);
  assert.equal(view.quick('root-a'), undefined);
  assert.match(view.removeFavorite('root-a').textContent, /Unavailable folder root-a/);
  view.removeFavorite('root-a').click();
  assert.deepEqual(f.saved(), ['root-b']);
});

test('missing physical folder stays pinned and can be retried under its exact ID', async (t) => {
  const f = fixture(t, ['root-a']);
  f.setRefresh(() => {
    throw Error('Registered folder moved');
  });
  const view = f.mount();
  await ready(view);
  assert.match(view.quick('root-a').textContent, /Unavailable/);
  assert.match(view.status.textContent, /Registered folder moved/);
  assert.deepEqual(f.saved(), ['root-a']);
  f.setRefresh((id) => listing(id === a.id ? a : b));
  view.quick('root-a').click();
  await ready(view);
  assert.doesNotMatch(view.quick('root-a').textContent, /Unavailable/);
  assert.equal(f.calls.at(-1).payload.rootId, 'root-a');
});

test('a late closed-panel refresh cannot alter a new panel or pin state', async (t) => {
  const f = fixture(t, ['root-b']);
  const blocked = deferred();
  let delay = true;
  f.setRefresh((id) => (id === 'root-b' && delay ? blocked.promise : listing(id === a.id ? a : b)));
  const old = f.mount();
  await ready(old);
  old.quick('root-b').click();
  await waitFor(() => old.root.disabled);
  closeAssetLibrary();
  delay = false;
  const current = f.mount();
  await ready(current);
  const status = current.status.textContent;
  const calls = f.calls.length;
  blocked.reject(Error('late failure'));
  await tick();
  await tick();
  assert.equal(old.panel.isConnected, false);
  assert.equal(current.root.value, 'root-a');
  assert.equal(current.status.textContent, status);
  assert.equal(f.calls.length, calls);
  assert.deepEqual(f.saved(), ['root-b']);
  assert.ok(current.quick('root-b'));
});

test('mismatched refresh response is rejected instead of retargeting a favorite', async (t) => {
  const f = fixture(t, ['root-b']);
  const view = f.mount();
  await ready(view);
  f.setRefresh(() => listing(a));
  view.quick('root-b').click();
  await ready(view);
  assert.equal(view.root.value, 'root-b');
  assert.match(view.status.textContent, /Invalid asset folder response/);
  assert.deepEqual(f.saved(), ['root-b']);
});

test.after(() => {
  closeAssetLibrary();
  clearMocks();
  delete globalThis.isTauri;
  delete globalThis.window;
  delete globalThis.document;
  win.happyDOM.abort();
});
