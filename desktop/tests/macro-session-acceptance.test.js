import test from 'node:test';
import assert from 'node:assert/strict';
import { blankDocument, connect, deserialize, evaluate, node } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { resolveArtworkMoveTarget } from '../src/artwork-move-target.js';
import { planMacroInsertion } from '../src/macro-insert-plan.js';

// Acceptance for the next production slice. These tests are intentionally RED
// until the Macro node, Session insertion and instance override are connected.
function definition() {
  return {
    format: 'nect.macro',
    formatVersion: 1,
    id: 'double',
    revision: 1,
    name: 'Double',
    input: { type: 'Path', nodeId: 'a', socket: 'path' },
    output: { type: 'Path', nodeId: 'b', port: null },
    nodes: ['a', 'b'].map((id) => ({
      id,
      type: 'Transform',
      params: { anchor: [0, 0], offset: [0, 0], scale: 1, rotation: 0 },
    })),
    edges: [{ sourceNodeId: 'a', sourcePort: null, targetNodeId: 'b', targetSocket: 'path' }],
    parameters: [
      { id: 'scale', label: 'Scale', nodeId: 'a', key: 'scale', type: 'Scalar', defaultValue: 1 },
    ],
  };
}

function fixture() {
  const doc = blankDocument(),
    layer = doc.layers[0],
    source = node('Rectangle'),
    transform = node('Transform'),
    fill = node('Fill'),
    output = node('Output');
  layer.nodes = [source, transform, fill, output];
  layer.output = output.id;
  connect(layer, source.id, transform.id, 'path');
  connect(layer, transform.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  const owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc));
  const send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });
  send('register-macro-definition', { definition: definition() });
  const plan = planMacroInsertion(owner.snapshot(), layer.id, {
    definitionId: 'double',
    revision: 1,
  });
  assert.equal(plan.supported, true, plan.reason);
  return {
    owner,
    send,
    layer,
    source,
    transform,
    fill,
    output,
    request: { expectedProjectRevision: owner.projectRevision, target: plan.target },
  };
}

function pathSnapshot(owner, layerId) {
  const doc = owner.snapshot(),
    layer = doc.layers.find((item) => item.id === layerId),
    result = evaluate(layer, {}, doc.imageAssets, doc);
  assert.ifError(result.error);
  return structuredClone({
    contours: result.path.contours,
    fill: result.path.fill,
    stroke: result.path.stroke,
  });
}

function assertSameArtwork(actual, expected, { sameIds = true } = {}) {
  assert.deepEqual(actual.fill, expected.fill);
  assert.deepEqual(actual.stroke, expected.stroke);
  assert.equal(actual.contours.length, expected.contours.length);
  for (const [index, contour] of expected.contours.entries()) {
    const next = actual.contours[index];
    if (sameIds) assert.equal(next.id, contour.id);
    assert.equal(next.closed, contour.closed);
    assert.equal(next.points.length, contour.points.length);
    for (const [pointIndex, point] of contour.points.entries()) {
      const nextPoint = next.points[pointIndex];
      if (sameIds) assert.equal(nextPoint.id, point.id);
      for (const key of ['position', 'handleA', 'handleB'])
        for (let axis = 0; axis < 2; axis++)
          assert.ok(Math.abs(nextPoint[key][axis] - point[key][axis]) < 1e-8);
    }
  }
}

function insert(f) {
  f.send('insert-macro', f.request);
  const layer = f.owner.snapshot().layers.find((item) => item.id === f.layer.id),
    instances = layer.nodes.filter((item) => item.type === 'Macro');
  assert.equal(instances.length, 1);
  return { layer, instance: instances[0] };
}

test('one Macro instance preserves the artwork and terminal Move target through Undo/Redo and reopen', () => {
  const f = fixture(),
    before = pathSnapshot(f.owner, f.layer.id),
    pastCount = f.owner.historyView.past.length,
    { layer, instance } = insert(f);
  assert.equal(f.owner.historyView.past.length, pastCount + 1);
  assert.equal(instance.params.path.source, f.source.id);
  assert.equal(
    layer.nodes.find((item) => item.id === f.transform.id).params.path.source,
    instance.id,
  );
  assert.deepEqual(instance.macro, {
    definitionId: 'double',
    definitionRevision: 1,
    overrides: {},
  });
  assertSameArtwork(pathSnapshot(f.owner, f.layer.id), before);
  const move = resolveArtworkMoveTarget(f.owner.snapshot(), f.layer.id);
  assert.equal(move.supported, true, move.reason);
  assert.equal(move.target.nodeId, f.transform.id);
  f.send('undo');
  assert.equal(
    f.owner.snapshot().layers[0].nodes.some((item) => item.id === instance.id),
    false,
  );
  f.send('redo');
  assert.equal(
    f.owner.snapshot().layers[0].nodes.some((item) => item.id === instance.id),
    true,
  );
  const saved = serializeCompositionProject(f.owner.projectSnapshot()),
    reopened = createCompositionSessionOwner(deserializeCompositionProject(saved, deserialize));
  assertSameArtwork(pathSnapshot(reopened, f.layer.id), before);
});

