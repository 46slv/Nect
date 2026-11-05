import test, { mock } from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  node,
  connect,
  evaluate,
  expression,
  serialize,
  deserialize,
  duplicateNodes,
  expressionReferenceTargets,
  previewColorExpression,
  CommandSession,
  parameterReferenceExpression as ref,
} from '../src/core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
  validateCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { prepareCompositionImport } from '../src/project-composition-import.js';
import { prepareProjectCompositionsImport } from '../src/composition-project-import.js';

function fixture() {
  const doc = blankDocument(),
    layer = doc.layers[0],
    curve = node('Curve'),
    sink = node('Value'),
    color = node('Color');
  curve.id = 'curve';
  curve.points[0].id = 'anchor';
  curve.points[0].handleA.value = [3, 4];
  curve.points[0].handleB.value = [-12, 5];
  sink.id = 'sink';
  color.id = 'color';
  layer.nodes.push(curve, sink, color);
  return { doc, layer, curve, sink, color };
}
const send = (session, type, payload = {}) => session.dispatch({ ...session.state, type, payload });
const read = (session) => evaluate(session.snapshot().layers[0]).run('sink');
const handle = (field, component) => ref('curve', `anchor.${field}`, component);
const apply = (session, f, expression) =>
  send(session, 'set-parameter-expression', {
    layerId: f.layer.id,
    nodeId: 'sink',
    key: 'value',
    expression,
  });

test('length grammar is limited to handle keys and requires schema 61 even when the owner is missing', () => {
  const missingHandle = 'ref("deleted", "anchor.handleA", "length")';
  for (const source of [
    'ref("deleted", "radius", "length")',
    'ref("deleted", "anchor.position", "length")',
    'ref("deleted", "anchor.handleC", "length")',
  ])
    assert.throws(() => expression(source, () => 0, true), /length|component/i);

  for (const schemaVersion of [59, 60]) {
    const f = fixture();
    f.doc.schemaVersion = schemaVersion;
    f.sink.params.value.expression = missingHandle;
    assert.throws(() => deserialize(serialize(f.doc)), /schemaVersion 61/);
  }
  const f = fixture();
  f.doc.schemaVersion = 61;
  f.sink.params.value.expression = missingHandle;
  const restored = deserialize(serialize(f.doc));
  assert.equal(
    restored.layers[0].nodes.find((n) => n.id === 'sink').params.value.expression,
    missingHandle,
  );
  assert.throws(() => evaluate(restored.layers[0]).run('sink'), /target is missing/);
});

test('selected legacy Composition validates source grammar before import and preserves pending Redo', () => {
  for (const [schemaVersion, sourceExpression] of [59, 60].flatMap((version) => [
    [version, 'ref("deleted", "anchor.handleA", "length")'],
    [version, 'ref("deleted", "radius", "length")'],
  ])) {
    const f = fixture();
    f.doc.schemaVersion = schemaVersion;
    f.sink.params.value.expression = sourceExpression;
    const source = migrateLegacyDocumentToCompositionProject(f.doc);
    const clean = migrateLegacyDocumentToCompositionProject(blankDocument()).compositions[0];
    clean.id = 'unselected-clean';
    source.compositions.push(clean);
    const sourceBefore = serializeCompositionProject(source);

    const target = fixture(),
      owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(target.doc));
    send(owner, 'set-parameter-expression', {
      layerId: target.layer.id,
      nodeId: 'sink',
      key: 'value',
      expression: handle('handleA', 'length'),
    });
    send(owner, 'set-parameter-base', {
      layerId: target.layer.id,
      nodeId: 'sink',
      key: 'value',
      value: 17,
    });
    send(owner, 'undo');
    const before = serializeCompositionProject(owner.projectSnapshot()),
      state = { ...owner.state },
      future = JSON.stringify(owner.historyView.future);
    assert.throws(
      () =>
        send(owner, 'import-project-compositions', {
          sourceProject: source,
          compositionIds: [source.compositions[0].id],
        }),
      /length|schemaVersion 61/,
    );
    assert.equal(serializeCompositionProject(owner.projectSnapshot()), before);
    assert.deepEqual(owner.state, state);
    assert.equal(JSON.stringify(owner.historyView.future), future);
    assert.equal(serializeCompositionProject(source), sourceBefore);
    send(owner, 'redo');
    assert.equal(
      owner.snapshot().layers[0].nodes.find((n) => n.id === 'sink').params.value.value,
      17,
    );

    // An invalid unselected Composition must not block an independent selection.
    const imported = prepareProjectCompositionsImport(owner.projectSnapshot(), source, [
      'unselected-clean',
    ]).project;
    assert.equal(imported.compositions.length, owner.projectSnapshot().compositions.length + 1);
  }
});

