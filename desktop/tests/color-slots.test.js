import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  node,
  connect,
  evaluate,
  validate,
  CommandSession,
  serialize,
  deserialize,
} from '../src/core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  validateCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { prepareProjectCompositionsImport } from '../src/composition-project-import.js';

const original = { hex: '#123456', alpha: 0.25 };
const changed = { hex: '#abcdef', alpha: 0.75 };

function fixture(edit = () => {}) {
  const doc = blankDocument(),
    layer = doc.layers[0],
    shape = node('Rectangle'),
    directFill = node('Fill'),
    color = node('Color'),
    sourcedFill = node('Fill'),
    scalar = node('Value'),
    output = layer.nodes[0];
  directFill.params.color.value = { hex: '#112233', alpha: 0.6 };
  color.params.value.value = { hex: '#445566', alpha: 0.4 };
  layer.nodes = [shape, directFill, color, sourcedFill, scalar, output];
  connect(layer, shape.id, directFill.id, 'path');
  connect(layer, shape.id, sourcedFill.id, 'path');
  connect(layer, color.id, sourcedFill.id, 'color');
  connect(layer, directFill.id, output.id, 'path');
  edit({ doc, layer, shape, directFill, color, sourcedFill, scalar });
  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: layer.id, nodeId: directFill.id, key: 'color', ...payload },
    });
  const read = (nodeId = directFill.id, key = 'color') =>
    session.snapshot().layers[0].nodes.find((item) => item.id === nodeId).params[key];
  const result = () => {
    const snapshot = session.snapshot();
    return evaluate(snapshot.layers[0], {}, snapshot.imageAssets, snapshot);
  };
  const create = (name = 'Brand', value = original) =>
    send('create-color-slot', { name, value }).slotId;
  return {
    doc,
    layer,
    shape,
    directFill,
    color,
    sourcedFill,
    scalar,
    session,
    send,
    read,
    result,
    create,
  };
}

function stateOf(session) {
  return {
    document: serialize(session.snapshot()),
    state: session.state,
    past: structuredClone(session.historyView.past),
    future: structuredClone(session.historyView.future),
  };
}

function rejectsWithoutMutation(f, type, payload, pattern = /Color|slot|parameter|locked|driven/i) {
  const before = stateOf(f.session);
  assert.throws(() => f.send(type, payload), pattern);
  assert.deepEqual(stateOf(f.session), before);
}

test('shared colors use schema 54, stable slot IDs and sibling bindings, not a widened Color value', () => {
  const f = fixture(),
    legacy = serialize(f.session.snapshot()),
    slotId = f.create();
  assert.equal(typeof slotId, 'string');
  assert.ok(slotId.length > 0);
  f.send('bind-color-slot', { slotId });
  const snapshot = f.session.snapshot();
  assert.equal(snapshot.schemaVersion, 54);
  assert.deepEqual(snapshot.colorSlots, [{ id: slotId, name: 'Brand', value: original }]);
  assert.equal(f.read().colorSlotId, slotId);
  assert.deepEqual(Object.keys(f.read().value).sort(), ['alpha', 'hex']);
  assert.deepEqual(f.read().value, { hex: '#112233', alpha: 0.6 });
  const reopenedLegacy = deserialize(legacy);
  assert.equal(reopenedLegacy.colorSlots, undefined);
  assert.equal(serialize(reopenedLegacy), legacy);
  f.send('rename-color-slot', { slotId, name: 'Renamed brand' });
  assert.equal(f.session.snapshot().colorSlots[0].id, slotId);
  assert.equal(f.read().colorSlotId, slotId);
  assert.deepEqual(f.result().parameterValue(f.directFill.id, 'color'), original);
});

