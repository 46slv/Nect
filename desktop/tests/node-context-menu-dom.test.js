import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDocument, node, serialize } from '../src/core.js';

// Synthetic DOM integration: not browser/WebView visual acceptance.
test('node header context actions through real DOM handlers', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement']) globalThis[key] = win[key];
  globalThis.requestAnimationFrame = () => 0;
  globalThis.DOMPoint = class {
    constructor(x, y) {
      this.x = x;
      this.y = y;
    }
    matrixTransform() {
      return this;
    }
  };
  const matrix = {
    a: 1,
    b: 0,
    c: 0,
    d: 1,
    e: 0,
    f: 0,
    inverse() {
      return this;
    },
  };
  win.SVGElement.prototype.getScreenCTM = () => matrix;
  win.SVGSVGElement.prototype.getScreenCTM = () => matrix;
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
    '\nwindow.feedbackState = () => ({ past: history.past.length, dirty: documents.dirty, selection: [...selection] }); window.feedbackZoom = () => { graphZoom = 1; };';
  source +=
    '\nconst deadline = Date.now() + 5000; while (!bundledFont) { if (Date.now() > deadline) throw Error("Font timeout"); await new Promise(r => setTimeout(r, 10)); }';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  t.after(() => win.happyDOM.abort());
  const $ = (selector) => document.querySelector(selector);
  const fire = (el, type, x = 0, y = 0, extra = {}) =>
    el.dispatchEvent(
      new win.PointerEvent(type, {
        bubbles: true,
        cancelable: true,
        pointerId: 7,
        button: 0,
        clientX: x,
        clientY: y,
        ...extra,
      }),
    );
  const key = (name, target = $('.graph-scroll'), extra = {}) =>
    target.dispatchEvent(
      new win.KeyboardEvent('keydown', { bubbles: true, cancelable: true, key: name, ...extra }),
    );
  const card = (id) => $(`.graph .node[data-id="${id}"]`);
  const clickNode = (id, shiftKey = false) => {
    fire(card(id).querySelector('.node-head .type-tag'), 'pointerdown', 0, 0, { shiftKey });
    fire(document, 'pointerup');
  };
  const state = () => win.feedbackState();
  let doc, a, b, fill, output;
  const reset = (locked = false, curve = false, remap = false) => {
    doc = makeDocument();
    a = node(curve ? 'Curve' : 'Circle', 100, 100);
    if (curve) {
      a.points[0].handleA.value = [-20, 0];
      a.points[0].handleB.value = [20, 0];
    }
    b = node(remap ? 'Path Remap' : 'Rectangle', 500, 100);
    if (remap) b.params.path.source = a.id;
    fill = node('Fill', 900, 100);
    fill.params.path.source = a.id;
    output = node('Output', 1300, 100);
    output.params.path.source = fill.id;
    doc.layers[0].nodes = [a, b, fill, output];
    doc.layers[0].output = output.id;
    doc.layers[0].locked = locked;
    win.nect.load(serialize(doc));
    win.feedbackZoom();
    clickNode(a.id);
    clickNode(b.id, true);
  };
  const nodes = () => win.nect.document.layers[0].nodes;
  const snapshot = () => serialize(win.nect.document);

  const menu = () => $('.node-context-menu');
  const action = (id) => menu().querySelector(`[data-node-context-action="${id}"]`);
  const open = (id) => {
    const title = card(id).querySelector('.node-head strong');
    fire(title, 'pointerdown', 30, 40, { button: 2 });
    fire(document, 'pointerup', 30, 40, { button: 2 });
    return fire(title, 'contextmenu', 30, 40, { button: 2 });
  };
  await t.test('selected header preserves group; opening and dismissal are session-only', () => {
    reset();
    const before = snapshot(),
      initial = state();
    assert.equal(open(a.id), false);
    assert.ok(menu());
    assert.deepEqual(state(), initial);
    assert.equal(snapshot(), before);
    key('Escape', document.activeElement);
    assert.equal(menu(), null);
    assert.deepEqual(state(), initial);
    open(fill.id);
    assert.deepEqual(state().selection, [fill.id]);
    assert.equal(state().past, initial.past);
    assert.equal(state().dirty, initial.dirty);
    fire(document.body, 'pointerdown');
    assert.equal(menu(), null);
    open(a.id);
    const stale = action('duplicate');
    win.nect.load(serialize(doc));
    assert.equal(menu(), null);
    const replaced = snapshot();
    stale.click();
    assert.equal(snapshot(), replaced);
  });
  await t.test('duplicate group preserves internal links and is one undo', () => {
    reset();
    clickNode(fill.id, true);
    const before = snapshot(),
      past = state().past;
    open(a.id);
    action('duplicate').click();
    assert.equal(menu(), null);
    assert.equal(nodes().length, 7);
    assert.equal(state().past, past + 1);
    const copies = nodes().filter((n) => state().selection.includes(n.id));
    assert.equal(copies.length, 3);
    assert.equal(
      copies.find((n) => n.type === 'Fill').params.path.source,
      copies.find((n) => n.type === 'Circle').id,
    );
    key('z', $('.graph-scroll'), { ctrlKey: true });
    assert.equal(snapshot(), before);
  });
  await t.test('Ctrl/Cmd+D duplicates beside the group, retains focus and repeats safely', () => {
    reset();
    const before = snapshot(),
      past = state().past;
    assert.equal(key('d', $('.graph-scroll'), { ctrlKey: true }), false);
    let copies = nodes().filter((n) => state().selection.includes(n.id));
    assert.equal(copies.length, 2);
    assert.ok(copies[0].position[0] >= 1564);
    assert.deepEqual(
      copies[1].position.map((v, i) => v - copies[0].position[i]),
      [400, 0],
    );
    assert.equal(document.activeElement, $('.graph-scroll'));
    assert.equal(state().past, past + 1);
    assert.equal(key('d', document.activeElement, { ctrlKey: true, repeat: true }), false);
    assert.equal(state().past, past + 1);
    key('z', document.activeElement, { ctrlKey: true });
    assert.equal(snapshot(), before);
    reset();
    assert.equal(key('D', $('.graph-scroll'), { metaKey: true }), false);
    const first = state().selection;
    assert.equal(key('d', document.activeElement, { metaKey: true }), false);
    assert.equal(nodes().length, 8);
    assert.ok(state().selection.every((id) => !first.includes(id)));
  });
  await t.test('duplication shortcut respects inputs, other views, IME, dialogs and locks', () => {
    reset();
    const before = snapshot(),
      past = state().past;
    for (const target of [card(a.id).querySelector('input'), $('.canvas-stage'), document.body])
      assert.equal(key('d', target, { ctrlKey: true }), true);
    for (const extra of [{ isComposing: true }, { keyCode: 229 }, { repeat: true }])
      assert.equal(key('d', $('.graph-scroll'), { ctrlKey: true, ...extra }), false);
    for (const extra of [{ shiftKey: true }, { altKey: true }])
      assert.equal(key('d', $('.graph-scroll'), { ctrlKey: true, ...extra }), true);
    const dialog = document.createElement('dialog');
    dialog.setAttribute('open', '');
    document.body.append(dialog);
    key('d', $('.graph-scroll'), { ctrlKey: true });
    dialog.remove();
    const field = card(a.id).querySelector('input');
    field.dispatchEvent(new win.Event('compositionstart', { bubbles: true }));
    key('d', $('.graph-scroll'), { ctrlKey: true });
    field.dispatchEvent(new win.Event('compositionend', { bubbles: true }));
    assert.equal(snapshot(), before);
    assert.equal(state().past, past);
    reset(true);
    const locked = snapshot();
    assert.equal(key('d', $('.graph-scroll'), { ctrlKey: true }), false);
    assert.equal(snapshot(), locked);
  });
  await t.test('single collapsed node, empty selection and active drag are safe', () => {
    reset();
    a.collapsed = true;
    win.nect.load(serialize(doc));
    for (const id of state().selection) clickNode(id, true);
    assert.deepEqual(state().selection, []);
    const empty = snapshot();
    assert.equal(key('d', $('.graph-scroll'), { ctrlKey: true }), false);
    assert.equal(snapshot(), empty);
    clickNode(a.id);
    const before = snapshot();
    fire(card(a.id).querySelector('.node-head .type-tag'), 'pointerdown');
    assert.equal(key('d', $('.graph-scroll'), { ctrlKey: true }), false);
    assert.equal(snapshot(), before);
    fire(document, 'pointerup');
    key('d', $('.graph-scroll'), { ctrlKey: true });
    const copy = nodes().find((n) => state().selection.includes(n.id));
    assert.equal(copy.collapsed, true);
    assert.equal(copy.position[1], a.position[1]);
    assert.ok(copy.position[0] >= a.position[0] + 264 + 24);
    assert.notEqual(copy.id, a.id);
  });
  await t.test('delete group protects Output, cleans wires, and undoes atomically', () => {
    reset();
    clickNode(output.id, true);
    const before = snapshot(),
      past = state().past;
    open(a.id);
    assert.match(action('delete').title, /Output nodes are retained/);
    action('delete').click();
    assert.deepEqual(
      nodes().map((n) => n.id),
      [fill.id, output.id],
    );
    assert.equal(nodes()[0].params.path.source, undefined);
    assert.deepEqual(state().selection, [output.id]);
    assert.equal(state().past, past + 1);
    key('z', $('.graph-scroll'), { ctrlKey: true });
    assert.equal(snapshot(), before);
    open(output.id);
    // Output is now the only selected node after reopening an unselected header.
    fire(document.body, 'pointerdown');
    clickNode(output.id);
    open(output.id);
    assert.equal(action('delete').disabled, true);
    assert.equal(action('output').disabled, true);
  });
  await t.test('Use as output targets the clicked node only and retains prior graph', () => {
    reset();
    const before = snapshot(),
      past = state().past;
    open(b.id);
    assert.deepEqual(new Set(state().selection), new Set([a.id, b.id]));
    action('output').click();
    const paint = nodes().find(
      (n) => n.id === nodes().find((n) => n.id === output.id).params.path.source,
    );
    assert.equal(paint.params.path.source, b.id);
    assert.ok(nodes().some((n) => n.id === fill.id));
    assert.equal(state().past, past + 1);
    key('z', $('.graph-scroll'), { ctrlKey: true });
    assert.equal(snapshot(), before);
  });
  await t.test(
    'locked composition, locked Output, scalar and incompatible output show reasons',
    () => {
      reset(true);
      const before = snapshot(),
        initial = state();
      open(a.id);
      for (const id of ['output', 'duplicate', 'delete']) {
        assert.equal(action(id).disabled, true);
        assert.match(action(id).textContent, /locked/);
        action(id).click();
      }
      assert.equal(snapshot(), before);
      assert.deepEqual(state(), initial);
      reset();
      output.params.path.locked = true;
      win.nect.load(serialize(doc));
      open(a.id);
      assert.match(action('output').textContent, /Output input is locked/);
      const scalar = node('Math');
      doc.layers[0].nodes.push(scalar);
      win.nect.load(serialize(doc));
      open(scalar.id);
      assert.match(action('output').textContent, /no artwork output/);
      const imageOutput = node('Image Output');
      doc.layers[0].nodes = [a, b, imageOutput];
      doc.layers[0].output = imageOutput.id;
      win.nect.load(serialize(doc));
      open(a.id);
      assert.match(action('output').textContent, /Incompatible/);
    },
  );
  await t.test('collapsed node works and input context menu remains native', () => {
    reset();
    a.collapsed = true;
    win.nect.load(serialize(doc));
    open(a.id);
    assert.ok(menu());
    key('Escape', document.activeElement);
    const input = card(b.id).querySelector('input');
    assert.ok(input);
    assert.equal(fire(input, 'contextmenu', 0, 0, { button: 2 }), true);
    assert.equal(menu(), null);
    assert.equal(fire(card(b.id).querySelector('.node-body'), 'contextmenu'), false);
    assert.equal(menu(), null);
  });
  await t.test('active rename and composition keep their editing context', () => {
    reset();
    const before = snapshot(),
      past = state().past;
    const title = card(a.id).querySelector('.node-head strong');
    title.dispatchEvent(new win.MouseEvent('dblclick', { bubbles: true }));
    const input = card(a.id).querySelector('[data-rename-name]');
    assert.ok(input);
    assert.equal(fire(input, 'contextmenu', 0, 0, { button: 2 }), true);
    open(b.id);
    assert.equal(menu(), null);
    assert.equal(state().past, past);
    assert.equal(snapshot(), before);
    key('Escape', input);
    reset();
    const field = card(a.id).querySelector('input');
    field.dispatchEvent(new win.Event('compositionstart', { bubbles: true }));
    open(b.id);
    assert.equal(menu(), null);
    field.dispatchEvent(new win.Event('compositionend', { bubbles: true }));
  });
  await t.test('keyboard navigation, Enter, Tab and repeated reopening', () => {
    reset();
    open(a.id);
    const past = state().past;
    assert.equal(document.activeElement, action('output'));
    key('ArrowDown', document.activeElement);
    assert.equal(document.activeElement, action('duplicate'));
    key('Enter', document.activeElement);
    assert.equal(menu(), null);
    assert.equal(state().past, past + 1);
    open(a.id);
    open(b.id);
    assert.equal(document.querySelectorAll('.node-context-menu').length, 1);
    key('Tab', document.activeElement);
    assert.equal(menu(), null);
    assert.equal(document.activeElement, $('.graph-scroll'));
  });
  await t.test(
    'Graph Copy/Paste preserves selection links in one Undo and leaves input/IME copy native',
    () => {
      reset();
      clickNode(fill.id, true);
      const before = snapshot(),
        past = state().past;
      assert.equal(key('c', $('.graph-scroll'), { ctrlKey: true }), false);
      assert.equal(snapshot(), before);
      assert.equal(state().past, past);
      assert.equal(key('v', $('.graph-scroll'), { ctrlKey: true }), false);
      assert.equal(nodes().length, 7);
      assert.equal(state().selection.length, 3);
      assert.equal(state().past, past + 1);
      const copies = nodes().filter((n) => state().selection.includes(n.id));
      assert.equal(
        copies.find((n) => n.type === 'Fill').params.path.source,
        copies.find((n) => n.type === 'Circle').id,
      );
      key('z', $('.graph-scroll'), { ctrlKey: true });
      assert.equal(snapshot(), before);
      const input = $('.inspector input');
      assert.equal(key('c', input, { ctrlKey: true }), true);
      assert.equal(key('v', input, { ctrlKey: true }), true);
      assert.equal(key('c', $('.graph-scroll'), { ctrlKey: true, isComposing: true }), true);
      assert.equal(key('v', $('.graph-scroll'), { ctrlKey: true, keyCode: 229 }), true);
      assert.equal(key('v', $('.canvas-stage'), { ctrlKey: true }), true);
      assert.equal(snapshot(), before);
      reset();
      const empty = snapshot();
      key('v', $('.graph-scroll'), { ctrlKey: true });
      assert.equal(snapshot(), empty);
      assert.match($('.message').textContent, /clipboard is empty/);
    },
  );
});