test('legacy source grammar checks Vector2, Color scalar and paused expressions', () => {
  for (const [kind, oldSource] of ['Vector2', 'Color', 'paused'].flatMap((type) => [
    [type, 'ref("deleted", "anchor.handleB", "length")'],
    [type, 'ref("deleted", "radius", "length")'],
  ])) {
    const f = fixture();
    f.doc.schemaVersion = 60;
    if (kind === 'Vector2') f.curve.points[0].handleA.expression = `${oldSource}; 0`;
    if (kind === 'Color') f.color.params.value.expression = `alpha(value, ${oldSource} / 20)`;
    if (kind === 'paused') {
      const driver = node('Value');
      driver.id = 'paused-driver';
      f.layer.nodes.push(driver);
      f.sink.params.value.source = driver.id;
      f.sink.params.value.expression = oldSource;
    }
    assert.throws(() => deserialize(serialize(f.doc)), /length|schemaVersion 61/);
    const source = migrateLegacyDocumentToCompositionProject(f.doc);
    const destination = migrateLegacyDocumentToCompositionProject(blankDocument());
    const before = serializeCompositionProject(destination);
    assert.throws(
      () => prepareProjectCompositionsImport(destination, source),
      /length|schemaVersion 61/,
    );
    assert.equal(serializeCompositionProject(destination), before);
  }
});

test('schema 61 missing-owner handle length imports as missing and keeps the source version', () => {
  const f = fixture();
  f.doc.schemaVersion = 61;
  f.sink.params.value.expression = 'ref("deleted", "anchor.handleA", "length")';
  const source = migrateLegacyDocumentToCompositionProject(deserialize(serialize(f.doc)));
  const destination = migrateLegacyDocumentToCompositionProject(blankDocument());
  const imported = prepareProjectCompositionsImport(destination, source).project;
  const layer = imported.compositions.at(-1).layers[0];
  const sink = layer.nodes.find((n) => n.type === 'Value');
  assert.equal(imported.schemaVersion, 61);
  assert.equal(sink.params.value.expression, f.sink.params.value.expression);
  assert.throws(() => evaluate(layer).run(sink.id), /target is missing/);
});

test('handle components and lengths read effective relative vectors in Apply, live evaluation and Color preview', () => {
  const f = fixture(),
    session = new CommandSession(f.doc);
  for (const [expression, expected] of [
    [handle('handleA', 'x'), 3],
    [handle('handleA', 'y'), 4],
    [handle('handleA', 'length'), 5],
    [handle('handleB', 'x'), -12],
    [handle('handleB', 'y'), 5],
    [handle('handleB', 'length'), 13],
  ]) {
    apply(session, f, expression);
    assert.equal(read(session), expected);
    assert.equal(session.snapshot().schemaVersion, 61);
  }
  const colorExpression = `alpha(value, ${handle('handleA', 'length')} / 10)`;
  assert.equal(
    previewColorExpression(session.snapshot().layers[0], 'color', 'value', colorExpression).alpha,
    0.5,
  );
  send(session, 'set-parameter-expression', {
    layerId: f.layer.id,
    nodeId: 'color',
    key: 'value',
    expression: colorExpression,
  });
  assert.equal(evaluate(session.snapshot().layers[0]).run('color').alpha, 0.5);
  const before = session.snapshot().layers[0],
    point = before.nodes.find((n) => n.id === 'curve').points[0];
  point.handleA.expression = 'value * 2; value * 2';
  assert.equal(evaluate(before).run('color').alpha, 1);

  const driven = fixture(),
    driver = node('Vector');
  driver.id = 'driver';
  driver.params.value.value = [5, 12];
  driven.layer.nodes.push(driver);
  connect(driven.layer, 'driver', 'curve', 'anchor.handleA');
  const connected = new CommandSession(driven.doc);
  apply(connected, driven, handle('handleA', 'length'));
  assert.equal(read(connected), 13);
  send(connected, 'set-parameter', {
    layerId: driven.layer.id,
    nodeId: 'driver',
    key: 'value',
    value: [8, 15],
  });
  assert.equal(read(connected), 17);
});

