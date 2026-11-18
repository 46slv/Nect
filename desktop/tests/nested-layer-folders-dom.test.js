import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { blankDocument, blankLayer, exportSVG } from '../src/core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  descendantLayerIds,
  isNestedFolder,
  planLayerFolderMove,
  planLayerFolderOrder,
  planLayerFolderRemoval,
} from '../src/layer-folders.js';

// Actual panel markup/selection/action functions in a synthetic DOM, not native GUI acceptance.
async function fixture(t) {
  const win = new Window();
  t.after(() => win.happyDOM.abort());
  const doc = blankDocument();
  doc.layers = ['Back', 'Middle', 'Front', 'Top'].map((name) => blankLayer(name));
  const owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc));
  const send = (type, payload) => owner.dispatch({ ...owner.state, type, payload });
  const source = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  const functions = source.slice(
    source.indexOf('function syncLayerFolderSelection('),
    source.indexOf('function render(resetView'),
  );
  const actions = source.slice(
    source.indexOf("  if (d.action === 'add-layer-folder') {"),
    source.indexOf('  if (d.folderToggle !== undefined) {'),
  );
  const app = win.document.createElement('div');
  win.document.body.append(app);
  const make = new Function(
    'session',
    'app',
    'descendantLayerIds',
    'isNestedFolder',
    'planLayerFolderMove',
    'planLayerFolderOrder',
    'planLayerFolderRemoval',
    `
    const selectedLayerFolders = new Set(), collapsedLayerFolders = new Map();
    let folderSelectionScope = null, artworkSelection = new Set(), artworkTarget = null;
    let history = session.historyView, variableAction = null, nameEdit = null;
    const deletionBusy = () => false, esc = (v) => String(v).replaceAll('&','&amp;').replaceAll('"','&quot;').replaceAll('<','&lt;');
    const nameInput = () => '', notify = (message) => { throw Error(message); };
    ${functions}
    function render() { app.innerHTML = layerPanelRows(history.doc).map((row) => typeof row === 'string' ? row : '<div data-layer="'+row.id+'" data-folder-depth="'+row.folderDepth+'"></div>').join(''); }
    function commandChange(type,payload) { const result = session.dispatch({...session.state,type,payload}); render(); return result; }
    function click(d,e={}) { ${actions} }
    return { render, click, rows: () => layerPanelRows(history.doc), selectLayerFolder, selectedLayerFolders, collapsedLayerFolders,
      layerFolderActionState, refreshLayerFolderActions,
      selectLayers(ids) { artworkSelection = new Set(ids); artworkTarget = ids[0] || null; },
      replaceSession(next) { session=next; history=next.historyView; },
    };
  `,
  );
  const ui = make(
    owner,
    app,
    descendantLayerIds,
    isNestedFolder,
    planLayerFolderMove,
    planLayerFolderOrder,
    planLayerFolderRemoval,
  );
  return { owner, send, ids: doc.layers.map((x) => x.id), app, ui };
}

test('panel selects sibling Folder and Layer, wraps once, renders depth and collapses descendants without mutation', async (t) => {
  const { owner, send, ids, app, ui } = await fixture(t);
  const inner = send('create-layer-folder', { name: 'Inner', layerIds: [ids[1]] }).folderId;
  const before = owner.snapshot(),
    svg = exportSVG(before);
  ui.render();
  ui.selectLayers([ids[2]]);
  ui.click({ folderSelect: inner }, { shiftKey: true });
  assert.equal(
    app.querySelector(`[data-folder-select="${inner}"]`).getAttribute('aria-pressed'),
    'true',
  );
  ui.click({ action: 'add-layer-folder' });
  const grouped = owner.snapshot(),
    outer = grouped.editor.layerFolders.find((f) => f.id !== inner);
  assert.equal(grouped.editor.layerFolders.find((f) => f.id === inner).parentId, outer.id);
  assert.deepEqual(grouped.layers, before.layers);
  assert.equal(exportSVG(grouped), svg);
  assert.equal(app.querySelector(`[data-layer-folder="${inner}"]`).dataset.folderDepth, '1');
  assert.equal(app.querySelector(`[data-layer="${ids[1]}"]`).dataset.folderDepth, '2');
  assert.equal(app.querySelector(`[data-layer="${ids[2]}"]`).dataset.folderDepth, '1');
  const saved = owner.projectSnapshot(),
    revision = owner.projectRevision;
  ui.collapsedLayerFolders.get(owner.activeCompositionId).add(outer.id);
  ui.render();
  assert.equal(app.querySelector(`[data-layer-folder="${inner}"]`), null);
  assert.equal(app.querySelector(`[data-layer="${ids[1]}"]`), null);
  assert.ok(app.querySelector(`[data-layer="${ids[0]}"]`));
  assert.deepEqual(owner.projectSnapshot(), saved);
  assert.equal(owner.projectRevision, revision);
  send('undo');
  assert.deepEqual(owner.snapshot(), before);
  send('redo');
  assert.deepEqual(owner.snapshot(), grouped);
});

