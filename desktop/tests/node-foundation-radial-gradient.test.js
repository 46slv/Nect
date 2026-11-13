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
  node,
  outputPorts,
  outputType,
  radialGradientSettings,
  serialize,
} from '../src/core.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { radialGradientDeclaration } from '../src/radial-gradient-fill-declaration.js';
import { evaluateRadialGradient } from '../src/radial-gradient-fill-binding.js';

function fixture() {
  const doc = blankDocument(),
    layer = doc.layers[0],
    output = layer.nodes[0],
    shape = node('Rectangle'),
    fill = node('Fill'),
    color = node('Color'),
    gradient = node('Radial Gradient Fill'),
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
  return { doc, layer, gradient };
}

test('Radial Gradient Fill uses its registered declaration without changing its interface', () => {
  const registered = builtinNodeRegistry.get('Radial Gradient Fill'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Radial Gradient Fill'),
    current = node('Radial Gradient Fill'),
    types = Object.keys(definitions),
    catalog = nodeCatalog.map((entry) => entry.typeId);
  assert.ok(registered && manifest);
  assert.deepEqual(registered.declaration, radialGradientDeclaration);
  assert.equal(manifest.binding.evaluate, evaluateRadialGradient);
  assert.equal(outputType(current), 'PaintedPath');
  assert.deepEqual(outputPorts(current), [{ id: undefined, label: '', type: 'PaintedPath' }]);
  assert.equal(bypassContract(current).input, 'path');
  for (const parameter of radialGradientDeclaration.parameters) {
    assert.equal(definitions[current.type][parameter.id].type, parameter.type);
    assert.deepEqual(current.params[parameter.id].value, parameter.default);
  }
  assert.equal(types[types.indexOf(current.type) - 1], 'Linear Gradient Fill');
  assert.equal(types[types.indexOf(current.type) + 1], 'Angular Gradient Fill');
  assert.equal(catalog[catalog.indexOf(current.type) - 1], 'Linear Gradient Fill');
  assert.equal(catalog[catalog.indexOf(current.type) + 1], 'Halftone');
  const { typeId, ...metadata } = nodeCatalog.find((entry) => entry.typeId === current.type);
  assert.equal(typeId, current.type);
  assert.deepEqual(metadata, radialGradientDeclaration.catalog);
});

test('binding preserves geometry and stroke, and records stops only after valid evaluation', () => {
  const source = {
      contours: [{ id: 'c', points: [{ id: 'p', position: [2, 3] }] }],
      fill: { hex: '#abcdef', alpha: 1 },
      stroke: { hex: '#000000', alpha: 1, width: 2 },
      fingerprint: 'retained',
    },
    before = structuredClone(source),
    values = {
      path: source,
      center: [1, 2],
      radius: 50,
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
  const result = evaluateRadialGradient(undefined, read, context),
    expected = radialGradientSettings((key) => values[key]);
  assert.deepEqual(calls, [
    'path',
    'clone',
    'center',
    'radius',
    'startColor',
    'endColor',
    ['record', 'fill', ['startColor', 'endColor'], expected],
  ]);
  assert.deepEqual(result.fill, expected);
  assert.deepEqual(result.stroke, source.stroke);
  assert.equal(result.fingerprint, source.fingerprint);
  assert.deepEqual(source, before);
  assert.throws(
    () => evaluateRadialGradient(undefined, () => null, context),
    /Connect a PaintedPath input/,
  );
  assert.throws(
    () =>
      evaluateRadialGradient(undefined, () => ({ kind: 'painted-group', children: [] }), context),
    /Edit paint in the source Composition/,
  );
  calls.length = 0;
  values.radius = 0;
  assert.throws(() => evaluateRadialGradient(undefined, read, context), /radius/);
  assert.equal(
    calls.some((call) => Array.isArray(call) && call[0] === 'record'),
    false,
  );
  assert.deepEqual(source, before);
});

test('registered radial gradient keeps stop IDs and stroke through evaluation and history', () => {
  const { doc, layer, gradient } = fixture(),
    saved = serialize(doc),
    first = evaluate(layer, {}, {}, doc),
    result = first.run(gradient.id),
    warm = evaluate(layer, {}, {}, doc);
  assert.deepEqual(warm.run(gradient.id), result);
  assert.ok(warm.stats.reused > 0);
  assert.deepEqual(exportScene(doc)[0].paintProvenance, {
    fill: { startColor: 'first', endColor: 'second' },
    stroke: 'outline',
  });
  assert.equal(serialize(doc), saved);
  const session = new CommandSession(doc),
    send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload }),
    provenance = () => exportScene(session.snapshot())[0].paintProvenance;
  send('bind-color-slot', {
    layerId: layer.id,
    nodeId: gradient.id,
    key: 'endColor',
    slotId: 'first',
  });
  assert.deepEqual(provenance().fill, { startColor: 'first', endColor: 'first' });
  send('undo');
  assert.equal(serialize(session.snapshot()), saved);
  send('set-node-bypassed', { layerId: layer.id, nodeId: gradient.id, bypassed: true });
  assert.equal(provenance().fill, 'base');
  assert.equal(provenance().stroke, 'outline');
  send('undo');
  assert.equal(serialize(session.snapshot()), saved);
  const revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  assert.throws(
    () =>
      send('set-parameter', { layerId: layer.id, nodeId: gradient.id, key: 'radius', value: 0 }),
    /radius/,
  );
  assert.equal(serialize(session.snapshot()), saved);
  assert.equal(session.state.expectedRevision, revision);
  assert.equal(JSON.stringify(session.historyView), history);
  const reopened = deserialize(serialize(session.snapshot()));
  assert.deepEqual(exportScene(reopened)[0].paintProvenance, provenance());
  assert.equal(serialize(reopened), saved);
});