test('zero and very large finite lengths work; overflow rejects without changing history', () => {
  const f = fixture();
  f.curve.points[0].handleA.value = [0, 0];
  const session = new CommandSession(f.doc);
  apply(session, f, handle('handleA', 'length'));
  assert.equal(read(session), 0);
  send(session, 'set-parameter', {
    layerId: f.layer.id,
    nodeId: 'curve',
    key: 'anchor.handleA',
    value: [1e200, 1e200],
  });
  assert.equal(read(session), Math.hypot(1e200, 1e200));
  const bad = fixture();
  bad.curve.points[0].handleA.value = [Number.MAX_VALUE, Number.MAX_VALUE];
  const rejected = new CommandSession(bad.doc);
  send(rejected, 'set-parameter', {
    layerId: bad.layer.id,
    nodeId: 'sink',
    key: 'value',
    value: 7,
  });
  send(rejected, 'undo');
  const before = serialize(rejected.snapshot()),
    revision = rejected.state.expectedRevision;
  assert.throws(() => apply(rejected, bad, handle('handleA', 'length')), /finite/);
  assert.equal(serialize(rejected.snapshot()), before);
  assert.equal(rejected.state.expectedRevision, revision);
  send(rejected, 'redo');
  assert.equal(read(rejected), 7);
});

test('IDs survive rename, point reorder, deletion, reload and Undo', () => {
  const f = fixture(),
    extra = structuredClone(f.curve.points[0]);
  extra.id = 'other-point';
  extra.handleA.value = [20, 21];
  f.curve.points.push(extra);
  const session = new CommandSession(f.doc);
  apply(session, f, handle('handleA', 'length'));
  send(session, 'rename', { layerId: f.layer.id, nodeId: 'curve', name: 'Renamed' });
  const reordered = session.snapshot();
  reordered.layers[0].nodes.find((n) => n.id === 'curve').points.reverse();
  assert.equal(evaluate(deserialize(serialize(reordered)).layers[0]).run('sink'), 5);
  send(session, 'delete-point', { layerId: f.layer.id, nodeId: 'curve', pointId: 'anchor' });
  assert.throws(() => read(session), /point is missing/);
  assert.throws(
    () => evaluate(deserialize(serialize(session.snapshot())).layers[0]).run('sink'),
    /point is missing/,
  );
  send(session, 'undo');
  assert.equal(read(session), 5);
  send(session, 'redo');
  assert.throws(() => read(session), /point is missing/);
  const before = serialize(session.snapshot()),
    g = fixture();
  g.curve.points[0].id = 'point-collision';
  g.sink.params.value.expression = ref('curve', 'point-collision.handleA', 'length');
  g.doc.schemaVersion = 61;
  const s = new CommandSession(g.doc);
  send(s, 'delete-point', { layerId: g.layer.id, nodeId: 'curve', pointId: 'point-collision' });
  const random = mock.method(globalThis.crypto, 'randomUUID', () => 'collision');
  try {
    // Re-use protection is separately exercised using the allocator's actual ID format.
    assert.throws(
      () => send(s, 'create-point', { layerId: g.layer.id, nodeId: 'curve', position: [1, 2] }),
      /retarget/,
    );
  } finally {
    random.mock.restore();
  }
  assert.equal(serialize(session.snapshot()), before);
});

