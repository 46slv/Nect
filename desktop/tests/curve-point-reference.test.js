import test, { mock } from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  node,
  layer,
  evaluate,
  validate,
  serialize,
  deserialize,
  duplicateNodes,
  CommandSession,
  parameterReferenceExpression as ref,
} from '../src/core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
  activeCompositionDocument,
  applyActiveCompositionDocument,
  validateCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { prepareCompositionImport } from '../src/project-composition-import.js';
import { prepareProjectCompositionsImport } from '../src/composition-project-import.js';
import { duplicateArtworkLayer, duplicateArtworkLayers } from '../src/artwork-duplication.js';
import { duplicationDocument } from './fixtures/artwork-duplication.js';

function fixture() {
  const doc = blankDocument(),
    l = doc.layers[0],
    curve = node('Curve'),
    sink = node('Value');
  curve.id = 'curve';
  curve.points[0].id = 'anchor';
  curve.points[0].position.value = [30, 40];
  sink.id = 'sink';
  l.nodes.push(curve, sink);
  return { doc, l, curve, sink, expr: ref(curve.id, 'anchor.position', 'x') };
}

const send = (session, type, payload = {}) => session.dispatch({ ...session.state, type, payload });
const read = (session, nodeId = 'sink') => evaluate(session.snapshot().layers[0]).run(nodeId);

test('Apply follows the stable anchor, evaluated sources, movement and save/reload', () => {
  const f = fixture(),
    session = new CommandSession(f.doc);
  send(session, 'set-parameter-expression', {
    layerId: f.l.id,
    nodeId: f.sink.id,
    key: 'value',
    expression: `${f.expr} + ${ref('curve', 'anchor.position', 'y')}`,
  });
  assert.equal(read(session), 70);
  assert.equal(session.snapshot().schemaVersion, 60);
  send(session, 'translate-points', {
    layerId: f.l.id,
    nodeId: f.curve.id,
    pointIds: ['anchor'],
    delta: [5, 7],
  });
  assert.equal(read(session), 82);
  const saved = serialize(session.snapshot());
  assert.equal(evaluate(deserialize(saved).layers[0]).run('sink'), 82);
  send(session, 'undo');
  assert.equal(read(session), 70);
  send(session, 'redo');
  assert.equal(read(session), 82);

  // The same mutable layer and the evaluator's shared cache must follow changes.
  const l = session.snapshot().layers[0],
    anchor = l.nodes.find((n) => n.id === 'curve').points[0];
  assert.equal(evaluate(l).run('sink'), 82);
  anchor.position.value = [20, 10];
  assert.equal(evaluate(l).run('sink'), 30);
  anchor.position.expression = 'value * 2; value + 3';
  assert.equal(evaluate(l).run('sink'), 53);
});

test('Apply rejects missing/wrong owners, keys, components, types and cycles without losing Redo', () => {
  const f = fixture(),
    wrong = node('Vector'),
    color = node('Color'),
    otherLayer = layer(),
    foreign = node('Curve');
  wrong.id = 'wrong';
  color.id = 'color';
  foreign.id = 'foreign';
  f.l.nodes.push(wrong, color);
  otherLayer.nodes.push(foreign);
  f.doc.layers.push(otherLayer);
  const session = new CommandSession(f.doc),
    apply = (expression, target = 'sink') =>
      send(session, 'set-parameter-expression', {
        layerId: f.l.id,
        nodeId: target,
        key: 'value',
        expression,
      });
  send(session, 'set-parameter', { layerId: f.l.id, nodeId: 'sink', key: 'value', value: 6 });
  send(session, 'undo');
  const before = serialize(session.snapshot()),
    revision = session.state.expectedRevision;
  for (const expression of [
    ref('curve', 'missing.position', 'x'),
    ref('wrong', 'anchor.position', 'x'),
    ref('curve', 'anchor.handleA', 'x'),
    ref('curve', 'anchor.position'),
    'ref("curve", "anchor.position", "z")',
    ref('foreign', `${foreign.points[0].id}.position`, 'x'),
    ref('missing-node', 'anchor.position', 'x'),
  ]) {
    assert.throws(() => apply(expression));
    assert.equal(serialize(session.snapshot()), before);
    assert.equal(session.state.expectedRevision, revision);
  }
  assert.throws(() => apply(f.expr, 'color'), /Color/);
  assert.equal(serialize(session.snapshot()), before);
  send(session, 'redo');
  assert.equal(read(session), 6);

  // The source point depends on the consumer; the back edge must be rejected.
  send(session, 'set-parameter-expression', {
    layerId: f.l.id,
    nodeId: 'curve',
    key: 'anchor.position',
    expression: `${ref('sink', 'value')}; 40`,
  });
  const cycleBefore = serialize(session.snapshot());
  assert.throws(() => apply(f.expr), /cycle/i);
  assert.equal(serialize(session.snapshot()), cycleBefore);
});

