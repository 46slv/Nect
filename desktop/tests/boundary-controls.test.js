import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { ensureBoundaryControls, syncBoundaryViewerMode } from '../src/boundary-controls.js';

function fixture() {
  const window = new Window();
  const app = window.document.createElement('div');
  app.innerHTML = `<div class="canvas-wrap" data-viewer-mode="artwork"><button data-viewer-mode="viewers">Viewers</button><div class="canvas-toolbar"><div class="canvas-toolbar-group"><button data-action="zoom-out">−</button><output data-zoom="canvas">100%</output><button data-action="zoom-in">+</button></div><button data-pen-finish>Finish</button></div></div><section class="graph-panel"><div class="graph-toolbar"><strong>Node graph</strong><span class="graph-layer-name">Layer</span><span class="spacer"></span><select id="add-node"></select><button data-action="find-node">Find</button><button data-action="graph-overview">Overview</button></div><div class="node-preset-controls"><input aria-label="Preset name"></div><details class="node-group-controls" data-node-groups><summary>Groups</summary><div class="node-group-fields"><input aria-label="Group name"></div></details><div class="graph-body"></div></section>`;
  window.document.body.append(app);
  return { window, app };
}

test('boundary disclosures retain real controls and keep hidden content inert', () => {
  const { window, app } = fixture();
  const find = app.querySelector('[data-action="find-node"]');
  const zoom = app.querySelector('[data-action="zoom-in"]');
  const graphBody = app.querySelector('.graph-body');
  ensureBoundaryControls(app, window.document);
  ensureBoundaryControls(app, window.document);
  assert.equal(app.querySelectorAll('[data-action="find-node"]').length, 1);
  assert.equal(app.querySelector('[data-action="find-node"]'), find);
  assert.equal(app.querySelector('[data-action="zoom-in"]'), zoom);
  assert.equal(app.querySelector('.graph-body'), graphBody);
  const graph = app.querySelector('[aria-label="Toggle node graph controls"]');
  const view = app.querySelector('[aria-label="Toggle artboard view controls"]');
  const graphExtras = app.querySelector('#boundary-graph-extras');
  const canvasExtras = app.querySelector('#boundary-canvas-extras');
  assert.equal(graphExtras.hidden && graphExtras.inert, true);
  assert.equal(canvasExtras.hidden && canvasExtras.inert, true);
  assert.equal(app.querySelector('[data-pen-finish]').closest('[hidden]'), null);
  graph.click();
  assert.equal(graph.getAttribute('aria-expanded'), 'true');
  assert.equal(graphExtras.hidden || graphExtras.inert, false);
  find.focus();
  view.click();
  assert.equal(graphExtras.hidden || graphExtras.inert, false);
  assert.equal(window.document.activeElement, find);
  assert.equal(view.getAttribute('aria-expanded'), 'true');
  assert.equal(canvasExtras.hidden || canvasExtras.inert, false);
  view.dispatchEvent(
    new window.KeyboardEvent('keydown', { key: 'Escape', bubbles: true, cancelable: true }),
  );
  assert.equal(view.getAttribute('aria-expanded'), 'false');
  assert.equal(window.document.activeElement, view);
  assert.equal(graph.getAttribute('aria-expanded'), 'true');
  view.click();
  graph.click();
  assert.equal(graph.getAttribute('aria-expanded'), 'false');
  assert.equal(view.getAttribute('aria-expanded'), 'true');
  assert.equal(canvasExtras.hidden || canvasExtras.inert, false);
  graph.click();
  find.focus();
  graph.click();
  assert.equal(window.document.activeElement, graph);
  assert.equal(view.getAttribute('aria-expanded'), 'true');
  graph.click();
  zoom.focus();
  app.querySelector('.canvas-wrap').dataset.viewerMode = 'viewers';
  syncBoundaryViewerMode(app);
  assert.equal(app.querySelector('.canvas-toolbar').hidden, true);
  assert.equal(window.document.activeElement, app.querySelector('[data-viewer-mode="viewers"]'));
  assert.equal(graph.getAttribute('aria-expanded'), 'true');
  app.querySelector('.canvas-wrap').dataset.viewerMode = 'artwork';
  syncBoundaryViewerMode(app);
  assert.equal(app.querySelector('.canvas-toolbar').hidden, false);
  assert.equal(view.getAttribute('aria-expanded'), 'true');
  window.happyDOM.abort();
});
