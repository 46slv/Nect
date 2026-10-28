import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import {
  createWorkspacePanels,
  clampLayerHeight,
  WORKSPACE_PREFERENCES_KEY,
} from '../src/workspace-panels.js';

const markup =
  '<div class="workspace"><nav class="tools"></nav><main class="middle"></main><aside class="right"><section class="layers"></section><section class="inspector"></section></aside></div>';
function fixture(saved = null) {
  const window = new Window();
  const root = window.document.createElement('div');
  window.document.body.append(root);
  root.innerHTML = markup;
  let panel = null,
    assets = 0,
    align = 0;
  const values = new Map([[WORKSPACE_PREFERENCES_KEY, saved]]);
  const controller = createWorkspacePanels({
    getAssetPanel: () => panel,
    onAssets: () => assets++,
    onAlign: () => align++,
    storage: { getItem: (k) => values.get(k), setItem: (k, v) => values.set(k, v) },
  });
  controller.attach(root);
  return {
    window,
    root,
    controller,
    values,
    get assets() {
      return assets;
    },
    get align() {
      return align;
    },
    open() {
      panel = window.document.createElement('aside');
      panel.innerHTML = '<input value="kept">';
      window.document.body.append(panel);
      controller.refresh();
      return panel;
    },
  };
}

test('layer height clamps against available tool and Inspector space', () => {
  assert.equal(clampLayerHeight(400, 500, 180), 186);
  assert.equal(clampLayerHeight(-1, 500), 96);
  assert.equal(clampLayerHeight(NaN, 500), 160);
  assert.equal(clampLayerHeight(Infinity, 500), 160);
});

test('invalid stored workspace preferences cannot block a mounted editor', () => {
  const f = fixture('invalid');
  assert.equal(f.controller.layersHeight, 160);
  assert.equal(f.root.querySelectorAll('.workspace-panel-bar').length, 1);
  assert.equal(f.root.querySelector('.layers').style.height, '160px');
  f.window.happyDOM.abort();
});

test('workspace controls do not multiply after repeated attach', () => {
  const f = fixture();
  f.controller.attach(f.root);
  f.root.querySelector('[data-workspace-panel="assets"]').click();
  f.root.querySelector('[data-workspace-panel="align"]').click();
  assert.equal(f.assets, 1);
  assert.equal(f.align, 1);
  assert.equal(f.root.querySelectorAll('[data-layers-resize]').length, 1);
  f.window.happyDOM.abort();
});

test('keyboard resizing persists only workspace height and respects bounds', () => {
  const f = fixture();
  const separator = f.root.querySelector('[data-layers-resize]');
  separator.dispatchEvent(
    new f.window.KeyboardEvent('keydown', { key: 'ArrowDown', bubbles: true }),
  );
  assert.equal(f.controller.layersHeight, 176);
  assert.deepEqual(JSON.parse(f.values.get(WORKSPACE_PREFERENCES_KEY)), { layersHeight: 176 });
  separator.dispatchEvent(new f.window.KeyboardEvent('keydown', { key: 'End' }));
  assert.equal(f.controller.layersHeight, 366);
  separator.dispatchEvent(new f.window.KeyboardEvent('keydown', { key: 'Home' }));
  assert.equal(f.controller.layersHeight, 96);
  f.window.happyDOM.abort();
});

test('persistent library DOM survives app rerender with its controls intact', () => {
  const f = fixture();
  const panel = f.open();
  const input = panel.querySelector('input');
  input.value = 'retained search';
  f.root.innerHTML = markup;
  f.controller.attach(f.root);
  assert.ok(panel.isConnected);
  assert.equal(panel.parentElement, f.window.document.body);
  assert.equal(panel.querySelector('input'), input);
  assert.equal(input.value, 'retained search');
  assert.equal(
    f.root.querySelector('[data-workspace-panel="assets"]').getAttribute('aria-pressed'),
    'true',
  );
  assert.equal(f.root.querySelector('[data-workspace-asset-slot]').hidden, false);
  assert.ok(f.root.querySelector('.layers'));
  assert.ok(f.root.querySelector('.inspector'));
  f.window.happyDOM.abort();
});

test('persistent preferred height clamps temporarily while a tool is visible', () => {
  const f = fixture(JSON.stringify({ layersHeight: 350 }));
  f.open();
  assert.equal(f.controller.layersHeight, 350);
  assert.equal(f.root.querySelector('.layers').style.height, '186px');
  f.window.happyDOM.abort();
});

test('viewport movement realigns the portal even when sidebar size is unchanged', () => {
  const f = fixture();
  const panel = f.open();
  const slot = f.root.querySelector('[data-workspace-asset-slot]');
  let left = 675;
  slot.getBoundingClientRect = () => ({ left, top: 180, width: 290, height: 240 });
  f.controller.refresh();
  assert.equal(panel.style.left, '675px');
  left = 575;
  f.window.dispatchEvent(new f.window.Event('resize'));
  assert.equal(panel.style.left, '575px');
  assert.equal(panel.style.width, '290px');
  f.window.happyDOM.abort();
});
