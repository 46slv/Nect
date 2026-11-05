import test, { mock } from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  layer,
  node,
  CommandSession,
  deserialize,
  serialize,
  evaluate,
  parameterReferenceExpression as ref,
} from '../src/core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

const send = (session, type, payload = {}) => session.dispatch({ ...session.state, type, payload });

function presetFixture({ point = 'arbitrary', includeOwner = false, ordinary = false } = {}) {
  const doc = blankDocument(),
    l = doc.layers[0],
    driver = node('Value'),
    sink = node('Value'),
    target = node(ordinary ? 'Value' : 'Curve'),
    other = node('Value');
  doc.schemaVersion = 60;
  driver.id = 'preset-driver';
  sink.id = 'preset-sink';
  target.id = 'deleted-node';
  other.id = 'unrelated';
  if (ordinary) target.params.value.value = 7;
  else target.points[0].id = point;
  sink.params.value.source = driver.id;
  sink.params.value.expression = ordinary
    ? ref('deleted-node', 'value')
    : ref('deleted-node', 'arbitrary.position', 'x');
  l.nodes.push(target, other);
  const source = [driver, sink];
  if (includeOwner) {
    const curve = structuredClone(target);
    curve.points[0].id = 'arbitrary';
    source.unshift(curve);
  }
  doc.nodePresets = [
    { id: 'legacy-preset', name: 'Legacy', nodes: source, fontAssets: {}, imageAssets: {} },
  ];
  return { doc, l, driver, sink, target, other };
}

test('legacy preset Apply refuses an accidental external Curve point binding with Redo intact', () => {
  for (const schemaVersion of [59, 60]) {
    const f = presetFixture();
    f.doc.schemaVersion = schemaVersion;
    const beforeRead = serialize(f.doc);
    assert.equal(serialize(deserialize(beforeRead)), beforeRead, 'old preset stays readable');
    const session = new CommandSession(f.doc);
    send(session, 'set-parameter', {
      layerId: f.l.id,
      nodeId: 'unrelated',
      key: 'value',
      value: 9,
    });
    send(session, 'undo');
    const before = serialize(session.snapshot()),
      revision = session.state.expectedRevision;
    assert.throws(
      () =>
        send(session, 'apply-node-preset', {
          layerId: f.l.id,
          presetId: 'legacy-preset',
        }),
      /retarget/i,
    );
    assert.equal(serialize(session.snapshot()), before);
    assert.equal(session.state.expectedRevision, revision);
    assert.deepEqual(session.snapshot().nodePresets, f.doc.nodePresets);
    send(session, 'redo');
    assert.equal(
      session.snapshot().layers[0].nodes.find((n) => n.id === 'unrelated').params.value.value,
      9,
    );
  }
});

test('preset Apply preserves unresolved point references and ordinary external parameters', () => {
  for (const ordinary of [false, true]) {
    const f = presetFixture({ point: 'different', ordinary });
    if (!ordinary) {
      const otherCurve = node('Curve');
      otherCurve.points[0].id = 'arbitrary';
      f.l.nodes.push(otherCurve);
    }
    const session = new CommandSession(f.doc);
    const result = send(session, 'apply-node-preset', {
      layerId: f.l.id,
      presetId: 'legacy-preset',
    });
    const copied = session.snapshot().layers[0].nodes.filter((n) => result.nodeIds.includes(n.id)),
      sink = copied.find((n) => n.params.value.expression);
    assert.equal(sink.params.value.expression, f.sink.params.value.expression);
    assert.ok(copied.some((n) => n.id === sink.params.value.source));
    send(session, 'disconnect', { layerId: f.l.id, nodeId: sink.id, key: 'value' });
    const evaluated = evaluate(session.snapshot().layers[0]);
    if (ordinary) assert.equal(evaluated.run(sink.id), 7);
    else assert.throws(() => evaluated.run(sink.id), /point is missing/);
  }
});

test('Composition Session rejects the same preset collision without changing project history', () => {
  for (const schemaVersion of [59, 60]) {
    const f = presetFixture();
    f.doc.schemaVersion = schemaVersion;
    const project = migrateLegacyDocumentToCompositionProject(f.doc),
      session = createCompositionSessionOwner(project);
    send(session, 'set-parameter', {
      layerId: f.l.id,
      nodeId: 'unrelated',
      key: 'value',
      value: 9,
    });
    send(session, 'undo');
    const before = serializeCompositionProject(session.projectSnapshot()),
      revision = session.state.expectedRevision;
    assert.throws(
      () => send(session, 'apply-node-preset', { layerId: f.l.id, presetId: 'legacy-preset' }),
      /retarget/i,
    );
    assert.equal(serializeCompositionProject(session.projectSnapshot()), before);
    assert.equal(session.state.expectedRevision, revision);
    send(session, 'redo');
    assert.equal(
      session.snapshot().layers[0].nodes.find((n) => n.id === 'unrelated').params.value.value,
      9,
    );
  }
});

