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
import { createLayerClipboardOwner } from '../src/layer-clipboard-owner.js';

// Session acceptance for the bounded connected-Macro route.
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

function fixture(options = {}, registeredDefinition = definition()) {
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
  const owner = createCompositionSessionOwner(
    migrateLegacyDocumentToCompositionProject(doc),
    options,
  );
  const send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });
  send('register-macro-definition', { definition: registeredDefinition });
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

function largeMacro() {
  const f = fixture();
  f.send('set-parameter', {
    layerId: f.layer.id,
    nodeId: f.source.id,
    key: 'center',
    value: [0, 0],
  });
  f.send('set-parameter', { layerId: f.layer.id, nodeId: f.source.id, key: 'size', value: [1, 1] });
  f.request = {
    expectedProjectRevision: f.owner.projectRevision,
    target: planMacroInsertion(f.owner.snapshot(), f.layer.id, {
      definitionId: 'double',
      revision: 1,
    }).target,
  };
  const { instance } = insert(f);
  f.send('set-macro-override', {
    layerId: f.layer.id,
    nodeId: instance.id,
    parameterId: 'scale',
    value: 1e305,
  });
  return { f, instance };
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

test('zero is a real override, and Reset restores the pinned default in one Undo step', () => {
  const f = fixture(),
    { instance } = insert(f),
    before = pathSnapshot(f.owner, f.layer.id);
  f.send('set-macro-override', {
    layerId: f.layer.id,
    nodeId: instance.id,
    parameterId: 'scale',
    value: 0,
  });
  const zero = f.owner.snapshot().layers[0].nodes.find((item) => item.id === instance.id);
  assert.equal(zero.macro.overrides.scale, 0);
  const pastCount = f.owner.historyView.past.length;
  f.send('reset-macro-override', {
    layerId: f.layer.id,
    nodeId: instance.id,
    parameterId: 'scale',
  });
  const reset = f.owner.snapshot().layers[0].nodes.find((item) => item.id === instance.id);
  assert.equal(Object.hasOwn(reset.macro.overrides, 'scale'), false);
  assertSameArtwork(pathSnapshot(f.owner, f.layer.id), before);
  assert.equal(f.owner.historyView.past.length, pastCount + 1);
  f.send('undo');
  assert.equal(
    f.owner.snapshot().layers[0].nodes.find((item) => item.id === instance.id).macro.overrides
      .scale,
    0,
  );
  f.send('redo');
  assertSameArtwork(pathSnapshot(f.owner, f.layer.id), before);
});

test('locked Macro edits and unknown published IDs preserve the Project and Redo', () => {
  const f = fixture(),
    { instance } = insert(f);
  f.send('set-layer-state', { layerId: f.layer.id, key: 'locked', value: true });
  const locked = serializeCompositionProject(f.owner.projectSnapshot()),
    lockedHistory = f.owner.historyState;
  assert.throws(
    () =>
      f.send('set-macro-override', {
        layerId: f.layer.id,
        nodeId: instance.id,
        parameterId: 'scale',
        value: 2,
      }),
    /unlock/i,
  );
  assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), locked);
  assert.deepEqual(f.owner.historyState, lockedHistory);
  f.send('undo');
  const before = serializeCompositionProject(f.owner.projectSnapshot()),
    history = f.owner.historyState;
  assert.equal(history.canRedo, true);
  assert.throws(
    () =>
      f.send('set-macro-override', {
        layerId: f.layer.id,
        nodeId: instance.id,
        parameterId: 'unknown',
        value: 2,
      }),
    /parameter/i,
  );
  assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), before);
  assert.deepEqual(f.owner.historyState, history);
});

