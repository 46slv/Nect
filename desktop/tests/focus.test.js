import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { captureFocus, restoreFocus } from '../src/focus.js';
test('focus restoration distinguishes graph/inspector and retains text selection without stale targets', () => {
  const win = new Window();
  const root = win.document.body;
  const markup =
    '<div class="graph"><div class="field" data-node="n" data-key="text"><input aria-label="Text" value="graph"></div></div><div class="inspector-body"><div class="field" data-node="n" data-key="text"><input aria-label="Text" value="inspector"></div></div>';
  root.innerHTML = markup;
  const input = root.querySelector('.inspector-body input');
  input.focus();
  input.value = 'draft value';
  input.setSelectionRange(2, 7);
  const snapshot = captureFocus(root);
  root.innerHTML = markup;
  restoreFocus(root, snapshot);
  assert.equal(win.document.activeElement, root.querySelector('.inspector-body input'));
  assert.equal(win.document.activeElement.value, 'draft value');
  assert.equal(win.document.activeElement.selectionStart, 2);
  assert.equal(win.document.activeElement.selectionEnd, 7);
  root.querySelector('.inspector-body input').remove();
  restoreFocus(root, snapshot);
  assert.notEqual(win.document.activeElement, root.querySelector('.graph input'));
});

test('clean number adopts current model while numeric and text drafts survive', () => {
  const win = new Window();
  const root = win.document.body;
  const numberMarkup = (value) => `<input type="number" aria-label="Amount" value="${value}">`;
  const numberCase = (value, badInput, expected) => {
    root.innerHTML = numberMarkup('310');
    const input = root.querySelector('input');
    input.focus();
    input.value = value;
    if (badInput) Object.defineProperty(input, 'validity', { value: { badInput: true } });
    const snapshot = captureFocus(root);
    root.innerHTML = numberMarkup('300');
    restoreFocus(root, snapshot);
    const restored = root.querySelector('input');
    assert.equal(win.document.activeElement, restored);
    assert.equal(restored.defaultValue, '300');
    assert.equal(restored.value, expected);
  };
  numberCase('310', false, '300');
  numberCase('320', false, '320');
  numberCase('', false, '');
  numberCase('310', true, '310');
  root.innerHTML = '<input type="text" aria-label="Expression" value="x">';
  const text = root.querySelector('input');
  text.focus();
  text.value = 'x + 1';
  text.setSelectionRange(1, 3);
  const draft = captureFocus(root);
  root.innerHTML = '<input type="text" aria-label="Expression" value="y">';
  restoreFocus(root, draft);
  assert.equal(root.querySelector('input').value, 'x + 1');
  assert.equal(root.querySelector('input').selectionStart, 1);
  assert.equal(root.querySelector('input').selectionEnd, 3);
  root.innerHTML = '<input type="text" aria-label="Expression" value="y" disabled>';
  restoreFocus(root, draft);
  assert.notEqual(win.document.activeElement, root.querySelector('input'));
});

test('workspace panel focus restores only the same visible section after replacement', () => {
  const win = new Window();
  const root = win.document.body;
  const markup =
    '<aside><section id="workspace-panel-layers" data-workspace-focus-target="layers" tabindex="-1"></section><section id="workspace-panel-inspector" data-workspace-focus-target="inspector" tabindex="-1"></section></aside>';
  root.innerHTML = markup;
  root.querySelector('#workspace-panel-inspector').focus();
  const state = captureFocus(root);
  assert.ok(state);
  root.innerHTML = markup;
  restoreFocus(root, state);
  assert.equal(win.document.activeElement, root.querySelector('#workspace-panel-inspector'));
  root.innerHTML = markup;
  root.querySelector('aside').hidden = true;
  restoreFocus(root, state);
  assert.notEqual(win.document.activeElement, root.querySelector('#workspace-panel-inspector'));
  win.happyDOM.abort();
});

test('shared color action focus follows its stable row and falls back to a visible filter', () => {
  const win = new Window(),
    root = win.document.body;
  const markup = (label, hidden = '') => `<div class="shared-colors-panel">
    <input data-shared-color-filter aria-label="Find saved colors">
    <div data-shared-color-row="a"><button data-shared-color-copy-toggle aria-label="Color actions for same">Actions</button></div>
    <div data-shared-color-row="b" ${hidden}><button data-shared-color-copy-toggle aria-label="Color actions for ${label}">Actions</button></div>
  </div>`;
  root.innerHTML = markup('same');
  root.querySelector('[data-shared-color-row="b"] button').focus();
  const state = captureFocus(root);
  root.innerHTML = markup('renamed');
  restoreFocus(root, state);
  assert.equal(
    win.document.activeElement,
    root.querySelector('[data-shared-color-row="b"] button'),
  );

  root.innerHTML = markup('renamed', 'hidden');
  restoreFocus(root, state);
  assert.equal(win.document.activeElement, root.querySelector('[data-shared-color-filter]'));
  root.querySelector('[data-shared-color-filter]').hidden = true;
  root.querySelector('[data-shared-color-filter]').blur();
  restoreFocus(root, state);
  assert.notEqual(win.document.activeElement, root.querySelector('[data-shared-color-filter]'));
  assert.notEqual(
    win.document.activeElement,
    root.querySelector('[data-shared-color-row="a"] button'),
  );
  win.happyDOM.abort();
});
