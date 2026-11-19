import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  node,
  connect,
  disconnect,
  evaluate,
  serialize,
  deserialize,
  exportSVG,
  CommandSession,
  activeParameters,
} from '../src/core.js';
import {
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { evaluateBlend } from '../src/blend-node-binding.js';
function fixture(s = 1) {
  const doc = document(),
    l = doc.layers[0],
    shapes = [node('Rectangle'), node('Rectangle'), node('Rectangle')],
    fills = shapes.map(() => node('Fill')),
    blend = node('Blend'),
    out = node('Output');
  shapes.forEach((n, i) => {
    n.params.center.value = [i * 100, 0];
    fills[i].params.color.value = { hex: ['#ff0000', '#00ff00', '#0000ff'][i], alpha: 1 };
  });
  blend.operation = 'three-source-steps';
  blend.params.steps.value = s;
  l.nodes = [...shapes, ...fills, blend, out];
  l.output = out.id;
  shapes.forEach((n, i) => {
    connect(l, n.id, fills[i].id, 'path');
    connect(l, fills[i].id, blend.id, ['a', 'b', 'c'][i]);
  });
  connect(l, blend.id, out.id, 'path');
  return { doc, l, shapes, fills, blend };
}
const result = (doc) => evaluate(doc.layers[0], {}, doc.imageAssets, doc);
test('explicit three-source spans keep ordered authored endpoints and one B', () => {
  for (const s of [0, 1, 5, 126]) {
    const { doc } = fixture(s),
      before = serialize(doc),
      r = result(doc);
    assert.equal(r.error, null);
    assert.equal(r.path.children.length, 2 * s + 3);
    assert.deepEqual(
      [
        r.path.children[0].fill.hex,
        r.path.children[s + 1].fill.hex,
        r.path.children.at(-1).fill.hex,
      ],
      ['#ff0000', '#00ff00', '#0000ff'],
    );
    assert.equal(r.path.children.filter((c) => c.fill.hex === '#00ff00').length, 1);
    assert.equal(serialize(doc), before);
    const ids = r.path.children.flatMap((c) => [
      c.contours[0].id,
      ...c.contours[0].points.map((p) => p.id),
    ]);
    assert.equal(new Set(ids).size, ids.length);
    assert.equal(exportSVG(deserialize(before)), exportSVG(doc));
  }
});
test('invalid steps and missing C diagnose without mutating graph', () => {
  for (const s of [-1, 0.5, 127, 254, NaN]) {
    const { doc } = fixture(s),
      before = JSON.stringify(doc);
    assert.match(result(doc).error, Number.isNaN(s) ? /finite Scalar/ : /0 to 126/);
    assert.equal(JSON.stringify(doc), before);
  }
  const { doc, blend } = fixture();
  disconnect(blend, 'c');
  blend.params.c.value = null;
  assert.ok(result(doc).error);
});
test('C changes, Undo Redo and Expand preserve actual appearance', () => {
  const { doc, l, shapes, blend } = fixture(),
    session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({ ...session.state, type, payload: { layerId: l.id, ...payload } });
  const before = serialize(session.snapshot()),
    svg = exportSVG(session.snapshot());
  send('set-parameter', { nodeId: shapes[2].id, key: 'center', value: [300, 80] });
  const edited = serialize(session.snapshot());
  assert.notEqual(exportSVG(session.snapshot()), svg);
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  send('redo');
  assert.equal(serialize(session.snapshot()), edited);
  const preExpand = exportSVG(session.snapshot());
  send('expand-blend', { nodeId: blend.id });
  assert.equal(exportSVG(session.snapshot()), preExpand);
  send('undo');
  assert.equal(serialize(session.snapshot()), edited);
});
test('old modes ignore C and old serialized nodes migrate to inactive null C', () => {
  const { doc, blend } = fixture();
  blend.operation = 'steps';
  const svg = exportSVG(doc);
  assert.equal(
    activeParameters(blend).some(([key]) => key === 'c'),
    false,
  );
  delete blend.params.c;
  const reopened = deserialize(serialize(doc));
  assert.equal(exportSVG(reopened), svg);
  assert.equal(reopened.layers[0].nodes.find((n) => n.id === blend.id).params.c.value, null);
  blend.operation = 'three-source-steps';
  assert.throws(() => deserialize(serialize(doc)), /requires C/);
});
test('either incompatible span rejects whole output and combined budget is enforced', () => {
  const paint = (n) => ({
    fill: { hex: '#000000', alpha: 1 },
    contours: [
      {
        closed: true,
        points: Array.from({ length: n }, (_, i) => ({
          position: [Math.cos((i * 2 * Math.PI) / n) * 100, Math.sin((i * 2 * Math.PI) / n) * 100],
          handleA: [0, 0],
          handleB: [0, 0],
        })),
      },
    ],
  });
  const a = paint(400),
    b = paint(400),
    c = paint(400);
  let grouped = 0;
  const context = {
    nodeId: 'fixture',
    groupAppearance: (values) => {
      grouped++;
      return values;
    },
  };
  assert.throws(
    () => evaluateBlend('three-source-steps', (key) => ({ a, b, c, steps: 126 })[key], context),
    /combined geometry budget/,
  );
  assert.equal(grouped, 0);
  c.fill = null;
  c.stroke = { hex: '#000000', alpha: 1, width: 1 };
  assert.throws(
    () => evaluateBlend('three-source-steps', (key) => ({ a, b, c, steps: 0 })[key], context),
    /matching fill/,
  );
  assert.equal(grouped, 0);
});

test('Composition codec and explicit mode switch retain C wire without changing old Steps', () => {
  const { doc, l, blend } = fixture(),
    session = new CommandSession(doc),
    send = (type, payload = {}) =>
      session.dispatch({ ...session.state, type, payload: { layerId: l.id, ...payload } });
  const before = serialize(session.snapshot());
  const project = migrateLegacyDocumentToCompositionProject(doc),
    saved = serializeCompositionProject(project);
  assert.equal(serializeCompositionProject(deserializeCompositionProject(saved)), saved);
  send('set-operation', { nodeId: blend.id, operation: 'steps' });
  const current = session.snapshot().layers[0].nodes.find((n) => n.id === blend.id);
  assert.equal(current.params.c.source, blend.params.c.source);
  assert.equal(result(session.snapshot()).path.children.length, 3);
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  send('disconnect', { nodeId: blend.id, key: 'c' });
  const invalid = serialize(session.snapshot());
  assert.ok(result(session.snapshot()).error);
  assert.throws(() => send('expand-blend', { nodeId: blend.id }));
  assert.equal(serialize(session.snapshot()), invalid);
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
});
test('all source blockers propagate, middle endpoint keeps exact geometry and opacity', () => {
  const shape = (offset, hex, alpha) => ({
    fill: { hex, alpha },
    exportBlockers: [hex],
    contours: [
      {
        closed: true,
        points: [
          [0, 0],
          [20, 0],
          [0, 20],
        ].map(([x, y]) => ({ position: [x + offset, y], handleA: [0, 0], handleB: [0, 0] })),
      },
    ],
  });
  const a = shape(0, '#ff0000', 0),
    b = shape(40, '#00ff00', 0.4),
    c = shape(80, '#0000ff', 1),
    before = JSON.stringify([a, b, c]);
  const values = evaluateBlend('three-source-steps', (key) => ({ a, b, c, steps: 1 })[key], {
    nodeId: 'three',
    groupAppearance: (v) => v,
  });
  assert.equal(values.length, 5);
  assert.deepEqual(values[2].fill, b.fill);
  assert.deepEqual(
    values[2].contours[0].points.map((p) => [p.position, p.handleA, p.handleB]),
    b.contours[0].points.map((p) => [p.position, p.handleA, p.handleB]),
  );
  for (const value of values)
    assert.deepEqual(value.exportBlockers, ['#ff0000', '#00ff00', '#0000ff']);
  assert.equal(JSON.stringify([a, b, c]), before);
});