test('invalid types, projections, missing targets and cycles reject atomically and retain Redo', () => {
  const f = fixture(),
    vector = node('Vector');
  vector.id = 'vector';
  f.layer.nodes.push(vector);
  const session = new CommandSession(f.doc);
  send(session, 'set-parameter', { layerId: f.layer.id, nodeId: 'sink', key: 'value', value: 7 });
  send(session, 'undo');
  const before = serialize(session.snapshot()),
    revision = session.state.expectedRevision;
  for (const bad of [
    ref('curve', 'missing.handleA', 'x'),
    ref('vector', 'anchor.handleA', 'x'),
    ref('curve', 'anchor.handleA'),
    'ref("curve", "anchor.position", "length")',
    'ref("vector", "value", "length")',
    'ref("curve", "anchor.handleA", "z")',
    '1 / 0',
  ]) {
    assert.throws(() => apply(session, f, bad));
    assert.equal(serialize(session.snapshot()), before);
    assert.equal(session.state.expectedRevision, revision);
  }
  assert.throws(() =>
    send(session, 'set-parameter-expression', {
      layerId: f.layer.id,
      nodeId: 'color',
      key: 'value',
      expression: handle('handleA', 'x'),
    }),
  );
  assert.equal(serialize(session.snapshot()), before);
  send(session, 'redo');
  assert.equal(read(session), 7);
  send(session, 'set-parameter-expression', {
    layerId: f.layer.id,
    nodeId: 'curve',
    key: 'anchor.handleA',
    expression: `${ref('sink', 'value')}; 4`,
  });
  const cyclic = serialize(session.snapshot());
  assert.throws(() => apply(session, f, handle('handleA', 'length')), /cycle/i);
  assert.equal(serialize(session.snapshot()), cyclic);
});

test('copy and both Composition import routes map matching owner/point and preserve unmatched keys', () => {
  const f = fixture(),
    other = node('Curve');
  other.id = 'other';
  other.points[0].id = 'other-anchor';
  f.layer.nodes.push(other);
  f.doc.schemaVersion = 61;
  f.sink.params.value.expression = `${handle('handleA', 'length')} + ${ref('curve', 'other-anchor.handleB', 'y')}`;
  const [curve, sink, copiedOther] = duplicateNodes(f.layer, ['curve', 'sink', 'other']);
  assert.equal(
    sink.params.value.expression,
    `${ref(curve.id, `${curve.points[0].id}.handleA`, 'length')} + ${ref(curve.id, 'other-anchor.handleB', 'y')}`,
  );
  assert.notEqual(copiedOther.points[0].id, 'other-anchor');
  assert.throws(() => evaluate(f.layer).run(sink.id), /missing/);
  f.sink.params.value.expression = handle('handleB', 'length');
  const source = migrateLegacyDocumentToCompositionProject(f.doc);
  const dest = migrateLegacyDocumentToCompositionProject(fixture().doc);
  const wrapped = prepareProjectCompositionsImport(dest, source).project;
  const legacy = prepareCompositionImport(fixture().doc, serialize(f.doc), f.layer.id);
  for (const layer of [wrapped.compositions.at(-1).layers[0], legacy.layers.at(-1)]) {
    const c = layer.nodes.find((n) => n.type === 'Curve' && n.id.startsWith('curve')),
      s = layer.nodes.find((n) => n.type === 'Value');
    assert.notEqual(c.id, 'curve');
    assert.notEqual(c.points[0].id, 'anchor');
    assert.equal(s.params.value.expression, ref(c.id, `${c.points[0].id}.handleB`, 'length'));
    assert.equal(evaluate(layer).run(s.id), 13);
  }
  assert.equal(wrapped.schemaVersion, 61);
  assert.equal(wrapped.compositionSchemaVersion, 2);
  assert.equal(legacy.schemaVersion, 61);
});

