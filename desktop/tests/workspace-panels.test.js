import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { createWorkspacePanels, DRAWER_PREFERENCES_KEY } from '../src/workspace-panels.js';

const markup = `<div class="workspace"><nav class="tools"></nav><main class="middle"><div class="canvas-wrap"><div class="canvas-stage"></div></div><section class="graph-panel"></section></main><aside class="right"><section class="layers"><div class="panel-head">Layers</div></section><section class="alignment-panel">Align</section><section class="shared-colors-panel">Colors</section><section class="inspector">Inspector</section></aside></div>`;
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
    onAlign: () => {}, onColors: () => {},
  });
  controller.attach(root);
  controller.close('inspector', false);
  return { window, root, controller, get panel() { return panel; } };
}

test('separate sides switch one real panel each without changing the graph', () => {
  const f = fixture();
  const graph = f.root.querySelector('.graph-panel');
  f.controller.select('layers');
  f.controller.select('colors');
  assert.equal(f.root.querySelector('.layers').hidden, false);
  assert.equal(f.root.querySelector('.shared-colors-panel').hidden, false);
  assert.equal(f.root.querySelector('.inspector').hidden, true);
  f.controller.select('inspector');
  assert.equal(f.root.querySelector('.layers').hidden, true);
  assert.equal(f.root.querySelector('.inspector').hidden, false);
  assert.equal(f.root.querySelector('.shared-colors-panel').hidden, false);
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
  const panel = f.panel, input = panel.querySelector('input');
  input.value = 'retained search';
  const slot = f.root.querySelector('[data-workspace-asset-slot]');
  slot.getBoundingClientRect = () => ({ left: 55, top: 70, width: 300, height: 420 });
  f.controller.refresh();
  const rect = [panel.style.left, panel.style.top, panel.style.width, panel.style.height];
  slot.getBoundingClientRect = () => ({ left: 0, top: 0, width: 0, height: 0 });
  f.controller.setRetreat(true);
  assert.deepEqual([panel.style.left, panel.style.top, panel.style.width, panel.style.height], rect);
  assert.equal(panel.style.opacity, '0');
  assert.equal(panel.style.pointerEvents, 'none');
  assert.equal(panel.isConnected, true);
  f.controller.setRetreat(false);
  assert.equal(panel.hidden, false);
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
  inspector.append(input); input.focus();
  input.dispatchEvent(new f.window.KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
  assert.equal(f.root.querySelector('[data-drawer-side="right"]').hidden, false);
  inspector.focus();
  inspector.dispatchEvent(new f.window.KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
  assert.equal(f.root.querySelector('[data-drawer-side="right"]').hidden, true);
  assert.equal(f.window.document.activeElement, f.root.querySelector('[data-workspace-panel="inspector"]'));
  f.window.happyDOM.abort();
});


test('drawer preferences restore real owners and ignore invalid or legacy data', async () => {
  const w = new Window(), root = w.document.createElement('div');
  w.document.body.append(root);
  const values = new Map([[DRAWER_PREFERENCES_KEY, JSON.stringify({ left: 'assets', right: 'layers' })], ['nect.workspace-panels.v1', '{broken']]);
  let panel = null, opens = 0, focused = null;
  const storage = { getItem: (key) => values.get(key), setItem: (key, value) => values.set(key, value) };
  const create = () => createWorkspacePanels({
    storage, getAssetPanel: () => panel,
    onAssets: (focus) => { opens++; focused = focus; panel = w.document.createElement('aside'); w.document.body.append(panel); },
    onAlign: () => {}, onColors: () => {},
  });
  root.innerHTML = markup;
  const controller = create(); controller.attach(root);
  await Promise.resolve();
  assert.equal(opens, 1); assert.equal(focused, false);
  assert.equal(root.querySelector('.layers').hidden, false);
  assert.equal(panel.hidden, false);
  controller.select('inspector');
  assert.deepEqual(JSON.parse(values.get(DRAWER_PREFERENCES_KEY)), { left: 'assets', right: 'inspector' });
  controller.setRetreat(true);
  assert.deepEqual(JSON.parse(values.get(DRAWER_PREFERENCES_KEY)), { left: 'assets', right: 'inspector' });
  values.set(DRAWER_PREFERENCES_KEY, '{broken');
  root.innerHTML = markup;
  const fallback = create(); fallback.attach(root);
  assert.equal(root.querySelector('.inspector').hidden, false);
  w.happyDOM.abort();
});
