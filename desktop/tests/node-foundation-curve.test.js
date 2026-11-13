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
  exportSVG,
  node,
  outputPorts,
  outputType,
  serialize,
} from '../src/core.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { curveDeclaration } from '../src/curve-node-declaration.js';
import { evaluateCurve } from '../src/curve-node-binding.js';

function fixture() {
  const doc = blankDocument(),
    layer = doc.layers[0],
    output = layer.nodes[0],
    curve = node('Curve'),
    vector = node('Vector'),
    fill = node('Fill');
  doc.schemaVersion = 54;
  curve.id = 'curve';
  vector.id = 'driver';
  curve.points.forEach((point, i) => (point.id = ['first', 'second', 'third'][i]));
  curve.points[0].position.value = [25, 35];
  curve.points[0].handleA.value = [-8, 3];
  curve.points[0].handleB.value = [9, -4];
  vector.params.value.value = [90, 110];
  layer.nodes = [vector, curve, fill, output];
  connect(layer, vector.id, curve.id, 'first.position');
  connect(layer, curve.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  return { doc, layer, curve, vector };
}

test('Curve registration retains its current interface, factory and placement', () => {
  const registered = builtinNodeRegistry.get('Curve'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Curve'),
    current = node('Curve'),
    types = Object.keys(definitions),
    catalog = nodeCatalog.map((entry) => entry.typeId);
  assert.ok(registered && manifest);
  assert.deepEqual(registered.declaration, curveDeclaration);
  assert.equal(manifest.binding.evaluate, evaluateCurve);
  assert.equal(outputType(current), 'Path');
  assert.deepEqual(outputPorts(current), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(bypassContract(current).supported, false);
  assert.equal(definitions.Curve.closed.type, 'Boolean');
  assert.equal(current.params.closed.value, true);
  assert.equal(current.points.length, 3);
  assert.equal(new Set(current.points.map((point) => point.id)).size, 3);
  assert.equal(types[types.indexOf('Curve') - 1], 'Star');
  assert.equal(types[types.indexOf('Curve') + 1], 'Multi Path');
  assert.equal(catalog[catalog.indexOf('Curve') - 1], 'Star');
  assert.equal(catalog[catalog.indexOf('Curve') + 1], 'Multi Path');
  const { typeId, ...metadata } = nodeCatalog.find((entry) => entry.typeId === 'Curve');
  assert.equal(typeId, 'Curve');
  assert.deepEqual(metadata, curveDeclaration.catalog);
});

test('binding evaluates stable point IDs and both handles in authored order', () => {
  const calls = [],
    values = {
      'a.position': [1, 2],
      'a.handleA': [-1, 0],
      'a.handleB': [3, 0],
      'b.position': [4, 5],
      'b.handleA': [0, -2],
      'b.handleB': [0, 2],
    },
    context = {
      nodeId: 'curve',
      pointReader() {
        calls.push('index');
        return (id, field) => {
          calls.push(`${id}.${field}`);
          return values[`${id}.${field}`];
        };
      },
      mapPointIds(project) {
        calls.push('points');
        return ['a', 'b'].map(project);
      },
    };
  const result = evaluateCurve(
    undefined,
    (key) => {
      calls.push(key);
      return false;
    },
    context,
  );
  assert.deepEqual(calls, [
    'index',
    'closed',
    'points',
    'a.position',
    'a.handleA',
    'a.handleB',
    'b.position',
    'b.handleA',
    'b.handleB',
  ]);
  assert.deepEqual(result, {
    contours: [
      {
        id: 'curve',
        closed: false,
        points: [
          { id: 'a', position: [1, 2], handleA: [-1, 0], handleB: [3, 0] },
          { id: 'b', position: [4, 5], handleA: [0, -2], handleB: [0, 2] },
        ],
      },
    ],
  });
});

test('registered Curve keeps driven point identity through edit, reorder, history and save', () => {
  const { doc, layer, curve, vector } = fixture(),
    session = new CommandSession(doc),
    send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload }),
    path = (snapshot = session.snapshot()) =>
      evaluate(snapshot.layers[0], {}, snapshot.imageAssets, snapshot).run(curve.id),
    original = serialize(session.snapshot()),
    initial = path();
  assert.deepEqual(initial.contours[0].points[0], {
    id: 'first',
    position: [90, 110],
    handleA: [-8, 3],
    handleB: [9, -4],
  });
  send('rename', { layerId: layer.id, nodeId: curve.id, name: 'Renamed curve' });
  send('set-parameter', { layerId: layer.id, nodeId: vector.id, key: 'value', value: [120, 150] });
  send('translate-points', {
    layerId: layer.id,
    nodeId: curve.id,
    pointIds: ['second'],
    delta: [5, 7],
  });
  const edited = serialize(session.snapshot()),
    editedPath = path();
  assert.deepEqual(editedPath.contours[0].points[0].position, [120, 150]);
  assert.deepEqual(editedPath.contours[0].points[1].position, [455, 157]);
  assert.deepEqual(editedPath.contours[0].points[0].handleA, [-8, 3]);
  send('undo');
  assert.deepEqual(path().contours[0].points[1].position, [450, 150]);
  send('redo');
  assert.equal(serialize(session.snapshot()), edited);
  const revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  assert.throws(
    () =>
      send('set-parameter', {
        layerId: layer.id,
        nodeId: vector.id,
        key: 'value',
        value: ['bad', 1],
      }),
    /Expected Vector2/,
  );
  assert.equal(serialize(session.snapshot()), edited);
  assert.equal(session.state.expectedRevision, revision);
  assert.equal(JSON.stringify(session.historyView), history);
  const reopened = deserialize(edited);
  assert.deepEqual(path(reopened), editedPath);
  assert.equal(exportSVG(reopened), exportSVG(session.snapshot()));
  reopened.layers[0].nodes.find((item) => item.id === curve.id).points.reverse();
  const reordered = deserialize(serialize(reopened)),
    byId = new Map(path(reordered).contours[0].points.map((point) => [point.id, point]));
  assert.deepEqual(byId.get('first').position, [120, 150]);
  assert.deepEqual(byId.get('first').handleA, [-8, 3]);
  assert.deepEqual(byId.get('second').position, [455, 157]);
  send('undo');
  send('undo');
  send('undo');
  assert.equal(serialize(session.snapshot()), original);
  assert.deepEqual(path(), initial);
});
