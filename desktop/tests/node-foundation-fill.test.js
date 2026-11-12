import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  node,
  connect,
  activeParameters,
  bypassContract,
  enableRemapPoint,
  evaluate,
  exportScene,
  CommandSession,
  serialize,
  deserialize,
  definitions,
  outputPorts,
  outputType,
} from '../src/core.js';
import { createNodeRegistry } from '../src/node-foundation.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nectNodeHost } from '../src/nect-node-host.js';
import { fillDeclaration } from '../src/fill-node-declaration.js';
import { evaluateFill } from '../src/fill-node-binding.js';
import { colorCoveragePasses } from '../src/color-coverage-passes.js';

const color = { hex: '#123456', alpha: 0.5 };

test('production Fill uses its trusted declaration and existing painted interface', () => {
  const registered = builtinNodeRegistry.get('Fill'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Fill'),
    current = node('Fill'),
    other = node('Fill');
  assert.ok(registered && manifest);
  assert.equal(manifest.binding.evaluate, evaluateFill);
  assert.deepEqual(registered.declaration, fillDeclaration);
  assert.deepEqual(manifest.placement, { definitionAfter: 'Expression', catalogAfter: 'Color' });
  assert.deepEqual(
    activeParameters(current).map(([id]) => id),
    ['path', 'color'],
  );
  assert.deepEqual(outputPorts(current), [{ id: undefined, label: '', type: 'PaintedPath' }]);
  assert.equal(outputType(current), 'PaintedPath');
  assert.equal(outputType(current, 'default'), undefined);
  assert.equal(bypassContract(current).supported, true);
  current.params.color.value.alpha = 0;
  assert.equal(other.params.color.value.alpha, 1);
  assert.equal(definitions.Fill.color.value.alpha, 1);
});

function sceneFixture() {
  const doc = blankDocument(),
    layer = doc.layers[0],
    output = layer.nodes[0],
    shape = node('Rectangle'),
    stroke = node('Stroke'),
    fill = node('Fill'),
    first = node('Color'),
    second = node('Color');
  doc.schemaVersion = 54;
  doc.colorSlots = ['first', 'second', 'stroke'].map((id) => ({ id, name: id, value: color }));
  first.params.value.colorSlotId = 'first';
  second.params.value.colorSlotId = 'second';
  stroke.params.color.colorSlotId = 'stroke';
  layer.nodes = [shape, stroke, fill, first, second, output];
  connect(layer, shape.id, stroke.id, 'path');
  connect(layer, stroke.id, fill.id, 'path');
  connect(layer, first.id, fill.id, 'color');
  connect(layer, fill.id, output.id, 'path');
  return { doc, layer, output, shape, stroke, fill, first, second };
}

function sessionState(session) {
  return {
    document: serialize(session.snapshot()),
    state: session.state,
    past: structuredClone(session.historyView.past),
    future: structuredClone(session.historyView.future),
  };
}

test('Fill declaration preserves the current interface, defaults and catalog', () => {
  const registry = createNodeRegistry(
    [fillDeclaration],
    new Map([['builtin.fill.v1', { evaluate: evaluateFill }]]),
    nectNodeHost,
  );
  const resolved = registry.get('Fill');
  const existing = node('Fill');
  assert.deepEqual(
    fillDeclaration.parameters.map(({ id, type, default: value }) => [id, type, value]),
    Object.entries(definitions.Fill).map(([id, parameter]) => [
      id,
      parameter.type,
      parameter.value,
    ]),
  );
  assert.deepEqual(Object.keys(existing.params), ['path', 'color']);
  assert.equal(resolved.declaration.resultType, 'PaintedPath');
  assert.equal(resolved.declaration.bypassInput, 'path');
  assert.equal(resolved.declaration.catalog.labelJa, '塗り');
  assert.ok(Object.isFrozen(resolved.declaration));
});

test('binding clones before painting, records only after a successful color read, and keeps identity', () => {
  const source = {
      contours: [{ id: 'c', points: [{ id: 'p', position: [2, 3] }] }],
      fill: { hex: '#abcdef', alpha: 1 },
      stroke: { hex: '#000000', alpha: 1, width: 2 },
      fingerprint: 'retained',
    },
    before = structuredClone(source),
    calls = [],
    read = (key) => {
      calls.push(key);
      return key === 'path' ? source : color;
    },
    context = {
      cloneEvaluated(value) {
        calls.push('clone');
        return structuredClone(value);
      },
      recordPaintSlot(value, paint, key) {
        calls.push(['record', paint, key, value.fill]);
      },
    };
  const result = evaluateFill(undefined, read, context);
  assert.deepEqual(calls, ['path', 'clone', 'color', ['record', 'fill', 'color', color]]);
  assert.notEqual(result, source);
  assert.deepEqual(source, before);
  assert.deepEqual(result.fill, color);
  assert.deepEqual(result.stroke, source.stroke);
  assert.equal(result.fingerprint, source.fingerprint);
  assert.deepEqual(result.contours, source.contours);

  calls.length = 0;
  assert.throws(() => evaluateFill(undefined, () => null, context), /Connect a PaintedPath input/);
  assert.deepEqual(calls, []);
  const group = { kind: 'painted-group', children: [] };
  assert.throws(
    () => evaluateFill(undefined, (key) => (key === 'path' ? group : color), context),
    /Edit paint in the source Composition/,
  );
  assert.deepEqual(calls, ['clone']);
  calls.length = 0;
  assert.throws(
    () =>
      evaluateFill(
        undefined,
        (key) => {
          if (key === 'path') return source;
          throw Error('Expected Color');
        },
        context,
      ),
    /Expected Color/,
  );
  assert.deepEqual(calls, ['clone']);
  assert.deepEqual(source, before);
});

test('equal-color source rewiring preserves distinct slot identities, stroke, history and save', () => {
  const { doc, layer, fill, first, second } = sceneFixture(),
    unusedFill = node('Fill');
  layer.nodes.push(unusedFill);
  const session = new CommandSession(doc),
    send = (type, payload = {}) =>
      session.dispatch({ ...session.state, type, payload: { layerId: layer.id, ...payload } }),
    scene = () => exportScene(session.snapshot())[0];
  const original = serialize(session.snapshot()),
    before = scene(),
    geometry = structuredClone(before.value.contours);
  assert.deepEqual(before.paintProvenance, { stroke: 'stroke', fill: 'first' });
  send('connect', { sourceId: second.id, nodeId: fill.id, key: 'color' });
  const rewired = serialize(session.snapshot()),
    after = scene();
  assert.deepEqual(after.value.fill, before.value.fill);
  assert.deepEqual(after.value.stroke, before.value.stroke);
  assert.deepEqual(after.value.contours, geometry);
  assert.deepEqual(after.paintProvenance, { stroke: 'stroke', fill: 'second' });
  assert.equal(
    session.snapshot().layers[0].nodes.find((n) => n.id === first.id).params.value.colorSlotId,
    'first',
  );
  send('undo');
  assert.equal(serialize(session.snapshot()), original);
  assert.deepEqual(scene().paintProvenance, { stroke: 'stroke', fill: 'first' });
  const redoPending = sessionState(session);
  assert.throws(
    () =>
      send('set-parameter', {
        nodeId: unusedFill.id,
        key: 'color',
        value: { hex: 'bad', alpha: 2 },
      }),
    /Expected Color/,
  );
  assert.deepEqual(sessionState(session), redoPending);
  send('redo');
  assert.equal(serialize(session.snapshot()), rewired);
  assert.deepEqual(exportScene(deserialize(rewired))[0].paintProvenance, {
    stroke: 'stroke',
    fill: 'second',
  });
  send('disconnect', { nodeId: fill.id, key: 'color' });
  send('set-parameter', { nodeId: fill.id, key: 'color', value: color });
  assert.deepEqual(scene().value.fill, before.value.fill);
  assert.deepEqual(scene().paintProvenance, { stroke: 'stroke', fill: null });
});

test('null fallback follows live layer fill and a connected Color source takes precedence', () => {
  const { doc, layer, fill, first } = sceneFixture();
  delete fill.params.color.source;
  fill.params.color.value = null;
  layer.fill = '#112233';
  const run = () => evaluate(layer, {}, doc.imageAssets, doc);
  assert.deepEqual(run().run(fill.id).fill, { hex: '#112233', alpha: 1 });
  assert.equal(run().stats.computed, 0);
  layer.fill = '#445566';
  assert.deepEqual(run().run(fill.id).fill, { hex: '#445566', alpha: 1 });
  connect(layer, first.id, fill.id, 'color');
  assert.deepEqual(run().run(fill.id).fill, color);
  delete fill.params.color.source;
  assert.deepEqual(run().run(fill.id).fill, { hex: '#445566', alpha: 1 });
  assert.equal(
    deserialize(serialize(doc)).layers[0].nodes.find((n) => n.id === fill.id).params.color.value,
    null,
  );
});

test('a painted group rejects Fill before reading invalid color; bypass retains its input', () => {
  const { doc, layer, output, fill } = sceneFixture(),
    join = node('Join Geometry'),
    outer = node('Fill');
  layer.nodes.push(join, outer);
  connect(layer, fill.id, join.id, 'a');
  connect(layer, join.id, outer.id, 'path');
  connect(layer, outer.id, output.id, 'path');
  outer.params.color.value = { hex: 'bad', alpha: 2 };
  const before = JSON.stringify(doc),
    run = () => evaluate(layer, {}, doc.imageAssets, doc);
  assert.match(run().error, /Edit paint in the source Composition/);
  assert.equal(JSON.stringify(doc), before);
  outer.bypassed = true;
  const bypassed = run();
  assert.equal(bypassed.error, null);
  assert.deepEqual(bypassed.path, bypassed.run(join.id));
  assert.equal(bypassed.path.kind, 'painted-group');
});

test('Fill color and bypass preserve the current Convert to Path and Remap history semantics', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    output = layer.nodes[0],
    source = node('Imported Path'),
    fill = node('Fill'),
    convert = node('Convert to Path'),
    remap = node('Path Remap'),
    final = node('Fill');
  source.rawPath = 'M0 0 L100 0 L100 100 L0 100 Z';
  for (const [item, id] of [
    [source, 'shape'],
    [fill, 'fill'],
    [convert, 'convert'],
    [remap, 'remap'],
    [final, 'final'],
  ])
    item.id = id;
  layer.nodes.push(source, fill, convert, remap, final);
  for (const [from, to] of [
    [source, fill],
    [fill, convert],
    [convert, remap],
    [remap, final],
    [final, output],
  ])
    connect(layer, from.id, to.id, 'path');
  const initial = evaluate(layer, {}, doc.imageAssets, doc),
    converted = initial.run(convert.id),
    point = converted.contours[0].points[0];
  assert.equal(
    converted.fingerprint,
    'sha256:443cfef11556ae8b1c1dcab526d1fdb7a2382897335fb42e16ba4fa9a766bdf0',
  );
  assert.equal(point.id, 'shape:c0:p0');
  enableRemapPoint(remap, converted, point.id).params.position.value = [3, 1];
  const session = new CommandSession(deserialize(serialize(doc))),
    send = (type, payload = {}) =>
      session.dispatch({ ...session.state, type, payload: { layerId: layer.id, ...payload } }),
    read = () => {
      const snapshot = session.snapshot(),
        evaluation = evaluate(snapshot.layers[0], {}, snapshot.imageAssets, snapshot),
        path = evaluation.run(remap.id);
      return {
        error: evaluation.error,
        warnings: evaluation.warnings,
        fingerprint: evaluation.run(convert.id).fingerprint,
        point: path.contours[0].points[0],
      };
    },
    original = serialize(session.snapshot());
  const active = read();
  assert.equal(active.error, null);
  assert.deepEqual(active.warnings, []);
  assert.deepEqual(active.point.position, [3, 1]);
  send('set-parameter', { nodeId: fill.id, key: 'color', value: color });
  const recolored = serialize(session.snapshot());
  assert.deepEqual(read(), active);
  send('undo');
  assert.equal(serialize(session.snapshot()), original);
  send('redo');
  assert.equal(serialize(session.snapshot()), recolored);
  assert.deepEqual(read(), active);
  send('undo');
  send('set-node-bypassed', { nodeId: fill.id, bypassed: true });
  const bypassed = serialize(session.snapshot()),
    suspended = read();
  assert.equal(
    suspended.fingerprint,
    'sha256:3f977fcc5455f4d32b4c13c1f61895f59a176164cd7074b5944d6c353b3dd567',
  );
  assert.equal(suspended.point.id, point.id);
  assert.deepEqual(suspended.point.position, [0, 0]);
  assert.match(suspended.warnings.join(' '), /suspended/i);
  send('undo');
  assert.equal(serialize(session.snapshot()), original);
  assert.deepEqual(read(), active);
  send('redo');
  assert.equal(serialize(session.snapshot()), bypassed);
  assert.match(read().warnings.join(' '), /suspended/i);
  assert.equal(serialize(deserialize(bypassed)), bypassed);
});

