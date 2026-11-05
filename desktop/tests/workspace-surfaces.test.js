import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { document as makeDocument, CommandSession, serialize } from '../src/core.js';
import { createWorkspacePanels } from '../src/workspace-panels.js';
import { WORKSPACE_SURFACES, createWorkspaceSurfaceAdapters } from '../src/workspace-surfaces.js';

const markup = `<div class="workspace"><nav class="tools"></nav><main class="middle"><div class="canvas-wrap"><div class="canvas-stage"></div><div class="node-viewers"></div></div><section class="graph-panel"></section></main><aside class="right"><section class="layers"></section><section class="alignment-panel"></section><section class="shared-colors-panel"></section><section class="inspector"></section></aside></div>`;

test('invalid registration fails before DOM and preference mutation', () => {
  const win = new Window(),
    root = win.document.createElement('div');
  root.innerHTML = markup;
  win.document.body.append(root);
  let writes = 0;
  const adapters = createWorkspaceSurfaceAdapters({
    getAssetPanel: () => null,
    onAssets() {},
    onAlign() {},
  });
  const storage = { getItem: () => null, setItem: () => writes++ };
  const before = root.innerHTML;
  for (const [declarations, bindings] of [
    [[...WORKSPACE_SURFACES, WORKSPACE_SURFACES[0]], adapters],
    [WORKSPACE_SURFACES, { ...adapters, assets: { open: 42 } }],
    [WORKSPACE_SURFACES, { ...adapters, assets: { ...adapters.assets, canRetreat: 'yes' } }],
    [WORKSPACE_SURFACES, { ...adapters, unknown: adapters.layers }],
    [[...WORKSPACE_SURFACES, { id: 'unknown', side: 'left', label: 'Unknown' }], adapters],
    [
      WORKSPACE_SURFACES.map((entry) =>
        entry.id === 'layers' ? { ...entry, selector: '[' } : entry,
      ),
      adapters,
    ],
  ])
    assert.throws(
      () => createWorkspacePanels({ declarations, adapters: bindings, storage }),
      /Invalid|Unknown/,
    );
  assert.equal(root.innerHTML, before);
  assert.equal(writes, 0);
  win.happyDOM.abort();
});

test('portal hide, viewer suspend and retreat preserve live owner and Session', async () => {
  const win = new Window(),
    root = win.document.createElement('div');
  root.innerHTML = markup;
  win.document.body.append(root);
  const session = new CommandSession(makeDocument());
  const before = serialize(session.snapshot()),
    history = JSON.stringify(session.historyView),
    revision = session.state.expectedRevision;
  let panel = null,
    resolvePending,
    settled = false;
  const pending = new Promise((resolve) => {
    resolvePending = resolve;
  }).then(() => {
    settled = true;
    panel.dataset.completed = 'yes';
  });
  const adapters = createWorkspaceSurfaceAdapters({
    getAssetPanel: () => panel,
    onAssets: () => {
      panel = win.document.createElement('aside');
      panel.innerHTML = '<input data-asset-search>';
      win.document.body.append(panel);
    },
    onAlign() {},
  });
  const controller = createWorkspacePanels({ adapters });
  controller.attach(root);
  controller.select('assets');
  const original = panel,
    input = panel.querySelector('input');
  input.value = 'pending search';
  controller.setRetreat(true);
  assert.equal(panel.style.pointerEvents, 'none');
  controller.setRetreat(false);
  controller.select('colors');
  assert.equal(panel.hidden, true);
  assert.equal(panel.inert, true);
  assert.equal(panel.parentNode, win.document.body);
  assert.equal(panel, original);
  assert.equal(settled, false);
  resolvePending();
  await pending;
  assert.equal(panel.dataset.completed, 'yes');
  assert.equal(panel.hidden, true);
  controller.select('assets');
  assert.equal(panel, original);
  assert.equal(panel.querySelector('input'), input);
  assert.equal(input.value, 'pending search');
  const wrap = root.querySelector('.canvas-wrap');
  wrap.dataset.viewerMode = 'viewers';
  controller.refresh();
  assert.equal(panel.hidden, true);
  assert.equal(panel.inert, true);
  wrap.dataset.viewerMode = 'canvas';
  controller.refresh();
  assert.equal(panel.hidden, false);
  assert.equal(panel.inert, false);
  assert.equal(settled, true);
  assert.equal(serialize(session.snapshot()), before);
  assert.equal(JSON.stringify(session.historyView), history);
  assert.equal(session.state.expectedRevision, revision);
  win.happyDOM.abort();
});