test('artwork Move and Align keep the Macro instance and use the existing terminal Transform', () => {
  const f = fixture(),
    { instance } = insert(f),
    before = pathSnapshot(f.owner, f.layer.id),
    pastCount = f.owner.historyView.past.length;
  f.send('translate-artworks', { layerIds: [f.layer.id], delta: [18, -3] });
  const moved = f.owner.snapshot().layers[0],
    terminal = moved.nodes.find((item) => item.id === f.transform.id);
  assert.deepEqual(terminal.params.offset.value, [18, -3]);
  assert.deepEqual(moved.nodes.find((item) => item.id === instance.id).macro, instance.macro);
  assert.equal(f.owner.historyView.past.length, pastCount + 1);
  f.send('undo');
  assertSameArtwork(pathSnapshot(f.owner, f.layer.id), before);
  f.send('redo');
  assert.deepEqual(
    f.owner.snapshot().layers[0].nodes.find((item) => item.id === f.transform.id).params.offset
      .value,
    [18, -3],
  );
  f.send('align-artworks', { layerIds: [f.layer.id], action: 'left', reference: 'artboard' });
  const saved = serializeCompositionProject(f.owner.projectSnapshot()),
    reopened = createCompositionSessionOwner(deserializeCompositionProject(saved, deserialize));
  assertSameArtwork(pathSnapshot(reopened, f.layer.id), pathSnapshot(f.owner, f.layer.id));
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

for (const type of ['insert-macro', 'set-macro-override', 'reset-macro-override']) {
  test(`${type} rejects a reentrant edit without splitting Project and Session`, () => {
    let owner,
      armed = false,
      nested,
      nestedHistory;
    const f = fixture({
      fonts: () => {
        if (armed) {
          armed = false;
          owner.dispatch({
            ...owner.state,
            type: 'set-document-settings',
            payload: { key: 'width', value: 777 },
          });
          nested = owner.projectSnapshot();
          nestedHistory = owner.historyState;
        }
        return {};
      },
    });
    owner = f.owner;
    let instance;
    if (type !== 'insert-macro') {
      ({ instance } = insert(f));
      if (type === 'reset-macro-override')
        f.send('set-macro-override', {
          layerId: f.layer.id,
          nodeId: instance.id,
          parameterId: 'scale',
          value: 2,
        });
    }
    const payload =
      type === 'insert-macro'
        ? f.request
        : {
            layerId: f.layer.id,
            nodeId: instance.id,
            parameterId: 'scale',
            ...(type === 'set-macro-override' ? { value: 3 } : {}),
          };
    armed = true;
    assert.throws(() => f.send(type, payload), /STALE/);
    assert.ok(nested);
    assert.deepEqual(owner.projectSnapshot(), nested);
    assert.deepEqual(owner.historyState, nestedHistory);
    assert.equal(owner.snapshot().width, 777);
    const projectLayer = nested.compositions[0].layers.find((item) => item.id === f.layer.id),
      sessionLayer = owner.snapshot().layers.find((item) => item.id === f.layer.id);
    assert.deepEqual(sessionLayer, projectLayer);
    f.send('set-document-settings', { key: 'height', value: 888 });
    assert.equal(owner.projectSnapshot().compositions[0].width, 777);
    assert.equal(owner.snapshot().width, 777);
    assert.equal(owner.snapshot().height, 888);
    assert.deepEqual(
      owner.snapshot().layers.find((item) => item.id === f.layer.id),
      projectLayer,
    );
  });
}

test('a disconnected Macro cannot hide overflowing instance geometry', () => {
  const f = fixture(),
    { instance } = insert(f);
  f.send('disconnect', { layerId: f.layer.id, nodeId: f.transform.id, key: 'path' });
  const before = serializeCompositionProject(f.owner.projectSnapshot()),
    history = f.owner.historyState;
  assert.throws(
    () =>
      f.send('set-macro-override', {
        layerId: f.layer.id,
        nodeId: instance.id,
        parameterId: 'scale',
        value: Number.MAX_VALUE,
      }),
    /finite/i,
  );
  assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), before);
  assert.deepEqual(f.owner.historyState, history);
});

test('an upstream Rectangle edit cannot overflow an existing Macro instance', () => {
  const { f } = largeMacro();
  const before = serializeCompositionProject(f.owner.projectSnapshot()),
    history = f.owner.historyState;
  assert.throws(
    () =>
      f.send('set-parameter', {
        layerId: f.layer.id,
        nodeId: f.source.id,
        key: 'size',
        value: [10000, 10000],
      }),
    /finite/i,
  );
  assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), before);
  assert.deepEqual(f.owner.historyState, history);
});

