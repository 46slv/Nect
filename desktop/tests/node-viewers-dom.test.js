import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDoc, node, serialize } from '../src/core.js';

test('Viewer UI assigns explicit ports, stays independent of selection/history and resets on Open', async () => {
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
  source +=
    '\nwindow.viewerTest = { select(id) { selected=id; selection=new Set([id]); artworkTarget=null; render(); }, read() { return { selected, state: viewerState, past: history.past.length, dirty: documents.dirty }; }, edit(id) { commandChange("set-parameter", { layerId: current().id, nodeId: id, key: "size", value: [320, 220] }); }, preview() { updateCanvas(true); } };';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const doc = makeDoc(),
    l = doc.layers[0],
    a = node('Rectangle'),
    b = node('Separate Vector');
  l.nodes.push(a, b);
  win.nect.load(serialize(doc));
  const q = (selector) => document.querySelector(selector),
    click = (selector) => q(selector).click();
  const before = serialize(win.nect.document),
    svg = win.nect.export(),
    initial = win.viewerTest.read();
  win.viewerTest.select(a.id);
  click('[data-assign-viewer="0"]');
  assert.equal(q('.canvas-wrap').dataset.viewerMode, 'viewers');
  assert.equal(q('[data-viewer-status]').textContent, 'Live');
  assert.ok(q('.node-viewer-body svg path').getAttribute('stroke'));
  const firstView = [...win.viewerTest.read().state.slots[0].view];
  win.viewerTest.select(b.id);
  assert.equal(
    q('[data-assign-viewer="1"]').disabled,
    true,
    'multiple public ports require explicit choice',
  );
  const port = q('[aria-label="Viewer public output"]');
  port.value = '1';
  port.dispatchEvent(new win.Event('change', { bubbles: true }));
  click('[data-assign-viewer="1"]');
  assert.equal(win.viewerTest.read().state.slots[1].binding.port, 'y');
  assert.equal(win.viewerTest.read().state.slots[0].binding.nodeId, a.id);
  assert.equal(
    q('.node-viewers')
      .children[1].querySelector('[data-viewer-label]')
      .textContent.includes('Y · Scalar'),
    true,
  );
  const body = q('.node-viewer-body');
  body.dispatchEvent(new win.WheelEvent('wheel', { bubbles: true, cancelable: true, deltaY: 30 }));
  assert.notDeepEqual(win.viewerTest.read().state.slots[0].view, firstView);
  assert.equal(win.viewerTest.read().state.slots[1].view, null);
  body.dispatchEvent(new win.PointerEvent('pointerdown', { bubbles: true, button: 0 }));
  document.dispatchEvent(new win.PointerEvent('pointerup', { bubbles: true, button: 0 }));
  assert.equal(win.viewerTest.read().selected, b.id);
  body.dispatchEvent(new win.KeyboardEvent('keydown', { key: 'Delete', bubbles: true }));
  assert.equal(
    serialize(win.nect.document),
    before,
    'Viewer Delete must not delete the edit target',
  );
  click('[data-viewer-mode="artwork"]');
  click('[data-viewer-mode="viewers"]');
  assert.equal(win.viewerTest.read().state.slots[0].binding.nodeId, a.id);
  assert.equal(serialize(win.nect.document), before);
  assert.equal(win.nect.export(), svg);
  assert.equal(win.viewerTest.read().past, initial.past);
  assert.equal(win.viewerTest.read().dirty, initial.dirty);
  win.viewerTest.select(a.id);
  const pathBefore = q('.node-viewer-body svg path').getAttribute('d');
  const input = q('.inspector [data-key="size"] input');
  assert.ok(input);
  input.value = '320';
  input.dispatchEvent(new win.Event('input', { bubbles: true }));
  input.dispatchEvent(new win.Event('change', { bubbles: true }));
  win.viewerTest.preview();
  assert.notEqual(q('.node-viewer-body svg path').getAttribute('d'), pathBefore);
  assert.equal(win.viewerTest.read().state.slots[0].binding.nodeId, a.id);
  click('[data-viewer-clear="0"]');
  assert.equal(q('[data-viewer-status]').textContent, 'Unassigned');
  win.nect.load(before);
  assert.equal(win.viewerTest.read().state.mode, 'artwork');
  assert.equal(
    win.viewerTest.read().state.slots[1].binding,
    null,
    'new document session never retargets reused node IDs',
  );
  const gradient = node('Linear Gradient Fill');
  gradient.params.path.source = a.id;
  l.nodes.push(gradient);
  win.nect.load(serialize(doc));
  win.viewerTest.select(gradient.id);
  click('[data-assign-viewer="0"]');
  click('[data-assign-viewer="1"]');
  const defs = [...document.querySelectorAll('.node-viewers linearGradient')];
  assert.equal(defs.length, 2);
  assert.notEqual(defs[0].id, defs[1].id);
  const rendered = [...document.querySelectorAll('.node-viewers svg path')];
  assert.equal(rendered[0].getAttribute('fill'), `url(#${defs[0].id})`);
  assert.equal(rendered[1].getAttribute('fill'), `url(#${defs[1].id})`);
  assert.equal(win.viewerTest.read().state.slots[0].binding.nodeId, gradient.id);
  win.happyDOM.abort();
});
