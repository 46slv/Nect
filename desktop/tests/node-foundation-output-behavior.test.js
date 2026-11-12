import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  CommandSession,
  connect,
  deserialize,
  evaluate,
  exportSVG,
  hasEmptyOutput,
  node,
  serialize,
} from '../src/core.js';

test('only the designated opt-in Output may produce an intentional empty path', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    output = layer.nodes[0],
    other = node('Output');
  layer.nodes.push(other);
  assert.equal(hasEmptyOutput(layer, output), true);
  assert.deepEqual(evaluate(layer).run(output.id), { contours: [] });
  assert.equal(hasEmptyOutput(layer, other), false);
  assert.throws(() => evaluate(layer).run(other.id), /Connect a PaintedPath input/);
  layer.allowEmptyOutput = false;
  assert.throws(() => evaluate(layer).run(output.id), /Connect a PaintedPath input/);
  layer.allowEmptyOutput = true;
  assert.deepEqual(evaluate(layer).run(output.id), { contours: [] });
});

test('Output clones painted groups without changing source geometry or authored state', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    output = layer.nodes[0],
    curve = node('Curve'),
    closure = node('Set Closure'),
    fill = node('Fill'),
    join = node('Join Geometry');
  doc.schemaVersion = 31;
  curve.id = 'curve';
  closure.id = 'close';
  fill.id = 'fill';
  join.id = 'join';
  output.id = 'output';
  layer.output = output.id;
  layer.nodes = [curve, closure, fill, join, output];
  connect(layer, curve.id, closure.id, 'path');
  connect(layer, closure.id, fill.id, 'path');
  connect(layer, fill.id, join.id, 'a');
  connect(layer, join.id, output.id, 'path');
  const authored = serialize(doc),
    evaluation = evaluate(layer),
    source = evaluation.run(join.id),
    result = evaluation.run(output.id);
  assert.equal(result.kind, 'painted-group');
  assert.notEqual(result, source);
  assert.notEqual(result.children[0], source.children[0]);
  assert.deepEqual(result, source);
  assert.equal(result.fingerprint, source.fingerprint);
  assert.deepEqual(evaluate(layer).run(output.id), result);
  result.children[0].contours[0].points[0].position[0] += 1;
  assert.notDeepEqual(result.children[0], source.children[0]);
  assert.equal(serialize(doc), authored);
  const reopened = deserialize(authored);
  assert.equal(serialize(reopened), authored);
  assert.deepEqual(evaluate(reopened.layers[0]).run(output.id), source);
});

test('Output connection, rejected source and Undo/Redo use the common Session', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    output = layer.nodes[0],
    rectangle = node('Rectangle'),
    fill = node('Fill'),
    vector = node('Vector');
  layer.nodes.push(rectangle, fill, vector);
  connect(layer, rectangle.id, fill.id, 'path');
  const session = new CommandSession(doc),
    send = (type, payload = {}) =>
      session.dispatch({
        ...session.state,
        type,
        payload: { layerId: layer.id, nodeId: output.id, key: 'path', ...payload },
      }),
    before = serialize(session.snapshot());
  send('connect', { sourceId: fill.id });
  const connected = serialize(session.snapshot());
  assert.match(exportSVG(session.snapshot()), /<path/);
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  const revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  assert.throws(() => send('connect', { sourceId: vector.id }), /Cannot connect|Incompatible/);
  assert.equal(serialize(session.snapshot()), before);
  assert.equal(session.state.expectedRevision, revision);
  assert.equal(JSON.stringify(session.historyView), history);
  send('redo');
  assert.equal(serialize(session.snapshot()), connected);
  assert.equal(serialize(deserialize(connected)), connected);
});
