import { openDOMPanel } from './helpers/dom-workspace-controls.js';
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { blankDocument, blankLayer, node, point, serialize } from '../src/core.js';

test('Source Inspector round trips, guards, stale lists and document epochs', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement']) globalThis[key] = win[key];
  globalThis.MutationObserver = win.MutationObserver;
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
  source += `\nwindow.sourceTest = {
    select(id) { selected=id; selection=new Set([id]); artworkTarget=null; artworkSelection.clear(); render(); },
    artwork() { selected=null; selection.clear(); artworkTarget=activeLayer; artworkSelection=new Set([activeLayer]); render(); },
    read() { return { selected, selection:[...selection], artworkTarget, past:history.past.length, future:history.future.length,
      dirty:documents.dirty, viewers:structuredClone(viewerState), zoom, pan:[...pan], canvasFit, graphZoom,
      ticket:!!sourceReturn, picker:!!sourcePicker, points:structuredClone(pointSelection) }; },
    change(type,payload) { return commandChange(type,{layerId:current().id,...payload}); },
    svg() { return window.nect.export(); },
    flags(value) { composing=value; }, gesture(value) { drag=value ? {type:'test'} : null; },
    point(id,pid) { pointSelection={nodeId:id,ids:new Set([pid])}; },
    graph(z) { graphZoom=z; render(); }, undo() { stepHistory(); },
    open() { app.querySelector('[data-source-nav="open"]').click(); }
  };
  const fontTestDeadline=Date.now()+5000; while(!bundledFont) { if(Date.now()>fontTestDeadline) throw Error('font timeout'); await new Promise(r=>setTimeout(r,10)); }`;
  await import(
    'data:text/javascript;base64,' +
      Buffer.from(source + '\n//# sourceURL=source-navigation-test-main.js').toString('base64')
  );
  const api = win.sourceTest,
    q = (s) => document.querySelector(s),
    click = (s) => {
      const el = q(s);
      assert.ok(el, s);
      el.click();
    };
  const doc = blankDocument(),
    layer = doc.layers[0],
    output = layer.nodes[0],
    curve = node('Curve'),
    text = node('Text'),
    join = node('Join Geometry'),
    fill = node('Fill');
  curve.points = [point(0, 0), point(40, 0), point(40, 30), point(0, 30)];
  curve.closed = true;
  curve.collapsed = true;
  text.text = 'AB';
  join.params.a.source = curve.id;
  join.params.b.source = text.id;
  fill.params.path.source = join.id;
  output.params.path.source = fill.id;
  layer.nodes.push(curve, text, join, fill);
  const load = () => {
    win.nect.load(serialize(doc));
    document.activeElement?.blur();
    api.select(fill.id);
    openDOMPanel(document, 'inspector');
  };
  const enter = (id) => {
    click('[data-source-nav="open"]');
    click(`[data-source-id="${id}"]`);
  };
  load();
  await t.test(
    'open/cancel/enter/back preserve document, Output, cameras, viewers, dirty and Undo',
    () => {
      click('[data-assign-viewer="0"]');
      api.graph(1.4);
      const gs = q('.graph-scroll');
      gs.scrollLeft = 340;
      gs.scrollTop = 120;
      const before = serialize(win.nect.document),
        svg = api.svg(),
        state = api.read();
      click('[data-source-nav="open"]');
      assert.equal(api.read().selected, fill.id);
      click('[data-source-nav="cancel"]');
      assert.equal(api.read().ticket, false);
      enter(curve.id);
      assert.equal(api.read().selected, curve.id);
      assert.ok(q('.inspector [data-point-panel]'));
      assert.equal(q('.inspector [data-point-panel] input'), null, 'no implicit point selection');
      assert.equal(
        document.activeElement === q('[data-source-heading]'),
        true,
        'source heading receives focus',
      );
      click('[data-source-nav="back"]');
      assert.equal(api.read().selected, fill.id);
      assert.equal(api.read().ticket, false);
      assert.equal(q('.graph-scroll').scrollLeft, 340);
      assert.equal(q('.graph-scroll').scrollTop, 120);
      assert.equal(serialize(win.nect.document), before);
      assert.equal(api.svg(), svg);
      for (const key of [
        'past',
        'future',
        'dirty',
        'viewers',
        'zoom',
        'pan',
        'canvasFit',
        'graphZoom',
      ])
        assert.deepEqual(api.read()[key], state[key], key);
    },
  );
  await t.test('Artwork root and Text fields; Back retains an explicit Viewer reassignment', () => {
    api.artwork();
    enter(text.id);
    assert.equal(api.read().selected, text.id);
    assert.ok(q('.inspector textarea[data-text]'));
    assert.match(q('.source-navigation').textContent, /not a visible instance count/);
    click('[data-assign-viewer="1"]');
    const viewers = api.read().viewers;
    click('[data-source-nav="back"]');
    assert.equal(api.read().artworkTarget, layer.id);
    assert.deepEqual(api.read().viewers, viewers);
  });
  await t.test(
    'existing Curve edit adds one Undo; Back neither cancels edit nor restores stale point scope',
    () => {
      api.select(fill.id);
      api.point(curve.id, curve.points[0].id);
      enter(curve.id);
      const past = api.read().past;
      click('.inspector [data-point-panel-mode]');
      const input = q(
        `.inspector [data-key="${curve.points[0].id}.position"] input[data-axis="0"]`,
      );
      assert.ok(input, 'existing owned-point fields are available without expanding the card');
      input.focus();
      input.value = '3';
      input.dispatchEvent(new win.Event('change', { bubbles: true }));
      document.activeElement?.blur();
      assert.equal(api.read().past, past + 1);
      click('[data-source-nav="back"]');
      assert.equal(api.read().past, past + 1);
      assert.equal(api.read().points.ids.size, 0);
      assert.deepEqual(
        win.nect.document.layers[0].nodes.find((n) => n.id === curve.id).points[0].position.value,
        [3, 0],
      );
      api.undo();
      assert.deepEqual(
        win.nect.document.layers[0].nodes.find((n) => n.id === curve.id).points[0].position.value,
        [0, 0],
      );
    },
  );
  await t.test(
    'pointerdown prevents blur/commit of a numeric draft; IME and gestures refuse navigation',
    () => {
      load();
      const field = q('[data-dimension="width"]');
      field.focus();
      field.value = '777';
      let blurred = false;
      field.addEventListener('blur', () => (blurred = true));
      const before = serialize(win.nect.document),
        button = q('[data-source-nav="open"]');
      const event = new win.PointerEvent('pointerdown', {
        bubbles: true,
        cancelable: true,
        button: 0,
      });
      button.dispatchEvent(event);
      assert.equal(event.defaultPrevented, true);
      assert.equal(blurred, false);
      button.click();
      assert.equal(api.read().picker, false);
      assert.equal(serialize(win.nect.document), before);
      field.value = String(doc.width);
      field.blur();
      api.flags(true);
      button.click();
      assert.equal(api.read().picker, false);
      api.flags(false);
      api.gesture(true);
      button.click();
      assert.equal(api.read().picker, false);
      api.gesture(false);
      enter(text.id);
      const area = q('.inspector textarea[data-text]');
      area.focus();
      area.value = 'uncommitted';
      const back = q('[data-source-nav="back"]'),
        e = new win.PointerEvent('pointerdown', { bubbles: true, cancelable: true, button: 0 });
      back.dispatchEvent(e);
      assert.equal(e.defaultPrevented, true);
      back.click();
      assert.equal(api.read().ticket, true);
      area.value = 'AB';
      area.blur();
    },
  );
  await t.test(
    'stale list refresh requires a second explicit choice; lock does not prevent source inspection',
    () => {
      load();
      click('[data-source-nav="open"]');
      api.change('set-parameter', {
        nodeId: curve.id,
        key: curve.points[0].id + '.position',
        value: [8, 9],
      });
      click(`[data-source-id="${curve.id}"]`);
      assert.equal(api.read().selected, fill.id);
      assert.match(q('.status').textContent, /Review the refreshed/);
      click(`[data-source-id="${curve.id}"]`);
      assert.equal(api.read().selected, curve.id);
      const locked = structuredClone(doc);
      locked.layers[0].locked = true;
      win.nect.load(serialize(locked));
      api.select(fill.id);
      enter(curve.id);
      assert.match(q('.source-navigation').textContent, /read-only/);
      assert.equal(
        api.change('set-parameter', {
          nodeId: curve.id,
          key: curve.points[0].id + '.position',
          value: [9, 9],
        }),
        null,
      );
    },
  );
  await t.test(
    'missing return node stays missing; same IDs in replaced document discard ticket',
    () => {
      load();
      enter(curve.id);
      // Use the public command path to delete origin after replacing Output's input.
      api.change('disconnect', { nodeId: output.id, key: 'path' });
      api.change('delete-nodes', { nodeIds: [fill.id] });
      assert.equal(
        win.nect.document.layers[0].nodes.some((n) => n.id === fill.id),
        false,
      );
      click('[data-source-nav="back"]');
      assert.equal(api.read().selected, null);
      assert.equal(api.read().ticket, false);
      load();
      enter(text.id);
      win.nect.load(serialize(doc));
      assert.equal(api.read().ticket, false);
      assert.equal(q('[data-source-nav="back"]'), null);
    },
  );
  await t.test(
    'missing return composition is safe, while a second source trip keeps the original ticket',
    () => {
      const two = structuredClone(doc);
      two.layers.push(blankLayer('Other'));
      win.nect.load(serialize(two));
      api.select(fill.id);
      enter(curve.id);
      api.select(join.id);
      enter(text.id);
      click('[data-source-nav="back"]');
      assert.equal(api.read().selected, fill.id);
      enter(curve.id);
      openDOMPanel(document, 'layers');
      click(`[data-delete-layer="${layer.id}"]`);
      openDOMPanel(document, 'inspector');
      const before = serialize(win.nect.document),
        past = api.read().past;
      click('[data-source-nav="back"]');
      assert.equal(api.read().selected, null);
      assert.equal(api.read().ticket, false);
      assert.equal(serialize(win.nect.document), before);
      assert.equal(api.read().past, past);
      assert.match(q('.status').textContent, /Return composition no longer exists/);
    },
  );
  await t.test('Escape only cancels the source picker without clearing graph selection', () => {
    load();
    click('[data-source-nav="open"]');
    const candidate = q(`[data-source-id="${curve.id}"]`);
    candidate.focus();
    candidate.dispatchEvent(
      new win.KeyboardEvent('keydown', { key: 'Escape', bubbles: true, cancelable: true }),
    );
    assert.equal(api.read().picker, false);
    assert.equal(api.read().selected, fill.id);
    assert.equal(
      document.activeElement === q('[data-source-nav="open"]'),
      true,
      'source control receives focus',
    );
  });
  win.happyDOM.abort();
});
