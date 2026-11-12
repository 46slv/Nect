import test from 'node:test';
import assert from 'node:assert/strict';
import { blankDocument, node } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import { createCompositionViewStateStore } from '../src/composition-view-state.js';
import { bindViewer, createViewerState, resolveViewer } from '../src/node-viewers.js';

function defaults(document) {
  const layer = document.layers[0];
  return {
    activeLayer: layer.id,
    selected: null,
    selection: [],
    artworkTarget: null,
    artworkSelection: [],
    pointSelection: { nodeId: null, ids: [] },
    pointFocus: null,
    selectedGuide: null,
    zoom: 1,
    canvasFit: true,
    pan: [0, 0],
    graphZoom: 1,
    graphScroll: [0, 0],
    tool: 'select',
    viewerState: createViewerState(),
  };
}

function fixture() {
  const document = blankDocument(),
    layer = document.layers[0],
    rectangle = node('Rectangle'),
    separate = node('Separate Vector'),
    project = migrateLegacyDocumentToCompositionProject(document),
    other = migrateLegacyDocumentToCompositionProject(blankDocument());
  // Build both independent Compositions with real core node/output definitions.
  project.compositions[0].layers[0].nodes.push(rectangle, separate);
  project.compositions[0].id = 'composition-a';
  other.compositions[0].id = 'composition-b';
  project.compositions.push(other.compositions[0]);
  const owner = createCompositionSessionOwner(project),
    store = createCompositionViewStateStore(),
    send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });
  let view = defaults(owner.snapshot());
  function switchTo(id) {
    store.save(owner.activeCompositionId, view);
    owner.switchComposition(id);
    view = store.restore(id, owner.snapshot(), defaults(owner.snapshot()));
    return view;
  }
  return {
    owner,
    store,
    send,
    switchTo,
    rectangle,
    separate,
    layer,
    get view() {
      return view;
    },
  };
}

test('production Viewer tab round-trips preserve public bindings and History', () => {
  const f = fixture(),
    initial = f.owner.projectSnapshot();
  f.send('set-parameter', {
    layerId: f.layer.id,
    nodeId: f.rectangle.id,
    key: 'size',
    value: [310, 170],
  });
  const document = f.owner.snapshot(),
    layer = document.layers[0];
  bindViewer(
    f.view.viewerState,
    0,
    layer,
    layer.nodes.find((n) => n.id === f.rectangle.id),
  );
  bindViewer(
    f.view.viewerState,
    1,
    layer,
    layer.nodes.find((n) => n.id === f.separate.id),
    'y',
  );
  f.view.viewerState.slots[0].view = [3, 7, 190, 140];
  const expected = structuredClone(f.view.viewerState),
    project = f.owner.projectSnapshot(),
    history = f.owner.historyState;
  assert.equal(expected.slots[0].binding.port, undefined);
  assert.equal(resolveViewer(document, expected.slots[0]).status, 'Live');
  for (let repeat = 0; repeat < 2; repeat++) {
    assert.deepEqual(f.switchTo('composition-b').viewerState, createViewerState());
    const restored = f.switchTo('composition-a').viewerState;
    assert.deepEqual(restored, expected);
    assert.equal(resolveViewer(f.owner.snapshot(), restored.slots[0]).type, 'Path');
    assert.equal(resolveViewer(f.owner.snapshot(), restored.slots[1]).port, 'Y');
    assert.deepEqual(f.owner.projectSnapshot(), project);
    assert.deepEqual(f.owner.historyState, history);
  }
  f.send('undo');
  assert.deepEqual(f.owner.projectSnapshot(), initial);
  f.send('redo');
  assert.deepEqual(f.owner.projectSnapshot(), project);
});

test('restore validates exact public output identity, including absent default ports', () => {
  const f = fixture(),
    document = f.owner.snapshot(),
    layer = document.layers[0],
    base = defaults(document);
  for (const [nodeId, port, valid] of [
    [f.rectangle.id, undefined, true],
    [f.separate.id, 'x', true],
    [f.separate.id, 'y', true],
    [f.rectangle.id, 'out', false],
    [f.rectangle.id, '', false],
    [f.rectangle.id, null, false],
    [f.rectangle.id, 0, false],
    [f.rectangle.id, {}, false],
    [f.separate.id, undefined, false],
    [f.separate.id, 'position', false],
    ['deleted-node', undefined, false],
  ]) {
    const binding = { layerId: layer.id, nodeId, port };
    const viewerState = createViewerState();
    viewerState.slots[0].binding = binding;
    f.store.save('probe', {
      ...base,
      viewerState,
    });
    assert.deepEqual(
      f.store.restore('probe', document, base).viewerState.slots[0].binding,
      valid ? binding : null,
      `${nodeId}: ${String(port)}`,
    );
  }
  const text = node('Text');
  layer.nodes.push(text);
  for (const port of [undefined, 'position']) {
    const state = createViewerState();
    bindViewer(state, 0, layer, text, port);
    f.store.save('text', { ...base, viewerState: state });
    assert.deepEqual(f.store.restore('text', document, base).viewerState, state);
  }
  const absentPort = createViewerState();
  absentPort.slots[0].binding = { layerId: layer.id, nodeId: f.rectangle.id };
  f.store.save('absent-port', { ...base, viewerState: absentPort });
  assert.deepEqual(f.store.restore('absent-port', document, base).viewerState, absentPort);
});

test('deleted or foreign Viewer owners fail closed on tab return without retargeting', () => {
  const f = fixture(),
    document = f.owner.snapshot(),
    layer = document.layers[0];
  bindViewer(
    f.view.viewerState,
    0,
    layer,
    layer.nodes.find((n) => n.id === f.rectangle.id),
  );
  f.view.viewerState.slots[1].binding = {
    layerId: 'foreign-layer',
    nodeId: f.rectangle.id,
    port: undefined,
  };
  f.send('delete-node', { layerId: layer.id, nodeId: f.rectangle.id });
  const project = f.owner.projectSnapshot(),
    history = f.owner.historyState;
  f.switchTo('composition-b');
  assert.deepEqual(
    f.switchTo('composition-a').viewerState.slots.map((slot) => slot.binding),
    [null, null],
  );
  assert.deepEqual(f.owner.projectSnapshot(), project);
  assert.deepEqual(f.owner.historyState, history);
});