test('preset preflight includes point references nested in dormant Color expressions', () => {
  const f = presetFixture(),
    driver = node('Color'),
    sink = node('Color');
  driver.id = 'color-driver';
  sink.id = 'color-sink';
  sink.params.value.source = driver.id;
  sink.params.value.expression = `alpha(value, ${ref('deleted-node', 'arbitrary.position', 'x')} / 1000)`;
  f.doc.nodePresets[0].nodes = [driver, sink];
  const session = new CommandSession(f.doc),
    before = serialize(session.snapshot());
  assert.throws(
    () => send(session, 'apply-node-preset', { layerId: f.l.id, presetId: 'legacy-preset' }),
    /retarget/i,
  );
  assert.equal(serialize(session.snapshot()), before);
});

test('preset Apply remaps an included Curve and its point rather than binding the destination original', () => {
  const f = presetFixture({ includeOwner: true }),
    session = new CommandSession(f.doc);
  const result = send(session, 'apply-node-preset', { layerId: f.l.id, presetId: 'legacy-preset' });
  const copied = session.snapshot().layers[0].nodes.filter((n) => result.nodeIds.includes(n.id)),
    curve = copied.find((n) => n.type === 'Curve'),
    sink = copied.find((n) => n.params.value?.expression);
  assert.notEqual(curve.id, 'deleted-node');
  assert.notEqual(curve.points[0].id, 'arbitrary');
  assert.equal(sink.params.value.expression, ref(curve.id, `${curve.points[0].id}.position`, 'x'));
  send(session, 'disconnect', { layerId: f.l.id, nodeId: sink.id, key: 'value' });
  assert.equal(
    evaluate(session.snapshot().layers[0]).run(sink.id),
    curve.points[0].position.value[0],
  );
});

test('clipboard Paste refuses a missing source owner that resolves by collision at destination', () => {
  const doc = blankDocument(),
    source = doc.layers[0],
    destination = layer(),
    driver = node('Value'),
    sink = node('Value'),
    other = node('Value');
  doc.schemaVersion = 60;
  driver.id = 'driver';
  sink.id = 'sink';
  other.id = 'unrelated';
  sink.params.value.source = driver.id;
  sink.params.value.expression = ref('node-collision', 'point-collision.position', 'x');
  source.nodes.push(driver, sink, other);
  doc.layers.push(destination);
  const session = new CommandSession(doc);
  send(session, 'copy-nodes', { layerId: source.id, nodeIds: ['driver', 'sink'] });
  send(session, 'delete-node', { layerId: source.id, nodeId: 'sink' });
  const values = ['collision', 'collision', 'second', 'third'];
  const random = mock.method(globalThis.crypto, 'randomUUID', () => values.shift() || 'extra');
  try {
    send(session, 'create-node', { layerId: destination.id, nodeType: 'Curve' });
  } finally {
    random.mock.restore();
  }
  send(session, 'set-parameter', {
    layerId: source.id,
    nodeId: 'unrelated',
    key: 'value',
    value: 9,
  });
  send(session, 'undo');
  const before = serialize(session.snapshot()),
    revision = session.state.expectedRevision;
  assert.throws(() => send(session, 'paste-nodes', { layerId: destination.id }), /retarget/i);
  assert.equal(serialize(session.snapshot()), before);
  assert.equal(session.state.expectedRevision, revision);
  send(session, 'redo');
  assert.equal(
    session.snapshot().layers[0].nodes.find((n) => n.id === 'unrelated').params.value.value,
    9,
  );
  send(session, 'delete-node', { layerId: destination.id, nodeId: 'node-collision' });
  const pasted = send(session, 'paste-nodes', { layerId: source.id });
  const copied = session.snapshot().layers[0].nodes.filter((n) => pasted.nodeIds.includes(n.id)),
    copiedSink = copied.find((n) => n.params.value.expression);
  assert.equal(copiedSink.params.value.expression, sink.params.value.expression);
  assert.ok(copied.some((n) => n.id === copiedSink.params.value.source));
});