for (const mode of ['insert', 'override', 'upstream', 'downstream']) {
  test(`${mode} rejects finite Macro geometry that would overflow its terminal Transform`, () => {
    const registered = definition();
    if (mode === 'insert') registered.parameters[0].defaultValue = 1e307;
    const f = fixture({}, registered);
    f.send('set-parameter', {
      layerId: f.layer.id,
      nodeId: f.source.id,
      key: 'center',
      value: [2, 0],
    });
    f.send('set-parameter', {
      layerId: f.layer.id,
      nodeId: f.source.id,
      key: 'size',
      value: [1, 1],
    });
    f.send('set-parameter', {
      layerId: f.layer.id,
      nodeId: f.transform.id,
      key: 'scale',
      value: 100,
    });
    f.request = {
      expectedProjectRevision: f.owner.projectRevision,
      target: planMacroInsertion(f.owner.snapshot(), f.layer.id, {
        definitionId: 'double',
        revision: 1,
      }).target,
    };
    let instance;
    if (mode !== 'insert') {
      ({ instance } = insert(f));
      if (mode !== 'override')
        f.send('set-macro-override', {
          layerId: f.layer.id,
          nodeId: instance.id,
          parameterId: 'scale',
          value: mode === 'upstream' ? 1e304 : 1e305,
        });
    }
    f.send('set-document-settings', { key: 'width', value: 777 });
    f.send('undo');
    if (mode === 'insert') f.request.expectedProjectRevision = f.owner.projectRevision;
    const before = serializeCompositionProject(f.owner.projectSnapshot()),
      history = f.owner.historyState;
    assert.equal(history.canRedo, true);
    const attempt =
      mode === 'insert'
        ? () => f.send('insert-macro', f.request)
        : mode === 'override'
          ? () =>
              f.send('set-macro-override', {
                layerId: f.layer.id,
                nodeId: instance.id,
                parameterId: 'scale',
                value: 1e307,
              })
          : () =>
              f.send('set-parameter', {
                layerId: f.layer.id,
                nodeId: mode === 'upstream' ? f.source.id : f.transform.id,
                key: mode === 'upstream' ? 'center' : 'scale',
                value: mode === 'upstream' ? [2000, 0] : 10000,
              });
    assert.throws(attempt, /finite|overflow/i);
    assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), before);
    assert.deepEqual(f.owner.historyState, history);
    f.send('redo');
    assert.equal(f.owner.snapshot().width, 777);
  });
}

test('a cross-Layer driver cannot overflow an affected Macro', () => {
  const { f } = largeMacro(),
    layer = f.send('create-blank-layer').layerId,
    driver = f.send('create-node', { layerId: layer, nodeType: 'Value' }).nodeId;
  f.send('set-parameter', { layerId: layer, nodeId: driver, key: 'value', value: 1 });
  const ref = `ref(artwork, "${layer}", "${driver}", "value")`;
  f.send('set-parameter-expression', {
    layerId: f.layer.id,
    nodeId: f.source.id,
    key: 'size',
    expression: `${ref}; ${ref}`,
  });
  const before = serializeCompositionProject(f.owner.projectSnapshot()),
    history = f.owner.historyState;
  assert.throws(
    () => f.send('set-parameter', { layerId: layer, nodeId: driver, key: 'value', value: 10000 }),
    /finite/i,
  );
  assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), before);
  assert.deepEqual(f.owner.historyState, history);
});

test('a deleted external driver remains a diagnosed missing reference and Undo restores it', () => {
  const { f } = largeMacro(),
    layer = f.send('create-blank-layer').layerId,
    driver = f.send('create-node', { layerId: layer, nodeType: 'Value' }).nodeId;
  f.send('set-parameter', { layerId: layer, nodeId: driver, key: 'value', value: 1 });
  const ref = `ref(artwork, "${layer}", "${driver}", "value")`;
  f.send('set-parameter-expression', {
    layerId: f.layer.id,
    nodeId: f.source.id,
    key: 'size',
    expression: `${ref}; ${ref}`,
  });
  const before = serializeCompositionProject(f.owner.projectSnapshot());
  f.send('delete-node', { layerId: layer, nodeId: driver });
  const doc = f.owner.snapshot(),
    frame = evaluate(
      doc.layers.find((item) => item.id === f.layer.id),
      {},
      {},
      doc,
    );
  assert.match(frame.error, /missing|reference/i);
  f.send('undo');
  assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), before);
  assert.doesNotThrow(() => pathSnapshot(f.owner, f.layer.id));
});