test('one slot resolves direct Fill and a Color-node source; edits retain fallback and edges', () => {
  const f = fixture(),
    slotId = f.create();
  f.send('bind-color-slot', { slotId });
  f.send('bind-color-slot', { slotId, nodeId: f.color.id, key: 'value' });
  for (const value of [original, changed, { hex: '#000000', alpha: 0 }]) {
    f.send('set-color-slot', { slotId, value });
    const evaluated = f.result();
    assert.equal(evaluated.error, null);
    assert.deepEqual(evaluated.parameterValue(f.directFill.id, 'color'), value);
    assert.deepEqual(evaluated.run(f.directFill.id).fill, value);
    assert.deepEqual(evaluated.run(f.color.id), value);
    assert.deepEqual(evaluated.run(f.sourcedFill.id).fill, value);
  }
  assert.equal(f.read(f.sourcedFill.id).source, f.color.id);
  assert.deepEqual(f.read().value, { hex: '#112233', alpha: 0.6 });
  assert.deepEqual(f.read(f.color.id, 'value').value, { hex: '#445566', alpha: 0.4 });
});

test('equal HEX and alpha never merge independent shared-color identities', () => {
  const f = fixture(),
    first = f.create('First'),
    second = f.create('Second');
  assert.notEqual(first, second);
  assert.equal(f.session.snapshot().colorSlots.length, 2);
  f.send('bind-color-slot', { slotId: first });
  f.send('bind-color-slot', { slotId: second, nodeId: f.color.id, key: 'value' });
  f.send('set-color-slot', { slotId: first, value: changed });
  assert.deepEqual(f.result().run(f.directFill.id).fill, changed);
  assert.deepEqual(f.result().run(f.sourcedFill.id).fill, original);
  assert.equal(f.read(f.color.id, 'value').colorSlotId, second);
});

test('registry-only edits invalidate warm direct and upstream Color-node caches', () => {
  const f = fixture(),
    slotId = f.create();
  f.send('bind-color-slot', { slotId });
  f.send('bind-color-slot', { slotId, nodeId: f.color.id, key: 'value' });
  // Reuse the identical Layer object: changing graph object identity must not be
  // what makes a registry-only edit visible to the evaluator.
  const snapshot = f.session.snapshot(),
    layer = snapshot.layers[0],
    run = () => evaluate(layer, {}, snapshot.imageAssets, snapshot);
  assert.deepEqual(run().run(f.sourcedFill.id).fill, original);
  const warm = run();
  warm.run(f.sourcedFill.id);
  assert.equal(warm.stats.computed, 0);
  snapshot.colorSlots[0].value = { ...changed };
  const updated = run();
  assert.deepEqual(updated.run(f.directFill.id).fill, changed);
  assert.deepEqual(updated.run(f.sourcedFill.id).fill, changed);
  assert.ok(updated.stats.computed > 0);
  assert.deepEqual(f.session.snapshot().colorSlots[0].value, original, 'evaluation is detached');
});

test('unbind freezes the current resolved Color, not the pre-binding fallback', () => {
  const f = fixture(),
    slotId = f.create();
  for (const [nodeId, key] of [
    [f.directFill.id, 'color'],
    [f.color.id, 'value'],
  ])
    f.send('bind-color-slot', { slotId, nodeId, key });
  f.send('set-color-slot', { slotId, value: changed });
  for (const [nodeId, key] of [
    [f.directFill.id, 'color'],
    [f.color.id, 'value'],
  ]) {
    f.send('unbind-color-slot', { nodeId, key });
    assert.equal(Object.hasOwn(f.read(nodeId, key), 'colorSlotId'), false);
    assert.deepEqual(f.read(nodeId, key).value, changed);
  }
  f.send('set-color-slot', { slotId, value: original });
  assert.deepEqual(f.result().run(f.directFill.id).fill, changed);
  assert.deepEqual(f.result().run(f.sourcedFill.id).fill, changed);
});

test('create, bind, rename, edit and unbind each have exact single-step Undo/Redo and reopen', () => {
  const f = fixture();
  const roundTripCommand = (type, payload) => {
    const before = serialize(f.session.snapshot()),
      revision = f.session.state.expectedRevision,
      result = f.send(type, payload),
      after = serialize(f.session.snapshot());
    assert.equal(f.session.state.expectedRevision, revision + 1, type);
    f.send('undo');
    assert.equal(serialize(f.session.snapshot()), before, `${type} Undo`);
    f.send('redo');
    assert.equal(serialize(f.session.snapshot()), after, `${type} Redo`);
    const reopened = new CommandSession(deserialize(after));
    assert.equal(serialize(reopened.snapshot()), after, `${type} reopen`);
    return result;
  };
  const { slotId } = roundTripCommand('create-color-slot', { name: 'Brand', value: original });
  roundTripCommand('bind-color-slot', { slotId });
  roundTripCommand('rename-color-slot', { slotId, name: 'Renamed' });
  roundTripCommand('set-color-slot', { slotId, value: changed });
  roundTripCommand('unbind-color-slot', {});
  assert.deepEqual(f.result().run(f.directFill.id).fill, changed);
});

