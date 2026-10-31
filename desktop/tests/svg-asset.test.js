import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import {
  CommandSession,
  blankDocument,
  document,
  node,
  connect,
  serialize,
  deserialize,
  evaluate,
  exportSVG,
  nodeDependencies,
  validate,
} from '../src/core.js';
import { readSVGSource } from '../src/svg-tree.js';
import { resolveArtworkMoveTarget } from '../src/artwork-move-target.js';
const Parser = new Window().DOMParser;
const markup =
  '<svg width="120" height="80" viewBox="10 20 120 80"><rect x="10" y="20" width="50" height="40" fill="#e00000" fill-opacity="0.5"/><path d="M20 30H80" fill="none" stroke="#0044ff" stroke-width="3"/></svg>';
const tree = (text = markup) => readSVGSource(text, Parser);
const provenance = () => ({
  version: 1,
  mode: 'embedded',
  rootId: 'folder-fixture',
  relativePath: 'marks/mark.svg',
  sourceLocation: 'owned-fixture/marks/mark.svg',
  sha256: 'a'.repeat(64),
  author: 'Nect test',
  source: 'Self-authored',
  license: 'Test fixture',
});
test('library raster provenance is adopted before immutable-asset freezing and survives Undo/reopen', () => {
  const f = fixture(blankDocument()),
    before = serialize(f.s.snapshot());
  const png =
    'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP4z8DwHwAFAAH/iZk9HQAAAABJRU5ErkJggg==';
  const result = f.send('adopt-image', {
    name: 'photo.png',
    png,
    provenance: { ...provenance(), relativePath: 'photo.png' },
  });
  const after = f.s.snapshot();
  assert.equal(after.imageAssets[result.assetId].provenance.relativePath, 'photo.png');
  assert.equal(after.schemaVersion, 53);
  assert.equal(exportSVG(deserialize(serialize(after))), exportSVG(after));
  f.send('undo');
  assert.equal(serialize(f.s.snapshot()), before);
  f.send('redo');
  assert.deepEqual(f.s.snapshot(), after);
  const state = f.s.state,
    json = serialize(f.s.snapshot());
  assert.throws(() =>
    f.send('adopt-image', {
      name: 'bad.png',
      png,
      provenance: { ...provenance(), relativePath: '../bad.png' },
    }),
  );
  assert.deepEqual(f.s.state, state);
  assert.equal(serialize(f.s.snapshot()), json);
});
function fixture(doc = document()) {
  doc.layers[0].nodes.find((n) => n.id === doc.layers[0].output).params.path.locked = false;
  const s = new CommandSession(doc);
  const send = (type, payload = {}) => s.dispatch({ ...s.state, type, payload });
  const add = (options = {}) =>
    send('insert-svg-asset', {
      route: 'graph',
      layerId: doc.layers[0].id,
      source: tree(),
      name: 'mark.svg',
      provenance: provenance(),
      ...options,
    });
  return { s, send, add, doc };
}
test('new layer is SVG → painted Join → Out; existing identity/paint and single Undo/cold reopen', () => {
  const f = fixture(),
    before = f.s.snapshot(),
    json = serialize(before);
  const result = f.add({ route: 'layer', at: [70, 45] });
  const after = f.s.snapshot(),
    target = after.layers.find((l) => l.id === result.layerId),
    ref = target.nodes.find((n) => n.id === result.nodeId);
  assert.deepEqual(after.layers.slice(0, before.layers.length), before.layers);
  assert.equal(ref.type, 'SVG');
  assert.deepEqual(ref.params.position.value, [70, 45]);
  assert.deepEqual(ref.svgAsset, provenance());
  const join = target.nodes.find((n) => n.id === result.joinId);
  assert.equal(join.paintMode, true);
  assert.equal(join.params.a.source, ref.id);
  assert.equal(target.nodes.find((n) => n.id === target.output).params.path.source, join.id);
  assert.equal(
    target.nodes.some((n) => n.type === 'Fill'),
    false,
  );
  assert.equal(evaluate(target, {}, after.imageAssets, after).error, null);
  const svg = exportSVG(after);
  assert.match(svg, /#e00000/);
  assert.match(svg, /fill-opacity="0.5"/);
  assert.match(svg, /stroke-width="3"/);
  assert.equal(exportSVG(deserialize(serialize(after))), svg);
  assert.equal(f.s.historyView.past.length, 1);
  f.send('undo');
  assert.equal(serialize(f.s.snapshot()), json);
  f.send('redo');
  assert.deepEqual(f.s.snapshot(), after);
});
test('graph default is disconnected and preserves every existing node even with locked Output', () => {
  const d = document();
  const output = d.layers[0].nodes.find((n) => n.id === d.layers[0].output);
  output.params.path.locked = true;
  const s = new CommandSession(d);
  const before = s.snapshot();
  const result = s.dispatch({
    ...s.state,
    type: 'insert-svg-asset',
    payload: {
      route: 'graph',
      layerId: d.layers[0].id,
      source: tree(),
      name: 'mark.svg',
      provenance: provenance(),
    },
  });
  const after = s.snapshot();
  assert.deepEqual(after.layers[0].nodes.slice(0, -1), before.layers[0].nodes);
  assert.equal(result.joinId, null);
  assert.equal(after.layers[0].nodes.at(-1).type, 'SVG');
  assert.equal(exportSVG(after), exportSVG(before));
});
test('new SVG layer supports artboard-space movement without changing embedded source geometry', () => {
  const f = fixture(blankDocument()),
    result = f.add({ route: 'layer' });
  const before = f.s.snapshot(),
    target = resolveArtworkMoveTarget(before, result.layerId);
  assert.equal(target.supported, true, target.reason);
  f.send('translate-artworks', { layerIds: [result.layerId], delta: [32, 18] });
  const after = f.s.snapshot();
  assert.deepEqual(
    after.layers.find((l) => l.id === result.sourceLayerId),
    before.layers.find((l) => l.id === result.sourceLayerId),
  );
  assert.deepEqual(
    after.layers.find((l) => l.id === result.layerId).nodes.find((n) => n.id === result.nodeId)
      .params.position.value,
    [32, 18],
  );
});
test('graph safe join preserves bare Path paint and creates Join even for empty Output', () => {
  for (const doc of [document(), blankDocument()]) {
    const f = fixture(doc),
      before = f.s.snapshot();
    const result = f.add({ joinBeforeOut: true });
    const after = f.s.snapshot(),
      layer = after.layers[0],
      svg = layer.nodes.find((n) => n.id === result.nodeId),
      join = layer.nodes.find((n) => n.id === result.joinId);
    assert.equal(join.type, 'Join Geometry');
    assert.equal(join.paintMode, true);
    assert.equal(layer.nodes.find((n) => n.id === layer.output).params.path.source, join.id);
    assert.equal(
      layer.nodes.some((n) => n.type === 'Fill' && n.params.path.source === svg.id),
      false,
    );
    assert.equal(evaluate(layer, {}, after.imageAssets, after).error, null);
    assert.equal(before.layers[0].id, layer.id);
  }
});
test('exclusive terminal painted Join reuses trailing slot and preserves previous order and IDs', () => {
  const f = fixture();
  const first = f.add({ joinBeforeOut: true });
  const before = f.s.snapshot();
  const second = f.add({ joinBeforeOut: true }),
    after = f.s.snapshot();
  assert.equal(second.joinId, first.joinId);
  assert.equal(second.reusedJoin, true);
  const old = before.layers[0].nodes.find((n) => n.id === first.joinId),
    join = after.layers[0].nodes.find((n) => n.id === first.joinId);
  for (const key of old.joinInputOrder)
    if (old.params[key].source) assert.deepEqual(join.params[key], old.params[key]);
  const used = join.joinInputOrder.filter((k) => join.params[k].source);
  assert.equal(join.params[used.at(-1)].source, second.nodeId);
  assert.equal(after.layers[0].nodes.filter((n) => n.type === 'Join Geometry').length, 1);
});
for (const kind of ['shared', 'inactive-consumer', 'locked', 'bypassed'])
  test(`${kind} terminal Join is wrapped without mutation`, () => {
    const f = fixture();
    const first = f.add({ joinBeforeOut: true });
    const d = f.s.snapshot(),
      layer = d.layers[0],
      join = layer.nodes.find((n) => n.id === first.joinId);
    if (kind === 'locked') join.locked = true;
    else if (kind === 'bypassed') join.bypassed = true;
    else {
      const consumer = node('Transform');
      layer.nodes.push(consumer);
      connect(layer, join.id, consumer.id, 'path');
      if (kind === 'inactive-consumer') consumer.bypassed = true;
      assert.ok(nodeDependencies(consumer).some((d) => d.source === join.id));
    }
    const second = fixture(d);
    const before = second.s.snapshot();
    const result = second.add({ joinBeforeOut: true });
    const after = second.s.snapshot();
    assert.equal(result.reusedJoin, false);
    assert.notEqual(result.joinId, first.joinId);
    assert.deepEqual(
      after.layers[0].nodes.find((n) => n.id === first.joinId),
      before.layers[0].nodes.find((n) => n.id === first.joinId),
    );
  });
test('repeated placement mints independent SVG/source identities and edits do not update other copies', () => {
  const f = fixture(blankDocument()),
    first = f.add({ route: 'layer' }),
    second = f.add({ route: 'layer' });
  assert.notEqual(first.nodeId, second.nodeId);
  assert.notEqual(first.sourceLayerId, second.sourceLayerId);
  const before = f.s.snapshot(),
    source = before.layers.find((l) => l.id === first.sourceLayerId),
    fill = source.nodes.find((n) => n.type === 'Fill');
  f.send('set-parameter', {
    layerId: source.id,
    nodeId: fill.id,
    key: 'color',
    value: { hex: '#00ff00', alpha: 1 },
  });
  const after = f.s.snapshot();
  assert.deepEqual(
    after.layers.find((l) => l.id === second.sourceLayerId),
    before.layers.find((l) => l.id === second.sourceLayerId),
  );
  assert.match(exportSVG(after), /#00ff00/);
  assert.match(exportSVG(after), /#e00000/);
});
for (const text of [
  '<svg><script>alert(1)</script></svg>',
  '<svg><image href="https://example.test/private.png"/></svg>',
  '<svg><text>font</text></svg>',
  '<svg><foreignObject/></svg>',
])
  test('unsupported SVG refuses atomically and preserves redo ' + text.slice(0, 25), () => {
    const f = fixture();
    f.add();
    f.send('undo');
    const before = serialize(f.s.snapshot()),
      state = f.s.state,
      future = f.s.historyView.future.length;
    assert.throws(() => f.add({ source: tree(text) }));
    assert.equal(serialize(f.s.snapshot()), before);
    assert.deepEqual(f.s.state, state);
    assert.equal(f.s.historyView.future.length, future);
  });
test('malformed declarations and hostile portable syntax reject; stale envelope and locks preserve state', () => {
  assert.throws(() => tree('<!DOCTYPE svg [<!ENTITY x SYSTEM "file:///private">]><svg/>'));
  const f = fixture(),
    before = serialize(f.s.snapshot());
  assert.throws(() => f.add({ source: { tag: 'svg', attributes: [], children: [], evil: true } }));
  assert.throws(() => f.add({ provenance: { ...provenance(), relativePath: '../mark.svg' } }));
  assert.throws(() => f.add({ route: 'unknown' }));
  assert.equal(serialize(f.s.snapshot()), before);
  const envelope = f.s.state;
  f.add();
  assert.throws(
    () => f.s.dispatch({ ...envelope, type: 'insert-svg-asset', payload: {} }),
    /STALE_REVISION/,
  );
  const locked = f.s.snapshot();
  locked.layers[0].locked = true;
  const g = fixture(locked),
    json = serialize(g.s.snapshot());
  assert.throws(() => g.add(), /unlocked/);
  assert.equal(serialize(g.s.snapshot()), json);
});
test('schema and missing source are explicit; source file is unnecessary for cold reopen', () => {
  const f = fixture();
  const result = f.add({ route: 'layer' });
  const d = f.s.snapshot();
  assert.equal(d.schemaVersion, 53);
  const cold = new CommandSession(deserialize(serialize(d)));
  assert.equal(exportSVG(cold.snapshot()), exportSVG(d));
  const old = structuredClone(d);
  old.schemaVersion = 52;
  assert.throws(() => validate(old), /schemaVersion 53/);
  d.layers = d.layers.filter((l) => l.id !== result.sourceLayerId);
  assert.throws(() => exportSVG(d), /missing|Missing/);
});
test('SVG insertion over the node budget refuses its source, graph and history atomically', () => {
  const d = blankDocument();
  while (d.layers[0].nodes.length < 2000) d.layers[0].nodes.push(node('Value'));
  const f = fixture(d);
  f.send('rename', { layerId: d.layers[0].id, name: 'Renamed' });
  f.send('undo');
  const before = serialize(f.s.snapshot()),
    state = f.s.state,
    future = f.s.historyView.future.length;
  assert.throws(() => f.add(), /nodes|limit|budget/i);
  assert.equal(serialize(f.s.snapshot()), before);
  assert.deepEqual(f.s.state, state);
  assert.equal(f.s.historyView.future.length, future);
});
