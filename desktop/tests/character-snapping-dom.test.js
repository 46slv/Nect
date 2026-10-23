import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { characterInkBounds } from '../src/character-snapping.js';
import {
  document as makeDoc,
  node,
  connect,
  guideDefaults,
  setCharacterStyles,
  setCharacterOffsets,
  transformCharacterPoint,
} from '../src/core.js';
test('same-Text ink alignment and paired equal gaps: transformed offsets, zoom, locks, undo and cancellation', async () => {
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
    a: 2,
    b: 0,
    c: 0,
    d: 2,
    e: 80,
    f: 40,
    inverse() {
      const { a, b, c, d, e, f } = this,
        det = a * d - b * c;
      return {
        a: d / det,
        b: -b / det,
        c: -c / det,
        d: a / det,
        e: (c * f - d * e) / det,
        f: (b * e - a * f) / det,
      };
    },
  };
  globalThis.DOMPoint = class {
    constructor(x, y) {
      this.x = x;
      this.y = y;
    }
    matrixTransform(m) {
      return { x: this.x * m.a + this.y * m.c + m.e, y: this.x * m.b + this.y * m.d + m.f };
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
  source += '\nwindow.canvasPreview = () => displayDocument();';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  await new Promise((r) => setTimeout(r, 80));
  flush();

  const saved = () => JSON.stringify(win.nect.document);
  const stage = () => document.querySelector('.canvas-stage');
  const xy = ([x, y]) => [
    x * matrix.a + y * matrix.c + matrix.e,
    x * matrix.b + y * matrix.d + matrix.f,
  ];
  const pointer = (el, type, p, extra = {}) => {
    const [clientX, clientY] = xy(p);
    el.dispatchEvent(
      new win.PointerEvent(type, {
        bubbles: true,
        cancelable: true,
        button: 0,
        pointerId: 7,
        clientX,
        clientY,
        ...extra,
      }),
    );
    flush();
  };
  const undo = () => {
    document.querySelector('[data-action="undo"]').click();
    flush();
  };
  const redo = () => {
    document.querySelector('[data-action="redo"]').click();
    flush();
  };
  const near = (a, b) => assert.ok(Math.abs(a - b) < 0.0001, `${a} != ${b}`);
  const d = makeDoc(),
    l = d.layers[0],
    t = node('Text'),
    tr = node('Transform'),
    out = node('Output');
  t.text = 'AgA';
  t.params.position.value = [100, 180];
  t.params.size.value = 50;
  t.characterPositions = setCharacterStyles(t.text, undefined, { c2: { size: 70 } });
  l.nodes = [t, tr, out];
  l.output = out.id;
  connect(l, t.id, tr.id, 'path');
  connect(l, tr.id, out.id, 'path');
  d.editor = {
    ...guideDefaults(),
    snap: true,
    snapGuides: false,
    snapArtwork: false,
    snapGrid: false,
    snapArtboard: false,
  };
  const load = () => {
    win.nect.load(JSON.stringify(d));
    flush();
  };
  load();
  for (let i = 0; i < 250 && win.nect.evaluate().error; i++)
    await new Promise((r) => setTimeout(r, 20));
  assert.equal(win.nect.evaluate().error, null);
  const ink = () => {
    const r = win.nect.evaluate(),
      o = r.path.characterOccurrences[0];
    return r.textLayout(t.id).characters.map((g) => characterInkBounds(g.path, o.matrix));
  };
  const initial = ink();
  t.characterPositions = setCharacterOffsets(
    t.text,
    t.characterPositions,
    Object.fromEntries(
      initial.map((b, i) => ['c' + (i + 1), [[100, 200, 340][i] - b[0], i === 1 ? 40 : 0]]),
    ),
  );
  const mode = () => {
    document.querySelector('[data-character-mode]').click();
    flush();
  };
  const center = (b) => [(b[0] + b[2]) / 2, (b[1] + b[3]) / 2];
  // True ink bottoms: descender and larger size, distinct from baseline alignment.
  for (const zoom of [0.5, 1, 2]) {
    matrix.a = matrix.d = zoom;
    load();
    mode();
    const before = saved(),
      boxes = ink(),
      p = center(boxes[1]),
      dy = boxes[0][3] - boxes[1][3];
    pointer(stage(), 'pointerdown', p);
    pointer(document, 'pointermove', [p[0], p[1] + dy + 2 / zoom], { shiftKey: true });
    const guide = document.querySelector('[data-snap-kind="character"]');
    assert.ok(guide, 'same-Text guide with all other target kinds off');
    near(Number(guide.getAttribute('y1')), boxes[0][3]);
    pointer(document, 'pointerup', [p[0], p[1] + dy + 2 / zoom], { shiftKey: true });
    near(ink()[1][3], ink()[0][3]);
    const after = saved();
    undo();
    assert.equal(saved(), before);
    redo();
    assert.equal(saved(), after);
    assert.equal(document.querySelectorAll('.snap-match').length, 0);
  }
  matrix.a = matrix.d = 1;
  load();
  mode();
  const before = saved(),
    boxes = ink(),
    p = center(boxes[1]);
  const dx = (boxes[0][2] + boxes[2][0] - boxes[1][0] - boxes[1][2]) / 2;
  pointer(stage(), 'pointerdown', p);
  pointer(document, 'pointermove', [p[0] + dx - 2, p[1]], { shiftKey: true });
  const gap = document.querySelector('[data-snap-kind="character-gap"]');
  assert.ok(gap, 'equal-gap paired bracket is selected');
  assert.equal((gap.getAttribute('d').match(/M/g) || []).length, 6);
  pointer(document, 'pointermove', [p[0] + dx - 2, p[1]], { shiftKey: true, altKey: true });
  assert.equal(document.querySelectorAll('.snap-match').length, 0, 'Alt clears marks');
  pointer(document, 'pointermove', [p[0] + dx - 2, p[1]], { shiftKey: true });
  pointer(document, 'pointerup', [p[0] + dx - 2, p[1]], { shiftKey: true });
  const b = ink();
  near(b[1][0] - b[0][2], b[2][0] - b[1][2]);
  const gapAfter = saved();
  undo();
  assert.equal(saved(), before);
  redo();
  assert.equal(saved(), gapAfter);
  for (const cancel of ['Escape', 'pointercancel']) {
    load();
    mode();
    const p = center(ink()[1]);
    pointer(stage(), 'pointerdown', p);
    pointer(document, 'pointermove', [p[0] + dx - 2, p[1]], { shiftKey: true });
    if (cancel === 'Escape')
      stage().dispatchEvent(new win.KeyboardEvent('keydown', { bubbles: true, key: 'Escape' }));
    else pointer(document, 'pointercancel', p);
    flush();
    assert.equal(saved(), before);
    assert.equal(document.querySelectorAll('.snap-match').length, 0);
  }
  // Downstream affine changes physical movement but not the source offset contract.
  tr.params.rotation.value = 90;
  tr.params.scale.value = -2;
  load();
  mode();
  const rotatedBefore = saved(),
    rb = ink(),
    rp = center(rb[1]),
    dy = rb[0][1] - rb[1][1];
  pointer(stage(), 'pointerdown', rp);
  pointer(document, 'pointermove', [rp[0], rp[1] + dy + 2], { shiftKey: true });
  pointer(document, 'pointerup', [rp[0], rp[1] + dy + 2], { shiftKey: true });
  near(ink()[1][1], rb[0][1]);
  undo();
  assert.equal(saved(), rotatedBefore);
  l.locked = true;
  load();
  const locked = saved();
  const lp = center(ink()[1]);
  pointer(stage(), 'pointerdown', lp);
  pointer(document, 'pointermove', [lp[0] + 20, lp[1] + 20]);
  pointer(document, 'pointerup', lp);
  assert.equal(saved(), locked);
  win.happyDOM.abort();
});