test('unchanged registry and binding commands preserve revision and a pending Redo', () => {
  const f = fixture(),
    slotId = f.create();
  f.send('bind-color-slot', { slotId });
  f.send('set-color-slot', { slotId, value: changed });
  f.send('undo');
  const before = stateOf(f.session);
  for (const [type, payload] of [
    ['set-color-slot', { slotId, value: original }],
    ['rename-color-slot', { slotId, name: 'Brand' }],
    ['bind-color-slot', { slotId }],
    ['unbind-color-slot', { nodeId: f.color.id, key: 'value' }],
  ]) {
    assert.equal(f.send(type, payload).changed, false, type);
    assert.deepEqual(stateOf(f.session), before, type);
  }
  f.send('redo');
  assert.deepEqual(f.result().run(f.directFill.id).fill, changed);
});

test('invalid registry commands reject atomically without consuming Undo or Redo', () => {
  const f = fixture(),
    slotId = f.create();
  f.send('set-color-slot', { slotId, value: changed });
  f.send('undo');
  for (const value of [
    null,
    '#123456',
    [1, 0, 0, 1],
    { hex: '#fff', alpha: 1 },
    { hex: '#gg0000', alpha: 1 },
    { hex: '#123456', alpha: -0.1 },
    { hex: '#123456', alpha: 1.1 },
    { hex: '#123456', alpha: NaN },
    { hex: '#123456', alpha: Infinity },
    { hex: '#123456', alpha: 1, colorSlotId: slotId },
  ]) {
    rejectsWithoutMutation(f, 'create-color-slot', { name: 'Invalid', value });
    rejectsWithoutMutation(f, 'set-color-slot', { slotId, value });
  }
  for (const name of ['', '  ', 'x'.repeat(129), null]) {
    rejectsWithoutMutation(f, 'create-color-slot', { name, value: original });
    rejectsWithoutMutation(f, 'rename-color-slot', { slotId, name });
  }
  rejectsWithoutMutation(f, 'set-color-slot', { slotId: 'missing', value: original });
  rejectsWithoutMutation(f, 'rename-color-slot', { slotId: 'missing', name: 'Missing' });
  f.send('redo');
  assert.deepEqual(f.session.snapshot().colorSlots[0].value, changed);
});

test('stale shared-color commands never mutate document, history or revision', () => {
  const f = fixture(),
    stale = f.session.state,
    slotId = f.create(),
    before = stateOf(f.session);
  for (const type of [
    'create-color-slot',
    'set-color-slot',
    'rename-color-slot',
    'bind-color-slot',
    'unbind-color-slot',
  ]) {
    assert.throws(
      () =>
        f.session.dispatch({
          ...stale,
          type,
          payload: {
            slotId,
            name: 'Stale',
            value: changed,
            layerId: f.layer.id,
            nodeId: f.directFill.id,
            key: 'color',
          },
        }),
      /STALE_REVISION/,
    );
    assert.deepEqual(stateOf(f.session), before);
  }
});

test('binding refuses non-Color, connected, expression-driven and locked targets atomically', () => {
  const f = fixture(),
    slotId = f.create();
  rejectsWithoutMutation(f, 'bind-color-slot', { slotId: 'missing' });
  rejectsWithoutMutation(f, 'bind-color-slot', { slotId, nodeId: f.scalar.id, key: 'value' });
  rejectsWithoutMutation(f, 'bind-color-slot', { slotId, nodeId: f.sourcedFill.id });
  rejectsWithoutMutation(f, 'bind-color-slot', { slotId, nodeId: 'missing' });
  rejectsWithoutMutation(f, 'bind-color-slot', { slotId, layerId: 'missing' });
  rejectsWithoutMutation(f, 'bind-color-slot', { slotId, key: 'missing' });
  for (const edit of [
    ({ doc, directFill }) => {
      doc.schemaVersion = 42;
      directFill.params.color.expression = 'value';
    },
    ({ directFill }) => {
      directFill.params.color.locked = true;
    },
    ({ layer }) => {
      layer.locked = true;
    },
  ]) {
    const locked = fixture(edit),
      id = locked.create();
    rejectsWithoutMutation(locked, 'bind-color-slot', { slotId: id });
  }
});