test('a different Composition cannot overflow an inactive Macro through a live reference', () => {
  const { f } = largeMacro(),
    sourceComposition = f.owner.activeCompositionId,
    driverComposition = f.owner.createComposition({ name: 'Driver' }),
    driverLayer = f.owner.snapshot().layers[0].id,
    driver = f.send('create-node', { layerId: driverLayer, nodeType: 'Vector' }).nodeId;
  f.send('set-parameter', { layerId: driverLayer, nodeId: driver, key: 'value', value: [1, 1] });
  f.owner.switchComposition(sourceComposition);
  const ref = (component) =>
    `ref(componentComposition, "${driverComposition}", "${driverLayer}", "${driver}", "value", "${component}")`;
  f.send('set-parameter-expression', {
    layerId: f.layer.id,
    nodeId: f.source.id,
    key: 'size',
    expression: `${ref('x')}; ${ref('y')}`,
  });
  f.owner.switchComposition(driverComposition);
  const before = serializeCompositionProject(f.owner.projectSnapshot()),
    history = f.owner.historyState;
  assert.throws(
    () =>
      f.send('set-parameter', {
        layerId: driverLayer,
        nodeId: driver,
        key: 'value',
        value: [10000, 10000],
      }),
    /finite/i,
  );
  assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), before);
  assert.deepEqual(f.owner.historyState, history);
});

test('Viewer-only Macro geometry is checked while an unrelated Output is disconnected', () => {
  const { f, instance } = largeMacro();
  f.send('disconnect', { layerId: f.layer.id, nodeId: f.transform.id, key: 'path' });
  const before = serializeCompositionProject(f.owner.projectSnapshot()),
    history = f.owner.historyState;
  assert.throws(
    () =>
      f.send('set-parameter', {
        layerId: f.layer.id,
        nodeId: f.source.id,
        key: 'size',
        value: [10000, 10000],
      }),
    /finite/i,
  );
  assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), before);
  assert.deepEqual(f.owner.historyState, history);
  f.send('set-macro-override', {
    layerId: f.layer.id,
    nodeId: instance.id,
    parameterId: 'scale',
    value: 2,
  });
});

test('an upstream change checks a Viewer-only Macro even when the Output Macro remains valid', () => {
  const { f, instance } = largeMacro();
  f.send('set-macro-override', {
    layerId: f.layer.id,
    nodeId: instance.id,
    parameterId: 'scale',
    value: 2,
  });
  const project = f.owner.projectSnapshot(),
    layer = project.compositions[0].layers.find((item) => item.id === f.layer.id),
    viewer = structuredClone(layer.nodes.find((item) => item.id === instance.id));
  viewer.id = 'viewer-only-macro';
  viewer.macro.overrides.scale = 1e305;
  layer.nodes.push(viewer);
  const owner = createCompositionSessionOwner(project),
    send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload }),
    before = serializeCompositionProject(owner.projectSnapshot()),
    history = owner.historyState;
  assert.throws(
    () =>
      send('set-parameter', {
        layerId: f.layer.id,
        nodeId: f.source.id,
        key: 'size',
        value: [10000, 10000],
      }),
    /finite/i,
  );
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), before);
  assert.deepEqual(owner.historyState, history);
});

test('an unconnected Viewer Transform is checked when its Macro source changes', () => {
  const f = fixture();
  f.send('set-parameter', {
    layerId: f.layer.id,
    nodeId: f.source.id,
    key: 'center',
    value: [2, 0],
  });
  f.send('set-parameter', { layerId: f.layer.id, nodeId: f.source.id, key: 'size', value: [1, 1] });
  f.request = {
    expectedProjectRevision: f.owner.projectRevision,
    target: planMacroInsertion(f.owner.snapshot(), f.layer.id, {
      definitionId: 'double',
      revision: 1,
    }).target,
  };
  const { instance } = insert(f),
    project = f.owner.projectSnapshot(),
    layer = project.compositions[0].layers.find((item) => item.id === f.layer.id),
    viewer = node('Transform');
  viewer.params.path.source = instance.id;
  viewer.params.scale.value = 100;
  layer.nodes.push(viewer);
  const owner = createCompositionSessionOwner(project),
    send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload }),
    before = serializeCompositionProject(owner.projectSnapshot()),
    history = owner.historyState;
  assert.throws(
    () =>
      send('set-macro-override', {
        layerId: f.layer.id,
        nodeId: instance.id,
        parameterId: 'scale',
        value: 1e307,
      }),
    /finite|overflow/i,
  );
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), before);
  assert.deepEqual(owner.historyState, history);
});

