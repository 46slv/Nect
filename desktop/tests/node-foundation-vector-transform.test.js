import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  bypassContract,
  CommandSession,
  connect,
  definitions,
  deserialize,
  evaluate,
  node,
  outputPorts,
  outputType,
  parameter,
  serialize,
} from '../src/core.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { vectorTransformDeclaration } from '../src/vector-transform-declaration.js';
import { evaluateVectorTransform } from '../src/vector-transform-binding.js';
import { createNodeRegistry, parseNodeDeclaration } from '../src/node-foundation.js';
import { nectNodeHost } from '../src/nect-node-host.js';

test('Vector Transform resolves through its trusted binding and retains its interface', () => {
  const registered = builtinNodeRegistry.get('Vector Transform'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Vector Transform'),
    parsed = parseNodeDeclaration(JSON.stringify(vectorTransformDeclaration)),
    resolved = createNodeRegistry(
      [parsed],
      new Map([['builtin.vector-transform.v1', { evaluate: evaluateVectorTransform }]]),
      nectNodeHost,
    ).get('Vector Transform'),
    n = node('Vector Transform');
  assert.equal(manifest.binding.evaluate, evaluateVectorTransform);
  assert.deepEqual(registered.declaration, vectorTransformDeclaration);
  assert.ok(Object.isFrozen(registered.declaration));
  assert.equal(resolved.declaration.bypassInput, 'vector');
  assert.deepEqual(n.params.vector, {
    type: 'Vector2',
    value: [0, 0],
    expression: '',
    locked: false,
  });
  assert.deepEqual(Object.keys(n.params), ['vector', 'anchor', 'offset', 'scale', 'rotation']);
  assert.deepEqual(outputPorts(n), [{ id: undefined, label: '', type: 'Vector2' }]);
  assert.equal(outputType(n), 'Vector2');
  assert.equal(bypassContract(n).input, 'vector');
  const defs = Object.keys(definitions),
    catalog = nodeCatalog.map(({ typeId }) => typeId);
  assert.equal(defs[defs.indexOf(n.type) - 1], 'Transform');
  assert.equal(defs[defs.indexOf(n.type) + 1], 'Smooth Path');
  assert.equal(catalog[catalog.indexOf(n.type) - 1], 'Vector Math');
  assert.equal(catalog[catalog.indexOf(n.type) + 1], 'Separate Vector');
  const { typeId, ...metadata } = nodeCatalog.find((item) => item.typeId === n.type);
  assert.equal(typeId, n.type);
  assert.deepEqual(metadata, vectorTransformDeclaration.catalog);
});

test('Vector Transform binding reads only applicable optional controls', () => {
  const values = {
      vector: [11, 13],
      anchor: [2, 3],
      offset: [10, 20],
      scale: 2,
      rotation: 90,
      position: [100, 200],
      uniformScale: true,
      scaleWidth: 3,
      skew: 15,
      skewAxis: 10,
    },
    present = new Set(['anchor', 'position', 'uniformScale', 'scaleWidth', 'skew', 'skewAxis']),
    reads = [];
  evaluateVectorTransform(
    undefined,
    (key) => {
      reads.push(key);
      if (key === 'scaleWidth') throw Error('inactive control was read');
      return values[key];
    },
    { hasParameter: (key) => present.has(key) },
  );
  assert.deepEqual(reads, [
    'anchor',
    'scale',
    'offset',
    'rotation',
    'position',
    'uniformScale',
    'skew',
    'skewAxis',
    'vector',
  ]);
});

function fixture() {
  const doc = blankDocument(),
    layer = doc.layers[0],
    transform = node('Vector Transform');
  transform.id = 'vector-transform';
  transform.params.vector.value = [11, 13];
  layer.nodes.push(transform);
  return { doc, layer, transform };
}

test('Vector Transform preserves affine and optional control behavior', () => {
  const { layer, transform: n } = fixture(),
    run = () => evaluate(layer).run(n.id);
  assert.deepEqual(run(), [11, 13]);
  n.params.anchor.value = [2, 3];
  n.params.offset.value = [10, 20];
  n.params.scale.value = 2;
  n.params.rotation.value = 90;
  assert.deepEqual(run(), [-8, 41]);
  n.params.position = { ...parameter(n, 'position'), value: [100, 200] };
  n.params.uniformScale = { ...parameter(n, 'uniformScale'), value: false };
  n.params.scaleWidth = { ...parameter(n, 'scaleWidth'), value: 3 };
  n.params.skew = { ...parameter(n, 'skew'), value: 15 };
  n.params.skewAxis = { ...parameter(n, 'skewAxis'), value: 10 };
  assert.deepEqual(run(), [90.69828947895918, 243.0398035731485]);
  delete n.params.scaleWidth;
  assert.deepEqual(run(), [90.77100638997042, 233.6274054777517]);
  n.params.uniformScale.value = true;
  n.params.scaleWidth = { ...parameter(n, 'scaleWidth'), value: 3, expression: '1/0' };
  assert.deepEqual(run(), [90.77100638997042, 233.6274054777517]);
  n.params.uniformScale.value = false;
  assert.throws(run, /Expression must produce a finite number/);
});

test('Vector Transform keeps Session connections, atomic failures, bypass and history', () => {
  const { doc, layer, transform } = fixture(),
    vector = node('Vector');
  vector.id = 'vector-source';
  layer.nodes.push(vector);
  connect(layer, vector.id, transform.id, 'vector');
  const session = new CommandSession(doc),
    send = (type, payload = {}) =>
      session.dispatch({
        ...session.state,
        type,
        payload: { layerId: layer.id, nodeId: transform.id, ...payload },
      }),
    result = () => evaluate(session.snapshot().layers[0]).run(transform.id),
    before = serialize(session.snapshot());
  send('set-transform-control', { key: 'rotation', value: 90 });
  const rotated = serialize(session.snapshot()),
    rotatedResult = result();
  assert.notDeepEqual(rotatedResult, evaluate(layer).run(transform.id));
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  const revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  assert.throws(() => send('set-transform-control', { key: 'skew', value: 90 }), /Skew/);
  assert.equal(serialize(session.snapshot()), before);
  assert.equal(session.state.expectedRevision, revision);
  assert.equal(JSON.stringify(session.historyView), history);
  send('redo');
  assert.equal(serialize(session.snapshot()), rotated);
  assert.deepEqual(result(), rotatedResult);
  send('set-node-bypassed', { bypassed: true });
  const bypassed = serialize(session.snapshot()),
    evaluation = evaluate(session.snapshot().layers[0]);
  assert.deepEqual(evaluation.run(transform.id), evaluation.run(vector.id));
  send('undo');
  assert.equal(serialize(session.snapshot()), rotated);
  send('redo');
  assert.equal(serialize(session.snapshot()), bypassed);
  const reopened = deserialize(bypassed);
  assert.equal(serialize(reopened), bypassed);
  assert.equal(
    reopened.layers[0].nodes.find((n) => n.id === transform.id).params.vector.source,
    vector.id,
  );
});

test('optional transform controls remain Session-owned across save and history', () => {
  const { doc, layer, transform } = fixture(),
    session = new CommandSession(doc),
    send = (type, payload = {}) =>
      session.dispatch({
        ...session.state,
        type,
        payload: { layerId: layer.id, nodeId: transform.id, ...payload },
      });
  send('set-transform-control', { key: 'position', value: [100, 200] });
  send('set-transform-control', { key: 'uniformScale', value: false });
  send('set-transform-control', { key: 'scaleWidth', value: 3 });
  send('set-transform-control', { key: 'skewAxis', value: 10 });
  const beforeSkew = serialize(session.snapshot());
  send('set-transform-control', { key: 'skew', value: 15 });
  const edited = serialize(session.snapshot()),
    expected = evaluate(session.snapshot().layers[0]).run(transform.id);
  assert.notDeepEqual(expected, [11, 13]);
  send('undo');
  assert.equal(serialize(session.snapshot()), beforeSkew);
  send('redo');
  assert.equal(serialize(session.snapshot()), edited);
  const reopened = deserialize(edited);
  assert.equal(serialize(reopened), edited);
  assert.deepEqual(evaluate(reopened.layers[0]).run(transform.id), expected);
});

test('overflow stays out of the evaluation cache and valid controls or bypass recover', () => {
  const { doc, layer, transform } = fixture(),
    session = new CommandSession(doc),
    send = (type, payload = {}) =>
      session.dispatch({
        ...session.state,
        type,
        payload: { layerId: layer.id, nodeId: transform.id, ...payload },
      });
  send('set-parameter', { key: 'scale', value: 2 });
  send('set-parameter', { key: 'vector', value: [1e308, 1e308] });
  const authored = serialize(session.snapshot()),
    overflowLayer = session.snapshot().layers[0],
    first = evaluate(overflowLayer);
  assert.throws(() => first.run(transform.id), /Expected Vector2/);
  assert.equal(first.values.has(transform.id), false);
  assert.throws(() => first.run(transform.id), /Expected Vector2/);
  const next = evaluate(overflowLayer);
  assert.throws(() => next.run(transform.id), /Expected Vector2/);
  assert.equal(next.values.has(transform.id), false);
  const reopened = deserialize(authored);
  assert.equal(serialize(reopened), authored);
  assert.deepEqual(
    reopened.layers[0].nodes.find((item) => item.id === transform.id).params.vector.value,
    [1e308, 1e308],
  );
  send('set-parameter', { key: 'scale', value: 1 });
  assert.deepEqual(evaluate(session.snapshot().layers[0]).run(transform.id), [1e308, 1e308]);
  send('undo');
  assert.equal(serialize(session.snapshot()), authored);
  send('set-node-bypassed', { bypassed: true });
  assert.deepEqual(evaluate(session.snapshot().layers[0]).run(transform.id), [1e308, 1e308]);
});
