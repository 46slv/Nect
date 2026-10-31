import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { definitions, document as makeDocument, node, layer, serialize } from '../src/core.js';
import { nodeCategories, nodePickerItems, searchNodeItems } from '../src/node-catalog.js';

// Synthetic event/DOM checks, not real IME, rendering or Windows acceptance.
test('category picker scopes candidates only; keyboard, cancellation, stale elements and shared font filters', async (t) => {
  const win = new Window();
  globalThis.document = win.document;
  t.after(() => win.happyDOM.abort());
  const source = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  const start = source.indexOf('function openSearchPanel(');
  const end = source.indexOf('\n}\n', start) + 2;
  const { openSearchPanel } = await import(
    'data:text/javascript;base64,' +
      Buffer.from(source.slice(start, end) + '\nexport { openSearchPanel };').toString('base64')
  );
  const $ = (s) => document.querySelector(s);
  const key = (target, key, extra = {}) =>
    target.dispatchEvent(
      new win.KeyboardEvent('keydown', { bubbles: true, cancelable: true, key, ...extra }),
    );
  const trigger = document.createElement('button');
  document.body.append(trigger);
  trigger.focus();
  let commits = 0,
    cancels = 0,
    fallback = 0,
    picker;
  const open = () => {
    picker = openSearchPanel({
      title: 'ノードを追加',
      action: '追加',
      context: '作成先: Test',
      categories: nodeCategories,
      items: nodePickerItems(definitions),
      filterItems: searchNodeItems,
      onCommit: () => {
        commits++;
        picker.close();
      },
      onCancel: () => cancels++,
      fallbackFocus: () => fallback++,
    });
    return new Promise(queueMicrotask);
  };
  await open();
  const input = $('input'),
    select = $('select');
  const search = (value) => {
    input.value = value;
    input.dispatchEvent(new win.Event('input'));
  };
  const scope = (value) => {
    select.value = value;
    select.dispatchEvent(new win.Event('change'));
  };
  assert.equal($('.picker-list').getAttribute('role'), 'tree');
  assert.equal(document.querySelectorAll('.picker-category').length, 8);
  assert.equal($('.picker-option[data-id="Transform"]'), null);
  const list = $('.picker-list');
  list.scrollTop = 91;
  $('.picker-option[data-id="Circle"]').click();
  assert.equal(list.scrollTop, 91, 'ordinary candidate selection preserves the list viewport');
  const category = () => $('.picker-category[data-category="value"]');
  for (let i = 0; i < 3; i++) {
    category().querySelector('.picker-category-heading').click();
    assert.equal(category().getAttribute('aria-expanded'), 'true');
    assert.ok($('.picker-option[data-id="Value"]'));
    key(category(), 'ArrowLeft');
    assert.equal(category().getAttribute('aria-expanded'), 'false');
  }
  key(category(), 'Enter', { isComposing: true });
  key(category(), 'Enter', { keyCode: 229 });
  assert.equal(category().getAttribute('aria-expanded'), 'false');
  key(category(), 'ArrowRight');
  assert.equal(category().getAttribute('aria-expanded'), 'true');
  key(category(), ' ', { repeat: true });
  assert.equal(category().getAttribute('aria-expanded'), 'true');
  key(category(), ' ');
  assert.equal(category().getAttribute('aria-expanded'), 'false');
  $('.picker-category[data-category="create"] .picker-category-heading').click();
  assert.equal($('.picker-option'), null);
  assert.equal($('.picker-status').textContent, '', 'collapsed groups are not an empty search');
  assert.equal($('.picker-apply').disabled, true);
  search('transform');
  assert.equal($('.picker-list').getAttribute('role'), 'listbox');
  assert.equal($('.picker-category'), null);
  assert.deepEqual(
    [...document.querySelectorAll('.picker-option')].map((row) => row.dataset.id),
    ['Polar Transform', 'Transform', 'Vector Transform'],
  );
  assert.equal(commits, 0);
  search('Fill');
  assert.equal(picker.activeId, 'Angular Gradient Fill');
  scope('paint');
  assert.equal(picker.activeId, 'Angular Gradient Fill');
  scope('transform');
  assert.equal(picker.activeId, undefined);
  assert.match($('.picker-scope').textContent, /変形内 0件/);
  assert.equal($('.picker-apply').disabled, true);
  key(select, 'Enter');
  assert.equal(commits, 0);
  $('.picker-clear-scope').click();
  assert.equal(input.value, 'Fill');
  assert.equal(select.value, '');
  assert.equal(picker.activeId, 'Angular Gradient Fill');
  key(input, 'Enter', { isComposing: true });
  input.dispatchEvent(new win.Event('compositionstart', { bubbles: true }));
  key(input, 'Enter');
  $('.picker-apply').click();
  assert.equal(commits, 0);
  input.dispatchEvent(new win.Event('compositionend', { bubbles: true }));
  key(input, 'Enter', { repeat: true });
  assert.equal(commits, 0);
  const oldRow = $('.picker-option'),
    oldApply = $('.picker-apply');
  oldRow.click();
  assert.equal(commits, 0);
  key(input, 'Enter');
  assert.equal(commits, 1);
  oldRow.dispatchEvent(new win.Event('dblclick'));
  oldApply.click();
  key(input, 'Enter');
  assert.equal(commits, 1);
  assert.equal(document.activeElement, trigger);
  await open();
  assert.equal($('select').value, '');
  assert.equal($('input').value, '');
  key($('input'), 'ArrowDown');
  key($('input'), 'Escape');
  assert.equal(cancels, 1);
  assert.equal(commits, 1);
  await open();
  trigger.remove();
  $('.picker-cancel').click();
  assert.equal(fallback, 1);
  let favorite = false;
  picker = openSearchPanel({
    title: 'Font',
    action: 'Use',
    fontFilters: true,
    items: [
      {
        id: 'regular',
        label: 'Example Regular',
        metadata: { family: 'Example', weightClass: 400 },
      },
      { id: 'bold', label: 'Example Bold', metadata: { family: 'Example', weightClass: 700 } },
    ],
    isFavorite: () => favorite,
    toggleFavorite: () => (favorite = !favorite),
    onCommit: () => commits++,
    onCancel: () => cancels++,
  });
  await new Promise(queueMicrotask);
  assert.equal($('.picker-category-filters'), null);
  assert.equal($('.picker-context'), null);
  $('.picker-star').click();
  assert.equal(favorite, true);
  assert.equal(commits, 1);
  const weight = $('[aria-label="Available weight"]');
  weight.value = '700';
  weight.dispatchEvent(new win.Event('change'));
  assert.equal(picker.activeId, 'bold');
  key(weight, 'Enter');
  assert.equal(commits, 1);
  $('.picker-cancel').click();
});

