import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import {
  createWorkspacePanels,
  DRAWER_PREFERENCES_KEY,
  workspaceRailGeometry,
} from '../src/workspace-panels.js';

const markup = `<div class="workspace"><nav class="tools"></nav><main class="middle"><div class="canvas-wrap"><div class="canvas-stage"></div><div class="node-viewers"></div></div><section class="graph-panel"></section></main><aside class="right"><section class="layers"><div class="panel-head">Layers</div></section><section class="alignment-panel">Align</section><section class="shared-colors-panel">Colors</section><section class="inspector">Inspector</section></aside></div>`;
function fixture() {
  const window = new Window();
  const root = window.document.createElement('div');
  root.innerHTML = markup;
  window.document.body.append(root);
  let panel = null;
  const controller = createWorkspacePanels({
    getAssetPanel: () => panel,
    onAssets: () => {
      panel = window.document.createElement('aside');
      panel.innerHTML = '<input value="kept">';
      window.document.body.append(panel);
    },
    onAlign: () => {},
    onColors: () => {},
  });
  controller.attach(root);
  controller.close('inspector', false);
  return {
    window,
    root,
    controller,
    get panel() {
      return panel;
    },
  };
}

test('opposite sides switch exclusively without changing the graph', () => {
  const f = fixture();
  const graph = f.root.querySelector('.graph-panel');
  f.controller.select('layers');
  assert.equal(f.root.querySelector('[data-drawer-side="right"]').hidden, false);
  f.controller.select('colors');
  assert.equal(f.root.querySelector('[data-drawer-side="right"]').hidden, true);
  assert.equal(f.root.querySelector('[data-drawer-side="right"]').inert, true);
  assert.equal(f.root.querySelector('[data-drawer-side="left"]').hidden, false);
  f.controller.select('inspector');
  assert.equal(f.root.querySelector('[data-drawer-side="left"]').hidden, true);
  assert.equal(f.root.querySelector('[data-drawer-side="left"]').inert, true);
  assert.equal(f.root.querySelector('.inspector').hidden, false);
  assert.equal(f.root.querySelector('.graph-panel'), graph);
  assert.equal(graph.closest('[data-drawer-side]'), null);
  f.window.happyDOM.abort();
});

test('same trigger closes, hidden panels are inert, and dismissal restores focus', () => {
  const f = fixture();
  const trigger = f.root.querySelector('[data-workspace-panel="layers"]');
  trigger.click();
  assert.equal(trigger.getAttribute('aria-expanded'), 'true');
  trigger.click();
  assert.equal(trigger.getAttribute('aria-expanded'), 'false');
  assert.equal(f.root.querySelector('[data-drawer-side="right"]').hidden, true);
  assert.equal(f.root.querySelector('[data-drawer-side="right"]').inert, true);
  assert.equal(f.window.document.activeElement, trigger);
  f.window.happyDOM.abort();
});

test('asset DOM and search survive switch, render and temporary drag retreat', () => {
  const f = fixture();
  f.controller.select('assets');
  const panel = f.panel,
    input = panel.querySelector('input');
  input.value = 'retained search';
  const slot = f.root.querySelector('[data-workspace-asset-slot]');
  slot.getBoundingClientRect = () => ({ left: 55, top: 70, width: 300, height: 420 });
  f.controller.refresh();
  const rect = [panel.style.left, panel.style.top, panel.style.width, panel.style.height];
  slot.getBoundingClientRect = () => ({ left: 0, top: 0, width: 0, height: 0 });
  f.controller.setRetreat(true);
  assert.deepEqual(
    [panel.style.left, panel.style.top, panel.style.width, panel.style.height],
    rect,
  );
  assert.equal(panel.style.opacity, '0');
  assert.equal(panel.style.pointerEvents, 'none');
  assert.equal(panel.isConnected, true);
  slot.getBoundingClientRect = () => ({ left: 65, top: 80, width: 280, height: 400 });
  f.controller.setRetreat(false);
  assert.equal(panel.hidden, false);
  assert.deepEqual(
    [panel.style.left, panel.style.top, panel.style.width, panel.style.height],
    ['65px', '80px', '280px', '400px'],
  );
  f.controller.select('colors');
  assert.equal(panel.hidden, true);
  f.root.innerHTML = markup;
  f.controller.attach(f.root);
  f.controller.select('assets');
  assert.equal(f.panel, panel);
  assert.equal(panel.querySelector('input'), input);
  assert.equal(input.value, 'retained search');
  f.window.happyDOM.abort();
});