test('native validation rejects malformed registries, dangling bindings and widened literal Colors', () => {
  const f = fixture(),
    slotId = f.create();
  f.send('bind-color-slot', { slotId });
  const valid = f.session.snapshot();
  for (const mutate of [
    (doc) => {
      doc.schemaVersion = 53;
    },
    (doc) => {
      doc.colorSlots = {};
    },
    (doc) => {
      doc.colorSlots.push(structuredClone(doc.colorSlots[0]));
    },
    (doc) => {
      doc.colorSlots[0].name = '';
    },
    (doc) => {
      doc.colorSlots[0].value.extra = true;
    },
    (doc) => {
      doc.colorSlots = [];
    },
    (doc) => {
      delete doc.colorSlots;
    },
    (doc) => {
      doc.layers[0].nodes.find(
        (item) => item.id === f.directFill.id,
      ).params.color.value.colorSlotId = slotId;
    },
    (doc) => {
      doc.layers[0].nodes.find((item) => item.id === f.scalar.id).params.value.colorSlotId = slotId;
    },
    (doc) => {
      doc.layers[0].nodes.find((item) => item.id === f.sourcedFill.id).params.color.colorSlotId =
        slotId;
    },
    (doc) => {
      doc.layers[0].nodes.find((item) => item.id === f.directFill.id).params.color.expression =
        'value';
    },
  ]) {
    const invalid = structuredClone(valid);
    mutate(invalid);
    assert.throws(() => validate(invalid));
    assert.throws(() => deserialize(JSON.stringify(invalid)));
  }
});

test('project validation checks dangling shared-color bindings in inactive Compositions', () => {
  const f = fixture(),
    slotId = f.create();
  f.send('bind-color-slot', { slotId });
  const project = migrateLegacyDocumentToCompositionProject(f.session.snapshot()),
    inactive = migrateLegacyDocumentToCompositionProject(fixture().session.snapshot())
      .compositions[0];
  inactive.id = 'inactive-composition';
  const target = inactive.layers[0].nodes.find((item) => item.type === 'Fill');
  target.params.color.colorSlotId = slotId;
  project.compositions.push(inactive);
  assert.equal(validateCompositionProject(project), project);
  const saved = serializeCompositionProject(project);
  assert.deepEqual(deserializeCompositionProject(saved), project);
  const invalid = structuredClone(project);
  invalid.compositions[1].layers[0].nodes.find(
    (item) => item.id === target.id,
  ).params.color.colorSlotId = 'missing-slot';
  assert.throws(() => validateCompositionProject(invalid), /Color slot|dangling/i);
  assert.throws(
    () => deserializeCompositionProject(JSON.stringify(invalid)),
    /Color slot|dangling/i,
  );
  assert.equal(
    serializeCompositionProject(project),
    saved,
    'failure does not edit the source project',
  );
});

test('saving and instantiating a bound Color preset preserves shared identity through reopen', () => {
  const f = fixture(),
    slotId = f.create();
  f.send('bind-color-slot', { slotId, nodeId: f.color.id, key: 'value' });
  const { presetId } = f.send('save-node-preset', {
    nodeIds: [f.color.id],
    name: 'Shared brand Color',
  });
  const saved = f.session.snapshot();
  assert.equal(saved.nodePresets[0].nodes[0].params.value.colorSlotId, slotId);
  assert.deepEqual(saved.colorSlots, [{ id: slotId, name: 'Brand', value: original }]);
  const session = new CommandSession(deserialize(serialize(saved)));
  const send = (type, payload) => session.dispatch({ ...session.state, type, payload });
  const { nodeIds } = send('apply-node-preset', { layerId: f.layer.id, presetId });
  assert.equal(nodeIds.length, 1);
  assert.notEqual(nodeIds[0], f.color.id);
  const copy = session.snapshot().layers[0].nodes.find((item) => item.id === nodeIds[0]);
  assert.equal(copy.params.value.colorSlotId, slotId);
  assert.deepEqual(copy.params.value.value, f.read(f.color.id, 'value').value);
  send('set-color-slot', { slotId, value: changed });
  const updated = session.snapshot(),
    result = evaluate(updated.layers[0], {}, updated.imageAssets, updated);
  assert.deepEqual(result.run(copy.id), changed);
  assert.deepEqual(result.run(f.color.id), changed);
  assert.deepEqual(updated.nodePresets[0].nodes[0].params.value.colorSlotId, slotId);
  assert.equal(serialize(deserialize(serialize(updated))), serialize(updated));
});

