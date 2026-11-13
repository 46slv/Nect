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
  serialize,
} from '../src/core.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { strokeDeclaration } from '../src/stroke-node-declaration.js';
import { evaluateStroke } from '../src/stroke-node-binding.js';

function fixture() {
  const doc = blankDocument(),
    layer = doc.layers[0],
    output = layer.nodes[0],
    shape = node('Rectangle'),
    gradient = node('Linear Gradient Fill'),
    color = node('Color'),
    stroke = node('Stroke');
  doc.schemaVersion = 54;
  doc.colorSlots = [
    { id: 'first', name: 'First', value: { hex: '#123456', alpha: 0.4 } },
    { id: 'second', name: 'Second', value: { hex: '#123456', alpha: 0.8 } },
    { id: 'outline', name: 'Outline', value: { hex: '#abcdef', alpha: 0.5 } },
  ];
  gradient.params.startColor.colorSlotId = 'first';
  gradient.params.endColor.colorSlotId = 'second';
  color.params.value.colorSlotId = 'outline';
  stroke.strokeCap = 'round';
  stroke.strokeJoin = 'bevel';
  stroke.strokeDash = [12, 3, 2];
  stroke.params.width.value = 9;
  stroke.params.miterLimit.value = 6;
  stroke.params.dashOffset.value = -3;
  layer.nodes = [shape, gradient, color, stroke, output];
  connect(layer, shape.id, gradient.id, 'path');
  connect(layer, gradient.id, stroke.id, 'path');
  connect(layer, color.id, stroke.id, 'color');
  connect(layer, stroke.id, output.id, 'path');
  return { doc, layer, stroke };
}