for (const kind of ['join', 'repeater']) {
  test(`Fill preserves ${kind} geometry groups and exact paint identities`, () => {
    const { doc, layer, output, shape, stroke, fill } = sceneFixture();
    let grouped;
    if (kind === 'join') {
      const other = node('Rectangle');
      other.params.center.value = [100, 0];
      grouped = node('Join Geometry');
      layer.nodes.push(other, grouped);
      connect(layer, shape.id, grouped.id, 'a');
      connect(layer, other.id, grouped.id, 'b');
    } else {
      grouped = node('Geometry Repeater');
      grouped.operation = 'ae-2d';
      grouped.params.count.value = 2;
      grouped.params.position.value = [100, 0];
      layer.nodes.push(grouped);
      connect(layer, shape.id, grouped.id, 'path');
    }
    connect(layer, grouped.id, stroke.id, 'path');
    connect(layer, fill.id, output.id, 'path');
    const before = serialize(doc),
      result = exportScene(doc)[0];
    assert.ok(result.value.contours.length >= 2);
    assert.deepEqual(result.paintProvenance, { stroke: 'stroke', fill: 'first' });
    assert.deepEqual(
      colorCoveragePasses([result], doc).map((pass) => [pass.paint, pass.slotId]),
      [
        ['fill', 'first'],
        ['stroke', 'stroke'],
        ['fill', 'first'],
        ['stroke', 'stroke'],
      ],
    );
    assert.equal(serialize(doc), before);
    assert.deepEqual(exportScene(deserialize(before))[0].paintProvenance, result.paintProvenance);
  });
}
