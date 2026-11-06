import test, { mock } from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  blankLayer,
  node,
  connect,
  evaluate,
  CommandSession,
  serialize,
  deserialize,
  expressionReferenceTargetsForDocument,
} from '../src/core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
} from '../src/composition-project-model.js';
import { prepareProjectCompositionsImport } from '../src/composition-project-import.js';
import { prepareCompositionImport } from '../src/project-composition-import.js';
import { stageArtworkDuplicates, stageArtworkDuplicate } from '../src/artwork-duplication.js';
import { previewColorExpression } from '../src/core.js';
import { duplicationDocument } from './fixtures/artwork-duplication.js';

function fixture() {
  const doc = blankDocument(),
    a = doc.layers[0],
    b = blankLayer('Source');
  a.id = 'art-a';
  b.id = 'art-b';
  const sink = node('Value'),
    driver = node('Value'),
    curve = node('Curve');
  sink.id = 'sink';
  driver.id = 'driver';
  curve.id = 'curve';
  curve.points[0].id = 'point';
  driver.params.value.value = 7;
  curve.points[0].handleA.value = [3, 4];
  a.nodes.push(sink);
  b.nodes.push(driver, curve);
  doc.layers.push(b);
  return { doc, a, b, sink, driver, curve };
}
const send = (session, type, payload = {}) => session.dispatch({ ...session.state, type, payload });
const apply = (session, expression) =>
  send(session, 'set-parameter-expression', {
    layerId: 'art-a',
    nodeId: 'sink',
    key: 'value',
    expression,
  });
const read = (doc) => evaluate(doc.layers[0], {}, {}, doc).parameterValue('sink', 'value');

test('qualified scalar and Curve handle read another Artwork lazily, serialize, undo and redo', () => {
  const f = fixture(),
    session = new CommandSession(f.doc);
  apply(
    session,
    'ref(artwork, "art-b", "driver", "value") + ref(artwork, "art-b", "curve", "point.handleA", "length")',
  );
  let doc = session.snapshot();
  assert.equal(doc.schemaVersion, 62);
  assert.equal(read(doc), 12);
  assert.equal(read(deserialize(serialize(doc))), 12);
  send(session, 'set-parameter', { layerId: 'art-b', nodeId: 'driver', key: 'value', value: 9 });
  assert.equal(read(session.snapshot()), 14);
  send(session, 'undo');
  assert.equal(read(session.snapshot()), 12);
  send(session, 'redo');
  assert.equal(read(session.snapshot()), 14);
  assert.ok(
    expressionReferenceTargetsForDocument(session.snapshot(), 'art-a', 'sink').some(
      (item) => item.layerId === 'art-b' && item.nodeId === 'driver',
    ),
  );
});

test('mixed cross-Artwork cycles reject without history change', () => {
  const session = new CommandSession(fixture().doc);
  apply(session, 'ref(artwork, "art-b", "driver", "value")');
  const before = session.snapshot(),
    state = session.state;
  assert.throws(
    () =>
      send(session, 'set-parameter-expression', {
        layerId: 'art-b',
        nodeId: 'driver',
        key: 'value',
        expression: 'ref(artwork, "art-a", "sink", "value")',
      }),
    /cycle/i,
  );
  assert.deepEqual(session.snapshot(), before);
  assert.deepEqual(session.state, state);
});

test('Composition Reference Position is independent of its output, while a real parameter cycle rejects', () => {
  const f = fixture(),
    circle = node('Circle'),
    fill = node('Fill'),
    comp = node('Composition Reference');
  circle.id = 'circle';
  fill.id = 'fill';
  comp.id = 'comp';
  comp.compositionReference = { compositionId: 'art-a' };
  comp.params.position.value = [12, 10];
  f.a.nodes.push(circle, fill);
  f.b.nodes.push(comp);
  f.doc.schemaVersion = 31;
  connect(f.a, 'circle', 'fill', 'path');
  connect(f.a, 'fill', f.a.output, 'path');
  const session = new CommandSession(f.doc);
  send(session, 'set-parameter-expression', {
    layerId: 'art-a',
    nodeId: 'circle',
    key: 'radius',
    expression: 'ref(artwork, "art-b", "comp", "position", "x")',
  });
  const doc = session.snapshot();
  assert.equal(evaluate(doc.layers[0], {}, {}, doc).error, null);
  assert.ok(
    expressionReferenceTargetsForDocument(doc, 'art-a', 'circle').some(
      (target) => target.nodeId === 'comp' && target.key === 'position',
    ),
  );
  const before = session.snapshot();
  assert.throws(
    () =>
      send(session, 'set-parameter-expression', {
        layerId: 'art-b',
        nodeId: 'comp',
        key: 'position',
        expression: 'ref(artwork, "art-a", "circle", "radius"); 0',
      }),
    /cycle/i,
  );
  assert.deepEqual(session.snapshot(), before);
});

