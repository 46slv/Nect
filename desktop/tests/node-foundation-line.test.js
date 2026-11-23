import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  CommandSession,
  connect,
  definitions,
  deserialize,
  evaluate,
  exportSVG,
  node,
  outputType,
  pathBounds,
  serialize,
} from '../src/core.js';
import { nodePickerItems, searchNodeItems } from '../src/node-catalog.js';

test('Line is searchable and produces open geometry by default', () => {
  const line = node('Line');
  const items = nodePickerItems(definitions);
  for (const query of ['line segment', '\u76f4\u7dda'])
    assert.equal(searchNodeItems(items, query, 'create')[0].id, 'Line');
  assert.deepEqual(Object.keys(line.params), ['position', 'length', 'useWidth', 'width']);
  assert.equal(outputType(line), 'Path');
  const doc = blankDocument();
  const layer = doc.layers[0];
  layer.nodes.push(line);
  const shape = evaluate(layer, {}, doc.imageAssets, doc).run(line.id).contours[0];
  assert.equal(shape.closed, false);
  assert.deepEqual(
    shape.points.map((p) => p.position),
    [
      [250, 250],
      [450, 250],
    ],
  );
  assert.ok(shape.points.every((p) => [...p.handleA, ...p.handleB].every((v) => v === 0)));
});

test('Line created by the shared command renders through Stroke, survives undo and SVG round trip', () => {
  const doc = blankDocument(),
    layer = doc.layers[0];
  const session = new CommandSession(doc);
  const send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  const { nodeId } = send('create-node', { layerId: layer.id, nodeType: 'Line' });
  send('undo');
  assert.ok(!session.snapshot().layers[0].nodes.some((n) => n.id === nodeId));
  send('redo');
  send('set-parameter', { layerId: layer.id, nodeId, key: 'position', value: [10, 20] });
  send('set-parameter', { layerId: layer.id, nodeId, key: 'length', value: 125 });
  const edited = session.snapshot(),
    target = edited.layers[0];
  const line = target.nodes.find((n) => n.id === nodeId),
    stroke = node('Stroke');
  target.nodes.push(stroke);
  connect(target, line.id, stroke.id, 'path');
  connect(target, stroke.id, target.output, 'path');
  const before = evaluate(target, {}, edited.imageAssets, edited).run(line.id).contours[0];
  const roundTrip = deserialize(serialize(edited));
  const after = evaluate(roundTrip.layers[0], {}, roundTrip.imageAssets, roundTrip).run(line.id)
    .contours[0];
  assert.deepEqual(after, before);
  assert.deepEqual(
    after.points.map((p) => p.position),
    [
      [10, 20],
      [135, 20],
    ],
  );
  const svg = exportSVG(roundTrip);
  assert.match(svg, /stroke-width="2"/);
  assert.match(svg, /d="M10,20h125" fill="none"/);
  assert.equal(svg, exportSVG(edited));

  for (const value of [0, -3, Number.MIN_VALUE]) {
    line.params.length.value = value;
    assert.throws(
      () => evaluate(target, {}, edited.imageAssets, edited).run(line.id),
      /positive, finite/,
    );
  }
});

test('Transform rotates a Line; invalid length edits retain geometry, revision and history', () => {
  const doc = blankDocument(),
    layer = doc.layers[0];
  const line = node('Line'),
    transform = node('Transform'),
    stroke = node('Stroke');
  line.params.position.value = [40, 60];
  line.params.length.value = 120;
  transform.params.rotation.value = 90;
  layer.nodes.unshift(line, transform, stroke);
  connect(layer, line.id, transform.id, 'path');
  connect(layer, transform.id, stroke.id, 'path');
  connect(layer, stroke.id, layer.output, 'path');
  const session = new CommandSession(doc),
    initial = session.snapshot();
  const revision = session.state.expectedRevision;
  for (const value of [0, -20, Number.MIN_VALUE]) {
    assert.throws(
      () =>
        session.dispatch({
          ...session.state,
          type: 'set-parameter',
          payload: { layerId: layer.id, nodeId: line.id, key: 'length', value },
        }),
      /positive, finite/,
    );
  }
  assert.equal(session.state.expectedRevision, revision);
  assert.equal(session.historyView.past.length, 0);
  assert.deepEqual(session.snapshot(), initial);
  const contour = evaluate(layer, {}, doc.imageAssets, doc).run(transform.id).contours[0];
  const [start, end] = contour.points.map((p) => p.position);
  assert.ok(Math.abs(start[0] - end[0]) < 1e-9);
  assert.ok(Math.abs(end[1] - start[1] - 120) < 1e-9);
  assert.equal(exportSVG(deserialize(serialize(doc))), exportSVG(doc));
});