test('schema 60 dangling x/y and Color refs round-trip without promotion or rebinding', () => {
  const f = fixture(),
    fresh = node('Value');
  f.doc.schemaVersion = 60;
  f.sink.params.value.expression = ref('deleted', 'point-collision.handleA', 'x');
  f.color.params.value.expression = ref('deleted-color', 'point-collision.handleA');
  fresh.id = 'fresh';
  f.layer.nodes.push(fresh);
  const project = migrateLegacyDocumentToCompositionProject(deserialize(serialize(f.doc)));
  assert.equal(project.schemaVersion, 60);
  assert.deepEqual(deserializeCompositionProject(serializeCompositionProject(project)), project);
  const dest = migrateLegacyDocumentToCompositionProject(blankDocument());
  const imported = prepareProjectCompositionsImport(dest, project).project;
  assert.ok(imported.schemaVersion < 60);
  const owner = createCompositionSessionOwner(project);
  send(owner, 'set-parameter-expression', {
    layerId: f.layer.id,
    nodeId: 'fresh',
    key: 'value',
    expression: handle('handleA', 'length'),
  });
  assert.equal(owner.projectSnapshot().schemaVersion, 61);
  const reopened = createCompositionSessionOwner(
    deserializeCompositionProject(serializeCompositionProject(owner.projectSnapshot())),
  );
  assert.throws(
    () => evaluate(reopened.activeDocument().layers[0]).run('sink'),
    /target is missing/,
  );
  assert.throws(
    () => evaluate(reopened.activeDocument().layers[0]).run('color'),
    /target is missing/,
  );
  assert.equal(evaluate(reopened.activeDocument().layers[0]).run('fresh'), 5);
  const promotedImport = prepareProjectCompositionsImport(owner.projectSnapshot(), project).project;
  const importedLayer = promotedImport.compositions.at(-1).layers[0];
  const importedSink = importedLayer.nodes.find(
    (n) => n.params.value?.expression === f.sink.params.value.expression,
  );
  assert.throws(() => evaluate(importedLayer).run(importedSink.id), /target is missing/);
});

test('legacy wrapper codec preserves unresolved text and import prevents a new interpretation', () => {
  for (const pointId of ['missing', 'anchor']) {
    const f = fixture();
    f.doc.schemaVersion = 60;
    f.sink.params.value.expression = ref('curve', `${pointId}.handleA`, 'x');
    // The old wrapper codec accepted this shape, though its Document Session did not.
    const raw = migrateLegacyDocumentToCompositionProject(f.doc);
    assert.deepEqual(deserializeCompositionProject(serializeCompositionProject(raw)), raw);
    assert.throws(() => createCompositionSessionOwner(raw), /schemaVersion 61/);
    const legacyDestination = migrateLegacyDocumentToCompositionProject(fixture().doc);
    const legacyCopy = prepareProjectCompositionsImport(
      legacyDestination,
      raw,
    ).project.compositions.at(-1).layers[0];
    const legacyCurve = legacyCopy.nodes.find((n) => n.type === 'Curve');
    const legacySink = legacyCopy.nodes.find((n) => n.type === 'Value');
    assert.equal(
      legacySink.params.value.expression,
      ref(legacyCurve.id, `${pointId}.handleA`, 'x'),
    );
    if (pointId === 'anchor') assert.notEqual(legacyCurve.points[0].id, pointId);
    const dest = fixture(),
      session = new CommandSession(dest.doc);
    apply(session, dest, handle('handleA', 'length'));
    const promoted = migrateLegacyDocumentToCompositionProject(session.snapshot());
    const before = serializeCompositionProject(promoted);
    assert.throws(
      () => prepareProjectCompositionsImport(promoted, raw),
      /Legacy unresolved Curve handle expression/,
    );
    assert.equal(serializeCompositionProject(promoted), before);
  }
});

test('a schema 60 dangling handle key cannot rebind to newly allocated owner and point', () => {
  const f = fixture();
  f.doc.schemaVersion = 60;
  f.sink.params.value.expression = ref('node-collision', 'point-collision.handleA', 'x');
  const session = new CommandSession(f.doc);
  const before = serialize(session.snapshot());
  let count = 0;
  const random = mock.method(
    globalThis.crypto,
    'randomUUID',
    () => ['collision', 'collision', 'other-1', 'other-2'][count++],
  );
  try {
    assert.throws(() => send(session, 'create-node', { layerId: f.layer.id, nodeType: 'Curve' }));
  } finally {
    random.mock.restore();
  }
  assert.equal(serialize(session.snapshot()), before);
  assert.throws(() => read(session), /target is missing/);
});

