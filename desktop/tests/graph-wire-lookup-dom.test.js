import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { serialize } from '../src/core.js';
import { graphPerformanceFixture } from './fixtures/graph-performance.js';

test('wire target lookup retains first-field identity with bounded DOM scans', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const k of ['window', 'document', 'HTMLElement', 'MutationObserver']) globalThis[k] = win[k];
  globalThis.requestAnimationFrame = () => 0;
  win.HTMLElement.prototype.getBoundingClientRect = function () {
    const n = this.closest('.node');
    const x = n ? parseFloat(n.style.left) || 0 : 0,
      y = n ? parseFloat(n.style.top) || 0 : 0;
    return {
      x: x + (this.matches('.output') ? 264 : 0),
      y: y + 30,
      width: 12,
      height: 12,
      left: x,
      top: y,
      right: x + 12,
      bottom: y + 12,
    };
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
    '\nwindow.wireTestDraw=drawWires;window.wireTestPreview=(source)=>{wiring={source,pointer:[300,150]};drawWires();wiring=null;};window.wireTestPast=()=>history.past.length;';
  source += '\nwhile(!bundledFont)await new Promise(r=>setTimeout(r,10));';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  t.after(() => win.happyDOM.abort());
  const $ = (s) => document.querySelector(s),
    saved = () => serialize(win.nect.document);
  const load = (f = graphPerformanceFixture(16)) => {
    win.nect.load(serialize(f.doc));
    win.wireTestDraw(); // Execute the render's scheduled draw in this controlled RAF harness.
    const graph = $('.graph'),
      query = graph.querySelectorAll.bind(graph);
    let scans = 0;
    graph.querySelectorAll = (s) => {
      if (s === '.field') scans++;
      return query(s);
    };
    return { f, graph, scans: () => scans };
  };
  await t.test(
    'one field traversal for every redraw, unchanged wire paths and authored state',
    () => {
      const { f, scans } = load(),
        before = saved(),
        html = $('.wires').innerHTML,
        rev = win.nect.commandState;
      win.wireTestDraw();
      assert.equal(
        scans(),
        1,
        'one target index per draw, not one whole-field query per connection',
      );
      assert.equal($('.wires').innerHTML, html);
      assert.equal(document.querySelectorAll('.wire').length, f.connectionCount);
      assert.equal(saved(), before);
      assert.deepEqual(win.nect.commandState, rev);
      win.wireTestDraw();
      assert.equal(scans(), 2);
      assert.equal($('.wires').innerHTML, html);
    },
  );
  await t.test(
    'first duplicate field wins and a changed rendered field is resolved on the next draw',
    () => {
      const { f, graph } = load(),
        math = f.doc.layers[0].nodes.find((n) => n.type === 'Math'),
        original = graph.querySelector(`.field[data-node="${math.id}"][data-key="a"]`),
        duplicate = original.cloneNode(true),
        before = saved(),
        html = $('.wires').innerHTML;
      duplicate.querySelector('.input').getBoundingClientRect = () => ({
        x: 9999,
        y: 9999,
        width: 12,
        height: 12,
      });
      graph.append(duplicate);
      win.wireTestDraw();
      assert.equal($('.wires').innerHTML, html, 'retain the old find-first contract');
      original.remove();
      win.wireTestDraw();
      assert.match($('.wires').innerHTML, /10005/);
      assert.equal(saved(), before);
    },
  );
  await t.test(
    'disconnect and Undo rebuild current endpoints without a persistent stale index',
    () => {
      const { f } = load(),
        before = saved(),
        layer = f.doc.layers[0],
        math = layer.nodes.find((n) => n.type === 'Math');
      win.nect.dispatch({
        ...win.nect.commandState,
        type: 'disconnect',
        payload: { layerId: layer.id, nodeId: math.id, key: 'a' },
      });
      win.wireTestDraw();
      assert.equal(document.querySelectorAll('.wire').length, f.connectionCount - 1);
      assert.equal(win.wireTestPast(), 1);
      $('[data-action="undo"]').click();
      win.wireTestDraw();
      assert.equal(saved(), before);
      assert.equal(document.querySelectorAll('.wire').length, f.connectionCount);
    },
  );
  await t.test('unwired graphs and source-only pointer previews need no target traversal', () => {
    const f = graphPerformanceFixture(16);
    for (const n of f.doc.layers[0].nodes) for (const q of Object.values(n.params)) delete q.source;
    const { scans } = load(f),
      before = saved();
    win.wireTestDraw();
    assert.equal(scans(), 0);
    assert.equal(document.querySelectorAll('.wire').length, 0);
    win.wireTestPreview(f.dragNodeId);
    assert.equal(scans(), 0);
    assert.equal(document.querySelectorAll('.wire-preview').length, 1);
    assert.equal(saved(), before);
  });
});