test('deletion keeps missing typed refs through unrelated edits, preflight, reload and history', () => {
  const f = fixture(),
    repeater = node('Geometry Repeater'),
    mask = node('Mask');
  f.doc.schemaVersion = 50;
  repeater.id = 'repeater';
  repeater.operation = 'ae-2d';
  mask.id = 'mask';
  mask.params.feather = { type: 'Scalar', value: 0, expression: '', locked: false };
  f.l.nodes.push(repeater, mask);
  const session = new CommandSession(f.doc);
  send(session, 'set-parameter-expression', {
    layerId: f.l.id,
    nodeId: 'sink',
    key: 'value',
    expression: f.expr,
  });
  send(session, 'set-parameter-expression', {
    layerId: f.l.id,
    nodeId: 'repeater',
    key: 'count',
    expression: f.expr,
  });
  send(session, 'set-parameter-expression', {
    layerId: f.l.id,
    nodeId: 'mask',
    key: 'feather',
    expression: f.expr,
  });
  const beforeDelete = serialize(session.snapshot());
  send(session, 'delete-point', { layerId: f.l.id, nodeId: 'curve', pointId: 'anchor' });
  const deleted = serialize(session.snapshot());
  assert.throws(() => read(session), /point is missing/);
  const reopened = new CommandSession(deserialize(deleted));
  assert.throws(() => read(reopened), /point is missing/);
  send(session, 'set-parameter', {
    layerId: f.l.id,
    nodeId: 'repeater',
    key: 'rotation',
    value: 5,
  });
  assert.throws(() => read(session), /point is missing/);
  send(session, 'undo');
  assert.equal(serialize(session.snapshot()), deleted);
  send(session, 'undo');
  assert.equal(serialize(session.snapshot()), beforeDelete);
  assert.equal(read(session), 30);
  send(session, 'redo');
  assert.throws(() => read(session), /point is missing/);

  // A missing reference may be read from a file, but a new Apply must resolve it.
  const before = serialize(session.snapshot());
  assert.throws(
    () =>
      send(session, 'set-parameter-expression', {
        layerId: f.l.id,
        nodeId: 'sink',
        key: 'value',
        expression: f.expr,
      }),
    /missing/,
  );
  assert.equal(serialize(session.snapshot()), before);
});

test('deleted Curve owners retain typed missing references across reload and Undo', () => {
  const f = fixture(),
    session = new CommandSession(f.doc);
  send(session, 'set-parameter-expression', {
    layerId: f.l.id,
    nodeId: 'sink',
    key: 'value',
    expression: f.expr,
  });
  send(session, 'delete-node', { layerId: f.l.id, nodeId: 'curve' });
  const missing = deserialize(serialize(session.snapshot()));
  assert.throws(() => evaluate(missing.layers[0]).run('sink'), /target is missing/);
  send(session, 'undo');
  assert.equal(read(session), 30);
  send(session, 'redo');
  assert.throws(() => read(session), /target is missing/);
});

test('a newly created point cannot silently reuse a dangling point identity', () => {
  const collision = fixture();
  collision.curve.points[0].id = 'point-collision';
  collision.sink.params.value.expression = ref('curve', 'point-collision.position', 'x');
  collision.doc.schemaVersion = 60;
  const session = new CommandSession(collision.doc);
  send(session, 'delete-point', {
    layerId: collision.l.id,
    nodeId: 'curve',
    pointId: 'point-collision',
  });
  const before = serialize(session.snapshot());
  const fake = mock.method(globalThis.crypto, 'randomUUID', () => 'collision');
  try {
    assert.throws(
      () =>
        send(session, 'create-point', {
          layerId: collision.l.id,
          nodeId: 'curve',
          position: [1, 2],
        }),
      /retarget/,
    );
  } finally {
    fake.mock.restore();
  }
  assert.equal(serialize(session.snapshot()), before);
});