test('Stroke uses its registered declaration without changing its interface or placement', () => {
  const registered = builtinNodeRegistry.get('Stroke'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Stroke'),
    current = node('Stroke'),
    types = Object.keys(definitions),
    catalog = nodeCatalog.map((entry) => entry.typeId);
  assert.ok(registered && manifest);
  assert.deepEqual(registered.declaration, strokeDeclaration);
  assert.equal(manifest.binding.evaluate, evaluateStroke);
  assert.equal(outputType(current), 'PaintedPath');
  assert.deepEqual(outputPorts(current), [{ id: undefined, label: '', type: 'PaintedPath' }]);
  assert.equal(bypassContract(current).input, 'path');
  for (const parameter of strokeDeclaration.parameters) {
    assert.equal(definitions.Stroke[parameter.id].type, parameter.type);
    assert.deepEqual(current.params[parameter.id].value, parameter.default);
  }
  assert.equal(types[types.indexOf('Stroke') - 1], 'Line Screen');
  assert.equal(types[types.indexOf('Stroke') + 1], 'Output');
  assert.equal(catalog[catalog.indexOf('Stroke') - 1], 'Fill');
  assert.equal(catalog[catalog.indexOf('Stroke') + 1], 'Linear Gradient Fill');
  const { typeId, ...metadata } = nodeCatalog.find((entry) => entry.typeId === 'Stroke');
  assert.equal(typeId, 'Stroke');
  assert.deepEqual(metadata, strokeDeclaration.catalog);
});

test('binding clones the source and validates style before recording the stroke slot', () => {
  const source = {
      contours: [{ id: 'c', points: [{ id: 'p', position: [2, 3] }] }],
      fill: { kind: 'linear-gradient', start: [0, 0], end: [10, 0] },
      stroke: { hex: '#000000', alpha: 1, width: 2 },
      fingerprint: 'retained',
    },
    before = structuredClone(source),
    values = {
      path: source,
      width: 9,
      miterLimit: 6,
      dashOffset: -3,
      color: { hex: '#abcdef', alpha: 0.5 },
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
      strokeStyle() {
        calls.push('style');
        return { cap: 'round', join: 'bevel', dash: [12, 3, 2] };
      },
      hasParameter(key) {
        return Object.hasOwn(values, key);
      },
      recordPaintSlot(value, paint, key) {
        calls.push(['record', paint, key, value.stroke]);
      },
    };
  const expected = {
    hex: '#abcdef',
    alpha: 0.5,
    width: 9,
    cap: 'round',
    join: 'bevel',
    dash: [12, 3, 2],
    miterLimit: 6,
    dashOffset: -3,
  };
  const result = evaluateStroke(undefined, read, context);
  assert.deepEqual(calls, [
    'path',
    'clone',
    'width',
    'style',
    'miterLimit',
    'dashOffset',
    'color',
    ['record', 'stroke', 'color', expected],
  ]);
  assert.deepEqual(result.stroke, expected);
  assert.deepEqual(result.fill, source.fill);
  assert.equal(result.fingerprint, source.fingerprint);
  assert.deepEqual(source, before);
  assert.throws(
    () => evaluateStroke(undefined, () => null, context),
    /Connect a PaintedPath input/,
  );
  assert.throws(
    () => evaluateStroke(undefined, () => ({ kind: 'painted-group', children: [] }), context),
    /Edit paint in the source Composition/,
  );
  calls.length = 0;
  values.width = -1;
  assert.throws(() => evaluateStroke(undefined, read, context), /Stroke width/);
  assert.deepEqual(calls, ['path', 'clone', 'width']);
  values.width = 9;
  values.miterLimit = 0;
  calls.length = 0;
  assert.throws(() => evaluateStroke(undefined, read, context), /Miter limit/);
  assert.equal(
    calls.some((call) => Array.isArray(call) && call[0] === 'record'),
    false,
  );
  assert.deepEqual(source, before);
});

test('registered stroke keeps gradient, identity and style through history and save', () => {
  const { doc, layer, stroke } = fixture(),
    saved = serialize(doc),
    first = evaluate(layer, {}, {}, doc),
    result = first.run(stroke.id),
    warm = evaluate(layer, {}, {}, doc),
    initial = exportScene(doc)[0];
  assert.deepEqual(warm.run(stroke.id), result);
  assert.ok(warm.stats.reused > 0);
  assert.deepEqual(initial.paintProvenance, {
    fill: { startColor: 'first', endColor: 'second' },
    stroke: 'outline',
  });
  assert.deepEqual(initial.value.stroke, {
    hex: '#abcdef',
    alpha: 0.5,
    width: 9,
    cap: 'round',
    join: 'bevel',
    dash: [12, 3, 2],
    miterLimit: 6,
    dashOffset: -3,
  });
  assert.equal(serialize(doc), saved);

  const session = new CommandSession(doc),
    send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload }),
    scene = () => exportScene(session.snapshot())[0];
  send('set-node-settings', {
    layerId: layer.id,
    nodeId: stroke.id,
    settings: { strokeCap: 'square', strokeJoin: 'miter', strokeDash: [5, 4] },
  });
  const edited = serialize(session.snapshot());
  assert.equal(scene().value.stroke.cap, 'square');
  assert.deepEqual(scene().value.stroke.dash, [5, 4]);
  assert.deepEqual(scene().paintProvenance, initial.paintProvenance);
  send('undo');
  assert.equal(serialize(session.snapshot()), saved);
  send('redo');
  assert.equal(serialize(session.snapshot()), edited);
  const revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  assert.throws(
    () => send('set-parameter', { layerId: layer.id, nodeId: stroke.id, key: 'width', value: -1 }),
    /Stroke width/,
  );
  assert.equal(serialize(session.snapshot()), edited);
  assert.equal(session.state.expectedRevision, revision);
  assert.equal(JSON.stringify(session.historyView), history);
  const reopened = deserialize(edited);
  assert.deepEqual(exportScene(reopened)[0].paintProvenance, initial.paintProvenance);
  assert.deepEqual(exportScene(reopened)[0].value.stroke, scene().value.stroke);
  send('set-node-bypassed', { layerId: layer.id, nodeId: stroke.id, bypassed: true });
  assert.equal(scene().paintProvenance.stroke, undefined);
  assert.deepEqual(scene().paintProvenance.fill, initial.paintProvenance.fill);
});
