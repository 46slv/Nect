import test, { after } from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { mockIPC, clearMocks } from '@tauri-apps/api/mocks';

const win = new Window();
globalThis.window = win;
globalThis.document = win.document;
globalThis.isTauri = true;
let handler;
mockIPC((command, payload) => handler(command, payload));
const { mountAssetLibrary, closeAssetLibrary } = await import('../src/asset-library.js');
after(() => {
  clearMocks();
  win.happyDOM.abort();
});
async function settle(predicate) {
  for (let i = 0; i < 30 && !predicate(); i++)
    await new Promise((resolve) => setImmediate(resolve));
  assert.ok(predicate(), 'expected root catalog transition');
}
function mount(registered, { removeResponse } = {}) {
  let roots = structuredClone(registered);
  const calls = [];
  handler = (command, payload) => {
    calls.push({ command, payload });
    if (command === 'list_asset_folders') return roots;
    if (command === 'refresh_asset_folder') {
      const root = roots.find((entry) => entry.id === payload.rootId);
      assert.ok(root, 'refresh must use an existing opaque ID');
      return { root, generation: 'fixture', items: [], warnings: [] };
    }
    if (command === 'remove_asset_folder') {
      if (removeResponse) return removeResponse;
      roots = roots.filter((entry) => entry.id !== payload.rootId);
      return roots;
    }
    throw Error('Unexpected IPC: ' + command);
  };
  const options = {
    capture: () => assert.fail('choosing roots is not a document edit'),
    insert: () => assert.fail('choosing roots must not place artwork'),
  };
  const panel = mountAssetLibrary(options);
  const select = panel.querySelector('[aria-label="Asset folder"]');
  return { panel, select, calls, options };
}
const choices = (select) =>
  [...select.options].map((option) => ({
    id: option.value,
    text: option.textContent,
    title: option.title,
  }));

test('same-name native roots expose locations and still select/remove exact IDs', async (t) => {
  const registered = [
    { id: 'root-a', name: 'assets', path: 'C:\\ProjectA\\assets' },
    { id: 'root-b', name: 'assets', path: 'C:\\ProjectB\\assets' },
  ];
  const f = mount(registered);
  t.after(closeAssetLibrary);
  await settle(() => !f.select.disabled && f.select.options.length === 2);
  const labels = choices(f.select);
  assert.notEqual(labels[0].text, labels[1].text);
  assert.match(labels[0].text, /ProjectA/);
  assert.match(labels[1].text, /ProjectB/);
  assert.deepEqual(
    labels.map((entry) => entry.id),
    ['root-a', 'root-b'],
  );
  assert.equal(labels[1].title, registered[1].path);
  f.select.value = 'root-b';
  f.select.dispatchEvent(new win.Event('change', { bubbles: true }));
  await settle(() => !f.select.disabled);
  assert.deepEqual(f.calls.at(-1), {
    command: 'refresh_asset_folder',
    payload: { rootId: 'root-b' },
  });
  [...f.panel.querySelectorAll('button')].find((button) => button.textContent === 'Remove').click();
  await settle(() => !f.select.disabled && f.select.options.length === 1);
  assert.equal(f.select.value, 'root-a');
  assert.deepEqual(f.calls.find((call) => call.command === 'remove_asset_folder').payload, {
    rootId: 'root-b',
  });
  assert.equal(f.select.options[0].textContent, 'assets', 'unique names remain compact');
  assert.deepEqual(registered, [
    { id: 'root-a', name: 'assets', path: 'C:\\ProjectA\\assets' },
    { id: 'root-b', name: 'assets', path: 'C:\\ProjectB\\assets' },
  ]);
});