test('Text Position and Reference graph wires participate in cross-Artwork cycle checks', () => {
  const f = fixture(),
    text = node('Text'),
    reference = node('Reference'),
    vector = node('Vector');
  text.id = 'text';
  reference.id = 'reference';
  vector.id = 'vector';
  vector.params.value.value = [4, 8];
  reference.reference = { nodeId: 'vector', portId: 'output' };
  f.b.nodes.push(text, reference, vector);
  connect(f.b, 'reference', 'text', 'position');
  const session = new CommandSession(f.doc);
  apply(session, 'ref(artwork, "art-b", "text", "position", "x")');
  assert.equal(read(session.snapshot()), 4);
  const before = session.snapshot(),
    state = session.state;
  assert.throws(
    () =>
      send(session, 'set-parameter-expression', {
        layerId: 'art-b',
        nodeId: 'vector',
        key: 'value',
        expression: 'ref(artwork, "art-a", "sink", "value"); 0',
      }),
    /cycle/i,
  );
  assert.deepEqual(session.snapshot(), before);
  assert.deepEqual(session.state, state);
});

test('qualified vector components and Color scalar arguments share the same effective resolver', () => {
  const f = fixture(),
    vector = node('Vector'),
    color = node('Color');
  vector.id = 'vector';
  color.id = 'color';
  vector.params.value.value = [0.25, 0.75];
  f.b.nodes.push(vector);
  f.a.nodes.push(color);
  const session = new CommandSession(f.doc);
  send(session, 'set-parameter-expression', {
    layerId: 'art-a',
    nodeId: 'color',
    key: 'value',
    expression: 'alpha(value, ref(artwork, "art-b", "vector", "value", "x"))',
  });
  const doc = session.snapshot(),
    layer = doc.layers[0];
  assert.equal(evaluate(layer, {}, {}, doc).parameterValue('color', 'value').alpha, 0.25);
  assert.equal(
    previewColorExpression(
      layer,
      'color',
      'value',
      'alpha(value, ref(artwork, "art-b", "vector", "value", "y"))',
      {},
      {},
      doc,
    ).alpha,
    0.75,
  );
  assert.deepEqual(
    deserializeCompositionProject(
      serializeCompositionProject(migrateLegacyDocumentToCompositionProject(doc)),
    ),
    migrateLegacyDocumentToCompositionProject(doc),
  );
  assert.throws(
    () =>
      send(session, 'set-parameter-expression', {
        layerId: 'art-a',
        nodeId: 'color',
        key: 'value',
        expression: 'ref(artwork, "art-b", "vector", "value")',
      }),
    /Color|numeric/i,
  );
});

test('same-layer node duplication retains a qualified source, while unsupported clipboard and preset paths refuse', () => {
  const session = new CommandSession(fixture().doc);
  apply(session, 'ref(artwork, "art-b", "driver", "value")');
  const { nodeIds } = send(session, 'duplicate-nodes', { layerId: 'art-a', nodeIds: ['sink'] });
  const doc = session.snapshot();
  assert.equal(evaluate(doc.layers[0], {}, {}, doc).parameterValue(nodeIds[0], 'value'), 7);
  send(session, 'copy-nodes', { layerId: 'art-a', nodeIds: ['sink'] });
  const before = session.snapshot(),
    state = session.state;
  assert.throws(() => send(session, 'paste-nodes', { layerId: 'art-a' }), /Artwork references/);
  assert.throws(
    () =>
      send(session, 'save-node-preset', { layerId: 'art-a', nodeIds: ['sink'], name: 'qualified' }),
    /external|Artwork/,
  );
  assert.deepEqual(session.snapshot(), before);
  assert.deepEqual(session.state, state);
});

