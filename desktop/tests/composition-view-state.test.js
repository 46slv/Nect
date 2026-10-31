import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createCompositionViewStateStore } from '../src/composition-view-state.js';

function documentWith(suffix) {
  return {
    editor: { guides: [{ id: `guide-${suffix}` }] },
    layers: [{ id: `layer-${suffix}`, nodes: [{ id: `node-${suffix}`, type: 'Output' }] }],
  };
}

function defaults(suffix) {
  return {
    activeLayer: `layer-${suffix}`,
    selected: `node-${suffix}`,
    selection: [`node-${suffix}`],
    artworkTarget: null,
    artworkSelection: [],
    pointSelection: { nodeId: null, ids: [] },
    pointFocus: null,
    selectedGuide: null,
    zoom: 1,
    canvasFit: true,
    pan: [0, 0],
    graphZoom: 1,
    graphFitInset: 0,
    graphScroll: [0, 0],
    tool: 'select',
    guideIds: [`guide-${suffix}`],
    viewerState: {
      mode: 'artwork',
      slots: [
        { binding: null, view: null },
        { binding: null, view: null },
      ],
    },
  };
}

test('Composition view snapshots stay isolated by stable Composition ID', () => {
  const store = createCompositionViewStateStore(),
    a = documentWith('a'),
    b = documentWith('b');
  store.save('composition-a', {
    ...defaults('a'),
    zoom: 2.25,
    canvasFit: false,
    pan: [-32, 18],
    graphZoom: 1.5,
    graphFitInset: 25,
    graphScroll: [220, 410],
    selectedGuide: 'guide-a',
  });
  store.save('composition-b', {
    ...defaults('b'),
    zoom: 0.75,
    pan: [5, -7],
    selected: null,
    selection: [],
  });

  assert.equal(store.restore('composition-a', a, defaults('a')).zoom, 2.25);
  assert.deepEqual(store.restore('composition-a', a, defaults('a')).pan, [-32, 18]);
  assert.deepEqual(store.restore('composition-a', a, defaults('a')).graphScroll, [220, 410]);
  assert.equal(store.restore('composition-a', a, defaults('a')).graphFitInset, 25);
  assert.equal(store.restore('composition-a', a, defaults('a')).selectedGuide, 'guide-a');
  assert.equal(store.restore('composition-b', b, defaults('b')).zoom, 0.75);
  const oldView = defaults('b');
  delete oldView.graphFitInset;
  store.save('old-view', oldView);
  assert.equal(store.restore('old-view', b, defaults('b')).graphFitInset, 0);
  assert.deepEqual(store.restore('composition-b', b, defaults('b')).selection, []);
});

test('restore drops missing and cross-Composition Layer, Node, Guide and Viewer references', () => {
  const store = createCompositionViewStateStore(),
    doc = documentWith('a'),
    base = defaults('a');
  store.save('composition-a', {
    ...base,
    activeLayer: 'layer-b',
    selected: 'node-b',
    selection: ['node-a', 'node-b'],
    artworkTarget: 'layer-b',
    artworkSelection: ['layer-b'],
    selectedGuide: 'guide-b',
    viewerState: {
      mode: 'viewers',
      slots: [
        { binding: { layerId: 'layer-b', nodeId: 'node-b', port: 'out' }, view: null },
        { binding: { layerId: 'layer-a', nodeId: 'deleted-node', port: 'out' }, view: null },
      ],
    },
  });

  const restored = store.restore('composition-a', doc, base);
  assert.equal(restored.activeLayer, 'layer-a');
  assert.equal(restored.selected, 'node-a');
  assert.deepEqual(restored.selection, ['node-a']);
  assert.equal(restored.artworkTarget, null);
  assert.deepEqual(restored.artworkSelection, []);
  assert.equal(restored.selectedGuide, null);
  assert.equal(restored.viewerState.mode, 'viewers');
  assert.deepEqual(
    restored.viewerState.slots.map((slot) => slot.binding),
    [null, null],
  );
});

test('deleted Composition snapshots are pruned and project replacement clears all state', () => {
  const store = createCompositionViewStateStore(),
    a = documentWith('a');
  store.save('composition-a', defaults('a'));
  store.save('composition-deleted', defaults('deleted'));
  store.prune([{ id: 'composition-a' }]);
  assert.equal(store.size, 1);
  store.forget('composition-a');
  assert.equal(store.size, 0);
  store.save('composition-a', defaults('a'));
  store.clear();
  assert.equal(store.size, 0);
  assert.equal(store.restore('composition-a', a, defaults('a')).zoom, 1);
});
test('direct point selection preserves only current authored point IDs', () => {
  const store = createCompositionViewStateStore(),
    doc = {
      layers: [
        {
          id: 'layer-a',
          nodes: [
            {
              id: 'curve-a',
              type: 'Curve',
              points: [{ id: 'point-a' }, { id: 'point-b' }],
            },
          ],
        },
      ],
    },
    base = {
      ...defaults('a'),
      selected: 'curve-a',
      selection: ['curve-a'],
      pointSelection: { nodeId: 'curve-a', ids: ['point-a', 'point-removed'] },
      pointFocus: { nodeId: 'curve-a', pointId: 'point-removed' },
    };
  store.save('composition-a', base);

  const restored = store.restore('composition-a', doc, base);
  assert.deepEqual(restored.pointSelection, { nodeId: 'curve-a', ids: ['point-a'] });
  assert.equal(restored.pointFocus, null);
});