test('Width toggles 2-point Path or a centered 4-point shape and preserves the stored width', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    line = node('Line');
  line.params.position.value = [10, 20];
  line.params.length.value = 125;
  layer.nodes.push(line);
  connect(layer, line.id, layer.output, 'path');
  const session = new CommandSession(doc);
  const set = (key, value) =>
    session.dispatch({
      ...session.state,
      type: 'set-parameter',
      payload: { layerId: layer.id, nodeId: line.id, key, value },
    });
  const contour = (doc) =>
    evaluate(doc.layers[0], {}, doc.imageAssets, doc).run(line.id).contours[0];
  const open = contour(session.snapshot());
  set('useWidth', true);
  set('width', 20);
  const thick = contour(session.snapshot());
  assert.equal(thick.closed, true);
  assert.deepEqual(
    thick.points.map((p) => p.position),
    [
      [10, 30],
      [10, 10],
      [135, 10],
      [135, 30],
    ],
  );
  assert.notEqual(thick.id, open.id);
  assert.ok(thick.points.every((p) => !open.points.some((q) => q.id === p.id)));
  set('useWidth', false);
  assert.deepEqual(contour(session.snapshot()), open);
  const saved = session.snapshot();
  assert.deepEqual(contour(deserialize(serialize(saved))), open);
  assert.equal(
    deserialize(serialize(saved)).layers[0].nodes.find((n) => n.id === line.id).params.width.value,
    20,
  );
  set('useWidth', true);
  assert.deepEqual(contour(session.snapshot()), thick);
  set('width', 0);
  assert.deepEqual(contour(session.snapshot()), open);
  assert.throws(() => set('width', -1), /nonnegative/);
  session.dispatch({ ...session.state, type: 'undo' });
  assert.deepEqual(contour(session.snapshot()), thick);
  session.dispatch({ ...session.state, type: 'redo' });
  assert.deepEqual(contour(session.snapshot()), open);
});

test('Filled Line through Join, and stroked Line area matches a Rectangle outline', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    line = node('Line'),
    rect = node('Rectangle'),
    join = node('Join Geometry'),
    fill = node('Fill'),
    stroke = node('Stroke');
  line.params.position.value = [10, 20];
  line.params.length.value = 125;
  line.params.useWidth.value = true;
  line.params.width.value = 20;
  rect.params.center.value = [72.5, 20];
  rect.params.size.value = [125, 20];
  layer.nodes.push(line, rect, join, fill, stroke);
  connect(layer, line.id, join.id, 'a');
  connect(layer, rect.id, join.id, 'b');
  connect(layer, join.id, fill.id, 'path');
  connect(layer, fill.id, layer.output, 'path');
  const result = evaluate(layer, {}, doc.imageAssets, doc);
  assert.deepEqual(
    result.run(join.id).contours.map((c) => c.closed),
    [true, true],
  );
  assert.equal((exportSVG(doc).match(/<path[^>]*fill="#36434e"/g) ?? []).length, 2);
  connect(layer, stroke.id, layer.output, 'path');
  stroke.strokeDash = [12, 5];
  const stroked = new Map();
  for (const n of [line, rect]) {
    connect(layer, n.id, stroke.id, 'path');
    const v = evaluate(layer, {}, doc.imageAssets, doc).run(stroke.id);
    assert.equal(v.contours[0].closed, true);
    const points = v.contours[0].points.map((p) => p.position);
    const svg = exportSVG(doc);
    stroked.set(n.type, { points, bounds: pathBounds(v), stroke: v.stroke, svg });
    assert.match(svg, /stroke-width="2"/);
    assert.match(svg, /stroke-dasharray="12[ ,]+5"/);
  }
  assert.deepEqual(stroked.get('Line'), stroked.get('Rectangle'));
});

test('Previous two-parameter trial Line reopens with the same open geometry', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    line = node('Line');
  layer.nodes.push(line);
  line.params.length.value = 123;
  const baseline = evaluate(layer, {}, doc.imageAssets, doc).run(line.id);
  delete line.params.useWidth;
  delete line.params.width;
  const migrated = deserialize(JSON.stringify(doc)),
    target = migrated.layers[0];
  assert.equal(target.nodes.find((n) => n.id === line.id).params.useWidth.value, false);
  assert.deepEqual(evaluate(target, {}, migrated.imageAssets, migrated).run(line.id), baseline);
});