test('repeat attach does not multiply triggers and Escape does not steal text editing', () => {
  const f = fixture();
  f.controller.attach(f.root);
  assert.equal(f.root.querySelectorAll('[data-workspace-panel]').length, 5);
  f.root.innerHTML = markup;
  f.controller.attach(f.root);
  assert.equal(f.root.querySelectorAll('[data-workspace-panel]').length, 5);
  f.controller.select('inspector');
  const inspector = f.root.querySelector('.inspector');
  const input = f.window.document.createElement('input');
  inspector.append(input);
  input.focus();
  input.dispatchEvent(new f.window.KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
  assert.equal(f.root.querySelector('[data-drawer-side="right"]').hidden, false);
  inspector.focus();
  inspector.dispatchEvent(new f.window.KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
  assert.equal(f.root.querySelector('[data-drawer-side="right"]').hidden, true);
  assert.equal(
    f.window.document.activeElement,
    f.root.querySelector('[data-workspace-panel="inspector"]'),
  );
  f.window.happyDOM.abort();
});

test('drawer preferences restore real owners and ignore invalid or legacy data', async () => {
  const w = new Window(),
    root = w.document.createElement('div');
  w.document.body.append(root);
  const values = new Map([
    [DRAWER_PREFERENCES_KEY, JSON.stringify({ left: 'assets', right: 'layers' })],
    ['nect.workspace-panels.v1', '{broken'],
  ]);
  let panel = null,
    opens = 0,
    focused = null;
  const storage = {
    getItem: (key) => values.get(key),
    setItem: (key, value) => values.set(key, value),
  };
  const create = () =>
    createWorkspacePanels({
      storage,
      getAssetPanel: () => panel,
      onAssets: (focus) => {
        opens++;
        focused = focus;
        panel = w.document.createElement('aside');
        w.document.body.append(panel);
      },
      onAlign: () => {},
      onColors: () => {},
    });
  root.innerHTML = markup;
  const controller = create();
  controller.attach(root);
  await Promise.resolve();
  assert.equal(opens, 0, 'legacy both-open state chooses the right side deterministically');
  assert.equal(root.querySelector('.layers').hidden, false);
  assert.equal(root.querySelector('[data-drawer-side=left]').hidden, true);
  controller.select('inspector');
  assert.deepEqual(JSON.parse(values.get(DRAWER_PREFERENCES_KEY)), {
    left: null,
    right: 'inspector',
    lastSide: 'right',
  });
  controller.setRetreat(true);
  assert.deepEqual(JSON.parse(values.get(DRAWER_PREFERENCES_KEY)), {
    left: null,
    right: 'inspector',
    lastSide: 'right',
  });
  values.set(DRAWER_PREFERENCES_KEY, '{broken');
  root.innerHTML = markup;
  const fallback = create();
  fallback.attach(root);
  assert.equal(root.querySelector('[data-drawer-side=right]').hidden, true);
  w.happyDOM.abort();
});

test('Viewer mode suspends the Assets portal and restores its live state on return', () => {
  const f = fixture();
  f.controller.select('assets');
  const panel = f.panel,
    input = panel.querySelector('input');
  input.value = 'viewer-round-trip';
  const wrap = f.root.querySelector('.canvas-wrap');
  wrap.dataset.viewerMode = 'viewers';
  f.controller.refresh();
  assert.equal(panel.hidden, true);
  assert.equal(panel.inert, true);
  assert.equal(f.root.querySelector('[data-drawer-side="left"]').dataset.motionEnabled, 'false');
  assert.equal(f.root.querySelector('.workspace-panel-bar-left').hidden, true);
  wrap.dataset.viewerMode = 'artwork';
  f.controller.refresh();
  assert.equal(panel.hidden, false);
  assert.equal(panel.querySelector('input'), input);
  assert.equal(input.value, 'viewer-round-trip');
  f.window.happyDOM.abort();
});

test('Viewer mode moves focus out of a suspended Color panel', () => {
  const f = fixture();
  const wrap = f.root.querySelector('.canvas-wrap');
  const tabs = f.window.document.createElement('div');
  tabs.className = 'viewer-tabs';
  tabs.innerHTML = '<button data-viewer-mode="viewers">Viewers</button>';
  wrap.prepend(tabs);
  f.controller.select('colors');
  const input = f.window.document.createElement('input');
  f.root.querySelector('.shared-colors-panel').append(input);
  input.focus();
  wrap.dataset.viewerMode = 'viewers';
  f.controller.refresh();
  assert.equal(f.window.document.activeElement, tabs.querySelector('button'));
  assert.equal(f.root.querySelector('[data-drawer-side="left"]').inert, true);
  f.window.happyDOM.abort();
});

test('rapid close, reopen and switch keeps only the latest child interactive', () => {
  const f = fixture();
  const host = f.root.querySelector('[data-drawer-side="right"]');
  const layers = f.root.querySelector('.layers');
  const inspector = f.root.querySelector('.inspector');
  const trigger = f.root.querySelector('[data-workspace-panel="layers"]');
  trigger.click();
  trigger.click();
  assert.equal(host.hidden, true);
  assert.equal(host.inert, true);
  assert.equal(f.window.document.activeElement, trigger);
  // The old child can remain painted for exit but is inside an inert host.
  assert.equal(layers.hidden, false);
  trigger.click();
  assert.equal(host.hidden, false);
  assert.equal(host.inert, false);
  assert.equal(layers.hidden, false);
  f.controller.select('inspector');
  assert.equal(layers.hidden, true);
  assert.equal(inspector.hidden, false);
  assert.equal(host.dataset.activePanel, 'inspector');
  assert.equal(f.root.querySelectorAll('.inspector').length, 1);
  assert.equal(f.window.document.activeElement, inspector);
  assert.equal(trigger.getAttribute('aria-expanded'), 'false');
  f.window.happyDOM.abort();
});

test('asset portal uses the stable slot origin and retreats without motion', () => {
  const f = fixture();
  f.controller.select('assets');
  const host = f.root.querySelector('[data-drawer-side="left"]');
  const slot = host.querySelector('[data-workspace-asset-slot]');
  slot.getBoundingClientRect = () => ({ left: 55, top: 70, width: 300, height: 420 });
  f.controller.refresh();
  assert.equal(f.panel.style.left, '55px');
  f.controller.setRetreat(true);
  assert.equal(host.dataset.motionSuppressed, 'true');
  assert.equal(f.panel.dataset.motionSuppressed, 'true');
  assert.equal(host.inert, true);
  assert.equal(f.panel.style.pointerEvents, 'none');
  f.controller.setRetreat(false);
  assert.equal(host.hidden, false);
  assert.equal(host.inert, false);
  assert.equal(f.panel.style.left, '55px');
  f.window.happyDOM.abort();
});

test('drawer controls expose named, decorative SVGs and live pressed state', () => {
  const f = fixture();
  for (const button of f.root.querySelectorAll('[data-workspace-panel]')) {
    assert.ok(button.getAttribute('aria-label'));
    assert.equal(button.querySelector('svg').getAttribute('aria-hidden'), 'true');
    assert.equal(button.textContent, '');
  }
  const trigger = f.root.querySelector('[data-workspace-panel="colors"]');
  trigger.click();
  assert.equal(trigger.getAttribute('aria-pressed'), 'true');
  trigger.click();
  assert.equal(trigger.getAttribute('aria-pressed'), 'false');
  f.window.happyDOM.abort();
});

test('animation is requested by selection, not attach or drag retreat', () => {
  const f = fixture();
  const host = f.root.querySelector('[data-drawer-side="left"]');
  assert.equal(host.dataset.motionEnabled, 'false');
  f.controller.select('assets');
  assert.equal(host.dataset.motionEnabled, 'true');
  assert.equal(f.panel.dataset.motionEnabled, 'true');
  f.controller.setRetreat(true);
  assert.equal(host.dataset.motionEnabled, 'false');
  f.controller.setRetreat(false);
  assert.equal(host.dataset.motionEnabled, 'false');
  f.controller.attach(f.root);
  assert.equal(host.dataset.motionEnabled, 'false');
  f.controller.close('assets', true, { animate: false });
  assert.equal(host.hidden, true);
  assert.equal(host.inert, true);
  assert.equal(host.dataset.motionEnabled, 'false');
  f.window.happyDOM.abort();
});

test('proximity is pointer-only; drag and blur cannot leave a stale peek', () => {
  const f = fixture();
  const bar = f.root.querySelector('.workspace-panel-bar-left');
  bar.getBoundingClientRect = () => ({ left: 40, right: 112, top: 50, bottom: 450, height: 400 });
  const frames = new Map();
  let next = 1;
  f.window.requestAnimationFrame = (callback) => {
    const id = next++;
    frames.set(id, callback);
    return id;
  };
  f.window.cancelAnimationFrame = (id) => frames.delete(id);
  const flush = () => {
    const callbacks = [...frames.values()];
    frames.clear();
    callbacks.forEach((callback) => callback());
  };
  const pointer = (type, init) =>
    f.window.document.dispatchEvent(new f.window.PointerEvent(type, { bubbles: true, ...init }));
  pointer('pointermove', { pointerType: 'mouse', clientX: 120, clientY: 250 });
  flush();
  assert.equal(bar.dataset.near, 'true');
  pointer('pointermove', { pointerType: 'touch', clientX: 500, clientY: 500 });
  flush();
  assert.equal(bar.dataset.near, 'true');
  const stage = f.root.querySelector('.canvas-stage');
  stage.addEventListener('pointerdown', (event) => event.stopPropagation());
  stage.addEventListener('pointermove', (event) => event.stopPropagation());
  stage.dispatchEvent(
    new f.window.PointerEvent('pointerdown', {
      bubbles: true,
      isPrimary: true,
      clientX: 300,
      clientY: 250,
    }),
  );
  assert.equal(bar.dataset.dragging, 'false');
  stage.dispatchEvent(
    new f.window.PointerEvent('pointermove', {
      bubbles: true,
      pointerType: 'mouse',
      buttons: 1,
      clientX: 306,
      clientY: 250,
    }),
  );
  assert.equal(bar.dataset.dragging, 'true');
  pointer('pointercancel', { pointerType: 'mouse' });
  assert.equal(bar.dataset.dragging, 'false');
  f.window.dispatchEvent(new f.window.Event('blur'));
  flush();
  assert.equal(bar.dataset.near, 'false');
  f.window.happyDOM.abort();
});

test('synchronous Assets mount refresh sees the original motion request', () => {
  const window = new Window();
  const root = window.document.createElement('div');
  root.innerHTML = markup;
  window.document.body.append(root);
  let panel, controller, observed;
  controller = createWorkspacePanels({
    getAssetPanel: () => panel,
    onAssets: () => {
      panel = window.document.createElement('aside');
      panel.hidden = true;
      window.document.body.append(panel);
      controller.refresh();
      observed = {
        host: root.querySelector('[data-drawer-side="left"]').dataset.motionEnabled,
        portal: panel.dataset.motionEnabled,
        hidden: panel.hidden,
      };
    },
    onAlign: () => {},
    onColors: () => {},
  });
  controller.attach(root);
  controller.select('assets');
  assert.deepEqual(observed, { host: 'true', portal: 'true', hidden: false });
  controller.setRetreat(true);
  assert.equal(root.querySelectorAll('.workspace-panel-bar[data-dragging="true"]').length, 2);
  controller.setRetreat(false);
  assert.equal(root.querySelectorAll('.workspace-panel-bar[data-dragging="false"]').length, 2);
  assert.equal(root.querySelectorAll('.workspace-panel-bar[data-near="true"]').length, 0);
  window.happyDOM.abort();
});

test('rail geometry scales to two and three controls and short panels', () => {
  for (const count of [2, 3])
    for (const height of [80, 228, 500]) {
      const geometry = workspaceRailGeometry(height, count);
      assert.equal(geometry.height, height);
      assert.doesNotMatch(geometry.edge, /NaN|Infinity/);
      assert.match(geometry.fill, /H6 Z$/);
      assert.match(geometry.edge, / L/, 'rounded tab retains the short straight shoulder');
    }
});

test('cross-side selection evacuates portal focus and keeps one active side', () => {
  const f = fixture();
  f.controller.select('assets');
  const panel = f.panel;
  const input = panel.querySelector('input');
  input.focus();
  f.controller.select('layers', { focus: false });
  assert.equal(panel.hidden, true);
  assert.equal(panel.inert, true);
  assert.equal(f.window.document.activeElement, f.root.querySelector('[data-workspace-panel="layers"]'));
  assert.deepEqual(f.controller.active, { left: null, right: 'layers' });
  f.controller.select('assets');
  assert.equal(f.panel, panel, 'rail switch retains the live Assets owner');
  assert.deepEqual(f.controller.active, { left: 'assets', right: null });
  f.controller.select('assets');
  assert.deepEqual(f.controller.active, { left: null, right: null });
  f.window.happyDOM.abort();
});

test('saved last side wins an old both-open preference without losing either owner data', async () => {
  const w = new Window();
  const root = w.document.createElement('div');
  root.innerHTML = markup;
  w.document.body.append(root);
  const values = new Map([[DRAWER_PREFERENCES_KEY, JSON.stringify({ left: 'assets', right: 'layers', lastSide: 'left' })]]);
  let panel;
  const controller = createWorkspacePanels({
    storage: { getItem: (key) => values.get(key), setItem: (key, value) => values.set(key, value) },
    getAssetPanel: () => panel,
    onAssets: () => { panel = w.document.createElement('aside'); w.document.body.append(panel); },
    onAlign: () => {},
    onColors: () => {},
  });
  controller.attach(root);
  await Promise.resolve();
  assert.deepEqual(controller.active, { left: 'assets', right: null });
  assert.equal(panel.hidden, false);
  assert.equal(root.querySelector('[data-drawer-side="right"]').hidden, true);
  controller.select('layers');
  assert.deepEqual(controller.active, { left: null, right: 'layers' });
  assert.equal(panel.isConnected, true);
  assert.equal(panel.hidden && panel.inert, true);
  w.happyDOM.abort();
});
