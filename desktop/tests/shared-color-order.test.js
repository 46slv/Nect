import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { createSharedColorsPanel } from '../src/shared-colors-panel.js';

test('name sorting is a project-scoped view and does not dispatch document edits', (t) => {
  const win = new Window();
  t.after(() => win.happyDOM.abort());
  win.document.body.innerHTML = '<main id="app"></main>';
  const app = win.document.querySelector('#app');
  const doc = {
    colorSlots: [
      { id: 'z', name: 'Zeta', value: { hex: '#123456', alpha: 1 } },
      { id: 'b', name: 'Beta', value: { hex: '#abcdef', alpha: 1 } },
      { id: 'a', name: 'Beta', value: { hex: '#abcdef', alpha: 1 } },
    ],
    layers: [],
  };
  let project = 1,
    session = { state: { sessionId: 'one', expectedRevision: 3 } };
  const calls = [],
    notices = [];
  const render = () => {
    app.innerHTML = panel.html();
  };
  const panel = createSharedColorsPanel({
    app,
    getSession: () => session,
    getDocument: () => doc,
    getProjectIdentity: () => project,
    dispatch: (...args) => calls.push(args),
    render,
    notify: (...args) => notices.push(args),
  });
  panel.toggle();
  const rows = () =>
    [...app.querySelectorAll('[data-shared-color-row]')].map((row) => row.dataset.sharedColorRow);
  const toggle = () => app.querySelector('[data-shared-color-sort]');
  const initial = structuredClone(doc);
  assert.deepEqual(rows(), ['z', 'b', 'a']);
  assert.equal(toggle().getAttribute('aria-pressed'), 'false');
  toggle().click();
  assert.deepEqual(rows(), ['b', 'a', 'z']);
  assert.equal(toggle().getAttribute('aria-pressed'), 'true');
  assert.equal(toggle(), win.document.activeElement);
  assert.deepEqual(doc, initial);
  assert.equal(calls.length, 0);

  // A different inner Composition session keeps the same project view order.
  session = { state: { sessionId: 'two', expectedRevision: 0 } };
  render();
  assert.deepEqual(rows(), ['b', 'a', 'z']);
  toggle().click();
  assert.deepEqual(rows(), ['z', 'b', 'a']);
  toggle().click();
  assert.deepEqual(rows(), ['b', 'a', 'z']);

  // A project replacement resets order and rejects controls from the old view.
  const stale = toggle();
  project = 2;
  stale.click();
  assert.match(notices.at(-1)[0], /stale/i);
  assert.deepEqual(rows(), ['z', 'b', 'a']);
  assert.equal(toggle().getAttribute('aria-pressed'), 'false');
  assert.deepEqual(doc, initial);
  assert.equal(calls.length, 0);
});

test('finding colors filters only the current project view and handles composition and empty lists', (t) => {
  const win = new Window();
  t.after(() => win.happyDOM.abort());
  win.document.body.innerHTML = '<main id="app"></main>';
  const app = win.document.querySelector('#app');
  const doc = {
    colorSlots: [
      { id: 'a', name: 'Beta', value: { hex: '#123456', alpha: 1 } },
      { id: 'b', name: 'Beta', value: { hex: '#123456', alpha: 1 } },
      { id: 'c', name: 'か\u3099', value: { hex: '#abcdef', alpha: 0 } },
    ],
    layers: [],
  };
  let project = 1;
  const session = { state: { sessionId: 'one', expectedRevision: 3 } };
  const calls = [];
  const render = () => (app.innerHTML = panel.html());
  const panel = createSharedColorsPanel({
    app,
    getSession: () => session,
    getDocument: () => doc,
    getProjectIdentity: () => project,
    dispatch: (...args) => calls.push(args),
    render,
    notify: () => {},
  });
  panel.toggle();
  const visible = () =>
    [...app.querySelectorAll('[data-shared-color-row]')]
      .filter((row) => !row.hidden)
      .map((row) => row.dataset.sharedColorRow);
  const find = app.querySelector('[data-shared-color-find]');
  assert.equal(find.getAttribute('aria-controls'), 'shared-colors-filter');
  find.click();
  let input = app.querySelector('[data-shared-color-filter]');
  assert.equal(win.document.activeElement, input);
  input.value = 'BETA';
  input.dispatchEvent(new win.Event('input', { bubbles: true }));
  assert.deepEqual(visible(), ['a', 'b']);
  assert.equal(app.querySelector('[data-shared-color-filter-status]').textContent, '2 of 3 colors');

  input.dispatchEvent(new win.CompositionEvent('compositionstart', { bubbles: true }));
  input.value = 'が';
  input.dispatchEvent(new win.InputEvent('input', { bubbles: true, isComposing: true }));
  assert.deepEqual(visible(), ['a', 'b']);
  input.dispatchEvent(new win.CompositionEvent('compositionend', { bubbles: true }));
  assert.deepEqual(visible(), ['c']);
  assert.equal(win.document.activeElement, input);
  render();
  input = app.querySelector('[data-shared-color-filter]');
  assert.equal(input.value, 'が');
  assert.deepEqual(visible(), ['c']);

  input.value = 'missing';
  input.dispatchEvent(new win.Event('input', { bubbles: true }));
  assert.deepEqual(visible(), []);
  assert.equal(app.querySelector('[data-shared-color-filter-empty]').hidden, false);
  input.value = 'x'.repeat(100000);
  input.dispatchEvent(new win.Event('input', { bubbles: true }));
  assert.deepEqual(visible(), []);
  assert.equal(doc.colorSlots.length, 3);
  project = 2;
  render();
  assert.equal(app.querySelector('[data-shared-color-filter]').hidden, true);
  assert.deepEqual(visible(), ['a', 'b', 'c']);
  app.querySelector('[data-shared-color-find]').click();
  doc.colorSlots = [];
  render();
  assert.equal(app.querySelector('[data-shared-color-filter-empty]'), null);
  assert.match(app.textContent, /No saved colors/);
  assert.equal(calls.length, 0);
});
