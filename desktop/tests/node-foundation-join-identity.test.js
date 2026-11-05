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
  CommandSession,
  setNodeBypassed,
  remapFingerprintMatches,
} from '../src/core.js';
const currentSource = 'sha256:bcdcfabe3115218642a036d557c290d87527979e2eca4744c3297ebeb5cd1149';
const currentGroup = 'sha256:338fab01feb1d0587df19ac8de535aa0b634d1417673eb88904eb8e206551f34';
const canonicalSource = 'sha256:fdac2e16e2d57ac4881efb8fcc0aeb286f9d831627631abcba06bcdc49b8eff7';
const canonicalGroup = 'sha256:01c1353863de6f5babae2c11ffa560282f06a8ba5fdd249faec1e560680fb0fd';
function id(n, value) {
  n.id = value;
  return n;
}
function base(nodes) {
  const doc = document(),
    layer = doc.layers[0],
    out = id(node('Output'), 'out');
  doc.schemaVersion = 31;
  layer.id = 'layer';
  layer.nodes = [...nodes, out];
  layer.output = out.id;
  return { doc, layer, out };
}

test('Painted Join retains distinct historical/current identity through clone and cache', () => {
  const curve = id(node('Curve'), 'curve'),
    close = id(node('Set Closure'), 'close'),
    fill = id(node('Fill'), 'fill'),
    join = id(node('Join Geometry'), 'join');
  curve.points.forEach((p, i) => {
    p.id = `p${i}`;
  });
  const { doc, layer, out } = base([curve, close, fill, join]);
  connect(layer, curve.id, close.id, 'path');
  connect(layer, close.id, fill.id, 'path');
  connect(layer, fill.id, join.id, 'a');
  connect(layer, join.id, out.id, 'path');
  assert.doesNotThrow(() => deserialize(serialize(doc)));
  const before = serialize(doc),
    first = evaluate(layer),
    closure = first.run(close.id),
    filled = first.run(fill.id),
    group = first.run(join.id),
    second = evaluate(layer),
    cached = second.run(join.id);
  for (const item of [closure, filled, group.children[0], cached.children[0]]) {
    assert.equal(item.fingerprint, canonicalSource);
    assert.equal(remapFingerprintMatches(item, canonicalSource), true);
    assert.equal(remapFingerprintMatches(item, currentSource), true);
    assert.equal(remapFingerprintMatches(item, 'unrelated'), false);
    assert.equal(remapFingerprintMatches(structuredClone(item), currentSource), false);
  }
  for (const item of [group, cached]) {
    assert.equal(item.fingerprint, canonicalGroup);
    assert.equal(remapFingerprintMatches(item, canonicalGroup), true);
    assert.equal(remapFingerprintMatches(item, currentGroup), true);
    assert.equal(remapFingerprintMatches(item, 'unrelated'), false);
    assert.equal(remapFingerprintMatches(structuredClone(item), currentGroup), false);
  }
  assert.deepEqual(first.stats, { computed: 5, reused: 0 });
  assert.deepEqual(second.stats, { computed: 0, reused: 5 });
  assert.equal(serialize(doc), before);
});

test('valid runtime distinguishes absent, zero-point, zero-contour, painted empty and disconnect', () => {
  for (const kind of [
    'unconnected',
    'bypass-null',
    'zero-point',
    'path-empty',
    'painted-empty',
    'painted-disconnected',
  ]) {
    const join = id(node('Join Geometry'), 'join'),
      source = id(node(kind === 'zero-point' ? 'Curve' : 'Multi Path'), 'source'),
      fill = id(node('Fill'), 'fill');
    if (kind === 'zero-point') source.points = [];
    const { doc, layer } = base([source, fill, join]);
    if (kind === 'path-empty' || kind === 'zero-point') connect(layer, source.id, join.id, 'a');
    if (kind === 'painted-empty' || kind === 'painted-disconnected') {
      connect(layer, source.id, fill.id, 'path');
      connect(layer, fill.id, join.id, 'a');
      if (kind === 'painted-disconnected') disconnect(join, 'a');
    }
    if (kind === 'bypass-null') setNodeBypassed(layer, join.id, true);
    assert.equal(serialize(deserialize(serialize(doc))), serialize(doc));
    const session = new CommandSession(doc),
      send = (type, payload = {}) =>
        session.dispatch({ ...session.state, type, payload: { layerId: layer.id, ...payload } });
    send('set-parameter-lock', { nodeId: join.id, key: 'b', locked: true });
    send('undo');
    const before = serialize(session.snapshot()),
      history = JSON.stringify(session.historyView),
      rev = session.state.expectedRevision;
    const current = session.snapshot().layers[0];
    if (kind === 'unconnected' || kind === 'painted-disconnected')
      assert.throws(() => evaluate(current).run(join.id), /Connect a path to Join Geometry/);
    else if (kind === 'bypass-null')
      assert.throws(() => evaluate(current).run(join.id), /Connect a to bypass Join Geometry/);
    else {
      const value = evaluate(current).run(join.id);
      assert.equal(value.contours.length, kind === 'zero-point' ? 1 : 0);
      if (kind === 'zero-point') assert.equal(value.contours[0].points.length, 0);
      if (kind === 'painted-empty') assert.equal(value.kind, 'painted-group');
    }
    assert.equal(serialize(session.snapshot()), before);
    assert.equal(JSON.stringify(session.historyView), history);
    assert.equal(session.state.expectedRevision, rev);
    send('redo');
    assert.equal(
      session.snapshot().layers[0].nodes.find((n) => n.id === join.id).params.b.locked,
      true,
    );
  }
});
