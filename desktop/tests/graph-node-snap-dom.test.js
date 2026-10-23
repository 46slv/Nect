import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDoc, serialize } from '../src/core.js';
test('graph alignment integrates with real DOM handlers, preview, history, cancellation and lock', async () => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  let pending = [];
  for (const key of ['window', 'document', 'HTMLElement']) globalThis[key] = win[key];
  globalThis.MutationObserver = win.MutationObserver;
  globalThis.requestAnimationFrame = (fn) => {
    pending.push(fn);
    return pending.length;
  };
  const flush = () => {
    const jobs = pending;
    pending = [];
    jobs.forEach((fn) => fn(0));
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
    '\nwindow.snapUI = () => ({ past: history.past.length, drag: drag?.type }); window.snapZoom = (z) => { graphZoom = z; }; window.snapPan = graphAutoPan;';
  // These suites synchronously create/evaluate Text after startup. Await the
  // font condition inside the test module, rather than racing filesystem I/O.
  source +=
    '\nconst fontTestDeadline = Date.now() + 5000; while (!bundledFont) { if (Date.now() >= fontTestDeadline) throw Error("Bundled test font did not finish loading"); await new Promise((r) => setTimeout(r, 10)); }';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const $ = (s) => document.querySelector(s);
  const doc = makeDoc();
  const nodes = doc.layers[0].nodes;
  nodes.forEach((node, i) => {
    node.position = [100 + i * 600, 100 + i * 600];
  });
  const [a, b] = nodes;
  b.position = [500, 500];
  const load = () => {
    win.nect.load(serialize(doc));
    win.snapZoom(1);
  };
  const element = (id) => $(`.node[data-id="${id}"]`);
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
  const key = (type, name, extra = {}) =>
    $('.graph-scroll').dispatchEvent(
      new win.KeyboardEvent(type, { bubbles: true, key: name, ...extra }),
    );
  const start = () => fire(element(a.id).querySelector('.node-head'), 'pointerdown', 100, 100);
  const pos = (id) => win.nect.document.layers[0].nodes.find((n) => n.id === id).position;
  load();
  const before = serialize(win.nect.document),
    past = win.snapUI().past;
  start();
  fire(document, 'pointermove', 496, 497);
  assert.equal(element(a.id).style.left, '500px');
  assert.equal(element(a.id).style.top, '500px');
  assert.equal($('.graph-snap-guides').children.length, 2);
  assert.equal(serialize(win.nect.document), before, 'preview does not author document');
  key('keydown', 'Control', { ctrlKey: true });
  assert.equal(element(a.id).style.left, '496px');
  assert.equal($('.graph-snap-guides').children.length, 0);
  key('keyup', 'Control');
  assert.equal(element(a.id).style.left, '500px');
  fire(document, 'pointerup', 496, 497);
  assert.deepEqual(pos(a.id), [500, 500]);
  assert.equal(win.snapUI().past, past + 1);
  assert.equal($('.graph-snap-guides').children.length, 0);
  $('[data-action="undo"]').click();
  assert.deepEqual(pos(a.id), [100, 100]);
  $('[data-action="redo"]').click();
  assert.deepEqual(pos(a.id), [500, 500]);
  for (const cancel of ['pointercancel', 'Escape']) {
    load();
    start();
    fire(document, 'pointermove', 496, 497);
    if (cancel === 'Escape') key('keydown', 'Escape');
    else fire(document, cancel);
    assert.deepEqual(pos(a.id), [100, 100]);
    assert.equal($('.graph-snap-guides').children.length, 0);
  }
  load();
  fire(element(b.id).querySelector('.node-head'), 'pointerdown', 500, 500, { shiftKey: true });
  fire(element(a.id).querySelector('.node-head'), 'pointerdown', 100, 100, { shiftKey: true });
  // Ensure both nodes are selected via actual Shift-click UI (load initially selects one).
  if (!element(a.id).classList.contains('selected'))
    fire(element(a.id).querySelector('.node-head'), 'pointerdown', 100, 100, { shiftKey: true });
  if (!element(b.id).classList.contains('selected'))
    fire(element(b.id).querySelector('.node-head'), 'pointerdown', 500, 500, { shiftKey: true });
  start();
  fire(document, 'pointermove', 104, 105);
  assert.equal(element(a.id).style.left, '104.5px');
  assert.equal(element(b.id).style.left, '504.5px');
  fire(document, 'pointerup');
  assert.deepEqual(
    pos(b.id).map((v, i) => v - pos(a.id)[i]),
    [400, 400],
  );
  load();
  Object.defineProperty(element(a.id), 'offsetWidth', { value: 300 });
  Object.defineProperty(element(b.id), 'offsetWidth', { value: 400 });
  start();
  fire(document, 'pointermove', 197, 250, { altKey: true });
  assert.equal(element(a.id).style.left, '200px', 'measured right edge aligns to target left edge');
  assert.equal($('.graph-snap-guides line').getAttribute('x1'), '500');
  fire(document, 'pointercancel', 0, 0, { pointerId: 999 });
  assert.equal(win.snapUI().drag, 'node', 'unrelated pointer cancellation is ignored');
  fire(document, 'pointercancel');
  load();
  const noOpPast = win.snapUI().past;
  start();
  fire(document, 'pointerup');
  assert.equal(win.snapUI().past, noOpPast, 'click without dragging creates no history');
  assert.equal($('.graph-snap-guides').children.length, 0);
  load();
  win.snapZoom(2);
  start();
  fire(document, 'pointermove', 894, 894);
  assert.equal(element(a.id).style.left, '500px');
  fire(document, 'pointercancel');
  load();
  start();
  const viewport = $('.graph-scroll'),
    graph = $('.graph');
  Object.defineProperties(viewport, { clientWidth: { value: 500 }, clientHeight: { value: 500 } });
  viewport.getBoundingClientRect = () => ({ left: 0, top: 0, width: 500, height: 500 });
  graph.getBoundingClientRect = () => ({ x: -viewport.scrollLeft, y: -viewport.scrollTop });
  fire(document, 'pointermove', 496, 497);
  win.snapPan(100);
  win.snapPan(150);
  assert.ok(viewport.scrollLeft > 0, 'edge autopan advances scrolling');
  assert.ok(
    Number.parseFloat(element(a.id).style.left) > 500,
    'autopan recalculates unsnapped world pointer',
  );
  assert.equal(
    $('.graph-snap-guides').querySelectorAll('line:not([data-snap-kind])').length,
    0,
    'old alignment guides clear after autopan moves away',
  );
  fire(document, 'pointercancel');
  assert.deepEqual(pos(a.id), [100, 100]);
  load();
  doc.layers[0].locked = true;
  load();
  start();
  fire(document, 'pointermove', 496, 497);
  fire(document, 'pointerup');
  assert.deepEqual(pos(a.id), [100, 100]);
  assert.equal($('.graph-snap-guides').children.length, 0);
  // Inspect an actual shipped graph, keeping its wiring/output untouched by layout.
  const example = await fs.readFile(
    new URL('../examples/boolean-remap.json', import.meta.url),
    'utf8',
  );
  win.nect.load(example);
  const sampleBefore = win.nect.document;
  const [first] = sampleBefore.layers[0].nodes;
  fire(element(first.id).querySelector('.node-head'), 'pointerdown', 100, 100);
  fire(document, 'pointermove', 137, 143, { ctrlKey: true });
  fire(document, 'pointerup');
  const sampleAfter = win.nect.document;
  assert.notDeepEqual(sampleAfter.layers[0].nodes[0].position, first.position);
  sampleAfter.layers[0].nodes[0].position = first.position;
  assert.deepEqual(
    sampleAfter,
    sampleBefore,
    'shipped Boolean/Remap graph changes only its dragged card position',
  );
  await win.happyDOM.abort();
});