test('real Add uses captured Session target, one history entry and leaves graph state unchanged on browsing', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'MutationObserver'])
    globalThis[key] = win[key];
  globalThis.requestAnimationFrame = () => 0;
  globalThis.fetch = async () => ({
    ok: true,
    arrayBuffer: async () => {
      const b = await fs.readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
      return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
    },
  });
  let source = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  source = source
    .replace("import './style.css';", '')
    .replace(
      /from '(\.\/[^']+)'/g,
      (_, path) => `from '${new URL('../src/' + path.slice(2), import.meta.url)}'`,
    );
  source +=
    '\nwindow.pickerTest = { open: openNodePicker, state: () => ({past: history.past.length, selection: [...selection], selected, dirty: documents.dirty, viewers: viewerState}), select: (ids) => {selection = new Set(ids); selected = ids[0];}, switchLayer: (id) => { activeLayer = id; }, touch: (id) => commandChange("rename", { nodeId: id, name: "Renamed" }), lock: () => commandChange("set-layer-state", { layerId: current().id, key: "locked", value: true }) };';
  // The startup bundled font is public test infrastructure, not any user's font.
  source +=
    '\nconst deadline = Date.now() + 5000; while (!bundledFont) {if (Date.now() > deadline) throw Error("Font timeout"); await new Promise(r => setTimeout(r, 10));}';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  t.after(() => win.happyDOM.abort());
  const $ = (s) => document.querySelector(s);
  const doc = makeDocument(),
    a = node('Circle'),
    b = node('Rectangle'),
    out = node('Output');
  out.params.path.source = a.id;
  doc.layers[0].nodes = [a, b, out];
  doc.layers[0].output = out.id;
  win.nect.load(serialize(doc));
  win.pickerTest.select([a.id, b.id]);
  const before = JSON.stringify(win.nect.document),
    state = JSON.stringify(win.pickerTest.state());
  win.pickerTest.open();
  await new Promise(queueMicrotask);
  for (let i = 0; i < 3; i++) {
    $('.picker-category[data-category="paint"] .picker-category-heading').click();
    $('.picker-option[data-id="Fill"]').click();
    $('.picker-category[data-category="paint"] .picker-category-heading').click();
  }
  assert.equal(JSON.stringify(win.nect.document), before);
  assert.equal(JSON.stringify(win.pickerTest.state()), state);
  const input = $('.search-panel input');
  input.value = '角丸';
  input.dispatchEvent(new win.Event('input'));
  const select = $('.search-panel select');
  select.value = 'transform';
  select.dispatchEvent(new win.Event('change'));
  assert.equal(JSON.stringify(win.nect.document), before);
  assert.equal(JSON.stringify(win.pickerTest.state()), state);
  $('.picker-cancel').click();
  assert.equal(JSON.stringify(win.pickerTest.state()), state);
  win.pickerTest.open();
  await new Promise(queueMicrotask);
  const row = $('.picker-option[data-id="Circle"]');
  row.click();
  row.dispatchEvent(new win.Event('dblclick'));
  assert.equal(win.nect.document.layers[0].nodes.length, 4);
  assert.equal(win.pickerTest.state().past, JSON.parse(state).past + 1);
  assert.equal(win.pickerTest.state().selection.length, 1);
  row.dispatchEvent(new win.Event('dblclick'));
  assert.equal(win.nect.document.layers[0].nodes.length, 4);
  $('[data-action="undo"]').click();
  assert.equal(JSON.stringify(win.nect.document), before);
  // Regression: an unconnected Mask must survive real Enter creation and render.
  // Rendering used to dereference a missing mask source after the command committed.
  win.pickerTest.open();
  await new Promise(queueMicrotask);
  const maskSearch = $('.search-panel input');
  maskSearch.value = 'Mask';
  maskSearch.dispatchEvent(new win.Event('input'));
  assert.ok($('.picker-option[data-id="Mask"]'));
  maskSearch.dispatchEvent(
    new win.KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      key: 'Enter',
    }),
  );
  assert.equal($('.search-panel'), null, 'Enter closes the picker');
  assert.equal(win.nect.document.layers[0].nodes.filter((n) => n.type === 'Mask').length, 1);
  const maskMode = $('[data-mask-luminance]');
  assert.ok(maskMode, 'Mask controls rendered, not an aborted render');
  assert.equal(maskMode.disabled, true, 'image-only mode is disabled without a source');
  assert.equal(win.pickerTest.state().past, JSON.parse(state).past + 1);
  const withMask = JSON.stringify(win.nect.document);
  $('[data-action="undo"]').click();
  assert.equal(JSON.stringify(win.nect.document), before);
  $('[data-action="redo"]').click();
  assert.equal(JSON.stringify(win.nect.document), withMask);
  assert.ok($('[data-mask-luminance]'), 'redo renders the disconnected Mask');
  win.pickerTest.open();
  await new Promise(queueMicrotask);
  $('.picker-option[data-id="Circle"]').dispatchEvent(new win.Event('dblclick'));
  assert.equal(win.nect.document.layers[0].nodes.length, 5, 'editing works after Mask');
  $('[data-action="undo"]').click();
  assert.equal(JSON.stringify(win.nect.document), withMask);
  $('[data-action="undo"]').click();
  assert.equal(JSON.stringify(win.nect.document), before);
  // Switching the active composition without changing the document must not
  // create in either the old or new target.
  const second = layer('Second');
  doc.layers.push(second);
  win.nect.load(serialize(doc));
  win.pickerTest.open();
  await new Promise(queueMicrotask);
  win.pickerTest.switchLayer(second.id);
  const switched = JSON.stringify(win.nect.document),
    switchedHistory = win.pickerTest.state().past;
  $('.picker-apply').click();
  assert.equal(JSON.stringify(win.nect.document), switched);
  assert.equal(win.pickerTest.state().past, switchedHistory);
  assert.match($('.picker-status').textContent, /作成先/);
  $('.picker-cancel').click();
  win.pickerTest.switchLayer(doc.layers[0].id);
  win.pickerTest.open();
  await new Promise(queueMicrotask);
  win.pickerTest.lock();
  const locked = JSON.stringify(win.nect.document),
    lockedHistory = win.pickerTest.state().past;
  $('.picker-apply').click();
  assert.equal(JSON.stringify(win.nect.document), locked);
  assert.equal(win.pickerTest.state().past, lockedHistory);
  $('.picker-cancel').click();
  win.nect.load(serialize(doc));
  win.pickerTest.open();
  await new Promise(queueMicrotask);
  win.pickerTest.touch(a.id);
  const revised = JSON.stringify(win.nect.document),
    revisedHistory = win.pickerTest.state().past;
  $('.picker-apply').click();
  assert.equal(JSON.stringify(win.nect.document), revised);
  assert.equal(win.pickerTest.state().past, revisedHistory);
  assert.match($('.message').textContent, /stale|revision/i);
  win.pickerTest.open();
  await new Promise(queueMicrotask);
  const staleApply = $('.picker-apply');
  // A document replacement makes the captured envelope stale even if the layer IDs survive.
  win.nect.load(serialize(doc));
  const replacement = JSON.stringify(win.nect.document),
    replacementHistory = win.pickerTest.state().past;
  staleApply.click();
  assert.equal(JSON.stringify(win.nect.document), replacement);
  assert.equal(win.pickerTest.state().past, replacementHistory);
});
