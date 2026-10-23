import test from 'node:test';
import assert from 'node:assert/strict';
import { blankDocument, node, serialize } from '../src/core.js';
import { queryShapeSources } from '../src/source-navigation.js';

function fixture() {
  const doc = blankDocument(),
    layer = doc.layers[0],
    output = layer.nodes[0];
  const add = (type, id) => {
    const n = node(type);
    n.id = id;
    layer.nodes.push(n);
    return n;
  };
  const text = add('Text', 'text'),
    curve = add('Curve', 'curve');
  text.text = 'AB';
  const outline = add('Text to Path', 'outline'),
    remap = add('Path Remap', 'remap');
  const transform = add('Transform', 'transform'),
    join = add('Join Geometry', 'join'),
    fill = add('Fill', 'fill');
  outline.params.path.source = text.id;
  remap.params.path.source = outline.id;
  transform.params.path.source = remap.id;
  join.params.a.source = transform.id;
  join.params.b.source = curve.id;
  fill.params.path.source = join.id;
  output.params.path.source = fill.id;
  const root = { documentEpoch: 1, layerId: layer.id, nodeId: output.id };
  return {
    doc,
    layer,
    output,
    text,
    curve,
    outline,
    remap,
    transform,
    join,
    fill,
    add,
    root,
    query: () => queryShapeSources(doc, root, 3),
  };
}
test('known geometry-only BFS is pure and stable under label, position and array changes', () => {
  const f = fixture(),
    before = serialize(f.doc),
    result = f.query();
  assert.equal(result.complete, true);
  assert.deepEqual(
    result.candidates.map((c) => c.nodeId),
    ['curve', 'text'],
  );
  assert.equal(result.revision, 3);
  assert.equal(serialize(f.doc), before);
  f.layer.nodes.reverse();
  f.curve.name = 'Same';
  f.text.name = 'Same';
  f.text.position = [999, 999];
  assert.deepEqual(
    f.query().candidates.map((c) => c.nodeId),
    ['curve', 'text'],
  );
});
test('numeric Position, expression refs, Text attachment and disconnected trials are excluded', () => {
  const f = fixture(),
    x = f.add('Text', 'numeric'),
    p = f.add('Curve', 'attachment');
  f.transform.params.offset.source = x.id;
  f.transform.params.offset.sourcePort = 'position';
  f.text.pathAttachment = { nodeId: p.id };
  f.fill.params.color.expression = 'ref("numeric", "value")';
  assert.deepEqual(
    f.query().candidates.map((c) => c.nodeId),
    ['curve', 'text'],
  );
});
test('shared source deduplicates while direct socket count counts both Join inputs', () => {
  const f = fixture();
  f.join.params.a.source = f.text.id;
  f.join.params.b.source = f.text.id;
  const result = f.query();
  assert.equal(result.candidates.length, 1);
  assert.equal(result.candidates[0].directInputs, 3); // Outline plus Join A/B, including trial branches.
  assert.equal(result.candidates[0].directNodes, 2);
});
test('same names keep distinct IDs; terminal Text never traverses its attachment', () => {
  const f = fixture(),
    other = f.add('Text', 'other');
  other.name = f.text.name;
  f.join.params.a.source = f.text.id;
  f.join.params.b.source = other.id;
  assert.deepEqual(
    f.query().candidates.map((c) => c.nodeId),
    ['text', 'other'],
  );
  f.root.nodeId = f.text.id;
  assert.deepEqual(
    f.query().candidates.map((c) => c.nodeId),
    ['text'],
  );
});
test('bypassed Join uses identity A; unsupported type-changing bypass is incomplete', () => {
  const f = fixture();
  f.join.bypassed = true;
  assert.deepEqual(
    f.query().candidates.map((c) => c.nodeId),
    ['text'],
  );
  f.output.bypassed = true;
  assert.equal(f.query().complete, false);
  assert.equal(f.query().candidates.length, 0);
});
test('unknown and missing branches remain explicit even when one candidate exists', () => {
  const f = fixture(),
    unknown = f.add('Mirror', 'unknown');
  f.join.params.a.source = unknown.id;
  assert.equal(f.query().complete, false);
  assert.equal(f.query().candidates.length, 1);
  f.join.params.a.source = 'deleted';
  assert.match(f.query().diagnostics[0].reason, /Missing/);
  f.join.params.a.source = f.text.id;
  f.join.params.a.sourcePort = 'position';
  assert.equal(f.query().complete, false);
  assert.deepEqual(
    f.query().candidates.map((c) => c.nodeId),
    ['curve'],
  );
});
test('Boolean searches A then B; empty Join spare is not an unknown source', () => {
  const f = fixture(),
    boolean = f.add('Boolean', 'boolean');
  boolean.params.a.source = f.text.id;
  boolean.params.b.source = f.curve.id;
  f.fill.params.path.source = boolean.id;
  assert.deepEqual(
    f.query().candidates.map((c) => c.nodeId),
    ['text', 'curve'],
  );
  f.fill.params.path.source = f.join.id;
  delete f.join.params.b.source;
  assert.equal(f.query().complete, true);
});
test('cycles, missing roots, non-content ports and depth limit terminate explicitly', () => {
  const f = fixture();
  f.transform.params.path.source = f.join.id;
  assert.equal(f.query().complete, false);
  assert.ok(f.query().diagnostics.some((d) => /Cycle/.test(d.reason)));
  f.root.nodeId = 'gone';
  assert.equal(f.query().complete, false);
  f.root.nodeId = f.text.id;
  f.root.port = 'position';
  assert.equal(f.query().complete, false);
  delete f.root.port;
  let prior = f.text;
  for (let i = 0; i < 258; i++) {
    const n = f.add('Transform', `deep${i}`);
    n.params.path.source = prior.id;
    prior = n;
  }
  f.root.nodeId = prior.id;
  assert.ok(f.query().diagnostics.some((d) => /depth/.test(d.reason)));
});
