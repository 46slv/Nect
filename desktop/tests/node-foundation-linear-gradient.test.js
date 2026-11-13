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
  exportScene,
  linearGradientSettings,
  node,
  outputPorts,
  outputType,
  serialize,
} from '../src/core.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { linearGradientDeclaration } from '../src/linear-gradient-fill-declaration.js';
import { evaluateLinearGradient } from '../src/linear-gradient-fill-binding.js';

function fixture() {
  const doc = blankDocument(),
    layer = doc.layers[0],
    output = layer.nodes[0],
    shape = node('Rectangle'),
    fill = node('Fill'),
    color = node('Color'),
    gradient = node('Linear Gradient Fill'),
    stroke = node('Stroke');
  doc.schemaVersion = 54;
  doc.colorSlots = [
    { id: 'base', name: 'Base', value: { hex: '#111111', alpha: 1 } },
    { id: 'first', name: 'First', value: { hex: '#123456', alpha: 0.4 } },
    { id: 'second', name: 'Second', value: { hex: '#123456', alpha: 0.8 } },
    { id: 'outline', name: 'Outline', value: { hex: '#abcdef', alpha: 0.5 } },
  ];
  fill.params.color.colorSlotId = 'base';
  color.params.value.colorSlotId = 'first';
  gradient.params.endColor.colorSlotId = 'second';
  stroke.params.color.colorSlotId = 'outline';
  layer.nodes = [shape, fill, color, gradient, stroke, output];
  connect(layer, shape.id, fill.id, 'path');
  connect(layer, fill.id, gradient.id, 'path');
  connect(layer, color.id, gradient.id, 'startColor');
  connect(layer, gradient.id, stroke.id, 'path');
  connect(layer, stroke.id, output.id, 'path');
  return { doc, layer, shape, fill, gradient, stroke };
}