test('clipboard collision rejects atomically and matching references paste with new identities', () => {
  const f = fixture(),
    missing = node('Value');
  missing.id = 'missing-sink';
  missing.params.value.expression = ref('curve', 'point-collision.handleB', 'x');
  f.sink.params.value.expression = handle('handleA', 'length');
  f.layer.nodes.push(missing);
  f.doc.schemaVersion = 61;
  const session = new CommandSession(f.doc);
  send(session, 'copy-nodes', { layerId: f.layer.id, nodeIds: ['curve', 'sink', 'missing-sink'] });
  const before = serialize(session.snapshot());
  let count = 0;
  const random = mock.method(
    globalThis.crypto,
    'randomUUID',
    () => ['new-1', 'new-2', 'new-3', 'collision'][count++],
  );
  try {
    assert.throws(() => send(session, 'paste-nodes', { layerId: f.layer.id }), /collision/);
  } finally {
    random.mock.restore();
  }
  assert.equal(serialize(session.snapshot()), before);
  const { nodeIds } = send(session, 'paste-nodes', { layerId: f.layer.id });
  const copies = session.snapshot().layers[0].nodes.filter((n) => nodeIds.includes(n.id)),
    curve = copies.find((n) => n.type === 'Curve'),
    sink = copies.find((n) => n.params.value?.expression?.includes('.handleA')),
    dangling = copies.find((n) => n.params.value?.expression?.includes('.handleB'));
  assert.equal(
    sink.params.value.expression,
    ref(curve.id, `${curve.points[0].id}.handleA`, 'length'),
  );
  assert.equal(dangling.params.value.expression, ref(curve.id, 'point-collision.handleB', 'x'));
  assert.equal(evaluate(session.snapshot().layers[0]).run(sink.id), 5);
  assert.throws(() => evaluate(session.snapshot().layers[0]).run(dangling.id), /missing/);
});

test('schema 61 wrapper 2 saves and reopens; only selected features raise import floors', () => {
  const f = fixture(),
    owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(f.doc));
  send(owner, 'set-parameter-expression', {
    layerId: f.layer.id,
    nodeId: 'sink',
    key: 'value',
    expression: handle('handleA', 'length'),
  });
  const accepted = owner.projectSnapshot();
  assert.equal(accepted.schemaVersion, 61);
  assert.equal(accepted.compositionSchemaVersion, 2);
  assert.deepEqual(deserializeCompositionProject(serializeCompositionProject(accepted)), accepted);
  assert.throws(
    () => validateCompositionProject({ ...accepted, compositionSchemaVersion: 1 }),
    /version/,
  );
  // The wrapper retains the old codec's opaque-text acceptance; opening the
  // Document applies the stricter per-reference version check.
  const lied = { ...accepted, schemaVersion: 60 };
  assert.doesNotThrow(() => validateCompositionProject(lied));
  assert.throws(() => createCompositionSessionOwner(lied), /schemaVersion 61/);
  assert.throws(
    () => deserialize(serialize({ ...owner.activeDocument(), schemaVersion: 60 })),
    /schemaVersion 61/,
  );
  send(owner, 'undo');
  assert.notEqual(owner.projectSnapshot().schemaVersion, 61);
  send(owner, 'redo');
  assert.equal(owner.projectSnapshot().schemaVersion, 61);
  const plain = migrateLegacyDocumentToCompositionProject(blankDocument()).compositions[0];
  plain.id = 'plain';
  accepted.compositions.push(plain);
  const imported = prepareProjectCompositionsImport(
    migrateLegacyDocumentToCompositionProject(blankDocument()),
    accepted,
    ['plain'],
  ).project;
  assert.ok(imported.schemaVersion < 60);
  assert.equal(imported.compositionSchemaVersion, 1);
});