test('source schema and wrapper validate qualified syntax even when an input is connected', () => {
  const f = fixture(),
    local = node('Value');
  local.id = 'local';
  f.a.nodes.push(local);
  connect(f.a, 'local', 'sink', 'value');
  f.sink.params.value.expression = 'ref(artwork, "art-b", "driver", "value")';
  for (const version of [59, 60, 61]) {
    f.doc.schemaVersion = version;
    assert.throws(() => deserialize(JSON.stringify(f.doc)), /schemaVersion 62/);
    const oldWrapper = migrateLegacyDocumentToCompositionProject(f.doc);
    assert.doesNotThrow(() =>
      deserializeCompositionProject(serializeCompositionProject(oldWrapper)),
    );
  }
  f.doc.schemaVersion = 62;
  const modern = migrateLegacyDocumentToCompositionProject(f.doc);
  modern.compositions[0].layers[0].nodes.find((n) => n.id === 'sink').params.value.expression =
    'ref(artwork, "art-b", "driver", "value"';
  assert.throws(() => serializeCompositionProject(modern), /syntax|arguments|reference/i);
});

test('foreign output errors stay isolated and connected effective values update without stale caching', () => {
  const f = fixture(),
    circle = node('Circle'),
    fill = node('Fill'),
    upstream = node('Value');
  circle.id = 'broken-circle';
  fill.id = 'broken-fill';
  upstream.id = 'upstream';
  circle.params.radius.expression = 'ref("deleted", "value")';
  upstream.params.value.value = 20;
  f.b.nodes.push(circle, fill, upstream);
  connect(f.b, 'broken-circle', 'broken-fill', 'path');
  connect(f.b, 'broken-fill', f.b.output, 'path');
  connect(f.b, 'upstream', 'driver', 'value');
  const session = new CommandSession(f.doc);
  apply(session, 'ref(artwork, "art-b", "driver", "value")');
  const first = session.snapshot();
  const broken = evaluate(first.layers[1], {}, {}, first);
  assert.match(broken.error, /missing/i);
  assert.throws(() => broken.run('broken-circle'), /missing/i);
  assert.equal(broken.parameterValue('driver', 'value'), 20);
  assert.equal(evaluate(first.layers[1], {}, {}, first).parameterValue('driver', 'value'), 20);
  assert.equal(read(first), 20);
  send(session, 'set-parameter', { layerId: 'art-b', nodeId: 'upstream', key: 'value', value: 21 });
  assert.equal(read(session.snapshot()), 21);
});

test('source edit that invalidates a foreign Repeater rejects atomically', () => {
  const f = fixture(),
    repeater = node('Geometry Repeater');
  f.doc.schemaVersion = 37;
  repeater.id = 'repeat';
  repeater.operation = 'ae-2d';
  f.a.nodes.push(repeater);
  const session = new CommandSession(f.doc);
  send(session, 'set-parameter-expression', {
    layerId: 'art-a',
    nodeId: 'repeat',
    key: 'count',
    expression: 'ref(artwork, "art-b", "driver", "value")',
  });
  const before = session.snapshot(),
    state = session.state;
  assert.throws(
    () =>
      send(session, 'set-parameter', {
        layerId: 'art-b',
        nodeId: 'driver',
        key: 'value',
        value: 1000,
      }),
    /Repeater/i,
  );
  assert.deepEqual(session.snapshot(), before);
  assert.deepEqual(session.state, state);
  const started = send(session, 'begin-gesture', {
    target: { kind: 'parameter', layerId: 'art-b', nodeId: 'driver', key: 'value' },
  });
  const accepted = send(session, 'update-gesture', { ...started, update: { value: 8 } });
  const previewDoc = session.previewSnapshot(accepted.gestureId).document;
  assert.equal(
    evaluate(previewDoc.layers[0], {}, {}, previewDoc).parameterValue('repeat', 'count'),
    8,
  );
  const preview = session.previewSnapshot(accepted.gestureId);
  assert.throws(
    () => send(session, 'update-gesture', { ...accepted, update: { value: 1000 } }),
    /Repeater/i,
  );
  assert.deepEqual(session.previewSnapshot(accepted.gestureId), preview);
  send(session, 'cancel-gesture', accepted);
});