test('Linear Gradient Fill uses its registered declaration without changing its interface', () => {
  const registered = builtinNodeRegistry.get('Linear Gradient Fill'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Linear Gradient Fill'),
    current = node('Linear Gradient Fill'),
    types = Object.keys(definitions),
    catalog = nodeCatalog.map((entry) => entry.typeId);
  assert.ok(registered && manifest);
  assert.deepEqual(registered.declaration, linearGradientDeclaration);
  assert.equal(manifest.binding.evaluate, evaluateLinearGradient);
  assert.equal(outputType(current), 'PaintedPath');
  assert.deepEqual(outputPorts(current), [{ id: undefined, label: '', type: 'PaintedPath' }]);
  assert.equal(bypassContract(current).input, 'path');
  for (const parameter of linearGradientDeclaration.parameters) {
    assert.equal(definitions[current.type][parameter.id].type, parameter.type);
    assert.deepEqual(current.params[parameter.id].value, parameter.default);
  }
  assert.equal(types[types.indexOf(current.type) - 1], 'Fill');
  assert.equal(types[types.indexOf(current.type) + 1], 'Radial Gradient Fill');
  assert.equal(catalog[catalog.indexOf(current.type) - 1], 'Stroke');
  assert.equal(catalog[catalog.indexOf(current.type) + 1], 'Radial Gradient Fill');
  const { typeId, ...metadata } = nodeCatalog.find((entry) => entry.typeId === current.type);
  assert.equal(typeId, current.type);
  assert.deepEqual(metadata, linearGradientDeclaration.catalog);
});

test('binding clones geometry, preserves stroke and records stops only after valid evaluation', () => {
  const source = {
      contours: [{ id: 'c', points: [{ id: 'p', position: [2, 3] }] }],
      fill: { hex: '#abcdef', alpha: 1 },
      stroke: { hex: '#000000', alpha: 1, width: 2 },
      fingerprint: 'retained',
    },
    before = structuredClone(source),
    values = {
      path: source,
      start: [1, 2],
      end: [4, 6],
      startColor: { hex: '#123456', alpha: 0.4 },
      endColor: { hex: '#123456', alpha: 0.8 },
    },
    calls = [],
    read = (key) => {
      calls.push(key);
      return values[key];
    },
    context = {
      cloneEvaluated(value) {
        calls.push('clone');
        return structuredClone(value);
      },
      recordPaintStops(value, paint, keys) {
        calls.push(['record', paint, keys, value.fill]);
      },
    };
  const result = evaluateLinearGradient(undefined, read, context),
    expected = linearGradientSettings((key) => values[key]);
  assert.deepEqual(calls, [
    'path',
    'clone',
    'start',
    'end',
    'startColor',
    'endColor',
    ['record', 'fill', ['startColor', 'endColor'], expected],
  ]);
  assert.deepEqual(result.fill, expected);
  assert.deepEqual(result.stroke, source.stroke);
  assert.equal(result.fingerprint, source.fingerprint);
  assert.deepEqual(source, before);
  calls.length = 0;
  assert.throws(
    () => evaluateLinearGradient(undefined, () => null, context),
    /Connect a PaintedPath input/,
  );
  assert.throws(
    () =>
      evaluateLinearGradient(undefined, () => ({ kind: 'painted-group', children: [] }), context),
    /Edit paint in the source Composition/,
  );
  calls.length = 0;
  values.end = values.start;
  assert.throws(() => evaluateLinearGradient(undefined, read, context), /finite, nonzero/);
  assert.equal(
    calls.some((call) => Array.isArray(call) && call[0] === 'record'),
    false,
  );
  assert.deepEqual(source, before);
});

test('registered gradient preserves distinct same-color stop IDs and stroke through warm evaluation', () => {
  const { doc, layer, gradient } = fixture(),
    saved = serialize(doc),
    first = evaluate(layer, {}, {}, doc),
    result = first.run(gradient.id),
    warm = evaluate(layer, {}, {}, doc),
    scene = exportScene(doc)[0];
  assert.deepEqual(warm.run(gradient.id), result);
  assert.ok(warm.stats.reused > 0);
  assert.deepEqual(scene.paintProvenance, {
    fill: { startColor: 'first', endColor: 'second' },
    stroke: 'outline',
  });
  assert.equal(scene.value.fill.startColor.hex, scene.value.fill.endColor.hex);
  assert.equal(serialize(doc), saved);
});

test('stop identity, bypass, failed edit, Undo/Redo and save use the common Session', () => {
  const { doc, layer, gradient } = fixture(),
    session = new CommandSession(doc),
    send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload }),
    provenance = () => exportScene(session.snapshot())[0].paintProvenance,
    original = serialize(session.snapshot());
  send('bind-color-slot', {
    layerId: layer.id,
    nodeId: gradient.id,
    key: 'endColor',
    slotId: 'first',
  });
  assert.deepEqual(provenance().fill, { startColor: 'first', endColor: 'first' });
  send('undo');
  assert.equal(serialize(session.snapshot()), original);
  send('set-node-bypassed', { layerId: layer.id, nodeId: gradient.id, bypassed: true });
  const bypassed = serialize(session.snapshot());
  assert.equal(provenance().fill, 'base');
  assert.equal(provenance().stroke, 'outline');
  send('undo');
  assert.equal(serialize(session.snapshot()), original);
  send('redo');
  assert.equal(serialize(session.snapshot()), bypassed);
  send('undo');
  const revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  assert.throws(
    () =>
      send('set-parameter', {
        layerId: layer.id,
        nodeId: gradient.id,
        key: 'start',
        value: [500, 250],
      }),
    /finite, nonzero/,
  );
  assert.equal(serialize(session.snapshot()), original);
  assert.equal(session.state.expectedRevision, revision);
  assert.equal(JSON.stringify(session.historyView), history);
  const reopened = deserialize(serialize(session.snapshot()));
  assert.deepEqual(exportScene(reopened)[0].paintProvenance, provenance());
  assert.equal(serialize(reopened), original);
});
