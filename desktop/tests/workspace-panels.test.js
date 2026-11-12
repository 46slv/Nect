import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import {
  createWorkspacePanels,
  DRAWER_PREFERENCES_KEY,
  workspaceRailGeometry,
} from '../src/workspace-panels.js';
import { createWorkspaceSurfaceAdapters } from '../src/workspace-surfaces.js';
import { createEffectsPanel } from '../src/effects-panel.js';

function createTestWorkspacePanels({ storage, ...bindings }) {
  return createWorkspacePanels({ storage, adapters: createWorkspaceSurfaceAdapters(bindings) });
}

const markup = `<div class="workspace"><nav class="tools"></nav><main class="middle"><div class="canvas-wrap"><div class="canvas-stage"></div><div class="node-viewers"></div></div><section class="graph-panel"></section></main><aside class="right"><section class="layers"><div class="panel-head">Layers</div></section><section class="alignment-panel">Align</section><section class="shared-colors-panel">Colors</section><section class="inspector">Inspector</section></aside></div>`;
function fixture() {
  const window = new Window();
  const root = window.document.createElement('div');
  root.innerHTML = markup;
  window.document.body.append(root);
  let panel = null;
  const controller = createTestWorkspacePanels({
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

test('Effects keeps its search composition while another drawer is requested', (t) => {
  const win = new Window();
  t.after(() => win.happyDOM.abort());
  const root = win.document.createElement('div');
  root.innerHTML = markup;
  win.document.body.append(root);
  const effects = createEffectsPanel({
    document: win.document,
    getTarget: () => null,
    getEntries: () => [{ id: 'builtin:transform', label: 'Transform' }],
    onApply: () => assert.fail('No artwork is selected'),
  });
  const controller = createTestWorkspacePanels({
    getAssetPanel: () => null,
    onAssets: () => {},
    onAlign: () => {},
    createEffectsView: () => effects.createView(),
    canDeactivateEffects: () => effects.canDeactivate,
  });
  controller.attach(root);
  controller.select('effects');
  const input = root.querySelector('[data-effects-search]');
  input.focus();
  input.dispatchEvent(new win.CompositionEvent('compositionstart', { bubbles: true }));
  controller.select('inspector');
  assert.equal(controller.active.left, 'effects');
  assert.equal(win.document.activeElement, input);
  input.dispatchEvent(new win.CompositionEvent('compositionend', { bubbles: true }));
  controller.select('inspector');
  assert.equal(controller.active.right, 'inspector');
});

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

test('an active owner can defer close and same-side or cross-side switches', () => {
  const win = new Window();
  const root = win.document.createElement('div');
  root.innerHTML = markup;
  win.document.body.append(root);
  let blocked = true,
    writes = 0,
    assetOpens = 0;
  const controller = createTestWorkspacePanels({
    storage: { getItem: () => null, setItem: () => writes++ },
    getAssetPanel: () => null,
    onAssets: () => assetOpens++,
    onAlign: () => {},
    canDeactivateColors: () => !blocked,
  });
  controller.attach(root);
  controller.select('colors');
  const input = win.document.createElement('input');
  root.querySelector('.shared-colors-panel').append(input);
  input.focus();
  const beforeWrites = writes;
  controller.select('assets');
  controller.select('layers');
  controller.close('colors');
  controller.select('colors');
  assert.deepEqual(controller.active, { left: 'colors', right: null });
  assert.equal(root.querySelector('.shared-colors-panel').hidden, false);
  assert.equal(win.document.activeElement, input);
  assert.equal(writes, beforeWrites);
  assert.equal(assetOpens, 0);
  blocked = false;
  controller.select('assets');
  assert.deepEqual(controller.active, { left: 'assets', right: null });
  assert.equal(assetOpens, 1);
  win.happyDOM.abort();
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
  assert.equal(f.root.querySelectorAll('[data-workspace-panel]').length, 6);
  f.root.innerHTML = markup;
  f.controller.attach(f.root);
  assert.equal(f.root.querySelectorAll('[data-workspace-panel]').length, 6);
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
    createTestWorkspacePanels({
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

test('rapid close, reopen and switch keeps both editing panels interactive', () => {
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
  assert.equal(layers.hidden, false);
  assert.equal(inspector.hidden, false);
  assert.equal(host.dataset.activePanel, 'inspector');
  assert.equal(f.root.querySelectorAll('.inspector').length, 1);
  assert.equal(f.window.document.activeElement, inspector);
  assert.equal(trigger.getAttribute('aria-expanded'), 'true');
  assert.equal(trigger.getAttribute('aria-pressed'), 'false');
  f.window.happyDOM.abort();
});

test('Layers and Inspector share one stack with independent focus and stable DOM', () => {
  const f = fixture();
  const layers = f.root.querySelector('.layers');
  const inspector = f.root.querySelector('.inspector');
  const layersButton = f.root.querySelector('[data-workspace-panel="layers"]');
  const inspectorButton = f.root.querySelector('[data-workspace-panel="inspector"]');
  layersButton.click();
  assert.equal(layers.hidden, false);
  assert.equal(inspector.hidden, false);
  assert.equal(layersButton.getAttribute('aria-expanded'), 'true');
  assert.equal(inspectorButton.getAttribute('aria-expanded'), 'true');
  assert.equal(layersButton.getAttribute('aria-pressed'), 'true');
  assert.equal(inspectorButton.getAttribute('aria-pressed'), 'false');
  layers.scrollTop = 12;
  inspector.scrollTop = 24;
  inspectorButton.click();
  assert.equal(f.root.querySelector('.layers'), layers);
  assert.equal(f.root.querySelector('.inspector'), inspector);
  assert.equal(layers.scrollTop, 12);
  assert.equal(inspector.scrollTop, 24);
  assert.equal(f.window.document.activeElement, inspector);
  assert.equal(layersButton.getAttribute('aria-expanded'), 'true');
  assert.equal(inspectorButton.getAttribute('aria-pressed'), 'true');
  layers.focus();
  assert.equal(layersButton.getAttribute('aria-pressed'), 'true');
  layers.dispatchEvent(new f.window.KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
  assert.equal(f.root.querySelector('[data-drawer-side="right"]').hidden, true);
  assert.equal(layersButton.getAttribute('aria-expanded'), 'false');
  assert.equal(inspectorButton.getAttribute('aria-expanded'), 'false');
  assert.equal(f.window.document.activeElement, layersButton);
  f.window.happyDOM.abort();
});

test('Inspector composition blocks rail pointer blur and semantic switching until it ends', () => {
  const f = fixture();
  f.controller.select('inspector');
  const input = f.window.document.createElement('input');
  f.root.querySelector('.inspector').append(input);
  input.value = '編集中';
  input.focus();
  input.dispatchEvent(new f.window.CompositionEvent('compositionstart', { bubbles: true }));
  const layersButton = f.root.querySelector('[data-workspace-panel="layers"]');
  const down = new f.window.PointerEvent('pointerdown', { bubbles: true, cancelable: true });
  layersButton.dispatchEvent(down);
  assert.equal(down.defaultPrevented, true);
  const separator = f.root.querySelector('[data-workspace-split]');
  const splitDown = new f.window.PointerEvent('pointerdown', { bubbles: true, cancelable: true });
  separator.dispatchEvent(splitDown);
  assert.equal(splitDown.defaultPrevented, true);
  const layerAction = f.window.document.createElement('button');
  let activations = 0;
  layerAction.addEventListener('click', () => activations++);
  f.root.querySelector('.layers').append(layerAction);
  const sectionDown = new f.window.PointerEvent('pointerdown', { bubbles: true, cancelable: true });
  layerAction.dispatchEvent(sectionDown);
  layerAction.click();
  assert.equal(sectionDown.defaultPrevented, true);
  assert.equal(activations, 0);
  layersButton.click();
  f.controller.select('align');
  f.controller.close('inspector');
  assert.equal(f.controller.active.right, 'inspector');
  assert.equal(f.window.document.activeElement, input);
  assert.equal(input.value, '編集中');
  input.dispatchEvent(new f.window.CompositionEvent('compositionend', { bubbles: true }));
  layerAction.click();
  assert.equal(activations, 1);
  layersButton.click();
  assert.equal(f.controller.active.right, 'layers');
  assert.equal(f.root.querySelector('.inspector').hidden, false);
  assert.equal(input.value, '編集中');
  f.window.happyDOM.abort();
});

test('editing split keyboard and interrupted pointer gestures stay view-only and restore preferences', async () => {
  const win = new Window();
  const root = win.document.createElement('div');
  root.innerHTML = markup;
  win.document.body.append(root);
  const values = new Map([
    [DRAWER_PREFERENCES_KEY, JSON.stringify({ left: null, right: 'layers', lastSide: 'right' })],
  ]);
  const storage = {
    getItem: (key) => values.get(key),
    setItem: (key, value) => values.set(key, value),
  };
  const controller = createTestWorkspacePanels({
    storage,
    getAssetPanel: () => null,
    onAssets() {},
    onAlign() {},
    onColors() {},
  });
  controller.attach(root);
  await Promise.resolve();
  const host = root.querySelector('[data-drawer-side="right"]');
  Object.defineProperty(host, 'clientHeight', { configurable: true, value: 400 });
  host.getBoundingClientRect = () => ({ top: 100, height: 400 });
  controller.refresh();
  let split = root.querySelector('[data-workspace-split]');
  assert.equal(split.getAttribute('role'), 'separator');
  assert.equal(split.getAttribute('aria-orientation'), 'horizontal');
  assert.equal(split.getAttribute('aria-controls'), root.querySelector('.layers').id);
  assert.equal(Number(split.getAttribute('aria-valuenow')), 38);
  split.focus();
  split.dispatchEvent(
    new win.KeyboardEvent('keydown', { key: 'ArrowDown', bubbles: true, cancelable: true }),
  );
  const adjusted = Number(split.getAttribute('aria-valuenow'));
  assert.ok(adjusted > 38);
  assert.equal(JSON.parse(values.get(DRAWER_PREFERENCES_KEY)).splits.editing, adjusted);
  const stored = values.get(DRAWER_PREFERENCES_KEY);
  Object.defineProperty(host, 'clientHeight', { configurable: true, value: 120 });
  controller.refresh();
  assert.notEqual(Number(split.getAttribute('aria-valuenow')), adjusted);
  assert.equal(values.get(DRAWER_PREFERENCES_KEY), stored);
  Object.defineProperty(host, 'clientHeight', { configurable: true, value: 400 });
  controller.refresh();
  assert.equal(Number(split.getAttribute('aria-valuenow')), adjusted);
  split.dispatchEvent(
    new win.PointerEvent('pointerdown', {
      pointerId: 7,
      button: 0,
      isPrimary: true,
      bubbles: true,
      clientY: 250,
    }),
  );
  win.document.dispatchEvent(
    new win.PointerEvent('pointermove', { pointerId: 7, bubbles: true, clientY: 290, buttons: 1 }),
  );
  assert.notEqual(Number(split.getAttribute('aria-valuenow')), adjusted);
  win.document.dispatchEvent(
    new win.PointerEvent('pointercancel', { pointerId: 7, bubbles: true }),
  );
  assert.equal(Number(split.getAttribute('aria-valuenow')), adjusted);
  assert.equal(values.get(DRAWER_PREFERENCES_KEY), stored);
  split.dispatchEvent(
    new win.PointerEvent('pointerdown', {
      pointerId: 10,
      button: 0,
      isPrimary: true,
      bubbles: true,
      clientY: 250,
    }),
  );
  win.document.dispatchEvent(
    new win.PointerEvent('pointermove', { pointerId: 10, bubbles: true, clientY: 290, buttons: 1 }),
  );
  controller.select('align');
  win.document.dispatchEvent(new win.PointerEvent('pointerup', { pointerId: 10, bubbles: true }));
  assert.equal(JSON.parse(values.get(DRAWER_PREFERENCES_KEY)).splits.editing, adjusted);
  controller.select('layers');
  assert.equal(Number(split.getAttribute('aria-valuenow')), adjusted);
  const storedAfterSwitch = values.get(DRAWER_PREFERENCES_KEY);
  split.dispatchEvent(
    new win.PointerEvent('pointerdown', {
      pointerId: 8,
      button: 0,
      isPrimary: true,
      bubbles: true,
      clientY: 250,
    }),
  );
  win.document.dispatchEvent(
    new win.PointerEvent('pointermove', { pointerId: 8, bubbles: true, clientY: 290, buttons: 1 }),
  );
  root.innerHTML = markup;
  controller.attach(root);
  split = root.querySelector('[data-workspace-split]');
  win.document.dispatchEvent(
    new win.PointerEvent('pointermove', { pointerId: 8, bubbles: true, clientY: 320, buttons: 1 }),
  );
  win.document.dispatchEvent(new win.PointerEvent('pointerup', { pointerId: 8, bubbles: true }));
  assert.equal(values.get(DRAWER_PREFERENCES_KEY), storedAfterSwitch);
  assert.equal(Number(split.getAttribute('aria-valuenow')), adjusted);
  win.happyDOM.abort();
});

for (const action of ['click', 'ArrowUp', 'ArrowDown', 'Home', 'End'])
  test(`clamped split preserves its preferred size on no-op ${action}`, () => {
    const win = new Window();
    const root = win.document.createElement('div');
    root.innerHTML = markup;
    win.document.body.append(root);
    const original = JSON.stringify({
      left: null,
      right: 'layers',
      lastSide: 'right',
      splits: { editing: 38 },
    });
    let saved = original;
    const controller = createTestWorkspacePanels({
      storage: {
        getItem: () => saved,
        setItem: (_key, value) => {
          saved = value;
        },
      },
      getAssetPanel: () => null,
      onAssets() {},
      onAlign() {},
      onColors() {},
    });
    controller.attach(root);
    const host = root.querySelector('[data-drawer-side="right"]');
    Object.defineProperty(host, 'clientHeight', { configurable: true, value: 120 });
    controller.refresh();
    const split = root.querySelector('[data-workspace-split]');
    const clamped = Number(split.getAttribute('aria-valuenow'));
    assert.notEqual(clamped, 38);
    if (action === 'click') {
      split.dispatchEvent(
        new win.PointerEvent('pointerdown', {
          pointerId: 9,
          button: 0,
          isPrimary: true,
          bubbles: true,
        }),
      );
      win.document.dispatchEvent(
        new win.PointerEvent('pointerup', { pointerId: 9, bubbles: true }),
      );
    } else
      split.dispatchEvent(
        new win.KeyboardEvent('keydown', { key: action, bubbles: true, cancelable: true }),
      );
    assert.equal(saved, original);
    Object.defineProperty(host, 'clientHeight', { configurable: true, value: 400 });
    controller.refresh();
    assert.equal(Number(split.getAttribute('aria-valuenow')), 38);
    win.happyDOM.abort();
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
  controller = createTestWorkspacePanels({
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
  assert.equal(
    f.window.document.activeElement,
    f.root.querySelector('[data-workspace-panel="layers"]'),
  );
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
  const values = new Map([
    [DRAWER_PREFERENCES_KEY, JSON.stringify({ left: 'assets', right: 'layers', lastSide: 'left' })],
  ]);
  let panel;
  const controller = createTestWorkspacePanels({
    storage: { getItem: (key) => values.get(key), setItem: (key, value) => values.set(key, value) },
    getAssetPanel: () => panel,
    onAssets: () => {
      panel = w.document.createElement('aside');
      w.document.body.append(panel);
    },
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

test('legacy adapters retain DOM order, focus and sync visibility before portal measurement', async () => {
  const win = new Window(),
    root = win.document.createElement('div');
  root.innerHTML = markup;
  root
    .querySelector('.canvas-wrap')
    .insertAdjacentHTML(
      'afterbegin',
      '<div class="canvas-toolbar"><button data-action="align-panel"></button></div>',
    );
  win.document.body.append(root);
  const colors = root.querySelector('.shared-colors-panel');
  let panel = null,
    colorsCalls = 0,
    alignCalls = 0,
    assetCalls = 0;
  const controller = createTestWorkspacePanels({
    getAssetPanel: () => panel,
    onAssets: () => {
      assetCalls++;
      panel = win.document.createElement('aside');
      panel.innerHTML = '<input data-asset-search>';
      win.document.body.append(panel);
    },
    onAlign: () => {
      alignCalls++;
    },
    onColors: () => {
      colorsCalls++;
    },
  });
  controller.attach(root);
  await Promise.resolve();
  const left = root.querySelector('[data-drawer-side="left"]'),
    slot = left.querySelector('[data-workspace-asset-slot]');
  assert.equal(colors.parentNode, left);
  assert.deepEqual([...left.children], [slot, colors]);
  controller.select('colors');
  assert.equal(colors.hidden, false);
  assert.equal(win.document.activeElement, colors);
  assert.equal(colorsCalls, 1);
  const measures = [];
  slot.getBoundingClientRect = () => {
    measures.push({
      hidden: colors.hidden,
      pressed: root.querySelector('[data-action="align-panel"]').getAttribute('aria-pressed'),
    });
    return { left: 1, top: 2, width: 3, height: 4 };
  };
  controller.select('assets');
  assert.equal(assetCalls, 1);
  assert.ok(measures.length);
  assert.ok(measures.every((x) => x.hidden && x.pressed === 'false'));
  controller.select('align');
  assert.equal(alignCalls, 1);
  assert.equal(
    root.querySelector('[data-action="align-panel"]').getAttribute('aria-pressed'),
    'true',
  );
  assert.equal(win.document.activeElement, root.querySelector('.alignment-panel'));
  controller.refresh();
  controller.attach(root);
  assert.equal(alignCalls, 1);
  assert.equal(colorsCalls, 1);
  assert.equal(assetCalls, 1);
  assert.equal(colors.parentNode, left);
  assert.deepEqual([...left.children], [slot, colors]);
  controller.select('colors');
  assert.equal(colorsCalls, 2);
  assert.equal(colors.hidden, false);
  assert.equal(
    root.querySelector('[data-action="align-panel"]').getAttribute('aria-pressed'),
    'false',
  );
  win.happyDOM.abort();
});

test('incomplete attach target preserves the old portal and its Escape listener', () => {
  const win = new Window(),
    root = win.document.createElement('div');
  root.innerHTML = markup;
  win.document.body.append(root);
  let panel = null,
    opens = 0,
    writes = 0;
  const values = new Map(),
    storage = {
      getItem: (key) => values.get(key),
      setItem: (key, value) => {
        writes++;
        values.set(key, value);
      },
    };
  const controller = createTestWorkspacePanels({
    storage,
    getAssetPanel: () => panel,
    onAssets: () => {
      opens++;
      panel = win.document.createElement('aside');
      panel.innerHTML = '<input data-asset-search>';
      win.document.body.append(panel);
    },
    onAlign() {},
  });
  controller.attach(root);
  controller.select('assets');
  const original = panel,
    input = panel.querySelector('input');
  input.value = 'unsaved search';
  input.focus();
  const html = root.innerHTML,
    preferences = JSON.stringify([...values]),
    active = JSON.stringify(controller.active),
    beforeWrites = writes;
  const incomplete = win.document.createElement('div');
  incomplete.innerHTML = '<div class="canvas-wrap"></div>';
  win.document.body.append(incomplete);
  controller.attach(incomplete);
  assert.equal(root.innerHTML, html);
  assert.equal(panel, original);
  assert.equal(panel.parentNode, win.document.body);
  assert.equal(input.value, 'unsaved search');
  assert.equal(win.document.activeElement, input);
  assert.equal(JSON.stringify(controller.active), active);
  assert.equal(JSON.stringify([...values]), preferences);
  assert.equal(writes, beforeWrites);
  assert.equal(opens, 1);
  const trigger = root.querySelector('[data-workspace-panel="assets"]');
  trigger.focus();
  const escape = new win.KeyboardEvent('keydown', {
    key: 'Escape',
    bubbles: true,
    cancelable: true,
  });
  trigger.dispatchEvent(escape);
  assert.equal(escape.defaultPrevented, true);
  assert.equal(controller.active.left, null);
  assert.equal(panel.hidden, true);
  assert.equal(panel, input.parentNode);
  assert.equal(input.value, 'unsaved search');
  win.happyDOM.abort();
});

test('invalid roots and missing required descendants preserve queued restoration and observer', async () => {
  const win = new Window(),
    root = win.document.createElement('div');
  root.innerHTML = markup;
  win.document.body.append(root);
  let observed = 0,
    disconnected = 0,
    panel = null;
  const focusArgs = [];
  win.ResizeObserver = class {
    observe() {
      observed++;
    }
    disconnect() {
      disconnected++;
    }
  };
  const saved = JSON.stringify({ left: 'assets', right: null, lastSide: 'left' });
  const storage = {
    getItem: () => saved,
    setItem: () => {
      throw Error('unexpected write');
    },
  };
  const controller = createTestWorkspacePanels({
    storage,
    getAssetPanel: () => panel,
    onAssets: (focus) => {
      focusArgs.push(focus);
      panel = win.document.createElement('aside');
      win.document.body.append(panel);
    },
    onAlign() {},
  });
  controller.attach(root);
  const before = root.innerHTML;
  for (const target of [
    null,
    {},
    ...['canvas-wrap', 'right', 'tools'].map((missing) => {
      const el = win.document.createElement('div');
      el.innerHTML = markup;
      el.querySelector('.' + missing).remove();
      return el;
    }),
  ]) {
    assert.doesNotThrow(() => controller.attach(target));
    assert.equal(root.innerHTML, before);
    assert.equal(disconnected, 0);
    assert.equal(observed, 1);
    assert.equal(panel, null);
  }
  await Promise.resolve();
  assert.deepEqual(focusArgs, [false]);
  assert.ok(panel);
  assert.equal(controller.active.left, 'assets');
  assert.equal(panel.parentNode, win.document.body);
  assert.equal(disconnected, 0);
  win.happyDOM.abort();
});