test('pathless same-name roots keep session labels through refresh and panel remount', async (t) => {
  const roots = ['one', 'two', 'three'].map((id) => ({ id, name: 'assets' }));
  const f = mount(roots);
  t.after(closeAssetLibrary);
  await settle(() => !f.select.disabled && f.select.options.length === 3);
  const initial = choices(f.select);
  assert.equal(new Set(initial.map((entry) => entry.text)).size, 3);
  assert.ok(
    initial.every((entry) => !entry.text.includes('C:')),
    'never invent browser paths',
  );
  f.select.value = 'two';
  f.select.dispatchEvent(new win.Event('change', { bubbles: true }));
  await settle(() => !f.select.disabled);
  assert.deepEqual(choices(f.select), initial);
  closeAssetLibrary();
  const reopened = mountAssetLibrary(f.options);
  const select = reopened.querySelector('[aria-label="Asset folder"]');
  await settle(() => !select.disabled && select.options.length === 3);
  assert.deepEqual(choices(select), initial);
  select.value = 'one';
  [...reopened.querySelectorAll('button')]
    .find((button) => button.textContent === 'Remove')
    .click();
  await settle(() => !select.disabled && select.options.length === 2);
  assert.deepEqual(choices(select), initial.slice(1), 'remaining session labels do not renumber');
});

test('unique roots retain literal names and path hints without interpreting markup', async (t) => {
  const f = mount([
    { id: 'literal', name: '<img src=x onerror=alert(1)>', path: '/Project A/<assets>' },
    { id: 'other', name: 'Other' },
  ]);
  t.after(closeAssetLibrary);
  await settle(() => !f.select.disabled && f.select.options.length === 2);
  assert.equal(f.select.options[0].textContent, '<img src=x onerror=alert(1)>');
  assert.equal(f.select.options[0].title, '/Project A/<assets>');
  assert.equal(f.select.querySelectorAll('img,script').length, 0);
  assert.equal(f.select.options[1].textContent, 'Other');
});

test('visually equivalent names and generated-looking names remain distinguishable', async (t) => {
  const f = mount([
    { id: 'unicode-a', name: 'café', path: '\\\\server\\shareA\\café' },
    { id: 'unicode-b', name: 'cafe\u0301', path: '\\\\server\\shareB\\cafe\u0301' },
    { id: 'ordinal-name', name: '1. café' },
  ]);
  t.after(closeAssetLibrary);
  await settle(() => !f.select.disabled && f.select.options.length === 3);
  const labels = choices(f.select);
  assert.equal(new Set(labels.map((entry) => entry.text.normalize('NFC'))).size, 3);
  assert.match(labels[0].text, /shareA/);
  assert.match(labels[1].text, /shareB/);
});

test('late Remove from a closed panel cannot prune a newer catalog label or refresh it', async (t) => {
  let finishRemoval;
  const removeResponse = new Promise((resolve) => {
    finishRemoval = resolve;
  });
  const a = { id: 'late-a', name: 'assets' },
    b = { id: 'late-b', name: 'assets' },
    c = { id: 'late-c', name: 'assets' };
  const old = mount([a, b], { removeResponse });
  t.after(closeAssetLibrary);
  await settle(() => !old.select.disabled && old.select.options.length === 2);
  old.select.value = a.id;
  [...old.panel.querySelectorAll('button')]
    .find((button) => button.textContent === 'Remove')
    .click();
  await settle(() => old.calls.some((call) => call.command === 'remove_asset_folder'));
  closeAssetLibrary();
  const current = mount([b, c]);
  await settle(() => !current.select.disabled && current.select.options.length === 2);
  const expected = choices(current.select),
    calls = current.calls.length;
  finishRemoval([b]);
  await new Promise((resolve) => setImmediate(resolve));
  await new Promise((resolve) => setImmediate(resolve));
  assert.equal(current.calls.length, calls, 'closed Remove must not issue a new refresh');
  closeAssetLibrary();
  const reopened = mountAssetLibrary(current.options);
  const select = reopened.querySelector('[aria-label="Asset folder"]');
  await settle(() => !select.disabled && select.options.length === 2);
  assert.deepEqual(choices(select), expected);
});

test('whitespace-normalized root names get distinct visible labels without renaming roots', async (t) => {
  const f = mount([
    { id: 'space-a', name: 'assets' },
    { id: 'space-b', name: ' assets  ' },
  ]);
  t.after(closeAssetLibrary);
  await settle(() => !f.select.disabled && f.select.options.length === 2);
  const labels = choices(f.select);
  assert.notEqual(labels[0].text.trim(), labels[1].text.trim());
  assert.match(labels[1].text, / assets  $/);
});