test('SVG import retains shared registry and inactive Composition bindings through history and reopen', () => {
  const source = () => ({
    tag: 'svg',
    namespace: null,
    attributes: [
      ['width', '100'],
      ['height', '100'],
    ],
    children: [
      {
        tag: 'rect',
        namespace: null,
        attributes: [
          ['width', '50'],
          ['height', '50'],
          ['fill', '#112233'],
        ],
        children: [],
      },
    ],
  });
  const f = fixture(),
    slotId = f.create();
  f.send('bind-color-slot', { slotId });
  const beforeLegacy = f.session.snapshot();
  f.send('import-svg', { source: source() });
  const afterLegacy = f.session.snapshot();
  assert.deepEqual(afterLegacy.colorSlots, beforeLegacy.colorSlots);
  assert.equal(afterLegacy.schemaVersion, 54);
  assert.doesNotThrow(() => deserialize(serialize(afterLegacy)));
  f.send('undo');
  assert.deepEqual(f.session.snapshot(), beforeLegacy);
  f.send('redo');
  assert.deepEqual(f.session.snapshot(), afterLegacy);

  const owner = createCompositionSessionOwner(
      migrateLegacyDocumentToCompositionProject(beforeLegacy),
    ),
    siblingId = owner.activeCompositionId;
  owner.createComposition({ name: 'SVG target' });
  const before = owner.projectSnapshot(),
    send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });
  send('import-svg', { source: source() });
  const after = owner.projectSnapshot();
  assert.deepEqual(after.colorSlots, before.colorSlots);
  assert.deepEqual(after.compositions[0], before.compositions[0]);
  assert.notDeepEqual(after.compositions[1].layers, before.compositions[1].layers);
  assert.equal(after.schemaVersion, 54);
  assert.equal(validateCompositionProject(after), after);
  send('undo');
  assert.deepEqual(owner.projectSnapshot(), before);
  send('redo');
  assert.deepEqual(owner.projectSnapshot(), after);
  send('set-color-slot', { slotId, value: changed });
  owner.switchComposition(siblingId);
  const sibling = owner.snapshot();
  assert.equal(
    sibling.layers[0].nodes.find((item) => item.id === f.directFill.id).params.color.colorSlotId,
    slotId,
  );
  assert.deepEqual(
    evaluate(sibling.layers[0], {}, sibling.imageAssets, sibling).run(f.directFill.id).fill,
    changed,
  );
  const saved = serializeCompositionProject(owner.projectSnapshot()),
    reopened = createCompositionSessionOwner(deserializeCompositionProject(saved));
  assert.equal(serializeCompositionProject(reopened.projectSnapshot()), saved);
});

test('project preset validation rejects dangling and conflicting shared-color bindings', () => {
  const f = fixture(),
    slotId = f.create();
  f.send('bind-color-slot', { slotId, nodeId: f.color.id, key: 'value' });
  f.send('save-node-preset', { nodeIds: [f.color.id], name: 'Shared brand Color' });
  const project = migrateLegacyDocumentToCompositionProject(f.session.snapshot()),
    saved = serializeCompositionProject(project);
  for (const mutate of [
    (parameter) => {
      parameter.colorSlotId = 'missing-slot';
    },
    (parameter) => {
      parameter.expression = 'value';
    },
    (parameter) => {
      parameter.source = f.color.id;
    },
    (parameter) => {
      parameter.type = 'Scalar';
      parameter.value = 1;
    },
  ]) {
    const invalid = structuredClone(project);
    mutate(invalid.nodePresets[0].nodes[0].params.value);
    assert.throws(() => validateCompositionProject(invalid), /Color slot|dangling|conflicting/i);
    assert.throws(
      () => deserializeCompositionProject(JSON.stringify(invalid)),
      /Color slot|dangling|conflicting/i,
    );
  }
  assert.equal(serializeCompositionProject(project), saved);
});

