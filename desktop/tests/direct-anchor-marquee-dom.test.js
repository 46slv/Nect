import { openDOMPanel } from './helpers/dom-workspace-controls.js';
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { serialize, node, connect, evaluate } from '../src/core.js';
import { directSelectionSource } from '../src/direct-selection.js';
import { directMarqueeFixture } from './fixtures/direct-anchor-marquee.js';
import { compoundFixture } from './fixtures/contour-navigation.js';

test('Direct anchor marquee is a single-source, identity-guarded transient selection', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const k of ['window', 'document', 'HTMLElement', 'MutationObserver']) globalThis[k] = win[k];
  globalThis.requestAnimationFrame = () => 0;
  let view = { scale: 2, x: 80, y: 40 },
    measures = 0;
  const originalRect = win.HTMLElement.prototype.getBoundingClientRect;
  win.HTMLElement.prototype.getBoundingClientRect = function () {
    if (this.matches('.canvas-stage')) {
      measures++;
      return { left: 0, top: 0, right: 1100, bottom: 800, width: 1100, height: 800 };
    }
    return originalRect.call(this);
  };
  globalThis.DOMPoint = class {
    constructor(x, y) {
      this.x = x;
      this.y = y;
    }
    matrixTransform(m) {
      return { x: this.x * m.a + m.e, y: this.y * m.d + m.f };
    }
  };
  win.SVGElement.prototype.getScreenCTM = () => ({
    inverse: () => ({
      a: 1 / view.scale,
      d: 1 / view.scale,
      e: -view.x / view.scale,
      f: -view.y / view.scale,
    }),
  });
  win.SVGSVGElement.prototype.getScreenCTM = win.SVGElement.prototype.getScreenCTM;
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
    '\nwindow.anchorUI=()=>({layer:activeLayer,selected,selection:[...selection],points:[...pointSelection.ids],pointNode:pointSelection.nodeId,focus:pointFocus,past:history.past.length,future:history.future.length,drag:drag?.type});';
  source += '\nwindow.anchorSource=()=>directMarqueeSource();';
  source += '\nwindow.anchorRefresh=()=>updateCanvas(true);';
  // Simulate an independent Session update/owner change despite the GUI busy guard.
  source +=
    '\nwindow.anchorCommand=(type,payload)=>{const held=drag;drag=null;try{return session.dispatch({...session.state,type,payload});}finally{drag=held;}};';
  source +=
    '\nwindow.anchorOwner=(layer,node,point)=>{activeLayer=layer;selected=node;selection=new Set([node]);artworkTarget=null;artworkSelection=new Set();pointSelection={nodeId:node,ids:new Set([point])};pointFocus={nodeId:node,pointId:point};};';
  source += '\nwhile(!bundledFont) await new Promise(r=>setTimeout(r,10));';
  source += '\n//# sourceURL=main-direct-anchor-marquee-test.js';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  t.after(() => win.happyDOM.abort());
  const $ = (s) => document.querySelector(s),
    stage = () => $('.canvas-stage'),
    ui = () => structuredClone(win.anchorUI()),
    tick = () => new Promise((r) => setTimeout(r, 15)),
    snapshot = () => ({
      json: serialize(win.nect.document),
      svg: win.nect.export(),
      file: win.nect.fileState,
      command: win.nect.commandState,
      past: ui().past,
      future: ui().future,
    }),
    fire = (el, type, p = [0, 0], extra = {}) =>
      el.dispatchEvent(
        new win.PointerEvent(type, {
          bubbles: true,
          cancelable: true,
          pointerId: 17,
          button: 0,
          clientX: p[0] * view.scale + view.x,
          clientY: p[1] * view.scale + view.y,
          ...extra,
        }),
      ),
    key = (k) =>
      stage().dispatchEvent(
        new win.KeyboardEvent('keydown', { key: k, bubbles: true, cancelable: true }),
      ),
    drag = (a, b, extra = {}) => {
      fire($('.artboard'), 'pointerdown', a, extra);
      fire(document, 'pointermove', b, extra);
      fire(document, 'pointerup', b, extra);
    },
    pick = (id, extra = {}) => {
      const el = $(`.point-hit[data-point="${id}"]:not([data-handle])`);
      assert.ok(el, id);
      const p = [Number(el.getAttribute('cx')), Number(el.getAttribute('cy'))];
      fire(el, 'pointerdown', p, extra);
      fire(document, 'pointerup', p, extra);
    },
    load = (opts = {}) => {
      view = { scale: 2, x: 80, y: 40 };
      const f = directMarqueeFixture(opts);
      win.nect.load(serialize(f.doc));
      $('[data-tool="direct"]').click();
      return f;
    },
    pointRows = () =>
      [...document.querySelectorAll('.inspector-body [data-point-row].point-selected')].map(
        (r) => r.dataset.pointRow,
      );

  await t.test(
    'reverse replace previews only anchors, commits source IDs and synchronizes focus/rows once',
    () => {
      const f = load(),
        before = snapshot(),
        graph = $('.graph');
      fire($('.artboard'), 'pointerdown', [225, 135]);
      const reads = measures;
      fire(document, 'pointermove', [70, 70]);
      assert.equal(ui().drag, 'direct-marquee');
      assert.deepEqual(ui().points, []);
      assert.equal($('.graph'), graph);
      assert.equal(measures, reads);
      assert.equal($('.direct-anchor-marquee').style.pointerEvents, 'none');
      assert.equal($('.direct-anchor-marquee-guides').getAttribute('pointer-events'), 'none');
      assert.equal(
        document.querySelectorAll(
          '.direct-anchor-marquee-guides circle:not([style*="display: none"])',
        ).length,
        2,
      );
      assert.equal($('.direct-anchor-marquee').textContent, '2 anchors');
      fire(document, 'pointerup', [70, 70]);
      assert.deepEqual(ui().points, ['anchor-0', 'anchor-1']);
      assert.deepEqual(pointRows(), ui().points);
      assert.equal(ui().focus.pointId, 'anchor-0');
      assert.equal(document.querySelectorAll('.vertex.point-marker.active').length, 2);
      assert.equal($('.direct-anchor-marquee'), null);
      assert.equal(ui().selected, f.curve.id);
      assert.deepEqual(snapshot(), before);
    },
  );
  await t.test(
    'Shift unions the starting set across shrinking moves; focus is retained or chosen in source order',
    () => {
      load();
      pick('anchor-3');
      const before = snapshot();
      fire($('.artboard'), 'pointerdown', [70, 70], { shiftKey: true });
      fire(document, 'pointermove', [225, 230], { shiftKey: true });
      fire(document, 'pointermove', [225, 135], { shiftKey: false });
      fire(document, 'pointerup', [225, 135]);
      assert.deepEqual(ui().points, ['anchor-0', 'anchor-1', 'anchor-3']);
      assert.equal(ui().focus.pointId, 'anchor-3');
      drag([70, 70], [225, 135], { altKey: true });
      assert.deepEqual(ui().points, ['anchor-0', 'anchor-1']);
      assert.equal(ui().focus.pointId, 'anchor-0');
      assert.deepEqual(snapshot(), before);
    },
  );
  await t.test(
    '4 CSS-pixel threshold and empty rectangle keep blank-click and no-history behavior',
    () => {
      load();
      pick('anchor-1');
      const before = snapshot();
      drag([30, 30], [31.5, 30], { shiftKey: true });
      assert.deepEqual(ui().points, []);
      assert.equal(ui().pointNode, null);
      assert.equal(ui().focus, null);
      fire($('.artboard'), 'pointerdown', [30, 30]);
      fire(document, 'pointermove', [32, 30]);
      assert.equal($('.direct-anchor-marquee').hidden, false);
      fire(document, 'pointerup', [32, 30]);
      assert.equal(ui().pointNode, 'anchor-marquee-source');
      assert.deepEqual(ui().points, []);
      assert.deepEqual(snapshot(), before);
    },
  );
  await t.test(
    'displayed rotation plus nonuniform scale remains correct at two zoom/pan transforms',
    () => {
      for (const nextView of [
        { scale: 0.75, x: 140, y: 90 },
        { scale: 1.8, x: -70, y: 15 },
      ]) {
        const f = load({ transformed: true });
        view = nextView;
        const route = directSelectionSource(f.doc, f.layer.id),
          points = route.path.contours[0].points,
          p = points[0].position,
          a = p.map((v) => v - 12),
          b = p.map((v) => v + 12),
          before = snapshot();
        drag(a, b);
        assert.deepEqual(ui().points, ['anchor-0']);
        assert.notDeepEqual(p, f.curve.points[0].position.value);
        assert.deepEqual(snapshot(), before);
      }
    },
  );
  await t.test(
    'compound holes and reversed point storage use stable source order without unit expansion',
    () => {
      const f = compoundFixture([8, 4, 6, 2]);
      win.nect.load(serialize(f.doc));
      $('[data-tool="direct"]').click();
      const before = snapshot(),
        expected = f.path.points
          .filter((p) => p.position.value[0] <= 160 && p.position.value[1] <= 165)
          .map((p) => p.id);
      drag([10, 10], [160, 165]);
      assert.deepEqual(ui().points, expected);
      assert.equal(ui().focus.pointId, expected[0]);
      assert.ok(expected.some((id) => id.startsWith('scope-hole')));
      assert.deepEqual(snapshot(), before);
    },
  );
  await t.test(
    'locked/driven anchors and a locked route remain selectable; atomic edit guards still reject',
    () => {
      for (const options of [{ locked: true }, { driven: true }, { lockedRoute: true }]) {
        const f = directMarqueeFixture(options);
        if (options.lockedRoute) f.layer.locked = true;
        win.nect.load(serialize(f.doc));
        openDOMPanel(document, 'layers');
        $(`[data-select-layer="${f.layer.id}"]`).click();
        $('[data-tool="direct"]').click();
        const before = snapshot();
        drag([70, 70], [225, 135]);
        assert.deepEqual(ui().points, ['anchor-0', 'anchor-1'], JSON.stringify(options));
        key('ArrowRight');
        assert.deepEqual(snapshot(), before);
        assert.match(
          $('.status').textContent,
          options.lockedRoute ? /Select an editable source path/ : /locked|driven/i,
        );
      }
    },
  );
  await t.test(
    'Escape, pointercancel, lost capture and blur preserve exact starting selection/focus; late events cannot commit',
    () => {
      for (const cancel of ['Escape', 'pointercancel', 'lostpointercapture', 'blur']) {
        load();
        pick('anchor-1');
        const before = snapshot(),
          selection = ui();
        fire($('.artboard'), 'pointerdown', [70, 70]);
        fire(document, 'pointermove', [225, 235]);
        if (cancel === 'Escape') key('Escape');
        else if (cancel === 'blur') win.dispatchEvent(new win.Event('blur'));
        else fire(cancel === 'lostpointercapture' ? stage() : document, cancel, [225, 235]);
        assert.deepEqual(ui(), selection);
        assert.equal($('.direct-anchor-marquee'), null);
        fire(document, 'pointermove', [40, 40]);
        fire(document, 'pointerup', [225, 235]);
        assert.deepEqual(ui(), selection);
        assert.deepEqual(snapshot(), before);
      }
    },
  );
  await t.test(
    'artboard replacement, selected owner change and new Session discard stale selections',
    () => {
      for (const stale of ['artboard', 'owner', 'session']) {
        const f = load();
        pick('anchor-1');
        fire($('.artboard'), 'pointerdown', [70, 70]);
        fire(document, 'pointermove', [225, 235]);
        if (stale === 'artboard') win.anchorRefresh();
        else if (stale === 'owner')
          win.anchorOwner(f.other.id, f.otherCurve.id, f.otherCurve.points[0].id);
        else win.nect.load(serialize(directMarqueeFixture().doc));
        const before = snapshot(),
          next = ui();
        delete next.drag;
        fire(document, 'pointermove', [225, 235]);
        fire(document, 'pointerup', [225, 235]);
        const after = ui();
        delete after.drag;
        assert.deepEqual(after, next);
        assert.deepEqual(snapshot(), before);
        assert.equal($('.direct-anchor-marquee'), null);
      }
    },
  );
  await t.test(
    'Path Remap fingerprint/revision changes discard a captured set without rebinding or restoring over newer state',
    () => {
      const f = compoundFixture([4, 4, 4, 2]),
        remap = node('Path Remap');
      f.layer.nodes.splice(1, 0, remap);
      f.fill.params.path.source = null;
      connect(f.layer, f.path.id, remap.id, 'path');
      connect(f.layer, remap.id, f.fill.id, 'path');
      win.nect.load(serialize(f.doc));
      $('[data-tool="direct"]').click();
      const oldSource = win.anchorSource();
      fire($('.artboard'), 'pointerdown', [10, 10]);
      fire(document, 'pointermove', [160, 165]);
      assert.equal(ui().drag, 'direct-marquee');
      assert.throws(
        () =>
          win.nect.dispatch({
            ...win.nect.commandState,
            type: 'translate-points',
            payload: {
              layerId: f.layer.id,
              nodeId: f.path.id,
              pointIds: [f.path.points[0].id],
              delta: [1, 0],
            },
          }),
        /GESTURE_ACTIVE/,
      );
      win.anchorCommand('translate-points', {
        layerId: f.layer.id,
        nodeId: f.path.id,
        pointIds: [f.path.points[0].id],
        delta: [1, 0],
      });
      assert.ok(oldSource.scope.fingerprint);
      assert.notEqual(win.anchorSource().scope.fingerprint, oldSource.scope.fingerprint);
      assert.deepEqual(win.anchorSource().scope.ids, oldSource.scope.ids);
      const before = snapshot(),
        next = ui();
      delete next.drag;
      fire(document, 'pointerup', [160, 165]);
      const after = ui();
      delete after.drag;
      assert.deepEqual(after, next);
      assert.deepEqual(snapshot(), before);
      assert.deepEqual(win.nect.document.layers[0].nodes.find((n) => n.id === remap.id).edits, []);
      assert.ok(evaluate(win.nect.document.layers[0]).run(remap.id).contours.length);
    },
  );
  await t.test(
    'existing anchor/handle/segment/guide/pan routes win; clamp never extends preview outside viewport',
    async () => {
      const f = load(),
        before = snapshot();
      pick('anchor-0');
      const handle = $('.point-hit[data-point="anchor-0"][data-handle="handleB"]');
      assert.ok(handle);
      fire(handle, 'pointerdown', [150, 60]);
      assert.equal(ui().drag, 'point');
      fire(document, 'pointercancel', [150, 60]);
      fire($('.artboard'), 'pointerdown', [205, 150]);
      assert.notEqual(ui().drag, 'direct-marquee');
      fire(document, 'pointerup', [205, 150]);
      await tick();
      fire($('.artboard'), 'pointerdown', [30, 30], { button: 1 });
      assert.equal(ui().drag, 'pan');
      fire(document, 'pointercancel');
      win.nect.dispatch({
        ...win.nect.commandState,
        type: 'create-guide',
        payload: { axis: 'x', position: 40 },
      });
      const guideSnapshot = snapshot(),
        guide = $('[data-guide]');
      assert.ok(guide);
      fire(guide, 'pointerdown', [40, 50]);
      assert.equal(ui().drag, 'guide');
      fire(document, 'pointercancel');
      assert.deepEqual(snapshot(), guideSnapshot);
      fire($('.artboard'), 'pointerdown', [30, 30]);
      fire(document, 'pointermove', [100000, 100000]);
      const overlay = $('.direct-anchor-marquee');
      assert.ok(parseFloat(overlay.style.left) + parseFloat(overlay.style.width) <= 1100);
      assert.ok(parseFloat(overlay.style.top) + parseFloat(overlay.style.height) <= 800);
      key('Escape');
      assert.deepEqual(JSON.parse(before.json), JSON.parse(serialize(f.doc)));
    },
  );
});
