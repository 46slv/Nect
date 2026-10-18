import test from 'node:test';
import assert from 'node:assert/strict';
import { document, node, connect, serialize, evaluate, CommandSession } from '../src/core.js';
import { directSelectionSource } from '../src/direct-selection.js';
import { transformCharacterPoint, multiplyMatrices } from '../src/character-geometry.js';
function fixture() {
  const doc = document(),
    l = doc.layers[0],
    curve = node('Curve'),
    a = node('Transform'),
    b = node('Transform'),
    fill = node('Fill'),
    out = node('Output');
  l.nodes = [curve, a, b, fill, out];
  l.output = out.id;
  connect(l, curve.id, a.id, 'path');
  connect(l, a.id, b.id, 'path');
  connect(l, b.id, fill.id, 'path');
  connect(l, fill.id, out.id, 'path');
  a.params.scale.value = 1.7;
  a.params.rotation.value = 25;
  a.params.offset.value = [35, -12];
  b.params.scale.value = -0.8;
  b.params.rotation.value = -15;
  b.params.offset.value = [230, 180];
  const session = new CommandSession(doc),
    send = (type, payload = {}) =>
      session.dispatch({
        ...session.state,
        type,
        payload: { layerId: l.id, nodeId: curve.id, ...payload },
      });
  return { doc, l, curve, a, b, session, send };
}
test('direct source resolves nested affine geometry by stable IDs without changing authored data', () => {
  const f = fixture(),
    before = serialize(f.doc),
    r = directSelectionSource(f.doc, f.l.id),
    evaluation = evaluate(f.l);
  assert.equal(r.supported, true, r.reason);
  assert.equal(r.nodeId, f.curve.id);
  assert.deepEqual(
    r.matrix,
    multiplyMatrices(
      evaluation.transformValues(f.b.id).matrix,
      evaluation.transformValues(f.a.id).matrix,
    ),
  );
  const raw = evaluation.run(f.curve.id),
    p = raw.contours[0].points[0],
    q = r.path.contours[0].points[0];
  assert.equal(q.id, p.id);
  assert.deepEqual(q.position, transformCharacterPoint(r.matrix, p.position));
  assert.deepEqual(q.handleA, transformCharacterPoint([...r.matrix.slice(0, 4), 0, 0], p.handleA));
  assert.equal(serialize(f.doc), before);
  f.a.params.offset.expression = 'value';
  assert.equal(directSelectionSource(f.doc, f.l.id).supported, false);
  f.a.params.offset.expression = '';
  f.l.locked = true;
  assert.equal(directSelectionSource(f.doc, f.l.id).locked, true);
  const generated = node('Circle');
  f.l.nodes.push(generated);
  connect(f.l, generated.id, f.a.id, 'path');
  assert.equal(directSelectionSource(f.doc, f.l.id).supported, false);
});
test('point nudge is one atomic command and retains handles; late missing/locked/driven points reject the whole edit', () => {
  const f = fixture(),
    ids = f.curve.points.slice(0, 2).map((p) => p.id),
    before = serialize(f.session.snapshot());
  f.send('translate-points', { pointIds: ids, delta: [1, -10] });
  let points = f.session.snapshot().layers[0].nodes[0].points;
  for (let i = 0; i < 2; i++) {
    assert.deepEqual(
      points[i].position.value,
      f.curve.points[i].position.value.map((v, k) => v + [1, -10][k]),
    );
    assert.deepEqual(points[i].handleA, f.curve.points[i].handleA);
  }
  assert.equal(f.session.historyView.past.length, 1);
  f.send('undo');
  assert.equal(serialize(f.session.snapshot()), before);
  assert.throws(
    () => f.send('translate-points', { pointIds: [ids[0], 'missing'], delta: [1, 0] }),
    /Missing/,
  );
  assert.equal(serialize(f.session.snapshot()), before);
  f.send('set-parameter-lock', { key: `${ids[1]}.position`, locked: true });
  const locked = serialize(f.session.snapshot());
  assert.throws(() => f.send('translate-points', { pointIds: ids, delta: [1, 0] }), /locked/);
  assert.equal(serialize(f.session.snapshot()), locked);
});
test('explicit Remap nudge binds generated provenance and refuses suspended edits; generators stay read-only', () => {
  const doc = document(),
    l = doc.layers[0],
    remap = l.nodes[3],
    input = evaluate(l).run(remap.params.path.source),
    id = input.contours[0].points[0].id,
    s = new CommandSession(doc),
    send = (type, payload) =>
      s.dispatch({ ...s.state, type, payload: { layerId: l.id, ...payload } });
  send('translate-points', { nodeId: remap.id, pointIds: [id], delta: [1, 2] });
  let live = s.snapshot().layers[0].nodes.find((n) => n.id === remap.id);
  assert.equal(live.edits.length, 1);
  assert.equal(live.edits[0].fingerprint, input.fingerprint);
  send('set-parameter', { nodeId: l.nodes[0].id, key: 'radius', value: 91 });
  const changed = serialize(s.snapshot());
  assert.throws(
    () => send('translate-points', { nodeId: remap.id, pointIds: [id], delta: [1, 2] }),
    /suspended/,
  );
  assert.equal(serialize(s.snapshot()), changed);
  assert.throws(
    () => send('translate-points', { nodeId: l.nodes[0].id, pointIds: [id], delta: [1, 2] }),
    /editable/,
  );
  assert.equal(serialize(s.snapshot()), changed);
});
test('independent handle drag changes one vector and corner mode in one Undo, leaving the opposite handle unchanged', () => {
  const f = fixture(),
    p = f.curve.points[0];
  f.send('set-point-mode', { pointId: p.id, mode: 'symmetric' });
  f.send('set-parameter', { key: `${p.id}.handleB`, value: [20, 10] });
  const before = serialize(f.session.snapshot()),
    old = f.session.snapshot().layers[0].nodes[0].points[0],
    target = {
      kind: 'curve-point',
      layerId: f.l.id,
      nodeId: f.curve.id,
      pointId: p.id,
      key: 'handleB',
    };
  let token = f.send('begin-gesture', { target });
  token = f.send('update-gesture', { ...token, update: { delta: [7, -3], independent: true } });
  const preview = f.session.previewSnapshot(token.gestureId).document.layers[0].nodes[0].points[0];
  assert.equal(preview.handleType, 'corner');
  assert.deepEqual(preview.handleA, old.handleA);
  assert.deepEqual(
    preview.handleB.value,
    old.handleB.value.map((v, k) => v + [7, -3][k]),
  );
  assert.equal(serialize(f.session.snapshot()), before);
  f.send('commit-gesture', token);
  f.send('undo');
  assert.equal(serialize(f.session.snapshot()), before);
});
