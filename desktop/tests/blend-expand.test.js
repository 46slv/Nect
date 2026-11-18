import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  node,
  connect,
  CommandSession,
  exportSVG,
  serialize,
  deserialize,
} from '../src/core.js';
function fixture(strokeOnly = false) {
  const doc = blankDocument(),
    layer = doc.layers[0],
    a = node('Rectangle'),
    b = node('Rectangle'),
    blend = node('Blend');
  a.params.center.value = [100, 100];
  b.params.center.value = [300, 100];
  blend.params.steps.value = 1;
  const pa = node(strokeOnly ? 'Stroke' : 'Fill'),
    pb = node(strokeOnly ? 'Stroke' : 'Fill');
  pa.params.color.value = { hex: '#000000', alpha: 0.5 };
  pb.params.color.value = { hex: '#ffffff', alpha: 1 };
  layer.nodes.push(a, b, pa, pb, blend);
  connect(layer, a.id, pa.id, 'path');
  connect(layer, b.id, pb.id, 'path');
  connect(layer, pa.id, blend.id, 'a');
  connect(layer, pb.id, blend.id, 'b');
  connect(layer, blend.id, layer.output, 'path');
  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: layer.id, nodeId: blend.id, ...payload },
    });
  return { session, send, layer, a, b, blend };
}
for (const strokeOnly of [false, true])
  test(`expand Blend preserves ${strokeOnly ? 'stroke-only' : 'fill'} appearance, sources and history`, () => {
    const { session, send, a, b, blend } = fixture(strokeOnly),
      before = serialize(session.snapshot()),
      svg = exportSVG(session.snapshot());
    send('expand-blend');
    const expanded = session.snapshot(),
      after = serialize(expanded);
    assert.equal(expanded.layers[0].nodes.find((n) => n.id === blend.id).type, 'Join Geometry');
    assert.deepEqual(
      expanded.layers[0].nodes.find((n) => n.id === a.id),
      a,
    );
    assert.deepEqual(
      expanded.layers[0].nodes.find((n) => n.id === b.id),
      b,
    );
    assert.equal(expanded.layers[0].nodes.filter((n) => n.type === 'Multi Path').length, 3);
    assert.equal(exportSVG(expanded), svg);
    assert.equal(exportSVG(deserialize(after)), svg);
    send('undo');
    assert.equal(serialize(session.snapshot()), before);
    send('redo');
    assert.equal(serialize(session.snapshot()), after);
    send('set-parameter', { nodeId: a.id, key: 'center', value: [700, 700] });
    assert.equal(exportSVG(session.snapshot()), svg);
  });
test('expansion refuses invalid live output atomically', () => {
  const { session, send, blend } = fixture();
  send('set-parameter', { nodeId: blend.id, key: 'steps', value: 0 });
  // Break B through the shared command, then verify expansion has no partial commit.
  send('disconnect', { key: 'b' });
  const before = serialize(session.snapshot());
  assert.throws(() => send('expand-blend'));
  assert.equal(serialize(session.snapshot()), before);
});

test('expanded point edits are independent and persisted', () => {
  const { session, send } = fixture();
  send('expand-blend');
  const path = session.snapshot().layers[0].nodes.find((n) => n.type === 'Multi Path');
  const before = exportSVG(session.snapshot());
  send('set-parameter', { nodeId: path.id, key: `${path.points[0].id}.position`, value: [50, 80] });
  const changed = exportSVG(session.snapshot());
  assert.notEqual(changed, before);
  assert.equal(exportSVG(deserialize(serialize(session.snapshot()))), changed);
});
test('locked Blend expansion leaves authored state unchanged', () => {
  const f = fixture(),
    doc = f.session.snapshot();
  doc.layers[0].nodes.find((n) => n.id === f.blend.id).locked = true;
  const session = new CommandSession(doc),
    before = serialize(session.snapshot());
  assert.throws(
    () =>
      session.dispatch({
        ...session.state,
        type: 'expand-blend',
        payload: { layerId: f.layer.id, nodeId: f.blend.id },
      }),
    /Unlock/,
  );
  assert.equal(serialize(session.snapshot()), before);
});
