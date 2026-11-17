import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  node,
  connect,
  CommandSession,
  serialize,
  deserialize,
  evaluate,
  exportSVG,
} from '../src/core.js';
function fixture() {
  const doc = blankDocument(),
    l = doc.layers[0],
    a = node('Ellipse'),
    b = node('Rectangle'),
    fa = node('Fill'),
    fb = node('Fill');
  b.params.center.value = [300, 100];
  l.nodes.push(a, b, fa, fb);
  connect(l, a.id, fa.id, 'path');
  connect(l, b.id, fb.id, 'path');
  connect(l, fa.id, l.output, 'path');
  const session = new CommandSession(doc),
    send = (type, payload = {}) =>
      session.dispatch({ ...session.state, type, payload: { layerId: l.id, ...payload } });
  return { doc, l, a, b, fa, fb, session, send };
}
test('two selected painted nodes produce a live Blend in one reversible command', () => {
  const f = fixture(),
    before = serialize(f.session.snapshot()),
    svg = exportSVG(f.session.snapshot());
  const result = f.send('blend-selected', { nodeIds: [f.fa.id, f.fb.id], position: [600, 100] });
  const after = f.session.snapshot(),
    l = after.layers[0],
    blend = l.nodes.find((n) => n.type === 'Blend');
  assert.ok(blend);
  assert.equal(result.nodeId, blend.id);
  assert.equal(l.nodes.length, f.l.nodes.length + 1);
  for (const n of f.l.nodes)
    assert.deepEqual(
      l.nodes.find((x) => x.id === n.id),
      n,
    );
  assert.equal(exportSVG(after), svg); // Existing Output is not hijacked.
  const output = evaluate({ ...l, output: blend.id }).path;
  assert.ok(output.children.length >= 2);
  assert.equal(serialize(deserialize(serialize(after))), serialize(after));
  f.send('undo');
  assert.equal(serialize(f.session.snapshot()), before);
  f.send('redo');
  assert.equal(serialize(f.session.snapshot()), serialize(after));
  f.send('set-parameter', { nodeId: f.b.id, key: 'center', value: [800, 100] });
  const changed = f.session.snapshot();
  assert.notDeepEqual(evaluate({ ...changed.layers[0], output: blend.id }).path, output);
});
for (const kind of ['same', 'missing', 'unpainted', 'output', 'count'])
  test(`invalid selection ${kind} rejects atomically`, () => {
    const f = fixture(),
      before = serialize(f.session.snapshot());
    const ids = {
      same: [f.fa.id, f.fa.id],
      missing: [f.fa.id, 'missing'],
      unpainted: [f.a.id, f.b.id],
      output: [f.fa.id, f.l.output],
      count: [f.fa.id],
    }[kind];
    assert.throws(() => f.send('blend-selected', { nodeIds: ids }));
    assert.equal(serialize(f.session.snapshot()), before);
  });
test('locked composition rejects selection authoring', () => {
  const f = fixture();
  f.doc.layers[0].locked = true;
  const s = new CommandSession(f.doc),
    before = serialize(s.snapshot());
  assert.throws(
    () =>
      s.dispatch({
        ...s.state,
        type: 'blend-selected',
        payload: { layerId: f.l.id, nodeIds: [f.fa.id, f.fb.id] },
      }),
    /locked/,
  );
  assert.equal(serialize(s.snapshot()), before);
});