test('clipboard external references reserve the point identity, while copied owners remain independent', () => {
  for (const includeOwner of [false, true]) {
    const f = fixture();
    f.curve.points[0].id = 'point-collision';
    f.sink.params.value.expression = ref('curve', 'point-collision.position', 'x');
    f.doc.schemaVersion = 60;
    const session = new CommandSession(f.doc);
    send(session, 'copy-nodes', {
      layerId: f.l.id,
      nodeIds: includeOwner ? ['curve', 'sink'] : ['sink'],
    });
    send(session, 'delete-node', { layerId: f.l.id, nodeId: 'sink' });
    send(session, 'delete-point', {
      layerId: f.l.id,
      nodeId: 'curve',
      pointId: 'point-collision',
    });
    const saved = serialize(session.snapshot());
    const fake = mock.method(globalThis.crypto, 'randomUUID', () => 'collision');
    try {
      const create = () =>
        send(session, 'create-point', {
          layerId: f.l.id,
          nodeId: 'curve',
          position: [1, 2],
        });
      if (includeOwner) create();
      else {
        assert.throws(create, /retarget/);
        assert.equal(serialize(session.snapshot()), saved);
      }
    } finally {
      fake.mock.restore();
    }
    if (!includeOwner) send(session, 'undo'); // restores the original point, not a replacement
    const result = send(session, 'paste-nodes', { layerId: f.l.id });
    const pasted = session.snapshot().layers[0].nodes.filter((n) => result.nodeIds.includes(n.id)),
      sink = pasted.find((n) => n.type === 'Value'),
      pastedCurve = pasted.find((n) => n.type === 'Curve');
    assert.equal(
      sink.params.value.expression,
      includeOwner
        ? ref(pastedCurve.id, `${pastedCurve.points[0].id}.position`, 'x')
        : ref('curve', 'point-collision.position', 'x'),
    );
    assert.equal(evaluate(session.snapshot().layers[0]).run(sink.id), 30);
  }
});

test('paused point expressions retain external ownership in clipboard and preset preflight', () => {
  const f = fixture(),
    driver = node('Value');
  driver.id = 'driver';
  f.curve.points[0].id = 'point-collision';
  f.sink.params.value.expression = ref('curve', 'point-collision.position', 'x');
  f.sink.params.value.source = driver.id;
  f.doc.schemaVersion = 60;
  f.l.nodes.push(driver);
  const session = new CommandSession(f.doc);
  assert.throws(
    () =>
      send(session, 'save-node-preset', {
        layerId: f.l.id,
        nodeIds: ['sink', 'driver'],
        name: 'Partial',
      }),
    /external/i,
  );
  send(session, 'copy-nodes', { layerId: f.l.id, nodeIds: ['sink', 'driver'] });
  send(session, 'delete-node', { layerId: f.l.id, nodeId: 'sink' });
  send(session, 'delete-point', {
    layerId: f.l.id,
    nodeId: 'curve',
    pointId: 'point-collision',
  });
  const before = serialize(session.snapshot());
  const fake = mock.method(globalThis.crypto, 'randomUUID', () => 'collision');
  try {
    assert.throws(
      () =>
        send(session, 'create-point', {
          layerId: f.l.id,
          nodeId: 'curve',
          position: [1, 2],
        }),
      /retarget/,
    );
  } finally {
    fake.mock.restore();
  }
  assert.equal(serialize(session.snapshot()), before);
  send(session, 'undo');
  const result = send(session, 'paste-nodes', { layerId: f.l.id });
  const copy = session
    .snapshot()
    .layers[0].nodes.find((n) => result.nodeIds.includes(n.id) && n.params.value?.expression);
  assert.equal(copy.params.value.expression, ref('curve', 'point-collision.position', 'x'));
  assert.notEqual(copy.params.value.source, 'driver');
});