// Exercise the actual app resolver with a controlled evaluator. The Chrome fixture
// separately runs this same path with the real graph evaluator and tab controls.
const appSource = readFileSync(new URL('../src/main.js', import.meta.url), 'utf8'),
  resolverSource = appSource.slice(
    appSource.indexOf('function pointPanelIds('),
    appSource.indexOf('function scopedPointPanel('),
  );

function remapFixture() {
  const curve = { id: 'curve-a', type: 'Curve', points: [{ id: 'point-a' }, { id: 'point-b' }] },
    remap = {
      id: 'remap-a',
      type: 'Path Remap',
      points: [],
      params: { path: { source: curve.id } },
    },
    doc = { imageAssets: {}, layers: [{ id: 'layer-a', nodes: [curve, remap] }] },
    state = {
      ...defaults('a'),
      selected: remap.id,
      selection: [remap.id],
      pointSelection: { nodeId: remap.id, ids: ['point-b', 'point-a', 'point-b', 'deleted'] },
      pointFocus: { nodeId: remap.id, pointId: 'point-b' },
    },
    resolvePointIds = new Function(
      'evaluate',
      'resolveFonts',
      'activeFonts',
      resolverSource + '\nreturn pointPanelIds;',
    )(
      (layer, fonts, assets) => {
        assert.equal(layer, doc.layers[0]);
        assert.equal(assets, doc.imageAssets);
        assert.equal(fonts, 'resolved-fonts');
        return {
          run(id) {
            const source = layer.nodes.find((node) => node.id === id);
            if (!source) throw Error('Missing source');
            if (source.type !== 'Curve') return 12;
            return { contours: [{ points: source.points }] };
          },
        };
      },
      (document) => {
        assert.equal(document, doc);
        return 'resolved-fonts';
      },
      () => ({}),
    ),
    store = createCompositionViewStateStore({ resolvePointIds });
  store.save('composition-a', state);
  return { curve, remap, doc, state, store };
}

test('A to B to A restores source-derived Remap IDs/focus without retaining geometry', () => {
  const { remap, doc, state, store } = remapFixture(),
    before = JSON.stringify(doc);
  store.save('composition-b', defaults('b'));
  store.restore('composition-b', documentWith('b'), defaults('b'));
  const restored = store.restore('composition-a', doc, defaults('a'));
  assert.deepEqual(restored.pointSelection, { nodeId: remap.id, ids: ['point-b', 'point-a'] });
  assert.deepEqual(restored.pointFocus, state.pointFocus);
  assert.equal(JSON.stringify(doc), before);
  assert.deepEqual(remap.points, []);
  assert.deepEqual(Object.keys(restored.pointSelection), ['nodeId', 'ids']);
  restored.pointSelection.ids.pop();
  assert.deepEqual(store.restore('composition-a', doc, defaults('a')).pointSelection.ids, [
    'point-b',
    'point-a',
  ]);
});

test('Remap restore resolves fresh IDs and drops a deleted source point and its focus', () => {
  const { curve, doc, store } = remapFixture();
  curve.points.pop();
  const restored = store.restore('composition-a', doc, defaults('a'));
  assert.deepEqual(restored.pointSelection.ids, ['point-a']);
  assert.equal(restored.pointFocus, null);
});

for (const invalid of ['deleted source', 'disconnected input', 'non-path input', 'deleted owner'])
  test(`Remap restore drops selection and focus for ${invalid}`, () => {
    const { curve, remap, doc, store } = remapFixture();
    if (invalid === 'deleted source') doc.layers[0].nodes.shift();
    if (invalid === 'disconnected input') remap.params.path.source = null;
    if (invalid === 'non-path input') curve.type = 'Scalar';
    if (invalid === 'deleted owner') doc.layers[0].nodes.pop();
    const restored = store.restore('composition-a', doc, defaults('a'));
    assert.deepEqual(restored.pointSelection.ids, []);
    assert.equal(restored.pointFocus, null);
  });

test('the app resolver preserves direct Curve IDs/focus and ignores a foreign focus owner', () => {
  const { curve, doc, state, store } = remapFixture();
  state.pointSelection = { nodeId: curve.id, ids: ['point-a', 'deleted'] };
  state.pointFocus = { nodeId: curve.id, pointId: 'point-a' };
  store.save('composition-a', state);
  const restored = store.restore('composition-a', doc, defaults('a'));
  assert.deepEqual(restored.pointSelection, { nodeId: curve.id, ids: ['point-a'] });
  assert.deepEqual(restored.pointFocus, state.pointFocus);
  state.pointFocus.nodeId = 'foreign-node';
  store.save('composition-a', state);
  assert.equal(store.restore('composition-a', doc, defaults('a')).pointFocus, null);
});

test('a failing ID resolver is fail-closed and is scoped to the sanitized active Layer', () => {
  const { doc, state } = remapFixture(),
    store = createCompositionViewStateStore({
      resolvePointIds(node, layer, document) {
        assert.equal(node.id, 'remap-a');
        assert.equal(layer, doc.layers[0]);
        assert.equal(document, doc);
        throw Error('Invalid input');
      },
    });
  store.save('composition-a', { ...state, activeLayer: 'foreign-layer' });
  const restored = store.restore('composition-a', doc, defaults('a'));
  assert.deepEqual(restored.pointSelection, { nodeId: null, ids: [] });
  assert.equal(restored.pointFocus, null);
});