test('generic literal and base edits cannot silently mutate a shared-bound Color fallback', () => {
  const f = fixture(),
    slotId = f.create();
  f.send('bind-color-slot', { slotId });
  f.send('bind-color-slot', { slotId, nodeId: f.color.id, key: 'value' });
  f.send('set-color-slot', { slotId, value: changed });
  f.send('undo');
  for (const [nodeId, key] of [
    [f.directFill.id, 'color'],
    [f.color.id, 'value'],
  ])
    for (const type of ['set-parameter', 'set-parameter-base'])
      rejectsWithoutMutation(f, type, { nodeId, key, value: changed }, /Unbind|shared Color/i);
  f.send('redo');
  assert.deepEqual(f.result().run(f.directFill.id).fill, changed);
  assert.deepEqual(f.result().run(f.sourcedFill.id).fill, changed);
});

function freezeTree(value) {
  if (value && typeof value === 'object') {
    Object.values(value).forEach(freezeTree);
    Object.freeze(value);
  }
  return value;
}

function importFixture(mode) {
  const source = fixture().session.snapshot(),
    destination = fixture().session.snapshot(),
    red = { hex: '#ff0000', alpha: 0.3 },
    blue = { hex: '#0000ff', alpha: 0.8 },
    id = 'shared-brand';
  source.schemaVersion = 54;
  source.colorSlots = [
    { id, name: 'Source red', value: red },
    { id: 'unused-brand', name: 'Unselected green', value: { hex: '#00ff00', alpha: 1 } },
  ];
  const selected = source.layers[0],
    sourceFill = selected.nodes.find((item) => item.type === 'Fill'),
    sourceColor = selected.nodes.find((item) => item.type === 'Color');
  sourceFill.name = 'Imported direct fill';
  sourceColor.name = 'Imported Color';
  sourceFill.params.color.colorSlotId = id;
  sourceColor.params.value.colorSlotId = id;
  const unselected = fixture().session.snapshot().layers[0];
  unselected.nodes.find((item) => item.type === 'Fill').params.color.colorSlotId = 'unused-brand';
  source.layers.push(unselected);
  if (mode === 'different-value' || mode === 'same-value') {
    destination.schemaVersion = 54;
    destination.colorSlots = [
      { id, name: 'Destination brand', value: mode === 'same-value' ? red : blue },
    ];
    destination.layers[0].nodes.find((item) => item.type === 'Fill').params.color.colorSlotId = id;
  } else if (mode === 'entity-collision') {
    destination.layers[0].nodes.find((item) => item.type === 'Value').id = id;
  }
  validate(source);
  validate(destination);
  return { source, destination, selected, id, red, mode };
}

function assertImportedColors(result, importedLayer, destinationLayer, f) {
  const fill = importedLayer.nodes.find((item) => item.name === 'Imported direct fill'),
    color = importedLayer.nodes.find((item) => item.name === 'Imported Color'),
    importedId = fill.params.color.colorSlotId;
  assert.equal(color.params.value.colorSlotId, importedId, 'internal sharing is retained');
  if (f.mode !== 'no-registry')
    assert.notEqual(importedId, f.id, 'collisions always get a new identity');
  else assert.equal(importedId, f.id, 'noncolliding import retains the source identity');
  assert.deepEqual(result.colorSlots.find((slot) => slot.id === importedId).value, f.red);
  assert.ok(
    !result.colorSlots.some((slot) => slot.id === 'unused-brand'),
    'unselected slot is not copied',
  );
  const evaluation = evaluate(importedLayer, {}, result.imageAssets, result);
  assert.deepEqual(
    evaluation.run(fill.id).fill,
    f.red,
    'import retains source red, never destination blue',
  );
  assert.deepEqual(evaluation.run(color.id), f.red);
  assert.deepEqual(
    destinationLayer,
    f.destination.layers[0],
    'destination graph is never retargeted',
  );
  if (f.destination.colorSlots) {
    assert.deepEqual(result.colorSlots[0], f.destination.colorSlots[0]);
    assert.equal(result.colorSlots.length, 2, 'even equal values retain two distinct identities');
    const originalFill = destinationLayer.nodes.find((item) => item.type === 'Fill');
    assert.equal(originalFill.params.color.colorSlotId, f.id);
    assert.deepEqual(
      evaluate(destinationLayer, {}, result.imageAssets, result).run(originalFill.id).fill,
      f.destination.colorSlots[0].value,
    );
  } else assert.equal(result.colorSlots.length, 1);
}

