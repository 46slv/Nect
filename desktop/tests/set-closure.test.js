import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  node,
  point,
  connect,
  evaluate,
  exportSVG,
  pathData,
  serialize,
  deserialize,
  CommandSession,
} from '../src/core.js';

function wire(layer, source, target, key) {
  connect(layer, source.id, target.id, key);
}

function openCurve() {
  const curve = node('Curve');
  curve.params.closed.value = false;
  curve.points = [point(0, 0), point(100, 0), point(100, 100)];
  curve.points[0].handleB.value = [12, 5];
  curve.points[0].handleA.value = [-8, 6];
  curve.points[2].handleA.value = [-9, -4];
  curve.points[2].handleB.value = [6, -10];
  return curve;
}

test('Set Closure closes imported contours as real cubic paths and round-trips through JSON/SVG', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    imported = node('Imported Path'),
    closure = node('Set Closure'),
    stroke = node('Stroke'),
    output = layer.nodes[0];
  imported.rawPath = 'M0 0 C20 0 80 100 100 100 M200 200 L250 200 L250 240';
  layer.nodes.push(imported, closure, stroke);
  wire(layer, imported, closure, 'path');
  wire(layer, closure, stroke, 'path');
  wire(layer, stroke, output, 'path');

  const source = evaluate(layer).run(imported.id),
    before = structuredClone(source),
    closed = evaluate(layer).run(closure.id);
  assert.equal(closure.params.closed.value, true);
  assert.equal(source.contours.length, 2);
  assert.deepEqual(
    closed.contours.map((c) => c.closed),
    [true, true],
  );
  assert.deepEqual(
    closed.contours.map((c) => c.points.map((p) => [p.position, p.handleA, p.handleB])),
    before.contours.map((c) => c.points.map((p) => [p.position, p.handleA, p.handleB])),
  );
  assert.deepEqual(source, before);
  assert.equal(closed.unstable, true);
  assert.match(closed.fingerprint, /^sha256:/);
  assert.deepEqual(
    evaluate(layer)
      .run(closure.id)
      .contours.map((c) => [c.id, c.points.map((p) => p.id)]),
    closed.contours.map((c) => [c.id, c.points.map((p) => p.id)]),
  );

  const openNode = structuredClone(closure);
  openNode.params.closed.value = false;
  const openDoc = structuredClone(doc),
    openLayer = openDoc.layers[0];
  openLayer.nodes[openLayer.nodes.findIndex((n) => n.id === closure.id)] = openNode;
  const opened = evaluate(openLayer).run(closure.id);
  assert.deepEqual(
    opened.contours.map((c) => c.closed),
    [false, false],
  );
  assert.deepEqual(
    opened.contours.map((c) => c.points.map((p) => [p.handleA, p.handleB])),
    closed.contours.map((c) => c.points.map((p) => [p.handleA, p.handleB])),
  );
  assert.match(exportSVG(doc), /d="[^"]*z/);
  assert.doesNotMatch(exportSVG(openDoc), /d="[^"]*z/);

  const reopened = deserialize(serialize(doc));
  assert.deepEqual(evaluate(reopened.layers[0]).run(closure.id).contours, closed.contours);
});

test('Set Closure accepts generated grouped geometry and enables downstream Boolean', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    first = openCurve(),
    second = openCurve(),
    join = node('Join Geometry'),
    closure = node('Set Closure'),
    other = node('Rectangle'),
    boolean = node('Boolean');
  second.points = [point(140, 0), point(240, 0), point(240, 100)];
  layer.nodes.push(first, second, join, closure, other, boolean);
  wire(layer, first, join, 'a');
  wire(layer, second, join, 'b');
  wire(layer, join, closure, 'path');
  wire(layer, closure, boolean, 'a');
  wire(layer, other, boolean, 'b');
  const joined = evaluate(layer).run(join.id),
    source = structuredClone(joined),
    closed = evaluate(layer).run(closure.id);
  assert.equal(joined.contours.length, 2);
  assert.ok(joined.contours.every((c) => c.group));
  assert.deepEqual(
    closed.contours.map((c) => c.closed),
    [true, true],
  );
  assert.deepEqual(
    closed.contours.map((c) => c.group),
    source.contours.map((c) => c.group),
  );
  assert.deepEqual(
    closed.contours.map((c) => c.points.map((p) => [p.position, p.handleA, p.handleB])),
    source.contours.map((c) => c.points.map((p) => [p.position, p.handleA, p.handleB])),
  );
  assert.match(pathData(closed), /c6,-10 -108,-94 -100,-100z/i);
  assert.doesNotThrow(() => evaluate(layer).run(boolean.id));
  closure.params.closed.value = false;
  assert.throws(() => evaluate(layer).run(boolean.id), /Boolean requires closed contours/);
});