test('selecting schema 60 positions or schema 59 live Colors from a schema 61 source preserves their floors', () => {
  const f = fixture(),
    session = new CommandSession(f.doc);
  const livePlan = {
    version: 1,
    modelVersion: 2,
    solverVersion: 'experimental-affine-v1',
    colorSemanticsVersion: 'linear-srgb-straight-v1',
    paletteId: 'floor-palette',
    intent: {
      keyColor: {
        id: 'floor-key',
        role: 'Identity',
        value: { hex: '#aBcD09', alpha: 0 },
        hueHint: 17,
      },
      preset: {
        id: 'floor-fixture',
        version: 1,
        harmony: 'triadic',
        hueSteps: 12,
        slots: [
          {
            id: 'floor-derived',
            role: 'Background',
            position: 1,
            lightnessScale: 0,
            lightnessOffset: 0.83,
            chromaScale: 0,
            chromaOffset: 0,
            alpha: 1,
          },
        ],
      },
      slots: [{ id: 'floor-derived', role: 'Background', deltaL: 0.03, deltaRatioCmax: 0 }],
      settings: { areaAware: false },
    },
    allocations: [
      { slotId: 'floor-derived', name: 'Background' },
      { slotId: 'floor-key', name: 'Key' },
    ],
  };
  send(session, 'initialize-live-key-color-recipe', livePlan);
  apply(session, f, handle('handleA', 'length'));
  const source = migrateLegacyDocumentToCompositionProject(session.snapshot());

  const linked = migrateLegacyDocumentToCompositionProject(blankDocument()).compositions[0];
  linked.id = 'linked-composition';
  const linkedColor = node('Color');
  linkedColor.id = 'linked-color';
  linkedColor.params.value.colorSlotId = 'floor-derived';
  linked.layers[0].nodes.push(linkedColor);
  source.compositions.push(linked);

  const position = fixture();
  position.curve.id = 'position-curve';
  position.curve.points[0].id = 'position-point';
  position.sink.id = 'position-sink';
  position.color.id = 'position-color';
  position.sink.params.value.expression = ref('position-curve', 'position-point.position', 'x');
  position.doc.schemaVersion = 60;
  const positionComposition = migrateLegacyDocumentToCompositionProject(position.doc)
    .compositions[0];
  positionComposition.id = 'position-composition';
  source.compositions.push(positionComposition);
  const dest = migrateLegacyDocumentToCompositionProject(blankDocument());
  const colorImport = prepareProjectCompositionsImport(dest, source, [
    'linked-composition',
  ]).project;
  assert.equal(colorImport.schemaVersion, 59);
  assert.equal(colorImport.compositionSchemaVersion, 1);
  assert.equal(colorImport.keyColorRecipe.mode, 'live');
  const positionImport = prepareProjectCompositionsImport(dest, source, [
    'position-composition',
  ]).project;
  assert.equal(positionImport.schemaVersion, 60);
  assert.equal(positionImport.compositionSchemaVersion, 2);
  assert.equal(Object.hasOwn(positionImport, 'keyColorRecipe'), false);
  const legacySource = session.snapshot();
  legacySource.layers.push(linked.layers[0], positionComposition.layers[0]);
  const legacyColor = prepareCompositionImport(
    blankDocument(),
    serialize(legacySource),
    linked.layers[0].id,
  );
  assert.equal(legacyColor.schemaVersion, 59);
  assert.equal(legacyColor.keyColorRecipe.mode, 'live');
  const legacyPosition = prepareCompositionImport(
    blankDocument(),
    serialize(legacySource),
    positionComposition.layers[0].id,
  );
  assert.equal(legacyPosition.schemaVersion, 60);
  assert.equal(Object.hasOwn(legacyPosition, 'keyColorRecipe'), false);
});

test('picker exposes stable incoming/outgoing x, y, and length but not Color references', () => {
  const f = fixture(),
    targets = expressionReferenceTargets(f.layer, 'sink');
  for (const field of ['handleA', 'handleB'])
    for (const component of ['x', 'y', 'length'])
      assert.ok(targets.some((t) => t.expression === handle(field, component)));
  assert.ok(
    !expressionReferenceTargets(f.layer, 'color', 'Color').some((t) => t.key.includes('handle')),
  );
});
