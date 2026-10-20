import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDoc, parameter, serialize } from '../src/core.js';

test('graph autopan updates previews only after scrolling and preserves gesture history', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  t.after(() => win.happyDOM.abort());
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'MutationObserver'])
    globalThis[key] = win[key];
  const frames = new Map();
  let nextFrame = 0;
  globalThis.requestAnimationFrame = win.requestAnimationFrame = (fn) => {
    frames.set(++nextFrame, fn);
    return nextFrame;
  };
  win.cancelAnimationFrame = (id) => frames.delete(id);
  const flush = (time) => {
    const jobs = [...frames.values()];
    frames.clear();
    jobs.forEach((fn) => fn(time));
  };
  const matrix = {
    inverse() {
      return this;
    },
  };
  globalThis.DOMPoint = class {
    constructor(x, y) {
      this.x = x;
      this.y = y;
    }
    matrixTransform() {
      return { x: this.x, y: this.y };
    }
  };
  win.SVGElement.prototype.getScreenCTM = () => matrix;
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
  // Count entry into the shipped functions, without replacing their implementations.
  const counts = { gesture: 0, bounds: 0, wires: 0 };
  win.graphWork = counts;
  for (const [name, key] of [
    ['updateGraphGesture', 'gesture'],
    ['updateGraphBounds', 'bounds'],
    ['drawWires', 'wires'],
  ]) {
    const declaration = new RegExp(`function ${name}\\([^)]*\\) \\{`);
    assert.match(source, declaration);
    source = source.replace(declaration, (match) => `${match} window.graphWork.${key}++;`);
  }
  source +=
    '\nwindow.graphWorkState = () => ({ past: history.past.length, drag: drag?.type, wiring: !!wiring }); window.graphWorkZoom = (z) => { graphZoom = z; };';
  source +=
    '\nconst fontDeadline = Date.now() + 5000; while (!bundledFont) { if (Date.now() > fontDeadline) throw Error("Test font timeout"); await new Promise((r) => setTimeout(r, 10)); }';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const $ = (s) => document.querySelector(s);
  const fire = (target, type, x = 250, y = 250, extra = {}) =>
    target.dispatchEvent(
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
  const reset = () =>
    Object.keys(counts).forEach((key) => {
      counts[key] = 0;
    });
  const doc = makeDoc();
  doc.layers[0].nodes.forEach((n, i) => {
    n.position = [100 + i * 600, 100 + i * 600];
  });
  const nodeId = doc.layers[0].nodes[0].id;
  const node = () => $(`.node[data-id="${nodeId}"]`);
  const position = () => win.nect.document.layers[0].nodes[0].position;
  const load = () => {
    win.nect.load(serialize(doc));
    win.graphWorkZoom(1);
    const viewport = $('.graph-scroll'),
      graph = $('.graph');
    Object.defineProperties(viewport, {
      clientWidth: { value: 500 },
      clientHeight: { value: 500 },
      // happy-dom has no layout/scroll limits; model browser clamping explicitly.
      scrollLeft: {
        configurable: true,
        get() {
          return this.testX || 0;
        },
        set(v) {
          this.testX = Math.max(0, Math.min(v, parseFloat(graph.style.width) - 500));
        },
      },
      scrollTop: {
        configurable: true,
        get() {
          return this.testY || 0;
        },
        set(v) {
          this.testY = Math.max(0, Math.min(v, parseFloat(graph.style.height) - 500));
        },
      },
    });
    viewport.getBoundingClientRect = () => ({ left: 0, top: 0, width: 500, height: 500 });
    graph.getBoundingClientRect = () => ({ x: -viewport.scrollLeft, y: -viewport.scrollTop });
    flush(0);
    reset();
    return viewport;
  };
  const startNode = () => fire(node().querySelector('.node-head'), 'pointerdown');
  const observations = {};
  load();
  const before = serialize(win.nect.document),
    past = win.graphWorkState().past;
  startNode();
  fire(document, 'pointermove', 270, 280, { ctrlKey: true });
  observations.nodePointer = { ...counts };
  assert.equal(node().style.left, '120px', 'pointer preview remains synchronous');
  assert.equal(serialize(win.nect.document), before, 'preview does not author the document');
  reset();
  for (let i = 1; i <= 60; i++) flush(i * 16);
  observations.nodeIdle60 = { ...counts };
  fire(document, 'pointerup', 270, 280);
  assert.deepEqual(position(), [120, 130]);
  assert.equal(win.graphWorkState().past, past + 1, 'one drag creates one undo entry');
  $('[data-action="undo"]').click();
  assert.deepEqual(position(), [100, 100]);
  $('[data-action="redo"]').click();
  assert.deepEqual(position(), [120, 130]);

  load();
  startNode();
  fire(document, 'pointermove', 5, 5, { ctrlKey: true });
  reset();
  for (let i = 1; i <= 60; i++) flush(i * 16);
  observations.nodeClamped60 = { ...counts };
  fire(document, 'pointercancel');
  assert.deepEqual(position(), [100, 100]);

  let viewport = load();
  startNode();
  fire(document, 'pointermove', 495, 495, { ctrlKey: true });
  reset();
  flush(16); // First frame only establishes the clock; dt is zero.
  observations.nodeFirstEdgeFrame = { ...counts };
  reset();
  flush(32);
  observations.nodeScrolledFrame = { ...counts };
  assert.ok(viewport.scrollLeft > 0 && viewport.scrollTop > 0);
  const previewPosition = [parseFloat(node().style.left), parseFloat(node().style.top)];
  assert.ok(
    previewPosition[0] > 345 && previewPosition[1] > 345,
    'stationary edge pointer follows the scrolled world position',
  );
  fire(document, 'pointerup', 495, 495);
  assert.ok(
    position().every((v, i) => Math.abs(v - previewPosition[i]) < 1e-6),
    'release commits the autopan preview (DOM styles round fractional pixels)',
  );

  load();
  const wireBefore = serialize(win.nect.document),
    wirePast = win.graphWorkState().past;
  const output = $('.graph .output');
  fire(output, 'pointerdown');
  reset();
  fire(document, 'pointermove', 270, 280);
  observations.wirePointer = { ...counts };
  const previewPath = $('.wire-preview').getAttribute('d');
  reset();
  for (let i = 1; i <= 60; i++) flush(i * 16);
  observations.wireIdle60 = { ...counts };
  assert.equal($('.wire-preview').getAttribute('d'), previewPath);
  fire(document, 'pointercancel');
  assert.equal(serialize(win.nect.document), wireBefore);
  assert.equal(win.graphWorkState().past, wirePast);

  viewport = load();
  fire($('.graph .output'), 'pointerdown', 495, 495);
  reset();
  flush(16);
  observations.wireFirstEdgeFrame = { ...counts };
  reset();
  flush(32);
  observations.wireScrolledFrame = { ...counts };
  assert.ok(viewport.scrollLeft > 0 && viewport.scrollTop > 0);
  assert.ok($('.wire-preview'));
  fire(document, 'pointercancel');
  flush(48); // Cancellation must remove the repeating callback.
  assert.equal(win.graphWorkState().wiring, false);
  assert.equal(frames.size, 0);
  assert.equal(serialize(win.nect.document), wireBefore);

  load();
  const connectedField = [...document.querySelectorAll('.graph .field')].find((f) => {
    const n = doc.layers[0].nodes.find((n) => n.id === f.dataset.node);
    return n && parameter(n, f.dataset.key)?.source;
  });
  assert.ok(connectedField, 'fixture has an existing connection');
  const { node: targetId, key: targetKey } = connectedField.dataset;
  const q = parameter(
    doc.layers[0].nodes.find((n) => n.id === targetId),
    targetKey,
  );
  const field = () =>
    [...document.querySelectorAll('.graph .field')].find(
      (f) => f.dataset.node === targetId && f.dataset.key === targetKey,
    );
  fire(field().querySelector('.input'), 'pointerdown');
  fire($('.graph'), 'pointerup');
  const targetParam = () =>
    parameter(
      win.nect.document.layers[0].nodes.find((n) => n.id === targetId),
      targetKey,
    );
  assert.equal(targetParam().source, undefined, 'disconnect commits through the real handler');
  assert.equal(win.graphWorkState().past, wirePast + 1);
  $('[data-action="undo"]').click();
  assert.equal(serialize(win.nect.document), wireBefore);
  $('[data-action="redo"]').click();
  const sourceSocket = $(
    `[data-source="${q.source}"]${q.sourcePort ? `[data-source-port="${q.sourcePort}"]` : ':not([data-source-port])'}`,
  );
  assert.ok(
    sourceSocket,
    JSON.stringify({
      q,
      sockets: [...document.querySelectorAll('.graph .output')].map((el) => ({ ...el.dataset })),
    }),
  );
  fire(sourceSocket, 'pointerdown');
  fire(document, 'pointermove', 270, 280);
  flush(64);
  fire(field(), 'pointerup');
  assert.equal(targetParam().source, q.source, 'connect commits through the real handler');
  assert.equal(serialize(win.nect.document), wireBefore);
  assert.equal(win.graphWorkState().past, wirePast + 2);
  $('[data-action="undo"]').click();
  assert.equal(targetParam().source, undefined);
  $('[data-action="redo"]').click();
  assert.equal(targetParam().source, q.source);

  t.diagnostic(JSON.stringify(observations));
  assert.deepEqual(observations, {
    nodePointer: { gesture: 1, bounds: 1, wires: 1 },
    nodeIdle60: { gesture: 0, bounds: 0, wires: 0 },
    nodeClamped60: { gesture: 0, bounds: 0, wires: 0 },
    nodeFirstEdgeFrame: { gesture: 0, bounds: 0, wires: 0 },
    nodeScrolledFrame: { gesture: 1, bounds: 1, wires: 1 },
    wirePointer: { gesture: 0, bounds: 0, wires: 1 },
    wireIdle60: { gesture: 0, bounds: 0, wires: 0 },
    wireFirstEdgeFrame: { gesture: 0, bounds: 0, wires: 0 },
    wireScrolledFrame: { gesture: 0, bounds: 0, wires: 1 },
  });
});