test('nested legacy actions explain refusal, Ungroup promotes child, empty descendants render once', async (t) => {
  const { owner, send, ids, app, ui } = await fixture(t);
  const inner = send('create-layer-folder', { name: 'Inner', layerIds: [ids[1]] }).folderId;
  const empty = send('create-layer-folder', { name: 'Empty', layerIds: [] }).folderId;
  const outer = send('create-layer-folder', {
    name: 'Outer',
    folderIds: [inner, empty],
    layerIds: [ids[2]],
  }).folderId;
  ui.render();
  assert.equal(app.querySelectorAll(`[data-layer-folder="${empty}"]`).length, 1);
  const row = app.querySelector(`[data-layer-folder="${outer}"]`);
  for (const button of row.querySelectorAll(
    '[data-folder-add],[data-folder-out],[data-folder-order]',
  )) {
    assert.equal(button.disabled, true);
    assert.match(button.title, /Nested Folder/);
  }
  assert.equal(row.querySelector('[data-folder-dissolve]').disabled, false);
  const svg = exportSVG(owner.snapshot());
  send('dissolve-layer-folder', { folderId: outer });
  ui.render();
  assert.equal(app.querySelector(`[data-layer-folder="${inner}"]`).dataset.folderDepth, '0');
  assert.equal(exportSVG(owner.snapshot()), svg);
});

test('folder selection is view-only, toggles exactly and does not leak across revision or replacement', async (t) => {
  const { owner, send, ids, app, ui } = await fixture(t);
  const a = send('create-layer-folder', { name: 'A', layerIds: [ids[0]] }).folderId;
  const b = send('create-layer-folder', { name: 'B', layerIds: [ids[1]] }).folderId;
  ui.render();
  const before = owner.projectSnapshot();
  ui.click({ folderSelect: a });
  ui.click({ folderSelect: b }, { shiftKey: true });
  assert.deepEqual([...ui.selectedLayerFolders], [a, b]);
  ui.click({ folderSelect: a }, { shiftKey: true });
  assert.deepEqual([...ui.selectedLayerFolders], [b]);
  assert.deepEqual(owner.projectSnapshot(), before);
  send('rename-layer-folder', { folderId: b, name: 'New' });
  ui.render();
  assert.equal(ui.selectedLayerFolders.size, 0);
  ui.click({ folderSelect: b });
  const replacement = createCompositionSessionOwner(before);
  ui.replaceSession(replacement);
  ui.render();
  assert.equal(ui.selectedLayerFolders.size, 0);
  assert.equal(
    app.querySelector(`[data-folder-select="${b}"]`).getAttribute('aria-pressed'),
    'false',
  );
});

test('flat empty/boundary actions remain disabled and nested locked descendants cannot Ungroup', async (t) => {
  const { owner, send, ids, app, ui } = await fixture(t);
  const empty = send('create-layer-folder', { name: 'Empty', layerIds: [] }).folderId;
  const inner = send('create-layer-folder', { name: 'Inner', layerIds: [ids[0]] }).folderId;
  ui.render();
  assert.equal(
    app.querySelector(`[data-layer-folder="${empty}"] [data-folder-direction="forward"]`).disabled,
    true,
  );
  assert.equal(
    app.querySelector(`[data-layer-folder="${inner}"] [data-folder-direction="backward"]`).disabled,
    true,
  );
  const outer = send('create-layer-folder', { name: 'Outer', folderIds: [inner] }).folderId;
  const snapshot = owner.snapshot();
  snapshot.layers[0].locked = true;
  const state = ui.layerFolderActionState(
    snapshot,
    snapshot.editor.layerFolders.find((f) => f.id === outer),
    [],
  );
  assert.equal(state.dissolve, false);
  assert.match(state.reasons.dissolve, /Unlock descendant/);
});