test('missing references do not block independent Transform controls or hide invalid skew', () => {
  const f = fixture(),
    transform = node('Transform');
  transform.id = 'transform';
  f.l.nodes.push(transform);
  const session = new CommandSession(f.doc);
  send(session, 'set-parameter-expression', {
    layerId: f.l.id,
    nodeId: 'transform',
    key: 'rotation',
    expression: f.expr,
  });
  send(session, 'delete-point', { layerId: f.l.id, nodeId: 'curve', pointId: 'anchor' });
  send(session, 'set-parameter', {
    layerId: f.l.id,
    nodeId: 'transform',
    key: 'offset',
    value: [1, 2],
  });
  const before = serialize(session.snapshot());
  assert.throws(
    () =>
      send(session, 'set-parameter', {
        layerId: f.l.id,
        nodeId: 'transform',
        key: 'skew',
        value: 90,
      }),
    /Skew/,
  );
  assert.equal(serialize(session.snapshot()), before);
  assert.throws(
    () => evaluate(session.snapshot().layers[0]).transformValues('transform'),
    /missing/,
  );
});

test('node and generic layer duplication remap only matching Curve owner and point', () => {
  const f = fixture(),
    other = node('Curve');
  other.id = 'other';
  other.points[0].id = 'other-anchor';
  f.l.nodes.push(other);
  f.doc.schemaVersion = 60;
  f.sink.params.value.expression = `${f.expr} + ${ref('other', 'other-anchor.position', 'x')}`;
  const [copyCurve, copySink] = duplicateNodes(f.l, ['curve', 'sink']);
  assert.equal(
    copySink.params.value.expression,
    `${ref(copyCurve.id, `${copyCurve.points[0].id}.position`, 'x')} + ${ref('other', 'other-anchor.position', 'x')}`,
  );
  assert.equal(evaluate(f.l).run(copySink.id), 280);
  const onlySink = duplicateNodes(f.l, ['sink'])[0];
  assert.equal(onlySink.params.value.expression, f.sink.params.value.expression);

  const session = new CommandSession(f.doc);
  const result = send(session, 'duplicate-layer', { layerId: f.l.id });
  const copied = session.snapshot().layers.find((item) => item.id === result.layerId),
    c = copied.nodes.find((n) => n.name === 'Curve'),
    sink = copied.nodes.find((n) => n.name === 'Value');
  assert.ok(sink.params.value.expression.includes(`"${c.points[0].id}.position"`));
  assert.equal(evaluate(copied).run(sink.id), 280);
});

test('a dangling point ID belonging to another Curve never remaps to that Curve’s copy', () => {
  const f = fixture(),
    other = node('Curve');
  other.id = 'other';
  other.points[0].id = 'same-id';
  f.l.nodes.push(other);
  f.doc.schemaVersion = 60;
  f.sink.params.value.expression = ref('curve', 'same-id.position', 'x');
  validate(f.doc);
  const [curveCopy, sinkCopy, otherCopy] = duplicateNodes(f.l, ['curve', 'sink', 'other']);
  assert.equal(sinkCopy.params.value.expression, ref(curveCopy.id, 'same-id.position', 'x'));
  assert.notEqual(otherCopy.points[0].id, 'same-id');
  assert.throws(() => evaluate(f.l).run(sinkCopy.id), /missing/);
});

test('single and multi artwork copies retain internal point references', () => {
  const a = duplicationDocument('Curve'),
    b = duplicationDocument('Curve');
  for (const f of [a, b]) {
    f.doc.schemaVersion = 60;
    f.fill.params.color.expression = `alpha(value, ${ref(f.generator.id, `${f.generator.points[0].id}.position`, 'x')} / 1000)`;
  }
  const one = duplicateArtworkLayer(a.doc, a.layer.id);
  const single = a.doc.layers.find((l) => l.id === one.layerId),
    curve = single.nodes.find((n) => n.type === 'Curve'),
    fill = single.nodes.find((n) => n.type === 'Fill');
  assert.ok(fill.params.color.expression.includes(`"${curve.id}"`));
  assert.ok(fill.params.color.expression.includes(`"${curve.points[0].id}.position"`));
  assert.equal(evaluate(single).parameterValue(fill.id, 'color').alpha, 0.11);

  a.doc.layers.push(b.layer);
  const results = duplicateArtworkLayers(a.doc, [a.layer.id, b.layer.id]);
  for (const result of results) {
    const copy = a.doc.layers.find((l) => l.id === result.layerId),
      source = copy.nodes.find((n) => n.type === 'Curve'),
      paint = copy.nodes.find((n) => n.type === 'Fill');
    assert.ok(paint.params.color.expression.includes(`"${source.points[0].id}.position"`));
    assert.equal(evaluate(copy).parameterValue(paint.id, 'color').alpha, 0.11);
  }
});

