import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { serialize } from '../src/core.js';
import { graphPerformanceFixture } from './fixtures/graph-performance.js';

test('graph scale keeps world geometry and anchored scroll without inherited CSS zoom', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  t.after(() => win.happyDOM.abort());
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const k of ['window', 'document', 'HTMLElement', 'MutationObserver']) globalThis[k] = win[k];
  globalThis.requestAnimationFrame = () => 0;
  for (const [key, read] of Object.entries({
    offsetLeft: (el) => parseFloat(el.style.left) || 0,
    offsetTop: (el) => parseFloat(el.style.top) || 0,
    offsetWidth: (el) => parseFloat(el.style.width) || 264,
    offsetHeight: (el) => parseFloat(el.style.height) || 180,
    clientWidth: () => 800,
    clientHeight: () => 300,
  }))
    Object.defineProperty(win.HTMLElement.prototype, key, {
      configurable: true,
      get() {
        return read(this);
      },
    });
  const $ = (s) => win.document.querySelector(s);
  win.HTMLElement.prototype.getBoundingClientRect = function () {
    let x = 100,
      y = 200,
      width = 800,
      height = 300;
    const graph = $('.graph'),
      viewport = $('.graph-scroll');
    if (graph && (this === graph || this.closest('.node'))) {
      const z = Number(graph.style.transform.match(/scale\(([^)]+)\)/)?.[1] || 1);
      x += parseFloat(graph.style.left) - viewport.scrollLeft;
      y += parseFloat(graph.style.top) - viewport.scrollTop;
      if (this === graph) {
        width = parseFloat(graph.style.width) * z;
        height = parseFloat(graph.style.height) * z;
      } else {
        const node = this.closest('.node');
        x += (parseFloat(node.style.left) + (this.matches('.output') ? 244 : 0)) * z;
        y += (parseFloat(node.style.top) + 30) * z;
        width = height = 12 * z;
      }
    }
    return { x, y, left: x, top: y, width, height, right: x + width, bottom: y + height };
  };
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
      (_, p) => `from '${new URL('../src/' + p.slice(2), import.meta.url)}'`,
    );
  source +=
    '\nwindow.zoomTest=zoomGraph;window.boundsTest=updateGraphBounds;window.pointTest=graphPoint;window.drawTest=drawWires;window.viewTest=()=>({past:history.past.length,selection:[...selection],selected});';
  source +=
    '\nconst deadline=Date.now()+5000;while(!bundledFont){if(Date.now()>deadline)throw Error("Font unavailable");await new Promise(r=>setTimeout(r,10));}';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const f = graphPerformanceFixture(16);
  win.nect.load(serialize(f.doc));
  if ($('.alignment-panel')) $('.canvas-toolbar [data-action="align-panel"]').click();
  const viewport = $('.graph-scroll'),
    scroll = { scrollLeft: 0, scrollTop: 0 };
  // Happy DOM has no scrolling layout; emulate the native viewport's origin clamp.
  for (const key of Object.keys(scroll))
    Object.defineProperty(viewport, key, {
      configurable: true,
      get() {
        return scroll[key];
      },
      set(v) {
        scroll[key] = Math.max(0, v);
      },
    });
  win.zoomTest(1);
  const baseline = [
    serialize(win.nect.document),
    win.nect.export(),
    win.nect.fileState,
    win.viewTest(),
  ];
  const neutral = () =>
    assert.deepEqual(
      [serialize(win.nect.document), win.nect.export(), win.nect.fileState, win.viewTest()],
      baseline,
    );
  const near = (a, b) => a.forEach((v, i) => assert.ok(Math.abs(v - b[i]) < 1e-8, `${a} vs ${b}`));
  const paths = () =>
    [...win.document.querySelectorAll('.wire')].map((p) =>
      p
        .getAttribute('d')
        .match(/-?\d+(?:\.\d+)?(?:e[+-]?\d+)?/gi)
        .map(Number),
    );
  viewport.scrollLeft = 180;
  viewport.scrollTop = 100;
  win.drawTest();
  const wires = paths(),
    center = [500, 350],
    world = win.pointTest(...center);
  win.zoomTest(1.2);
  near(win.pointTest(...center), world);
  near([viewport.scrollLeft, viewport.scrollTop], [296, 150]);
  assert.equal($('.graph').style.transform, 'scale(1.2)');
  assert.equal($('.graph').style.zoom, '', 'no inherited CSS zoom');
  assert.equal(parseFloat($('.graph').style.left), 19.2);
  assert.equal(
    parseFloat($('.graph-space').style.width),
    (parseFloat($('.graph').style.width) + 32) * 1.2,
  );
  paths().forEach((p, i) => near(p, wires[i]));
  assert.equal(paths().length, f.connectionCount);
  neutral();

  const pointer = [250, 275],
    pointerWorld = win.pointTest(...pointer);
  win.zoomTest(0.6, ...pointer);
  near(win.pointTest(...pointer), pointerWorld);
  near([viewport.scrollLeft, viewport.scrollTop], [73, 37.5]);
  paths().forEach((p, i) => near(p, wires[i]));
  assert.equal($('[data-zoom="graph"]').textContent, '60%');
  neutral();

  for (const [next, scale] of [
    [0, 0.05],
    [99, 2],
  ]) {
    win.zoomTest(next);
    assert.equal($('.graph').style.transform, `scale(${scale})`);
    assert.ok(viewport.scrollLeft >= 0 && viewport.scrollTop >= 0);
    win.boundsTest([6000, 4000]);
    assert.ok(parseFloat($('.graph-space').style.width) >= 6000 + viewport.clientWidth);
    assert.ok(parseFloat($('.graph-space').style.height) >= 4000 + viewport.clientHeight);
    neutral();
  }
  await win.happyDOM.abort();
});