test('a second downstream Transform cannot overflow after a valid Macro and first Transform', () => {
  const f = fixture();
  f.send('set-parameter', {
    layerId: f.layer.id,
    nodeId: f.source.id,
    key: 'center',
    value: [2, 0],
  });
  f.send('set-parameter', { layerId: f.layer.id, nodeId: f.source.id, key: 'size', value: [1, 1] });
  f.send('set-parameter', {
    layerId: f.layer.id,
    nodeId: f.transform.id,
    key: 'scale',
    value: 100,
  });
  f.request = {
    expectedProjectRevision: f.owner.projectRevision,
    target: planMacroInsertion(f.owner.snapshot(), f.layer.id, {
      definitionId: 'double',
      revision: 1,
    }).target,
  };
  const { instance } = insert(f),
    project = f.owner.projectSnapshot(),
    layer = project.compositions[0].layers.find((item) => item.id === f.layer.id),
    next = node('Transform');
  next.params.path.source = f.transform.id;
  next.params.scale.value = 100;
  layer.nodes.push(next);
  layer.nodes.find((item) => item.id === f.fill.id).params.path.source = next.id;
  const owner = createCompositionSessionOwner(project),
    before = serializeCompositionProject(owner.projectSnapshot()),
    history = owner.historyState;
  assert.throws(
    () =>
      owner.dispatch({
        ...owner.state,
        type: 'set-macro-override',
        payload: { layerId: f.layer.id, nodeId: instance.id, parameterId: 'scale', value: 1e305 },
      }),
    /finite|overflow/i,
  );
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), before);
  assert.deepEqual(owner.historyState, history);
});

test('a Path modifier does not hide an overflowing downstream Transform', () => {
  const f = fixture();
  f.send('set-parameter', {
    layerId: f.layer.id,
    nodeId: f.source.id,
    key: 'center',
    value: [2, 0],
  });
  f.send('set-parameter', { layerId: f.layer.id, nodeId: f.source.id, key: 'size', value: [1, 1] });
  f.send('set-parameter', {
    layerId: f.layer.id,
    nodeId: f.transform.id,
    key: 'scale',
    value: 100,
  });
  f.request = {
    expectedProjectRevision: f.owner.projectRevision,
    target: planMacroInsertion(f.owner.snapshot(), f.layer.id, {
      definitionId: 'double',
      revision: 1,
    }).target,
  };
  const { instance } = insert(f),
    project = f.owner.projectSnapshot(),
    layer = project.compositions[0].layers.find((item) => item.id === f.layer.id),
    remap = node('Path Remap');
  remap.params.path.source = instance.id;
  layer.nodes.push(remap);
  layer.nodes.find((item) => item.id === f.transform.id).params.path.source = remap.id;
  const owner = createCompositionSessionOwner(project),
    before = serializeCompositionProject(owner.projectSnapshot()),
    history = owner.historyState;
  assert.throws(
    () =>
      owner.dispatch({
        ...owner.state,
        type: 'set-macro-override',
        payload: { layerId: f.layer.id, nodeId: instance.id, parameterId: 'scale', value: 1e307 },
      }),
    /finite|overflow/i,
  );
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), before);
  assert.deepEqual(owner.historyState, history);
});

test('a healthy Macro remains editable when a separate existing Macro output overflows', () => {
  const { f, instance } = largeMacro(),
    duplicate = f.send('duplicate-layer', { layerId: f.layer.id }),
    project = f.owner.projectSnapshot(),
    original = project.compositions[0].layers.find((item) => item.id === f.layer.id),
    source = original.nodes.find((item) => item.id === f.source.id),
    copy = project.compositions[0].layers.find((item) => item.id === duplicate.layerId),
    copyInstance = copy.nodes.find((item) => item.type === 'Macro');
  // A pre-existing invalid output must not stop edits to an independent route.
  source.params.size.value = [10000, 10000];
  const owner = createCompositionSessionOwner(project),
    before = owner.projectSnapshot();
  owner.dispatch({
    ...owner.state,
    type: 'set-macro-override',
    payload: {
      layerId: copy.id,
      nodeId: copyInstance.id,
      parameterId: 'scale',
      value: 2,
    },
  });
  const after = owner.projectSnapshot();
  assert.deepEqual(
    after.compositions[0].layers.find((item) => item.id === original.id),
    original,
  );
  assert.equal(
    after.compositions[0].layers
      .find((item) => item.id === copy.id)
      .nodes.find((item) => item.id === copyInstance.id).macro.overrides.scale,
    2,
  );
  assert.equal(
    before.compositions[0].layers[0].nodes.find((item) => item.id === instance.id).macro.overrides
      .scale,
    1e305,
  );
});