test('empty and one-point closed contours stay finite through Set Closure and Boolean', () => {
  for (const count of [0, 1]) {
    const doc = blankDocument(),
      layer = doc.layers[0],
      curve = openCurve(),
      closure = node('Set Closure'),
      other = node('Rectangle'),
      boolean = node('Boolean');
    curve.points = curve.points.slice(0, count);
    layer.nodes.push(curve, closure, other, boolean);
    wire(layer, curve, closure, 'path');
    wire(layer, closure, boolean, 'a');
    wire(layer, other, boolean, 'b');
    const closed = evaluate(layer).run(closure.id);
    assert.equal(closed.contours[0].closed, true);
    assert.equal(closed.contours[0].points.length, count);
    assert.doesNotMatch(pathData(closed), /NaN|Infinity/);
    assert.doesNotThrow(() => evaluate(layer).run(boolean.id));
  }
});

test('Set Closure Session edits honor Undo/Redo, locks and same-type bypass', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    curve = openCurve(),
    closure = node('Set Closure'),
    stroke = node('Stroke'),
    output = layer.nodes[0];
  layer.nodes.push(curve, closure, stroke);
  wire(layer, curve, closure, 'path');
  wire(layer, closure, stroke, 'path');
  wire(layer, stroke, output, 'path');
  const session = new CommandSession(doc),
    dispatch = (type, payload = {}) =>
      session.dispatch({
        ...session.state,
        type,
        payload: { layerId: layer.id, ...payload },
      }),
    getClosure = () => session.snapshot().layers[0].nodes.find((n) => n.id === closure.id),
    evaluateClosure = () => evaluate(session.snapshot().layers[0]).run(closure.id);

  dispatch('set-parameter', { nodeId: closure.id, key: 'closed', value: false });
  assert.equal(evaluateClosure().contours[0].closed, false);
  const opened = serialize(session.snapshot());
  dispatch('undo');
  assert.equal(evaluateClosure().contours[0].closed, true);
  dispatch('redo');
  assert.equal(serialize(session.snapshot()), opened);

  dispatch('set-parameter-lock', { nodeId: closure.id, key: 'closed', locked: true });
  assert.throws(
    () => dispatch('set-parameter', { nodeId: closure.id, key: 'closed', value: true }),
    /locked/,
  );
  dispatch('set-parameter-lock', { nodeId: closure.id, key: 'closed', locked: false });
  dispatch('set-node-bypassed', { nodeId: closure.id, bypassed: true });
  assert.equal(evaluateClosure().contours[0].closed, false);
  assert.equal(getClosure().bypassed, true);
  dispatch('undo');
  assert.equal(evaluateClosure().contours[0].closed, false);
  assert.equal(getClosure().bypassed, undefined);
  dispatch('redo');
  assert.equal(getClosure().bypassed, true);
});

test('Set Closure and downstream Boolean preflight roll invalid closure changes back atomically', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    curve = openCurve(),
    closure = node('Set Closure'),
    other = node('Rectangle'),
    boolean = node('Boolean');
  closure.params.closed.value = false;
  layer.nodes.push(curve, closure, other, boolean);
  wire(layer, curve, closure, 'path');
  wire(layer, closure, boolean, 'a');
  wire(layer, other, boolean, 'b');
  const session = new CommandSession(doc),
    dispatch = (type, payload = {}) =>
      session.dispatch({ ...session.state, type, payload: { layerId: layer.id, ...payload } });
  dispatch('set-parameter', { nodeId: closure.id, key: 'closed', value: true });
  assert.doesNotThrow(() => evaluate(session.snapshot().layers[0]).run(boolean.id));
  const before = serialize(session.snapshot()),
    state = session.state;
  assert.throws(
    () => dispatch('set-parameter', { nodeId: closure.id, key: 'closed', value: false }),
    /Boolean requires closed contours/,
  );
  assert.equal(serialize(session.snapshot()), before);
  assert.deepEqual(session.state, state);
});
