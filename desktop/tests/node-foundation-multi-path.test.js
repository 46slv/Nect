import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  bypassContract,
  CommandSession,
  connect,
  definitions,
  deserialize,
  enableRemapPoint,
  evaluate,
  exportSVG,
  node,
  outputPorts,
  outputType,
  point,
  serialize,
} from '../src/core.js';
import { createNodeRegistry } from '../src/node-foundation.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nectNodeHost } from '../src/nect-node-host.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { multiPathDeclaration } from '../src/multi-path-node-declaration.js';
import { evaluateMultiPath } from '../src/multi-path-node-binding.js';

function fixture() {
  const doc = blankDocument(),
    layer = doc.layers[0],
    output = layer.nodes[0],
    path = node('Multi Path'),
    vector = node('Vector'),
    remap = node('Path Remap'),
    fill = node('Fill');
  doc.schemaVersion = 54;
  path.id = 'multi';
  vector.id = 'driver';
  remap.id = 'remap';
  path.points = [
    [0, 0],
    [60, 0],
    [60, 40],
    [10, 10],
    [40, 30],
  ].map(([x, y], i) => {
    const item = point(x, y);
    item.id = `point-${i + 1}`;
    return item;
  });
  path.points[0].handleA.value = [-3, 2];
  path.points[1].handleB.value = [4, -5];
  path.contours = [
    { id: 'contour-a', closed: true, group: 'group-a', points: ['point-1', 'point-2', 'point-3'] },
    { id: 'contour-b', closed: false, points: ['point-4', 'point-5'] },
  ];
  path.units = [{ id: 'unit-one', contours: ['contour-a', 'contour-b'] }];
  vector.params.value.value = [80, 90];
  layer.nodes = [vector, path, remap, fill, output];
  connect(layer, vector.id, path.id, 'point-2.position');
  connect(layer, path.id, remap.id, 'path');
  connect(layer, remap.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  return { doc, layer, path, vector, remap };
}

test('Multi Path registers as a source without declared parameters and retains placement', () => {
  const registered = builtinNodeRegistry.get('Multi Path'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Multi Path'),
    current = node('Multi Path'),
    types = Object.keys(definitions),
    catalog = nodeCatalog.map((entry) => entry.typeId);
  assert.ok(registered && manifest);
  assert.deepEqual(registered.declaration, multiPathDeclaration);
  assert.equal(manifest.binding.evaluate, evaluateMultiPath);
  assert.deepEqual(current.params, {});
  assert.deepEqual(current.points, []);
  assert.deepEqual(current.contours, []);
  assert.deepEqual(current.units, []);
  assert.equal(outputType(current), 'Path');
  assert.deepEqual(outputPorts(current), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(bypassContract(current).supported, false);
  assert.equal(types[types.indexOf('Multi Path') - 1], 'Curve');
  assert.equal(types[types.indexOf('Multi Path') + 1], 'Boolean');
  assert.equal(catalog[catalog.indexOf('Multi Path') - 1], 'Curve');
  assert.equal(catalog[catalog.indexOf('Multi Path') + 1], 'Imported Path');
  const { typeId, ...metadata } = nodeCatalog.find((entry) => entry.typeId === 'Multi Path');
  assert.equal(typeId, 'Multi Path');
  assert.deepEqual(metadata, multiPathDeclaration.catalog);
});

test('zero parameters are valid, while missing outputs and invalid bypass remain rejected', () => {
  const bindings = new Map([['builtin.multi-path.v1', { evaluate: evaluateMultiPath }]]),
    registry = createNodeRegistry([multiPathDeclaration], bindings, nectNodeHost);
  assert.deepEqual(registry.get('Multi Path').declaration.parameters, []);
  assert.ok(Object.isFrozen(registry.get('Multi Path').declaration.parameters));
  assert.throws(
    () =>
      createNodeRegistry([{ ...multiPathDeclaration, parameters: null }], bindings, nectNodeHost),
    /Invalid parameter/,
  );
  assert.throws(
    () => createNodeRegistry([{ ...multiPathDeclaration, outputs: [] }], bindings, nectNodeHost),
    /Invalid output/,
  );
  assert.throws(
    () =>
      createNodeRegistry(
        [{ ...multiPathDeclaration, bypassInput: 'path' }],
        bindings,
        nectNodeHost,
      ),
    /Invalid bypass/,
  );
});

test('registered Multi Path preserves contour identities, groups and downstream Remap history', () => {
  const { doc, layer, path, vector, remap } = fixture(),
    originalAuthored = serialize(doc),
    first = evaluate(layer, {}, doc.imageAssets, doc),
    value = first.run(path.id),
    warm = evaluate(layer, {}, doc.imageAssets, doc);
  assert.equal(first.error, null);
  assert.deepEqual(warm.run(path.id), value);
  assert.ok(warm.stats.reused > 0);
  assert.deepEqual(
    value.contours.map(({ id, closed, group, points }) => [
      id,
      closed,
      group,
      points.map((item) => item.id),
    ]),
    [
      ['contour-a', true, 'group-a', ['point-1', 'point-2', 'point-3']],
      ['contour-b', false, undefined, ['point-4', 'point-5']],
    ],
  );
  assert.deepEqual(value.contours[0].points[1].position, [80, 90]);
  assert.deepEqual(value.contours[0].points[1].handleB, [4, -5]);
  assert.equal(value.unstable, true);
  assert.equal(
    value.fingerprint,
    '[[true,[[[0,0],[-3,2],[0,0]],[[80,90],[0,0],[4,-5]],[[60,40],[0,0],[0,0]]],"group-a"],[false,[[[10,10],[0,0],[0,0]],[[40,30],[0,0],[0,0]]]]]',
  );
  assert.equal(serialize(doc), originalAuthored);

  enableRemapPoint(remap, value, 'point-1').params.position.value = [3, 1];
  const session = new CommandSession(doc),
    send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload }),
    original = serialize(session.snapshot()),
    read = () => {
      const snapshot = session.snapshot(),
        evaluation = evaluate(snapshot.layers[0], {}, snapshot.imageAssets, snapshot);
      return { path: evaluation.run(remap.id), warnings: evaluation.warnings };
    },
    active = read();
  assert.deepEqual(active.path.contours[0].points[0].position, [3, 1]);
  assert.deepEqual(active.warnings, []);
  send('set-parameter', { layerId: layer.id, nodeId: vector.id, key: 'value', value: [81, 90] });
  const changed = serialize(session.snapshot());
  assert.match(read().warnings.join(' '), /suspended/i);
  send('undo');
  assert.equal(serialize(session.snapshot()), original);
  assert.deepEqual(read(), active);
  send('redo');
  assert.equal(serialize(session.snapshot()), changed);
  assert.match(read().warnings.join(' '), /suspended/i);
  send('undo');
  const reopened = deserialize(serialize(session.snapshot()));
  assert.equal(exportSVG(reopened), exportSVG(session.snapshot()));
  assert.deepEqual(reopened.layers[0].nodes.find((item) => item.id === path.id).units, path.units);
});

test('a contour-only edit invalidates the cached geometry and Remap fingerprint', () => {
  const { doc, layer, path, remap } = fixture(),
    initial = evaluate(layer, {}, doc.imageAssets, doc).run(path.id),
    originalFingerprint = initial.fingerprint;
  enableRemapPoint(remap, initial, 'point-1').params.position.value = [3, 1];
  const active = evaluate(layer, {}, doc.imageAssets, doc);
  assert.deepEqual(active.run(remap.id).contours[0].points[0].position, [3, 1]);
  assert.deepEqual(active.warnings, []);

  path.contours.reverse();
  const changed = evaluate(layer, {}, doc.imageAssets, doc),
    geometry = changed.run(path.id),
    remapped = changed.run(remap.id);
  assert.deepEqual(
    geometry.contours.map((contour) => contour.id),
    ['contour-b', 'contour-a'],
  );
  assert.notEqual(geometry.fingerprint, originalFingerprint);
  assert.match(changed.warnings.join(' '), /suspended/i);
  assert.deepEqual(remapped.contours[1].points[0].position, [0, 0]);
  assert.ok(changed.stats.computed > 0);
  const warm = evaluate(layer, {}, doc.imageAssets, doc);
  assert.deepEqual(warm.run(path.id), geometry);
  assert.ok(warm.stats.reused > 0);

  path.contours[0].closed = true;
  const closed = evaluate(layer, {}, doc.imageAssets, doc);
  assert.equal(closed.run(path.id).contours[0].closed, true);
  assert.ok(closed.stats.computed > 0);
});
