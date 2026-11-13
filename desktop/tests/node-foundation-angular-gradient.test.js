import test from 'node:test';
import assert from 'node:assert/strict';
import {
  angularGradientSettings,
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
  serialize,
} from '../src/core.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { angularGradientDeclaration } from '../src/angular-gradient-fill-declaration.js';
import { evaluateAngularGradient } from '../src/angular-gradient-fill-binding.js';

function fixture() {
  const doc = blankDocument(),
    layer = doc.layers[0],
    output = layer.nodes[0],
    shape = node('Rectangle'),
    fill = node('Fill'),
    color = node('Color'),
    gradient = node('Angular Gradient Fill'),
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
  gradient.params.color1.colorSlotId = 'second';
  gradient.params.color2.colorSlotId = 'first';
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

test('Angular Gradient Fill uses its registered declaration and retains the first catalog position', () => {
  const registered = builtinNodeRegistry.get('Angular Gradient Fill'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Angular Gradient Fill'),
    current = node('Angular Gradient Fill'),
    types = Object.keys(definitions),
    catalog = nodeCatalog.map((entry) => entry.typeId);
  assert.ok(registered && manifest);
  assert.deepEqual(registered.declaration, angularGradientDeclaration);
  assert.equal(manifest.binding.evaluate, evaluateAngularGradient);
  assert.equal(outputType(current), 'PaintedPath');
  assert.deepEqual(outputPorts(current), [{ id: undefined, label: '', type: 'PaintedPath' }]);
  assert.equal(bypassContract(current).input, 'path');
  for (const parameter of angularGradientDeclaration.parameters) {
    assert.equal(definitions[current.type][parameter.id].type, parameter.type);
    assert.deepEqual(current.params[parameter.id].value, parameter.default);
  }
  assert.equal(types[types.indexOf(current.type) - 1], 'Radial Gradient Fill');
  assert.equal(types[types.indexOf(current.type) + 1], 'Halftone');
  assert.deepEqual(catalog.slice(0, 3), [
    'Angular Gradient Fill',
    'Polar Transform',
    'Luminance to Points',
  ]);
  const { typeId, ...metadata } = nodeCatalog[0];
  assert.equal(typeId, current.type);
  assert.deepEqual(metadata, angularGradientDeclaration.catalog);
});

test('binding clones geometry and records four stops only after successful validation', () => {
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
      angle: -90,
      clockwise: true,
      startColor: { hex: '#123456', alpha: 0.4 },
      color1: { hex: '#123456', alpha: 0.8 },
      position1: 0.25,
      color2: { hex: '#123456', alpha: 0.4 },
      position2: 0.75,
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
  const result = evaluateAngularGradient(undefined, read, context),
    expected = angularGradientSettings((key) => values[key]);
  assert.deepEqual(calls, [
    'path',
    'clone',
    'center',
    'angle',
    'clockwise',
    'startColor',
    'color1',
    'position1',
    'color2',
    'position2',
    'endColor',
    ['record', 'fill', ['startColor', 'color1', 'color2', 'endColor'], expected],
  ]);
  assert.deepEqual(result.fill, expected);
  assert.deepEqual(result.stroke, source.stroke);
  assert.equal(result.fingerprint, source.fingerprint);
  assert.deepEqual(source, before);
  assert.throws(
    () => evaluateAngularGradient(undefined, () => null, context),
    /Connect a PaintedPath input/,
  );
  assert.throws(
    () =>
      evaluateAngularGradient(undefined, () => ({ kind: 'painted-group', children: [] }), context),
    /Edit paint in the source Composition/,
  );
  calls.length = 0;
  values.position1 = values.position2;
  assert.throws(() => evaluateAngularGradient(undefined, read, context), /stop positions/);
  assert.equal(
    calls.some((call) => Array.isArray(call) && call[0] === 'record'),
    false,
  );
  assert.deepEqual(source, before);
});

test('registered gradient retains four identities through cache, history and save', () => {
  const { doc, layer, gradient } = fixture(),
    saved = serialize(doc),
    first = evaluate(layer, {}, {}, doc),
    result = first.run(gradient.id),
    warm = evaluate(layer, {}, {}, doc),
    scene = exportScene(doc)[0];
  assert.deepEqual(warm.run(gradient.id), result);
  assert.ok(warm.stats.reused > 0);
  assert.deepEqual(scene.paintProvenance, {
    fill: { startColor: 'first', color1: 'second', color2: 'first', endColor: 'second' },
    stroke: 'outline',
  });
  assert.equal(scene.value.fill.startColor.hex, scene.value.fill.color1.hex);
  assert.equal(serialize(doc), saved);

  const session = new CommandSession(doc),
    send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload }),
    provenance = () => exportScene(session.snapshot())[0].paintProvenance;
  send('bind-color-slot', {
    layerId: layer.id,
    nodeId: gradient.id,
    key: 'color1',
    slotId: 'first',
  });
  assert.equal(provenance().fill.color1, 'first');
  send('undo');
  assert.equal(serialize(session.snapshot()), saved);
  send('set-node-bypassed', { layerId: layer.id, nodeId: gradient.id, bypassed: true });
  const bypassed = serialize(session.snapshot());
  assert.equal(provenance().fill, 'base');
  assert.equal(provenance().stroke, 'outline');
  send('undo');
  assert.equal(serialize(session.snapshot()), saved);
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
        key: 'position1',
        value: 0.9,
      }),
    /stop positions/,
  );
  assert.equal(serialize(session.snapshot()), saved);
  assert.equal(session.state.expectedRevision, revision);
  assert.equal(JSON.stringify(session.historyView), history);
  const reopened = deserialize(serialize(session.snapshot()));
  assert.deepEqual(exportScene(reopened)[0].paintProvenance, provenance());
  assert.equal(serialize(reopened), saved);
});
