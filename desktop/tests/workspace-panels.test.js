import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { createWorkspacePanels } from '../src/workspace-panels.js';

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
  f.controller.setRetreat(true);
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
