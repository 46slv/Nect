import test from 'node:test';
import assert from 'node:assert/strict';
import { createCompositionViewStateStore } from '../src/composition-view-state.js';

function documentWith(suffix) {
  return {
    editor: { guides: [{ id: `guide-${suffix}` }] },
    layers: [
      { id: `layer-${suffix}`, nodes: [{ id: `node-${suffix}`, type: 'Output' }] },
    ],
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
  assert.equal(store.restore('composition-a', a, defaults('a')).selectedGuide, 'guide-a');
  assert.equal(store.restore('composition-b', b, defaults('b')).zoom, 0.75);
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
  assert.deepEqual(restored.viewerState.slots.map((slot) => slot.binding), [null, null]);
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