test('an existing internal Macro overflow does not excuse a new downstream overflow', () => {
  const { f, instance } = largeMacro(),
    project = f.owner.projectSnapshot(),
    layer = project.compositions[0].layers.find((item) => item.id === f.layer.id);
  layer.nodes.find((item) => item.id === f.source.id).params.center.value = [10000, 0];
  layer.nodes.find((item) => item.id === instance.id).macro.overrides.scale = 1e307;
  layer.nodes.find((item) => item.id === f.transform.id).params.scale.value = 100;
  const owner = createCompositionSessionOwner(project),
    before = serializeCompositionProject(owner.projectSnapshot()),
    history = owner.historyState;
  assert.throws(
    () =>
      owner.dispatch({
        ...owner.state,
        type: 'set-parameter',
        payload: { layerId: f.layer.id, nodeId: f.source.id, key: 'center', value: [2, 0] },
      }),
    /finite|overflow/i,
  );
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), before);
  assert.deepEqual(owner.historyState, history);
});

test('a healthy Viewer Macro remains editable beside an existing broken output in the same Layer', () => {
  const { f, instance } = largeMacro(),
    project = f.owner.projectSnapshot(),
    layer = project.compositions[0].layers.find((item) => item.id === f.layer.id),
    secondSource = node('Rectangle'),
    second = structuredClone(layer.nodes.find((item) => item.id === instance.id));
  layer.nodes.find((item) => item.id === f.source.id).params.size.value = [10000, 10000];
  secondSource.params.center.value = [0, 0];
  secondSource.params.size.value = [1, 1];
  second.id = 'healthy-viewer-macro';
  second.params.path.source = secondSource.id;
  second.macro.overrides.scale = 1;
  layer.nodes.push(secondSource, second);
  const owner = createCompositionSessionOwner(project);
  owner.dispatch({
    ...owner.state,
    type: 'set-macro-override',
    payload: { layerId: f.layer.id, nodeId: second.id, parameterId: 'scale', value: 2 },
  });
  assert.equal(
    owner.snapshot().layers[0].nodes.find((item) => item.id === second.id).macro.overrides.scale,
    2,
  );
});

for (const mode of ['bypass', 'disconnected']) {
  test(`inactive Macro ${mode} permits source edits and refuses invalid reactivation`, () => {
    const { f, instance } = largeMacro();
    if (mode === 'bypass')
      f.send('set-node-bypassed', { layerId: f.layer.id, nodeId: instance.id, bypassed: true });
    else f.send('disconnect', { layerId: f.layer.id, nodeId: instance.id, key: 'path' });
    f.send('set-parameter', {
      layerId: f.layer.id,
      nodeId: f.source.id,
      key: 'size',
      value: [10000, 10000],
    });
    const before = serializeCompositionProject(f.owner.projectSnapshot()),
      history = f.owner.historyState;
    assert.throws(
      () =>
        mode === 'bypass'
          ? f.send('set-node-bypassed', {
              layerId: f.layer.id,
              nodeId: instance.id,
              bypassed: false,
            })
          : f.send('connect', {
              layerId: f.layer.id,
              nodeId: instance.id,
              key: 'path',
              sourceId: f.source.id,
            }),
      /finite/i,
    );
    assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), before);
    assert.deepEqual(f.owner.historyState, history);
  });
}

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

test('a Layer snapshot retains its copied Macro pin and override through paste and History', () => {
  const f = fixture(),
    { instance } = insert(f),
    clipboard = createLayerClipboardOwner(),
    context = { owner: f.owner, generation: 1, compositionId: f.owner.activeCompositionId },
    copiedPath = pathSnapshot(f.owner, f.layer.id);
  clipboard.copy({ ...context, layerId: f.layer.id });
  f.send('set-macro-override', {
    layerId: f.layer.id,
    nodeId: instance.id,
    parameterId: 'scale',
    value: 2,
  });
  const before = serializeCompositionProject(f.owner.projectSnapshot()),
    result = f.send('paste-layer-snapshot', { token: clipboard.token(context) }),
    pasted = f.owner.snapshot().layers.find((item) => item.id === result.layerId),
    pastedInstance = pasted.nodes.find((item) => item.type === 'Macro');
  assert.notEqual(pastedInstance.id, instance.id);
  assert.deepEqual(pastedInstance.macro, instance.macro);
  assertSameArtwork(pathSnapshot(f.owner, result.layerId), copiedPath, { sameIds: false });
  const saved = serializeCompositionProject(f.owner.projectSnapshot());
  f.send('undo');
  assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), before);
  f.send('redo');
  assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), saved);
  const reopened = createCompositionSessionOwner(deserializeCompositionProject(saved, deserialize));
  assertSameArtwork(pathSnapshot(reopened, result.layerId), copiedPath, { sameIds: false });
});

