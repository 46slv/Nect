import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { blankDocument, node, connect, serialize } from '../src/core.js';

// Synthetic DOM integration; actual viewport evidence is captured separately.
test('node actions lead expanded settings and Inspector identifies the current node first', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'MutationObserver'])
    globalThis[key] = win[key];
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
  win.SVGElement.prototype.getScreenCTM = () => ({ inverse: () => ({}) });
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
  source += `\nwindow.identityTest = {
    select(id) { selected=id; selection=new Set([id]); artworkTarget=null; artworkSelection.clear(); render(); },
    state() { return { selection:[...selection], past:history.past.length, dirty:documents.dirty }; },
    undo() { stepHistory(); }
  };
  const deadline = Date.now()+5000; while (!bundledFont) { if (Date.now()>deadline) throw Error('font timeout'); await new Promise(r=>setTimeout(r,10)); }`;
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  t.after(() => win.happyDOM.abort());
  const doc = blankDocument(),
    layer = doc.layers[0],
    curve = node('Curve'),
    repeat = node('Geometry Repeater'),
    fill = node('Fill'),
    output = node('Output');
  doc.schemaVersion = 37;
  repeat.name = 'Selected <geometry> & repeated source';
  repeat.operation = 'ae-2d';
  repeat.collapsed = false;
  layer.nodes = [curve, repeat, fill, output];
  layer.output = output.id;
  connect(layer, curve.id, repeat.id, 'path');
  connect(layer, repeat.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  const api = win.identityTest,
    q = (s) => document.querySelector(s),
    card = (id = repeat.id) => q(`.graph .node[data-id="${id}"]`),
    snapshot = () => serialize(win.nect.document),
    nodes = () => win.nect.document.layers[0].nodes;
  const reset = () => {
    win.nect.load(serialize(doc));
    api.select(repeat.id);
  };
  const identity = (name, type) => {
    const body = q('.inspector-body');
    assert.equal(body.firstElementChild, q('.inspector-node-title'));
    assert.equal(body.firstElementChild.textContent, name);
    assert.equal(body.children[1], q('.inspector-node-type'));
    assert.match(body.children[1].textContent, new RegExp(type));
    assert.equal(body.children[2], q('.viewer-assignment'));
    assert.equal(body.children[3], q('.source-navigation'));
    assert.equal(body.querySelectorAll('.inspector-node-title').length, 1);
  };
  await t.test('one action row before controls; passive render and selection never author', () => {
    reset();
    const before = snapshot(),
      state = api.state();
    api.select(repeat.id);
    identity(repeat.name, 'Geometry Repeater');
    const body = card().querySelector('.node-body');
    assert.equal(body.firstElementChild.className, 'node-actions');
    assert.deepEqual(
      [...body.firstElementChild.children].map((b) => b.textContent),
      ['Use as output', 'Duplicate', 'Delete'],
    );
    for (const attr of ['preview', 'duplicate', 'delete'])
      assert.equal(card().querySelectorAll(`[data-${attr}]`).length, 1);
    assert.equal(card().querySelectorAll('.node-actions').length, 1);
    assert.equal(snapshot(), before);
    assert.deepEqual(api.state(), state);
  });
  await t.test('moved Use as output keeps its command and one Undo', () => {
    reset();
    const before = snapshot(),
      past = api.state().past;
    card().querySelector('[data-preview]').click();
    const target = nodes().find((n) => n.id === output.id).params.path.source;
    // Existing preview-node may insert paint for a bare geometry output.
    assert.ok(
      target === repeat.id ||
        nodes().find((n) => n.id === target)?.params.path.source === repeat.id,
    );
    assert.equal(api.state().past, past + 1);
    api.undo();
    assert.equal(snapshot(), before);
  });
  await t.test('moved Duplicate and Delete retain independent one-Undo edits', () => {
    for (const action of ['duplicate', 'delete']) {
      reset();
      const before = snapshot(),
        past = api.state().past;
      card().querySelector(`[data-${action}]`).click();
      assert.equal(nodes().length, layer.nodes.length + (action === 'duplicate' ? 1 : -1));
      assert.equal(api.state().past, past + 1);
      if (action === 'duplicate') assert.ok(api.state().selection.every((id) => id !== repeat.id));
      else assert.ok(!nodes().some((n) => n.id === repeat.id));
      api.undo();
      assert.equal(snapshot(), before);
    }
  });
  await t.test('collapsed cards stay compact; Output never gains Delete or Use as output', () => {
    reset();
    const before = snapshot();
    card().querySelector('[data-collapse]').click();
    assert.equal(card().querySelector('.node-actions'), null);
    assert.ok(card().querySelector('.compact-body'));
    for (const attr of ['duplicate', 'delete']) {
      const button = card().querySelector(`[data-${attr}]`);
      assert.equal(card().querySelectorAll(`[data-${attr}]`).length, 1);
      assert.ok(button.closest('[hidden]'));
    }
    api.undo();
    assert.equal(snapshot(), before);
    const out = card(output.id);
    assert.equal(out.querySelector('[data-delete]'), null);
    assert.equal(out.querySelector('[data-preview]'), null);
    assert.equal(out.querySelectorAll('[data-duplicate]').length, 1);
  });
});
