import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { deserialize, serialize, node, connect, CommandSession, exportSVG } from '../src/core.js';
import { expandedNodePositions, intersects, unionBounds } from '../src/layout.js';
async function fixture() {
  const doc = deserialize(
    await fs.readFile(new URL('../examples/live-blend.json', import.meta.url), 'utf8'),
  );
  const l = doc.layers[0],
    blend = l.nodes.find((n) => n.type === 'Blend');
  const bfill = l.nodes.find((n) => n.id === blend.params.b.source),
    b = l.nodes.find((n) => n.id === bfill.params.path.source);
  b.params.center.value = [480, 270];
  const c = node('Circle', 40, 620),
    cf = node('Fill', 350, 620);
  c.params.center.value = [790, 270];
  c.params.radius.value = 65;
  cf.params.color.value = { hex: '#20b990', alpha: 1 };
  l.nodes.push(c, cf);
  connect(l, c.id, cf.id, 'path');
  connect(l, cf.id, blend.id, 'c');
  blend.operation = 'three-source-steps';
  blend.params.steps.value = 2;
  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: l.id, nodeId: blend.id, ...payload },
    });
  return { doc, l, blend, c, session, send };
}
const box = (n) => ({
  x: n.position[0],
  y: n.position[1],
  width: n.width || 264,
  height:
    n.type === 'Multi Path'
      ? 577
      : n.type === 'Fill'
        ? 229
        : n.type === 'Join Geometry'
          ? 695
          : 311,
});
test('three-source browser replay: Expand preserves sources and appearance without initial card collisions', async () => {
  const f = await fixture(),
    before = serialize(f.session.snapshot()),
    svg = exportSVG(f.doc);
  const measurements = Object.fromEntries(
    f.l.nodes.map((n) => [n.id, { width: box(n).width, height: box(n).height }]),
  );
  f.send('expand-blend', { graphMeasurements: measurements });
  const expanded = f.session.snapshot(),
    after = serialize(expanded),
    ns = expanded.layers[0].nodes;
  const oldIds = new Set(f.l.nodes.map((n) => n.id)),
    added = ns.filter((n) => !oldIds.has(n.id));
  assert.equal(added.length, 14);
  assert.equal(ns.find((n) => n.id === f.blend.id).type, 'Join Geometry');
  assert.equal(exportSVG(expanded), svg);
  for (const n of f.l.nodes) {
    const current = ns.find((x) => x.id === n.id);
    assert.deepEqual(current.position, n.position);
    if (n.id !== f.blend.id) assert.deepEqual(current, n);
  }
  for (const n of added)
    for (const other of ns)
      if (n !== other)
        assert.equal(intersects(box(n), box(other), 0), false, `${n.type} overlaps ${other.type}`);
  f.send('undo');
  assert.equal(serialize(f.session.snapshot()), before);
  f.send('redo');
  assert.equal(serialize(f.session.snapshot()), after);
  f.send('set-parameter', { nodeId: f.c.id, key: 'radius', value: 95 });
  assert.equal(exportSVG(f.session.snapshot()), svg);
  f.send('undo');
  const path = added.find((n) => n.type === 'Multi Path');
  f.send('set-parameter', {
    nodeId: path.id,
    key: `${path.points[0].id}.position`,
    value: [145, 80],
  });
  const edited = serialize(f.session.snapshot());
  assert.notEqual(exportSVG(f.session.snapshot()), svg);
  assert.equal(serialize(deserialize(edited)), edited);
});
test('placement is deterministic, bounded horizontally at both Steps limits, and ignores distant unrelated cards', () => {
  for (const count of [7, 255, 256]) {
    const old = [
      { id: 'source', type: 'Blend', position: [680, 180], width: 264 },
      { id: 'far', type: 'Circle', position: [100000, 0], width: 264 },
    ];
    const added = Array.from({ length: count }, (_, i) => [
      { id: `p${i}`, type: 'Multi Path', width: 264 },
      { id: `f${i}`, type: 'Fill', width: 264 },
      { id: `s${i}`, type: 'Stroke', width: 264 },
    ]).flat();
    const before = JSON.stringify([old, added]),
      positions = expandedNodePositions(old, added);
    assert.deepEqual(expandedNodePositions(old, added), positions);
    assert.equal(JSON.stringify([old, added]), before);
    const boxes = added.map((n, i) => ({
      x: positions[i][0],
      y: positions[i][1],
      width: 264,
      height: n.type === 'Multi Path' ? 768 : n.type === 'Fill' ? 320 : 1024,
    }));
    const extent = unionBounds(boxes);
    assert.ok(extent.x + extent.width <= 2490);
    assert.ok(extent.y + extent.height < 150000);
    for (let i = 0; i < boxes.length; i++)
      for (let j = 0; j < i; j++) assert.equal(intersects(boxes[i], boxes[j], 0), false);
    console.log(`Expand ${count} shapes / ${added.length} cards extent ${JSON.stringify(extent)}`);
  }
});
test('measured existing bounds are lower-bounded by stored width', () => {
  const old = [{ id: 'old', type: 'Circle', position: [40, 40], width: 700 }];
  const added = [{ id: 'new', type: 'Fill', width: 264 }];
  const pos = expandedNodePositions(old, added, { old: { width: 200, height: 2000 } })[0];
  assert.equal(
    intersects(
      { x: 40, y: 40, width: 700, height: 2000 },
      { x: pos[0], y: pos[1], width: 264, height: 320 },
    ),
    false,
  );
});

test('optional measurements ignore hidden, nullable, out-of-range and non-target hints', async () => {
  const old = [{ id: 'old', type: 'Circle', position: [40, 40], width: 264 }];
  const added = [{ id: 'new', type: 'Fill', width: 264 }];
  const fallback = expandedNodePositions(old, added);
  for (const hints of [
    null,
    undefined,
    {},
    { old: null },
    { old: { width: 0, height: -1 } },
    { old: { width: NaN, height: Infinity } },
    { old: { width: 551, height: 65537 } },
    { new: { width: 550, height: 65536 }, foreign: { width: 550, height: 65536 } },
  ])
    assert.deepEqual(expandedNodePositions(old, added, hints), fallback);
  assert.notDeepEqual(
    expandedNodePositions(old, added, { old: { width: 550, height: 2000 } }),
    fallback,
  );
  for (const graphMeasurements of [null, { foreign: { height: Infinity } }]) {
    const f = await fixture(),
      svg = exportSVG(f.doc);
    f.send('expand-blend', { graphMeasurements });
    assert.equal(exportSVG(f.session.snapshot()), svg);
  }
});
