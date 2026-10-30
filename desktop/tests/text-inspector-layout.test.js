import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { arrangeTextInspector, retainTextFontDisclosures } from '../src/text-inspector-layout.js';
import { normalizeSystemFontEntry } from '../src/storage.js';

test('text inspector keeps editing, status and source return visible without replacing controls', () => {
  const w = new Window();
  const root = w.document.createElement('div');
  root.innerHTML = `<div class="viewer-assignment">Output</div><div class="source-navigation"><button>Back</button></div>
    <div class="text-controls"><div class="field"><textarea data-text>日本語</textarea></div>
    <div class="field"><select data-font><option>Font</option></select></div>
    <p role="status">Font unavailable</p><div class="advanced-control">Advanced</div></div>
    <div class="field" data-key="size"><input value="64"></div>`;
  const input = root.querySelector('textarea');
  let edits = 0;
  input.oninput = () => edits++;
  arrangeTextInspector(root);
  assert.equal(root.firstElementChild.className, 'text-controls');
  assert.equal(root.querySelector('textarea'), input);
  input.dispatchEvent(new w.Event('input'));
  assert.equal(edits, 1);
  assert.equal(root.querySelector('[role="status"]').closest('details'), null);
  assert.equal(root.querySelector('.source-navigation').closest('details'), null);
  assert.equal(root.querySelector('.field[data-key="size"]').closest('details'), null);
  assert.equal(root.querySelector('.advanced-control').closest('details').open, false);
  assert.equal(root.querySelectorAll('textarea').length, 1);
});

test('Text identity stays first during normal and character editing without cloning controls', () => {
  for (const editingCharacters of [false, true]) {
    const w = new Window(),
      root = w.document.createElement('div');
    root.innerHTML = `<h3 class="inspector-node-title">Selected Text</h3><p class="inspector-node-type">Node · Text · Path</p>
      <div class="viewer-assignment">Output</div><div class="source-navigation"><button>Back</button></div>
      <div class="text-controls"><div class="field"><textarea data-text>日本語</textarea></div></div>
      <div class="character-panel"><div class="character-actions">Actions</div><div class="character-typography">Typography</div></div>`;
    const title = root.firstElementChild,
      type = title.nextElementSibling,
      text = root.querySelector('textarea'),
      characters = root.querySelector('.character-panel');
    arrangeTextInspector(root, editingCharacters);
    assert.equal(root.firstElementChild, title);
    assert.equal(title.nextElementSibling, type);
    assert.equal(
      type.nextElementSibling.className,
      editingCharacters ? 'character-panel' : 'text-controls',
    );
    assert.equal(root.querySelector('textarea'), text);
    assert.equal(root.querySelectorAll('.character-panel').length, 1);
    assert.equal(root.querySelector('.character-panel'), characters);
    assert.equal(root.querySelector('.source-navigation').closest('details'), null);
  }
});

test('localized installed-font aliases survive IPC normalization with bounded validated immutable values', () => {
  const names = [
    ' メイリオ ',
    'ＭＳ ゴシック',
    'メイリオ',
    '',
    '\u0000bad',
    ...Array(80).fill('Alias'),
  ];
  const row = normalizeSystemFontEntry(['system-1', 'Meiryo', null, { localizedNames: names }]);
  assert.deepEqual(row[3].localizedNames, ['メイリオ', 'ＭＳ ゴシック', 'Alias']);
  assert.ok(Object.isFrozen(row[3].localizedNames));
  assert.equal(row[0], 'system-1');
  assert.equal(row[1], 'Meiryo');
});

test('native Text uses the same primary Font entry with separate import and destructive actions', () => {
  const w = new Window(),
    root = w.document.createElement('div');
  root.innerHTML = `<div class="text-controls"><div class="field"><textarea data-os-text>日本語</textarea></div>
    <button data-apply-os-text>Apply text</button><button data-cancel-os-text>Cancel text edit</button>
    <section class="text-font-choice"><button data-change-font>Change font</button></section>
    <fieldset><input data-os-setting="size"></fieldset>
    <details class="text-font-advanced"><summary>Advanced font import</summary><button data-load-font>Import</button></details>
    <details class="text-actions"><summary>Text actions</summary><button data-bake-text-outline>Convert</button></details>
    <div class="advanced-control">Other typography</div></div>`;
  const textarea = root.querySelector('textarea'),
    font = root.querySelector('.text-font-choice');
  arrangeTextInspector(root);
  assert.equal(root.querySelector('textarea'), textarea);
  assert.equal(root.querySelector('.text-font-choice'), font);
  assert.equal(font.closest('details'), null);
  assert.equal(root.querySelector('[data-apply-os-text]').closest('details'), null);
  assert.equal(root.querySelector('[data-os-setting]').closest('details'), null);
  assert.equal(
    root.querySelector('[data-load-font]').closest('details').className,
    'text-font-advanced',
  );
  assert.equal(root.querySelector('.text-font-advanced').parentElement.className, 'text-controls');
  assert.equal(root.querySelector('.text-actions').parentElement.className, 'text-controls');
  w.happyDOM.abort();
});

test('Font imports and Text actions retain native summary clicks across synchronous rerenders', async () => {
  const w = new Window();
  const root = w.document.createElement('div');
  w.document.body.append(root);
  const render = () => {
    root.innerHTML = `<div class="text-controls"><textarea data-text>Unchanged text</textarea>
      <details class="text-font-advanced"><summary>Advanced font import</summary><button data-load-font="disclosure-fixture">Import</button></details>
      <details class="text-actions"><summary>Text actions</summary><button data-bake-text-outline="disclosure-fixture">Convert</button></details></div>`;
    retainTextFontDisclosures(root);
  };
  w.document.addEventListener('click', (event) => {
    if (event.target.matches('summary')) render();
  });
  render();
  for (const selector of ['.text-font-advanced', '.text-actions']) {
    const old = root.querySelector(selector);
    assert.equal(old.open, false);
    old.querySelector('summary').click();
    assert.equal(old.isConnected, false);
    assert.equal(root.querySelector(selector).open, true);
    // A detached element's queued toggle cannot overwrite the restored state.
    old.dispatchEvent(new w.Event('toggle'));
    await new Promise((resolve) => setTimeout(resolve, 0));
    render();
    assert.equal(root.querySelector(selector).open, true);
    root.querySelector(`${selector} > summary`).click();
    assert.equal(root.querySelector(selector).open, false);
    assert.equal(root.querySelector('textarea').value, 'Unchanged text');
  }
  await w.happyDOM.close();
});
