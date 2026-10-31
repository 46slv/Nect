import { openDOMPanel, revealDOMBoundary } from './helpers/dom-workspace-controls.js';
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { blankDocument, blankLayer, node, connect, serialize } from '../src/core.js';

test('placed node group controls share Session history and stable graph identities', async (t) => {
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
    '\nwindow.groupState = () => ({selection:[...selection], past:history.past.length, future:history.future.length});';
  source += '\nwhile (!bundledFont) await new Promise(r => setTimeout(r, 10));';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  t.after(() => win.happyDOM.abort());
  const $ = (s) => document.querySelector(s),
    tick = () => new Promise((r) => setTimeout(r, 20));
  const fire = (el, type, extra = {}) =>
    el.dispatchEvent(
      new win.PointerEvent(type, {
        bubbles: true,
        cancelable: true,
        pointerId: 11,
        button: 0,
        ...extra,
      }),
    );
  const pick = (id, shiftKey = false) => {
    fire($(`.graph .node[data-id="${id}"] .type-tag`), 'pointerdown', { shiftKey });
    fire(document, 'pointerup');
  };
  const open = () => {
    revealDOMBoundary(document, 'graph');
    const d = $('[data-node-groups]');
    if (!d.open) d.querySelector('summary').click();
    assert.equal(d.open, true, 'Groups disclosure opens');
  };
  const change = (el, value) => {
    el.value = value;
    el.dispatchEvent(new win.Event('change', { bubbles: true }));
  };
  const action = (kind) => $(`[data-node-group-action="${kind}"]`),
    act = (kind) => action(kind).click(),
    name = (value) => {
      $('[data-node-group-name]').value = value;
    },
    choose = (id) => change($('[data-node-group-select]'), id),
    groups = () => win.nect.document.layers[0].nodeGroups || [],
    saved = () => serialize(win.nect.document),
    checkpoint = () => ({
      doc: saved(),
      state: win.nect.commandState,
      file: win.nect.fileState,
      history: win.groupState(),
    });
  let a, b, l;
  const load = () => {
    const d = blankDocument();
    l = d.layers[0];
    a = node('Circle', 40, 50);
    b = node('Fill', 350, 50);
    l.nodes.push(a, b);
    connect(l, a.id, b.id, 'path');
    connect(l, b.id, l.output, 'path');
    win.nect.load(serialize(d));
    open();
    return d;
  };
  const staleClick = (button) => {
    $('#app').append(button);
    button.click();
    button.remove();
  };
  const key = (value, extra = {}) =>
    $('[data-node-group-name]').dispatchEvent(
      new win.KeyboardEvent('keydown', { key: value, bubbles: true, cancelable: true, ...extra }),
    );

  await t.test(
    'create from multi-selection, escaped badges, rename, membership and explicit one-Undo move',
    async () => {
      load();
      const originalNodes = win.nect.document.layers[0].nodes,
        svg = win.nect.export();
      pick(a.id);
      pick(b.id, true);
      open();
      name('<Shapes & paint>');
      act('create');
      const first = groups()[0].id;
      assert.deepEqual(groups()[0].nodeIds, [a.id, b.id]);
      assert.equal(win.groupState().past, 1);
      assert.equal(document.querySelectorAll('.node-group-label').length, 2);
      assert.equal($('.node-group-label').textContent, 'Group · <Shapes & paint>');
      assert.equal($('.node-group-label').children.length, 0);
      name('Shapes');
      act('rename');
      assert.equal(groups()[0].name, 'Shapes');
      pick(l.output);
      open();
      name('Output group');
      act('create');
      const second = groups()[1].id;
      pick(b.id);
      open();
      choose(second);
      assert.equal(action('add').disabled, true);
      const past = win.groupState().past;
      act('move');
      assert.equal(win.groupState().past, past + 1);
      assert.deepEqual(groups()[0].nodeIds, [a.id]);
      assert.deepEqual(groups()[1].nodeIds, [l.output, b.id]);
      $('[data-action="undo"]').click();
      assert.deepEqual(groups()[0].nodeIds, [a.id, b.id]);
      $('[data-action="redo"]').click();
      assert.deepEqual(groups()[0].nodeIds, [a.id]);
      choose(first);
      pick(l.output);
      open();
      choose(first);
      act('add');
      // Output already belongs elsewhere, so Add never steals it.
      assert.deepEqual(groups()[0].nodeIds, [a.id]);
      pick(a.id);
      open();
      choose(first);
      act('remove');
      assert.deepEqual(groups()[0].nodeIds, []);
      act('ungroup');
      assert.equal(groups().length, 1);
      assert.deepEqual(win.nect.document.layers[0].nodes, originalNodes);
      assert.equal(win.nect.export(), svg);
      await tick();
    },
  );
  await t.test(
    'Add admits ungrouped selected nodes, empty group survives last-member deletion and Undo',
    () => {
      load();
      pick(a.id);
      open();
      name('A');
      act('create');
      const id = groups()[0].id;
      pick(b.id);
      open();
      choose(id);
      act('add');
      assert.deepEqual(groups()[0].nodeIds, [a.id, b.id]);
      win.nect.dispatch({
        ...win.nect.commandState,
        type: 'delete-nodes',
        payload: { layerId: l.id, nodeIds: [a.id, b.id] },
      });
      assert.equal(groups()[0].id, id);
      assert.deepEqual(groups()[0].nodeIds, []);
      assert.match($('[data-node-group-select]').textContent, /A \(0\)/);
      $('[data-action="undo"]').click();
      assert.deepEqual(groups()[0].nodeIds, [a.id, b.id]);
    },
  );
  await t.test(
    'selection, group browsing, name drafts, Cancel/Escape and invalid names are document-neutral',
    () => {
      load();
      pick(a.id);
      open();
      name('A');
      act('create');
      const id = groups()[0].id;
      const before = checkpoint();
      choose('');
      choose(id);
      name('Draft');
      act('cancel');
      assert.equal($('[data-node-group-name]').value, 'A');
      name('Draft again');
      key('Escape');
      assert.equal($('[data-node-group-name]').value, 'A');
      assert.deepEqual(checkpoint(), before);
      name(' ');
      act('rename');
      assert.deepEqual(checkpoint(), before);
      name('IME');
      $('[data-node-group-name]').dispatchEvent(
        new win.Event('compositionstart', { bubbles: true }),
      );
      key('Enter', { isComposing: true });
      act('rename');
      assert.deepEqual(checkpoint(), before);
      $('[data-node-group-name]').dispatchEvent(new win.Event('compositionend', { bubbles: true }));
      key('Enter');
      assert.equal(groups()[0].name, 'IME');
      const after = checkpoint();
      key('Enter', { repeat: true });
      assert.deepEqual(checkpoint(), after);
    },
  );
  await t.test(
    'stale controls and document/composition switches cannot retarget group edits',
    () => {
      const d = load();
      pick(a.id);
      open();
      name('A');
      act('create');
      const old = action('ungroup').cloneNode(true);
      name('B');
      act('rename');
      const renamed = checkpoint();
      staleClick(old);
      assert.deepEqual(checkpoint(), renamed);
      const changedSelection = action('remove').cloneNode(true);
      pick(b.id);
      const selected = checkpoint();
      staleClick(changedSelection);
      assert.deepEqual(checkpoint(), selected);
      const oldDoc = action('ungroup').cloneNode(true);
      win.nect.load(serialize(d));
      const replaced = checkpoint();
      staleClick(oldDoc);
      assert.deepEqual(checkpoint(), replaced);
      assert.equal(groups().length, 0);
      assert.equal(document.querySelectorAll('.node-group-label').length, 0);
      const other = blankLayer('Other');
      d.layers.push(other);
      win.nect.load(serialize(d));
      pick(a.id);
      open();
      name('A');
      act('create');
      const switched = action('ungroup').cloneNode(true);
      openDOMPanel(document, 'layers');
      $(`[data-select-layer="${other.id}"]`).click();
      assert.equal($('[data-node-group-select]').options.length, 1);
      const next = checkpoint();
      staleClick(switched);
      assert.deepEqual(checkpoint(), next);
    },
  );
  await t.test(
    'save/reload retains group IDs and badges; locked composition disables edits',
    () => {
      load();
      pick(a.id);
      open();
      name('Persist');
      act('create');
      const text = saved(),
        id = groups()[0].id;
      win.nect.load(text);
      open();
      assert.equal(saved(), text);
      assert.equal(groups()[0].id, id);
      assert.equal($('.node-group-label').dataset.nodeGroupId, id);
      assert.equal($('[data-node-group-select]').value, '');
      win.nect.dispatch({
        ...win.nect.commandState,
        type: 'set-layer-state',
        payload: { layerId: l.id, key: 'locked', value: true },
      });
      choose(id);
      assert.ok(
        [...document.querySelectorAll('[data-node-group-action]')].every((b) => b.disabled),
      );
    },
  );
});