test('document and Composition codecs enforce both version floors and preserve history', () => {
  const f = fixture(),
    initial = migrateLegacyDocumentToCompositionProject(f.doc),
    owner = createCompositionSessionOwner(initial),
    initialText = serializeCompositionProject(initial);
  send(owner, 'set-parameter-expression', {
    layerId: f.l.id,
    nodeId: 'sink',
    key: 'value',
    expression: f.expr,
  });
  const accepted = owner.projectSnapshot(),
    saved = serializeCompositionProject(accepted);
  assert.equal(accepted.schemaVersion, 60);
  assert.equal(accepted.compositionSchemaVersion, 2);
  assert.deepEqual(deserializeCompositionProject(saved), accepted);
  assert.equal(evaluate(owner.activeDocument().layers[0]).run('sink'), 30);
  send(owner, 'undo');
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), initialText);
  send(owner, 'redo');
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), saved);
  send(owner, 'set-parameter-expression', {
    layerId: f.l.id,
    nodeId: 'sink',
    key: 'value',
    expression: '',
  });
  assert.equal(owner.projectSnapshot().schemaVersion, 60);
  assert.equal(owner.projectSnapshot().compositionSchemaVersion, 2);

  const raw = activeCompositionDocument(accepted, accepted.compositions[0].id);
  assert.throws(() => deserialize(serialize({ ...raw, schemaVersion: 59 })), /schemaVersion 60/);
  for (const altered of [
    { ...accepted, compositionSchemaVersion: 1 },
    { ...accepted, schemaVersion: 59 },
    { ...accepted, compositionSchemaVersion: 1, schemaVersion: 59 },
  ])
    assert.throws(() => validateCompositionProject(altered));
  assert.equal(migrateLegacyDocumentToCompositionProject(raw).compositionSchemaVersion, 2);
  assert.equal(
    applyActiveCompositionDocument(initial, initial.compositions[0].id, raw)
      .compositionSchemaVersion,
    2,
  );
  assert.equal(
    applyActiveCompositionDocument(accepted, accepted.compositions[0].id, {
      ...raw,
      schemaVersion: 27,
    }).schemaVersion,
    60,
  );
});

test('both import routes remap owner and point IDs and promote only selected references', () => {
  const f = fixture();
  f.doc.schemaVersion = 60;
  f.sink.params.value.expression = f.expr;
  const source = migrateLegacyDocumentToCompositionProject(f.doc),
    plain = migrateLegacyDocumentToCompositionProject(blankDocument());
  plain.compositions[0].id = 'plain-composition';
  source.compositions.push(plain.compositions[0]);
  const destination = migrateLegacyDocumentToCompositionProject(fixture().doc);
  const selected = prepareProjectCompositionsImport(destination, source, [
    source.compositions[0].id,
  ]).project;
  assert.equal(selected.schemaVersion, 60);
  assert.equal(selected.compositionSchemaVersion, 2);
  const imported = selected.compositions[1].layers[0],
    c = imported.nodes.find((n) => n.type === 'Curve'),
    sink = imported.nodes.find((n) => n.type === 'Value');
  assert.notEqual(c.id, 'curve');
  assert.notEqual(c.points[0].id, 'anchor');
  assert.equal(sink.params.value.expression, ref(c.id, `${c.points[0].id}.position`, 'x'));
  assert.equal(evaluate(imported).run(sink.id), 30);
  const unselected = prepareProjectCompositionsImport(destination, source, [
    'plain-composition',
  ]).project;
  assert.ok(unselected.schemaVersion < 60);
  assert.equal(unselected.compositionSchemaVersion, 1);
  const legacyImport = prepareCompositionImport(fixture().doc, serialize(f.doc), f.l.id);
  const legacyLayer = legacyImport.layers.at(-1),
    legacyCurve = legacyLayer.nodes.find((n) => n.type === 'Curve'),
    legacySink = legacyLayer.nodes.find((n) => n.type === 'Value');
  assert.equal(legacyImport.schemaVersion, 60);
  assert.equal(
    legacySink.params.value.expression,
    ref(legacyCurve.id, `${legacyCurve.points[0].id}.position`, 'x'),
  );
  assert.equal(evaluate(legacyLayer).run(legacySink.id), 30);

  const plainLayer = layer();
  f.doc.layers.push(plainLayer);
  const legacyPlain = prepareCompositionImport(fixture().doc, serialize(f.doc), plainLayer.id);
  assert.ok(legacyPlain.schemaVersion < 60);
});

