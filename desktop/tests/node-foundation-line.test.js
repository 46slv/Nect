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
  serialize,
} from '../src/core.js';
import { nodePickerItems, searchNodeItems } from '../src/node-catalog.js';

test('Line is searchable with just Position and Length, and produces open geometry', () => {
  const line = node('Line');
  const items = nodePickerItems(definitions);
  for (const query of ['line segment', '\u76f4\u7dda'])
    assert.equal(searchNodeItems(items, query, 'create')[0].id, 'Line');
  assert.deepEqual(Object.keys(line.params), ['position', 'length']);
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