for (const mode of ['exact-history-restore', 'branched-replacement']) {
  test(`a Layer snapshot tracks its Macro definition across ${mode}`, () => {
    let owner;
    const clipboard = createLayerClipboardOwner(),
      f = fixture({
        onCommitted: (state) => {
          if (owner && clipboard.hasCopy && state.sessionId === owner.id)
            clipboard.observe({
              owner,
              generation: 1,
              operation: state.operation,
              history: owner.historyView,
            });
        },
      });
    owner = f.owner;
    insert(f);
    const context = { owner, generation: 1, compositionId: owner.activeCompositionId },
      expected = pathSnapshot(owner, f.layer.id);
    clipboard.copy({ ...context, layerId: f.layer.id });
    const originalToken = clipboard.token(context);
    f.send('undo'); // insertion
    f.send('undo'); // definition registration
    assert.equal(clipboard.token(context), null);
    if (mode === 'exact-history-restore') {
      f.send('redo');
      const token = clipboard.token(context);
      assert.equal(token, originalToken);
      const result = f.send('paste-layer-snapshot', { token });
      assertSameArtwork(pathSnapshot(owner, result.layerId), expected, { sameIds: false });
    } else {
      f.send('set-document-settings', { key: 'width', value: 777 });
      const replacement = definition();
      replacement.parameters[0].defaultValue = 2;
      f.send('register-macro-definition', { definition: replacement });
      const before = serializeCompositionProject(owner.projectSnapshot()),
        history = owner.historyState;
      assert.equal(clipboard.token(context), null);
      assert.throws(
        () => f.send('paste-layer-snapshot', { token: originalToken }),
        /unavailable|clipboard/i,
      );
      assert.equal(serializeCompositionProject(owner.projectSnapshot()), before);
      assert.deepEqual(owner.historyState, history);
    }
  });
}

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
    ['duplicate-nodes', { layerId: f.layer.id, nodeIds: [instance.id] }],
    ['save-node-preset', { layerId: f.layer.id, nodeIds: [instance.id], name: 'Attempt' }],
    ['import-project-compositions', { sourceProject, compositionIds: [sourceCompositionId] }],
  ]) {
    assert.throws(() => f.send(type, payload), /macro|closure|support/i, type);
    assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), before);
    assert.deepEqual(f.owner.state, state);
    assert.deepEqual(f.owner.historyState, history);
  }
});

test('importing a different Composition without Macro instances remains available', () => {
  const f = fixture(),
    { instance } = insert(f),
    sourceProject = f.owner.projectSnapshot(),
    unrelated = migrateLegacyDocumentToCompositionProject(blankDocument()).compositions[0];
  unrelated.id = 'unrelated-composition';
  sourceProject.compositions.push(unrelated);
  const result = f.send('import-project-compositions', {
    sourceProject,
    compositionIds: [unrelated.id],
  });
  assert.equal(result.importedCompositionIds.length, 1);
  assert.ok(
    f.owner
      .projectSnapshot()
      .compositions.some((item) => item.id === result.importedCompositionIds[0]),
  );
  assert.ok(
    f.owner
      .projectSnapshot()
      .compositions[0].layers[0].nodes.some((item) => item.id === instance.id),
  );
  const destination = createCompositionSessionOwner(
      migrateLegacyDocumentToCompositionProject(blankDocument()),
    ),
    imported = destination.dispatch({
      ...destination.state,
      type: 'import-project-compositions',
      payload: { sourceProject, compositionIds: [unrelated.id] },
    });
  assert.equal(imported.importedCompositionIds.length, 1);
  assert.ok(
    destination
      .projectSnapshot()
      .compositions.some((item) => item.id === imported.importedCompositionIds[0]),
  );
});