test('a published override changes the connected output and is one reversible, persisted edit', () => {
  const f = fixture(),
    { instance } = insert(f),
    before = pathSnapshot(f.owner, f.layer.id),
    pastCount = f.owner.historyView.past.length;
  f.send('set-macro-override', {
    layerId: f.layer.id,
    nodeId: instance.id,
    parameterId: 'scale',
    value: 2,
  });
  const edited = pathSnapshot(f.owner, f.layer.id);
  assert.notDeepEqual(edited, before);
  assert.equal(f.owner.historyView.past.length, pastCount + 1);
  f.send('undo');
  assertSameArtwork(pathSnapshot(f.owner, f.layer.id), before);
  f.send('redo');
  assertSameArtwork(pathSnapshot(f.owner, f.layer.id), edited);
  const saved = serializeCompositionProject(f.owner.projectSnapshot()),
    reopened = createCompositionSessionOwner(deserializeCompositionProject(saved, deserialize));
  assertSameArtwork(pathSnapshot(reopened, f.layer.id), edited);
});

test('actual connected Path overflow and a stale insertion request preserve Project and Redo', () => {
  const f = fixture(),
    stale = f.request;
  f.send('set-parameter', {
    layerId: f.layer.id,
    nodeId: f.source.id,
    key: 'size',
    value: [100, 80],
  });
  const beforeStale = serializeCompositionProject(f.owner.projectSnapshot()),
    staleHistory = f.owner.historyState;
  assert.throws(() => f.send('insert-macro', stale), /STALE/i);
  assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), beforeStale);
  assert.deepEqual(f.owner.historyState, staleHistory);
  f.request = {
    expectedProjectRevision: f.owner.projectRevision,
    target: planMacroInsertion(f.owner.snapshot(), f.layer.id, {
      definitionId: 'double',
      revision: 1,
    }).target,
  };
  const { instance } = insert(f);
  f.send('set-document-settings', { key: 'width', value: 777 });
  f.send('undo');
  const before = serializeCompositionProject(f.owner.projectSnapshot()),
    history = f.owner.historyState;
  assert.equal(history.canRedo, true);
  assert.throws(
    () =>
      f.send('set-macro-override', {
        layerId: f.layer.id,
        nodeId: instance.id,
        parameterId: 'scale',
        value: Number.MAX_VALUE,
      }),
    /finite|overflow/i,
  );
  assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), before);
  assert.deepEqual(f.owner.historyState, history);
});

test('a complete same-Project duplicate creates an independent Macro instance with the same pin', () => {
  const f = fixture(),
    { instance } = insert(f),
    originalPath = pathSnapshot(f.owner, f.layer.id),
    copied = f.send('duplicate-layer', { layerId: f.layer.id }),
    copy = f.owner.snapshot().layers.find((item) => item.id === copied.layerId),
    copyInstance = copy.nodes.find((item) => item.type === 'Macro'),
    copyPath = pathSnapshot(f.owner, copied.layerId);
  assert.notEqual(copyInstance.id, instance.id);
  assert.deepEqual(copyInstance.macro, instance.macro);
  assertSameArtwork(copyPath, originalPath, { sameIds: false });
  f.send('set-macro-override', {
    layerId: copied.layerId,
    nodeId: copyInstance.id,
    parameterId: 'scale',
    value: 2,
  });
  assertSameArtwork(pathSnapshot(f.owner, f.layer.id), originalPath);
  assert.notDeepEqual(pathSnapshot(f.owner, copied.layerId), copyPath);
  f.send('undo');
  assertSameArtwork(pathSnapshot(f.owner, copied.layerId), copyPath);
  f.send('redo');
  const saved = serializeCompositionProject(f.owner.projectSnapshot()),
    reopened = createCompositionSessionOwner(deserializeCompositionProject(saved, deserialize));
  assert.notDeepEqual(pathSnapshot(reopened, copied.layerId), copyPath);
  assertSameArtwork(pathSnapshot(reopened, f.layer.id), originalPath);
});

test('operations without Macro definition closure reject atomically and preserve Redo', () => {
  const f = fixture(),
    { instance } = insert(f),
    sourceProject = f.owner.projectSnapshot(),
    sourceCompositionId = sourceProject.compositions[0].id;
  f.send('set-document-settings', { key: 'width', value: 777 });
  f.send('undo');
  const before = serializeCompositionProject(f.owner.projectSnapshot()),
    state = f.owner.state,
    history = f.owner.historyState;
  assert.equal(history.canRedo, true);
  for (const [type, payload] of [
    ['copy-nodes', { layerId: f.layer.id, nodeIds: [instance.id] }],
    ['save-node-preset', { layerId: f.layer.id, nodeIds: [instance.id], name: 'Attempt' }],
    ['import-project-compositions', { sourceProject, compositionIds: [sourceCompositionId] }],
  ]) {
    assert.throws(() => f.send(type, payload), /macro|closure|support/i, type);
    assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), before);
    assert.deepEqual(f.owner.state, state);
    assert.deepEqual(f.owner.historyState, history);
  }
});