test('indirect downstream bounded controls reject invalid effective values even with a disconnected output', () => {
  const f = fixture(),
    local = node('Value'),
    resample = node('Resample Path');
  f.doc.schemaVersion = 37;
  local.id = 'local';
  resample.id = 'resample';
  f.a.nodes.push(local, resample);
  connect(f.a, local.id, resample.id, 'count');
  const session = new CommandSession(f.doc);
  send(session, 'set-parameter-expression', {
    layerId: 'art-a',
    nodeId: 'local',
    key: 'value',
    expression: 'ref(artwork, "art-b", "driver", "value")',
  });
  const before = session.snapshot(),
    state = session.state;
  assert.throws(
    () =>
      send(session, 'set-parameter', {
        layerId: 'art-b',
        nodeId: 'driver',
        key: 'value',
        value: 1,
      }),
    /Resample count/i,
  );
  assert.deepEqual(session.snapshot(), before);
  assert.deepEqual(session.state, state);
});

test('deleted foreign point remains missing, cannot silently rebind, and Undo restores it', () => {
  const f = fixture();
  f.curve.points[0].id = 'point-collision';
  const session = new CommandSession(f.doc);
  apply(session, 'ref(artwork, "art-b", "curve", "point-collision.handleA", "length")');
  send(session, 'delete-point', { layerId: 'art-b', nodeId: 'curve', pointId: 'point-collision' });
  assert.throws(() => read(session.snapshot()), /missing/i);
  const before = session.snapshot(),
    state = session.state;
  const fake = mock.method(globalThis.crypto, 'randomUUID', () => 'collision');
  try {
    assert.throws(
      () => send(session, 'create-point', { layerId: 'art-b', nodeId: 'curve', position: [0, 0] }),
      /retarget/i,
    );
  } finally {
    fake.mock.restore();
  }
  assert.deepEqual(session.snapshot(), before);
  assert.deepEqual(session.state, state);
  send(session, 'undo');
  assert.equal(read(session.snapshot()), 5);
});

test('deleting a foreign node or layer cannot be reversed accidentally by a reused ID', () => {
  const f = fixture();
  f.driver.id = 'node-collision';
  const session = new CommandSession(f.doc);
  apply(session, 'ref(artwork, "art-b", "node-collision", "value")');
  send(session, 'delete-node', { layerId: 'art-b', nodeId: 'node-collision' });
  const before = session.snapshot(),
    state = session.state;
  const fake = mock.method(globalThis.crypto, 'randomUUID', () => 'collision');
  try {
    assert.throws(
      () => send(session, 'create-node', { layerId: 'art-b', nodeType: 'Value' }),
      /retarget/i,
    );
  } finally {
    fake.mock.restore();
  }
  assert.deepEqual(session.snapshot(), before);
  assert.deepEqual(session.state, state);
  send(session, 'undo');
  assert.equal(read(session.snapshot()), 7);
  send(session, 'delete-layer', { layerId: 'art-b' });
  assert.throws(() => read(session.snapshot()), /missing/i);
  const source = structuredClone(f.doc);
  source.layers = [source.layers[1]];
  send(session, 'import-composition', { sourceText: serialize(source), compositionId: 'art-b' });
  assert.notEqual(session.snapshot().layers[1].id, 'art-b');
  assert.throws(() => read(session.snapshot()), /missing/i);
  send(session, 'undo');
  send(session, 'undo');
  assert.equal(read(session.snapshot()), 7);
});

test('both import routes include dependencies and remap layer, node and owned point IDs', () => {
  const f = fixture(),
    session = new CommandSession(f.doc);
  apply(session, 'ref(artwork, "art-b", "curve", "point.handleA", "length")');
  const source = session.snapshot(),
    destination = structuredClone(source);
  const imported = prepareCompositionImport(destination, serialize(source), 'art-a');
  assert.equal(imported.layers.length, 4);
  const sinkLayer = imported.layers.find(
    (layer, i) => i >= 2 && layer.nodes.some((n) => n.id.startsWith('sink-import-')),
  );
  const sink = sinkLayer.nodes.find((n) => n.id.startsWith('sink-import-'));
  const sourceLayer = imported.layers.find(
    (layer, i) => i >= 2 && layer.nodes.some((n) => n.type === 'Curve'),
  );
  const curve = sourceLayer.nodes.find((n) => n.type === 'Curve');
  assert.match(
    sink.params.value.expression,
    new RegExp(`${sourceLayer.id}.*${curve.id}.*${curve.points[0].id}\\.handleA`),
  );
  assert.equal(evaluate(sinkLayer, {}, {}, imported).parameterValue(sink.id, 'value'), 5);
  const wrapped = migrateLegacyDocumentToCompositionProject(source);
  const wrappedImport = prepareProjectCompositionsImport(wrapped, wrapped).project;
  const composition = wrappedImport.compositions.at(-1),
    copiedSink = composition.layers
      .flatMap((layer) => layer.nodes)
      .find((n) => n.id.startsWith('sink-import-'));
  const copiedSource = composition.layers.find((layer) =>
    layer.nodes.some((n) => n.type === 'Curve'),
  );
  assert.ok(copiedSink.params.value.expression.includes(copiedSource.id));
  assert.equal(wrappedImport.schemaVersion, 62);
});

