import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDoc, serialize } from '../src/core.js';
test('graph overview navigation is view-only, measured, guarded and cancellable', async () => {
  let resize;
  const win = new Window({ url: 'http://localhost:5173' });
  win.ResizeObserver = class {
    constructor(fn) {
      resize = fn;
    }
    observe() {}
    disconnect() {}
  };
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
    '\nwindow.snapUI = () => ({ past: history.past.length, drag: drag?.type, selection: [...selection], selected }); window.snapZoom = (z) => { graphZoom = z; }; window.snapPan = graphAutoPan; window.overviewRefresh = updateGraphOverview; window.overviewGeometry = graphOverviewGeometry;';
  // These suites synchronously create/evaluate Text after startup. Await the
  // font condition inside the test module, rather than racing filesystem I/O.
  source +=
    '\nconst fontTestDeadline = Date.now() + 5000; while (!bundledFont) { if (Date.now() >= fontTestDeadline) throw Error("Bundled test font did not finish loading"); await new Promise((r) => setTimeout(r, 10)); }';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const $ = (s) => document.querySelector(s);
  const click = (action) => $(`[data-action="${action}"]`).click();
  const fire = (target, type, x = 120, y = 60, extra = {}) =>
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
  const key = (name) =>
    $('.graph-overview-map').dispatchEvent(
      new win.KeyboardEvent('keydown', { bubbles: true, cancelable: true, key: name }),
    );
  let width = 800,
    height = 300;
  const setup = () => {
    const viewport = $('.graph-scroll');
    Object.defineProperties(viewport, {
      clientWidth: { configurable: true, get: () => width },
      clientHeight: { configurable: true, get: () => height },
    });
    for (const card of document.querySelectorAll('.graph .node')) {
      Object.defineProperties(card, {
        offsetLeft: { configurable: true, get: () => parseFloat(card.style.left) || 0 },
        offsetTop: { configurable: true, get: () => parseFloat(card.style.top) || 0 },
        offsetWidth: { configurable: true, get: () => parseFloat(card.style.width) || 264 },
        offsetHeight: { configurable: true, get: () => 400 },
      });
    }
    $('.graph-overview-map').getBoundingClientRect = () => ({
      left: 0,
      top: 0,
      width: 240,
      height: 120,
    });
    win.overviewRefresh();
  };
  const doc = makeDoc();
  doc.layers[0].nodes.forEach((n, i) => {
    n.position = [i * 1200, i * 800];
  });
  win.nect.load(serialize(doc));
  setup();
  assert.equal($('.graph-overview').hidden, true);
  click('graph-overview');
  assert.equal($('.graph-overview').hidden, false);
  assert.equal($('[data-action="graph-overview"]').getAttribute('aria-expanded'), 'true');
  assert.equal(document.querySelectorAll('.overview-node').length, doc.layers[0].nodes.length);
  assert.equal(+$('.overview-node').getAttribute('height'), 400);
  const baseline = [
    serialize(win.nect.document),
    win.nect.export(),
    win.nect.fileState,
    win.snapUI().past,
    win.snapUI().selection,
    win.snapUI().selected,
  ];
  const input = $('.inspector input');
  if (input) input.value = '123.456';
  const viewport = $('.graph-scroll');
  fire($('.graph-overview-map'), 'pointerdown', 220, 100);
  assert.equal(win.snapUI().drag, 'graph-overview');
  assert.ok(viewport.scrollLeft > 0);
  const cachedNode = $('.overview-node');
  const first = viewport.scrollLeft,
    projection = $('.graph-overview-map').getAttribute('viewBox');
  fire(document, 'pointermove', 200, 90, { pointerId: 8 });
  assert.equal(viewport.scrollLeft, first, 'other pointer cannot move view');
  click('graph-zoom-in');
  click('graph-overview');
  assert.equal($('.graph').style.zoom, '1');
  assert.equal($('.graph-overview').hidden, false, 'toggle blocked during drag');
  fire(document, 'pointermove', 200, 90);
  assert.ok(viewport.scrollLeft < first);
  assert.equal($('.overview-node'), cachedNode, 'pan updates only viewport, not all card SVGs');
  assert.equal(
    $('.graph-overview-map').getAttribute('viewBox'),
    projection,
    'mapping frozen during drag',
  );
  fire(document, 'pointerup');
  assert.equal(win.snapUI().drag, undefined);
  assert.equal($('.graph-scroll'), viewport, 'no rerender on pointer-up');
  if (input) assert.equal(input.value, '123.456');
  const committed = [viewport.scrollLeft, viewport.scrollTop];
  for (const cancel of ['pointercancel', 'lostpointercapture', 'Escape', 'blur']) {
    fire($('.graph-overview-map'), 'pointerdown', 230, 110);
    fire(document, 'pointermove', 220, 100);
    if (cancel === 'Escape') key('Escape');
    else if (cancel === 'blur') win.dispatchEvent(new win.Event('blur'));
    else fire(cancel === 'lostpointercapture' ? $('.graph-overview-map') : document, cancel);
    assert.equal(win.snapUI().drag, undefined, cancel);
    assert.deepEqual(
      [viewport.scrollLeft, viewport.scrollTop],
      committed,
      cancel + ' restores original viewport',
    );
  }
  const geometry = win.overviewGeometry();
  const v = geometry.viewport,
    b = geometry.bounds;
  const x = ((v.x + v.width / 2 - b.x) / b.width) * 240;
  const y = ((v.y + v.height / 2 - b.y) / b.height) * 120;
  fire($('.graph-overview-map'), 'pointerdown', x, y);
  assert.ok(Math.abs(viewport.scrollLeft - committed[0]) < 1e-8, 'viewport grab has no jump');
  fire(document, 'pointerup');
  key('ArrowRight');
  assert.ok(Math.abs(viewport.scrollLeft - committed[0] - 80) < 1e-8);
  click('graph-zoom-in');
  assert.equal(+$('.overview-viewport').getAttribute('width'), width / 1.2);
  viewport.scrollLeft = 123;
  viewport.dispatchEvent(new win.Event('scroll'));
  assert.equal(+$('.overview-viewport').getAttribute('x'), 123 / 1.2);
  width = 600;
  height = 80;
  resize();
  assert.equal(+$('.overview-viewport').getAttribute('width'), 500);
  assert.equal($('.graph-overview').style.width, '128px', 'small panels contain overview');
  $('.graph .node').style.width = '520px';
  resize();
  assert.equal(+$('.overview-node').getAttribute('width'), 520);
  click('graph-overview');
  viewport.scrollLeft = 250;
  click('graph-overview');
  assert.equal(+$('.overview-viewport').getAttribute('x'), 250 / 1.2);
  key('Escape');
  assert.equal($('.graph-overview').hidden, true);
  assert.equal(document.activeElement, $('[data-action="graph-overview"]'));
  click('graph-overview');
  input?.dispatchEvent(new win.CompositionEvent('compositionstart', { bubbles: true }));
  fire($('.graph-overview-map'), 'pointerdown');
  assert.equal(win.snapUI().drag, undefined, 'IME blocks overview');
  input?.dispatchEvent(new win.CompositionEvent('compositionend', { bubbles: true }));
  fire($('.node-head'), 'pointerdown');
  fire($('.graph-overview-map'), 'pointerdown');
  assert.notEqual(win.snapUI().drag, 'graph-overview', 'node drag owns pointer');
  fire(document, 'pointercancel');
  setup();
  assert.deepEqual(
    [
      serialize(win.nect.document),
      win.nect.export(),
      win.nect.fileState,
      win.snapUI().past,
      win.snapUI().selection,
      win.snapUI().selected,
    ],
    baseline,
    'document, output, dirty/revisions and undo unchanged',
  );
  // Structural rerenders release an in-flight navigation without leaving a stale capture.
  fire($('.graph-overview-map'), 'pointerdown', 230, 110);
  win.nect.load(serialize(doc));
  setup();
  assert.equal(win.snapUI().drag, undefined);
  assert.equal($('.graph-overview').hidden, false, 'visibility survives document rerender');
  width = 0;
  resize();
  assert.equal($('.graph-overview-map').children.length, 0, 'zero-size layout is safe');
  width = 600;
  resize();
  assert.ok($('.overview-viewport'));
  // Maximum supported node count is represented without omitting distant cards.
  const graphRoot = $('.graph');
  for (let i = document.querySelectorAll('.graph .node').length; i < 2000; i++) {
    const card = document.createElement('div');
    card.className = 'node';
    card.style.left = `${i * 500}px`;
    card.style.top = `${i * 50}px`;
    graphRoot.append(card);
  }
  setup();
  assert.equal(document.querySelectorAll('.overview-node').length, 2000);
  // Empty and single-node views remain finite, including a large measured graph.
  for (const count of [1, 0]) {
    for (const card of [...document.querySelectorAll('.graph .node')].slice(count)) card.remove();
    setup();
    assert.equal(document.querySelectorAll('.overview-node').length, count);
    assert.ok(
      $('.graph-overview-map')
        .getAttribute('viewBox')
        .split(' ')
        .every((n) => Number.isFinite(+n)),
    );
    if (count) {
      $('.graph .node').style.left = '1000000px';
      resize();
      assert.ok(win.overviewGeometry().bounds.width > 1000000);
    }
  }
  win.happyDOM.abort();
});
