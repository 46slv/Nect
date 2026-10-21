import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { serialize } from '../src/core.js';
import { graphPerformanceFixture } from './fixtures/graph-performance.js';

test('Graph node Escape cancels the Session and renders the final UI once', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  t.after(() => win.happyDOM.abort());
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const k of ['window', 'document', 'HTMLElement', 'MutationObserver']) globalThis[k] = win[k];
  const frames = new Map();
  let seq = 0;
  globalThis.requestAnimationFrame = win.requestAnimationFrame = (fn) => {
    frames.set(++seq, fn);
    return seq;
  };
  win.cancelAnimationFrame = (id) => frames.delete(id);
  const flush = () => {
    const jobs = [...frames.values()];
    frames.clear();
    jobs.forEach((fn) => fn(0));
  };
  for (const [key, get] of Object.entries({
    offsetLeft: (e) => parseFloat(e.style.left) || 0,
    offsetTop: (e) => parseFloat(e.style.top) || 0,
    offsetWidth: () => 264,
    offsetHeight: () => 180,
    clientWidth: () => 1200,
    clientHeight: () => 800,
  }))
    Object.defineProperty(win.HTMLElement.prototype, key, {
      configurable: true,
      get() {
        return get(this);
      },
    });
  win.HTMLElement.prototype.getBoundingClientRect = function () {
    const n = this.closest('.node');
    const x = n ? parseFloat(n.style.left) + (this.matches('.output') ? 244 : 0) : 0,
      y = n ? parseFloat(n.style.top) + 30 : 0;
    return { x, y, left: x, top: y, width: 12, height: 12, right: x + 12, bottom: y + 12 };
  };
  globalThis.fetch = async () => ({
    ok: true,
    arrayBuffer: async () => {
      const b = await fs.readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
      return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
    },
  });
  let source = await fs.readFile(
    process.env.NECT_CANCEL_BASELINE_SOURCE || new URL('../src/main.js', import.meta.url),
    'utf8',
  );
  source = source
    .replace("import './style.css';", '')
    .replace(
      /from '(\.\/[^']+)'/g,
      (_, p) => `from '${new URL('../src/' + p.slice(2), import.meta.url)}'`,
    );
  source = source.replace(
    'function render(resetView = false, artworkResolution = null, graphScrollOverride = null) {',
    'function render(resetView = false, artworkResolution = null, graphScrollOverride = null) { window.cancelRenders++;',
  );
  win.cancelRenders = 0;
  source +=
    '\nwindow.cancelState=()=>({past:history.past.length,future:history.future.length,selected,selection:[...selection],gesture:session.hasGesture});';
  source +=
    '\nconst deadline=Date.now()+5000;while(!bundledFont){if(Date.now()>deadline)throw Error("Font unavailable");await new Promise(r=>setTimeout(r,10));}';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const $ = (s) => document.querySelector(s);
  const fire = (el, type, x = 100, y = 100, extra = {}) =>
    el.dispatchEvent(
      new win.PointerEvent(type, {
        bubbles: true,
        cancelable: true,
        pointerId: 7,
        button: 0,
        clientX: x,
        clientY: y,
        ctrlKey: true,
        ...extra,
      }),
    );
  const escape = () =>
    $('.graph-scroll').dispatchEvent(
      new win.KeyboardEvent('keydown', { bubbles: true, key: 'Escape' }),
    );
  const f = graphPerformanceFixture(16),
    [a, b] = f.doc.layers[0].nodes;
  f.doc.schemaVersion = Math.max(44, f.doc.schemaVersion);
  f.doc.layers[0].nodeGroups = [{ id: 'cancel-group', name: 'Keep group', nodeIds: [a.id, b.id] }];
  const card = (id) => $(`.node[data-id="${id}"]`);
  const start = (id = a.id, extra = {}) =>
    fire(card(id).querySelector('.node-head'), 'pointerdown', 100, 100, extra);
  const move = () => fire(document, 'pointermove', 140, 120);
  const load = () => {
    win.nect.load(serialize(f.doc));
    if ($('.alignment-panel')) $('.canvas-toolbar [data-action="align-panel"]').click();
    start();
    fire(document, 'pointerup');
    flush();
    win.cancelRenders = 0;
  };
  const saved = () => [
    serialize(win.nect.document),
    win.nect.export(),
    win.nect.fileState,
    win.cancelState().past,
    win.cancelState().future,
  ];
  const ui = () => [
    $('.inspector').innerHTML,
    $('.artboard').innerHTML,
    // Render tickets deliberately change. Compare every other byte and check
    // ticket freshness separately so stale-control protection stays covered.
    $('[data-node-groups]').innerHTML.replace(
      /data-node-group-token="\d+"/g,
      'data-node-group-token="CURRENT"',
    ),
    $('.wires').innerHTML,
  ];
  await t.test(
    'same selection restores all panels, groups, wires and snapshots with exactly one render',
    () => {
      load();
      const before = saved(),
        shown = ui(),
        selected = win.cancelState(),
        oldTicket = $('[data-node-group-token]').dataset.nodeGroupToken;
      start();
      move();
      assert.equal(card(a.id).style.left, '80px');
      assert.deepEqual(saved(), before);
      win.cancelRenders = 0;
      escape();
      flush();
      assert.equal(win.cancelRenders, 1);
      assert.notEqual($('[data-node-group-token]').dataset.nodeGroupToken, oldTicket);
      assert.deepEqual(saved(), before);
      assert.deepEqual(ui(), shown);
      assert.deepEqual(win.cancelState(), selected);
      assert.equal(card(a.id).style.left, '40px');
      assert.equal($('.graph-snap-guides').children.length, 0);
    },
  );
  await t.test(
    'new primary and multi-selection finish with the same Inspector/Canvas and selection as click-only',
    () => {
      load();
      start(b.id);
      fire(document, 'pointerup');
      flush();
      const expected = ui(),
        before = saved();
      load();
      start(b.id);
      move();
      win.cancelRenders = 0;
      escape();
      flush();
      assert.equal(win.cancelRenders, 1);
      assert.deepEqual(saved(), before);
      assert.deepEqual(ui(), expected);
      assert.deepEqual(win.cancelState().selection, [b.id]);
      start(a.id, { shiftKey: true, ctrlKey: false });
      flush();
      const groupBefore = saved(),
        selected = win.cancelState().selection;
      start(a.id);
      move();
      win.cancelRenders = 0;
      escape();
      flush();
      assert.equal(win.cancelRenders, 1);
      assert.deepEqual(saved(), groupBefore);
      assert.deepEqual(win.cancelState().selection, selected);
      assert.deepEqual(win.nect.document.layers[0].nodeGroups, f.doc.layers[0].nodeGroups);
    },
  );
  await t.test('cancel keeps Redo snapshots and a later commit is one Undo/Redo', () => {
    load();
    const original = saved()[0];
    start();
    move();
    fire(document, 'pointerup');
    flush();
    const committed = saved()[0];
    assert.equal(win.cancelState().past, 1);
    $('[data-action="undo"]').click();
    flush();
    const before = saved();
    assert.equal(win.cancelState().future, 1);
    start();
    move();
    escape();
    flush();
    assert.deepEqual(saved(), before);
    $('[data-action="redo"]').click();
    flush();
    assert.equal(saved()[0], committed);
    assert.equal(win.cancelState().past, 1);
    $('[data-action="undo"]').click();
    flush();
    assert.equal(saved()[0], original);
  });
  await t.test(
    'no-move Escape and other cancellation routes retain their own one-render behavior',
    () => {
      for (const route of ['Escape', 'pointercancel', 'blur']) {
        load();
        const before = saved();
        start();
        if (route !== 'Escape') move();
        win.cancelRenders = 0;
        if (route === 'Escape') escape();
        else if (route === 'blur') win.dispatchEvent(new win.Event('blur'));
        else fire(document, 'pointercancel');
        flush();
        assert.equal(win.cancelRenders, 1, route);
        assert.deepEqual(saved(), before);
        assert.equal(win.cancelState().gesture, false);
      }
    },
  );
});