test('legacy import command remaps shared colors without destination retargeting or unused slots', () => {
  for (const mode of ['different-value', 'same-value', 'no-registry', 'entity-collision']) {
    const f = importFixture(mode),
      sourceBefore = serialize(f.source),
      destinationBefore = serialize(f.destination),
      session = new CommandSession(freezeTree(f.destination));
    freezeTree(f.source);
    session.dispatch({
      ...session.state,
      type: 'import-composition',
      payload: { sourceText: sourceBefore, compositionId: f.selected.id },
    });
    const imported = session.snapshot();
    assert.equal(imported.layers.length, f.destination.layers.length + 1);
    assertImportedColors(imported, imported.layers.at(-1), imported.layers[0], f);
    assert.equal(serialize(f.source), sourceBefore);
    assert.equal(serialize(f.destination), destinationBefore);
    assert.doesNotThrow(() => deserialize(serialize(imported)));
    session.dispatch({ ...session.state, type: 'undo' });
    assert.equal(serialize(session.snapshot()), destinationBefore);
    session.dispatch({ ...session.state, type: 'redo' });
    assert.equal(serialize(session.snapshot()), serialize(imported));
  }
});

test('project import API remaps shared-color collisions including equal values and entity IDs', () => {
  for (const mode of ['different-value', 'same-value', 'no-registry', 'entity-collision']) {
    const f = importFixture(mode),
      source = migrateLegacyDocumentToCompositionProject(f.source),
      destination = migrateLegacyDocumentToCompositionProject(f.destination),
      destinationBefore = serializeCompositionProject(destination),
      selectedId = source.compositions[0].id;
    // Put the unselected Layer into a separate outer Composition so this path
    // also proves that a slot used only outside the selected graph is omitted.
    const unselected = source.compositions[0].layers.pop();
    source.compositions.push({
      ...structuredClone(source.compositions[0]),
      id: 'unselected-outer',
      layers: [unselected],
    });
    const splitBefore = serializeCompositionProject(source);
    const { project, compositionIds } = prepareProjectCompositionsImport(
      freezeTree(destination),
      freezeTree(source),
      [selectedId],
    );
    assert.equal(project.compositions.length, 2);
    assert.deepEqual(compositionIds, [project.compositions[1].id]);
    assert.equal(project.compositions[1].layers.length, 1);
    assertImportedColors(
      project,
      project.compositions[1].layers[0],
      project.compositions[0].layers[0],
      f,
    );
    assert.equal(serializeCompositionProject(source), splitBefore);
    assert.equal(serializeCompositionProject(destination), destinationBefore);
    assert.equal(
      serializeCompositionProject(
        deserializeCompositionProject(serializeCompositionProject(project)),
      ),
      serializeCompositionProject(project),
    );
  }
});

test('legacy migration reserves Color slot IDs before allocating the outer Composition identity', () => {
  const doc = fixture().session.snapshot();
  doc.schemaVersion = 54;
  doc.colorSlots = [{ id: 'legacy-composition-1', name: 'Reserved identity', value: original }];
  doc.layers[0].nodes.find((item) => item.type === 'Fill').params.color.colorSlotId =
    'legacy-composition-1';
  const before = serialize(doc),
    project = migrateLegacyDocumentToCompositionProject(freezeTree(doc));
  assert.notEqual(project.compositions[0].id, 'legacy-composition-1');
  assert.equal(project.colorSlots[0].id, 'legacy-composition-1');
  assert.equal(
    project.compositions[0].layers[0].nodes.find((item) => item.type === 'Fill').params.color
      .colorSlotId,
    'legacy-composition-1',
  );
  assert.equal(validateCompositionProject(project), project);
  assert.equal(serialize(doc), before);
});