test('Composition Session imports point references as one undoable transaction', () => {
  const f = fixture();
  f.doc.schemaVersion = 60;
  f.sink.params.value.expression = f.expr;
  const source = migrateLegacyDocumentToCompositionProject(f.doc),
    destination = fixture(),
    owner = createCompositionSessionOwner(
      migrateLegacyDocumentToCompositionProject(destination.doc),
    ),
    before = serializeCompositionProject(owner.projectSnapshot());
  const result = send(owner, 'import-project-compositions', {
    sourceProject: source,
    compositionIds: [source.compositions[0].id],
  });
  const after = serializeCompositionProject(owner.projectSnapshot()),
    imported = owner
      .projectSnapshot()
      .compositions.find((c) => c.id === result.importedCompositionIds[0]).layers[0],
    sink = imported.nodes.find((n) => n.type === 'Value');
  assert.equal(evaluate(imported).run(sink.id), 30);
  assert.equal(owner.projectSnapshot().compositionSchemaVersion, 2);
  send(owner, 'undo');
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), before);
  send(owner, 'redo');
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), after);
});

test('import does not create a point matching a dangling ID from a different Curve', () => {
  const f = fixture(),
    other = node('Curve');
  other.id = 'other';
  other.points[0].id = 'pt';
  f.l.nodes.push(other);
  f.doc.schemaVersion = 60;
  f.sink.params.value.expression = ref('curve', 'pt-import-1.position', 'x');
  const source = migrateLegacyDocumentToCompositionProject(f.doc),
    dest = fixture();
  dest.curve.points[0].id = 'pt';
  const project = prepareProjectCompositionsImport(
    migrateLegacyDocumentToCompositionProject(dest.doc),
    source,
  ).project;
  const imported = project.compositions[1].layers[0],
    curve = imported.nodes.find((n) => n.type === 'Curve' && n.id.startsWith('curve')),
    otherCopy = imported.nodes.find((n) => n.id === 'other'),
    sink = imported.nodes.find((n) => n.type === 'Value');
  assert.notEqual(otherCopy.points[0].id, 'pt-import-1');
  assert.equal(sink.params.value.expression, ref(curve.id, 'pt-import-1.position', 'x'));
  assert.throws(() => evaluate(imported).run(sink.id), /missing/);
  assert.doesNotThrow(() => deserializeCompositionProject(serializeCompositionProject(project)));
});

test('prepared Composition edits retain the promoted wrapper and Undo/Redo', async () => {
  const f = fixture(),
    text = node('Text');
  text.id = 'text';
  f.l.nodes.push(text);
  const owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(f.doc));
  send(owner, 'set-parameter-expression', {
    layerId: f.l.id,
    nodeId: 'sink',
    key: 'value',
    expression: f.expr,
  });
  const before = serializeCompositionProject(owner.projectSnapshot());
  const bridge = {
    prepareDocument: async () => ({}),
    preparedFonts: () => ({}),
    assertFresh() {},
    activate() {},
    cancel() {},
  };
  const token = await owner.prepareCommand(
    {
      ...owner.state,
      type: 'set-text-content',
      payload: { layerId: f.l.id, nodeId: 'text', text: 'Updated' },
    },
    bridge,
  );
  owner.commitPrepared(token);
  const after = serializeCompositionProject(owner.projectSnapshot());
  assert.notEqual(after, before);
  assert.equal(owner.projectSnapshot().compositionSchemaVersion, 2);
  send(owner, 'undo');
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), before);
  send(owner, 'redo');
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), after);
});