test('old opaque qualified-looking wrapper text stays opaque and cannot be promoted by import', () => {
  const f = fixture();
  f.doc.schemaVersion = 61;
  f.sink.params.value.expression = 'ref(artwork, "art-b", "driver", "value")';
  assert.throws(
    () => evaluate(f.a, {}, {}, f.doc).parameterValue('sink', 'value'),
    /schemaVersion 62/,
  );
  const old = migrateLegacyDocumentToCompositionProject(f.doc);
  assert.deepEqual(deserializeCompositionProject(serializeCompositionProject(old)), old);
  const empty = migrateLegacyDocumentToCompositionProject(blankDocument());
  const retained = prepareProjectCompositionsImport(empty, old).project;
  assert.equal(retained.schemaVersion, 58);
  assert.equal(
    retained.compositions.at(-1).layers[0].nodes.find((n) => n.id === 'sink').params.value
      .expression,
    f.sink.params.value.expression,
  );
  const colliding = structuredClone(f.doc);
  colliding.layers[0].nodes.find((node) => node.id === 'sink').params.value.expression = '';
  const collisionImport = prepareProjectCompositionsImport(
    migrateLegacyDocumentToCompositionProject(colliding),
    old,
  ).project;
  assert.equal(
    collisionImport.compositions.at(-1).layers[0].nodes.find((n) => n.type === 'Value').params.value
      .expression,
    f.sink.params.value.expression,
  );
  const newSession = new CommandSession(fixture().doc);
  apply(newSession, f.sink.params.value.expression);
  const modern = migrateLegacyDocumentToCompositionProject(newSession.snapshot());
  const before = serializeCompositionProject(modern);
  assert.throws(() => prepareProjectCompositionsImport(modern, old), /opaque|promot/i);
  assert.equal(serializeCompositionProject(modern), before);
});

test('multi Artwork duplicate remaps the selected dependency; single duplicate refuses it', () => {
  const one = duplicationDocument('Circle', false),
    two = duplicationDocument('Circle', false);
  const doc = one.doc;
  doc.layers = [one.layer, two.layer];
  const session = new CommandSession(doc);
  send(session, 'set-parameter-expression', {
    layerId: one.layer.id,
    nodeId: one.generator.id,
    key: 'radius',
    expression: `ref(artwork, "${two.layer.id}", "${two.generator.id}", "radius")`,
  });
  const source = session.snapshot();
  assert.throws(() => stageArtworkDuplicate(source, one.layer.id), /Select every Artwork/);
  const copies = stageArtworkDuplicates(source, [one.layer.id, two.layer.id]);
  const first = copies.find((c) => c.sourceLayerId === one.layer.id).layer;
  const second = copies.find((c) => c.sourceLayerId === two.layer.id).layer;
  const firstCircle = first.nodes.find((n) => n.type === 'Circle'),
    secondCircle = second.nodes.find((n) => n.type === 'Circle');
  assert.ok(firstCircle.params.radius.expression.includes(second.id));
  assert.ok(firstCircle.params.radius.expression.includes(secondCircle.id));
  const before = session.snapshot();
  send(session, 'duplicate-artworks', { layerIds: [one.layer.id, two.layer.id] });
  assert.equal(session.snapshot().layers.length, 4);
  send(session, 'undo');
  assert.deepEqual(session.snapshot(), before);
  send(session, 'redo');
  assert.equal(session.snapshot().layers.length, 4);
});
