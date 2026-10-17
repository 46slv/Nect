import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  node,
  connect,
  evaluate,
  CommandSession,
  serialize,
  deserialize,
  exportSVG,
  activeParameters,
} from '../src/core.js';
import { mirrorGeometry } from '../src/mirror-geometry.js';
const near = (a, b) => a.forEach((v, i) => assert.ok(Math.abs(v - b[i]) < 1e-8, `${a} != ${b}`));
function scene() {
  const doc = document(),
    l = doc.layers[0],
    source = node('Curve'),
    mirror = node('Mirror'),
    fill = node('Fill'),
    out = node('Output');
  source.points[0].handleA.value = [3, 4];
  source.points[0].handleB.value = [-5, 7];
  l.nodes = [source, mirror, fill, out];
  l.output = out.id;
  connect(l, source.id, mirror.id, 'path');
  connect(l, mirror.id, fill.id, 'path');
  connect(l, fill.id, out.id, 'path');
  mirror.params.center.value = [300, 200];
  return { doc, l, source, mirror };
}
test('Mirror reflects positions and relative Bezier handles about explicit axes without changing source', () => {
  const { l, source, mirror } = scene(),
    before = JSON.stringify(source);
  let r = evaluate(l);
  assert.equal(r.error, null);
  let p = r.run(mirror.id).contours[0].points[0];
  near(p.position, [350, 150]);
  near(p.handleA, [-3, 4]);
  near(p.handleB, [5, 7]);
  mirror.operation = 'up-down';
  p = evaluate(l).run(mirror.id).contours[0].points[0];
  near(p.position, [250, 250]);
  near(p.handleA, [3, -4]);
  mirror.operation = 'angle';
  mirror.params.angle.value = 45;
  p = evaluate(l).run(mirror.id).contours[0].points[1];
  near(p.position, [250, 350]);
  assert.equal(JSON.stringify(source), before);
  assert.ok(activeParameters(mirror).some(([k]) => k === 'angle'));
  mirror.operation = 'left-right';
  assert.ok(!activeParameters(mirror).some(([k]) => k === 'angle'));
});
test('Mirror live copy has separate groups, stable unique generated IDs and editable roundtrip', () => {
  const { doc, l, source, mirror } = scene();
  mirror.params.keepOriginal.value = true;
  const r = evaluate(l).run(mirror.id);
  assert.equal(r.contours.length, 2);
  assert.equal(new Set(r.contours.map((c) => c.group)).size, 2);
  assert.equal(new Set(r.contours.flatMap((c) => c.points.map((p) => p.id))).size, 6);
  near(r.contours[0].points[0].position, [250, 150]);
  near(r.contours[1].points[0].position, [350, 150]);
  assert.deepEqual(evaluate(deserialize(serialize(doc)).layers[0]).run(mirror.id), r);
  assert.equal((exportSVG(doc).match(/<path /g) || []).length, 2);
  const ids = r.contours.flatMap((c) => c.points.map((p) => p.id));
  source.points[0].position.value = [240, 150];
  const next = evaluate(l).run(mirror.id);
  near(next.contours[1].points[0].position, [360, 150]);
  assert.deepEqual(
    next.contours.flatMap((c) => c.points.map((p) => p.id)),
    ids,
  );
  assert.equal(next.unstable, true);
  mirror.bypassed = true;
  assert.deepEqual(evaluate(l).run(mirror.id), evaluate(l).run(source.id));
});
test('Mirror keeps hole contours together in each copy and rejects excessive groups', () => {
  const pt = { id: 'p', position: [1, 2], handleA: [0, 1], handleB: [1, 0] };
  const contour = { closed: true, points: [pt] };
  const source = {
    contours: [
      { ...contour, id: 'outer' },
      { ...contour, id: 'hole' },
    ],
  };
  const options = { center: [0, 0], angle: 90, keepOriginal: true, prefix: 'm' };
  const r = mirrorGeometry(source, options);
  assert.equal(r.contours[0].group, r.contours[1].group);
  assert.equal(r.contours[2].group, r.contours[3].group);
  assert.notEqual(r.contours[0].group, r.contours[2].group);
  assert.throws(
    () =>
      mirrorGeometry(
        { contours: Array.from({ length: 129 }, (_, i) => ({ ...contour, group: String(i) })) },
        options,
      ),
    /budget/,
  );
  assert.throws(() => mirrorGeometry(source, { ...options, angle: Infinity }), /finite/);
  assert.throws(() => mirrorGeometry(null, options), /Connect/);
});
test('Mirror common commands are atomic, undoable, locked and preserve source on invalid edits', () => {
  const { doc, l, mirror } = scene(),
    session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: l.id, nodeId: mirror.id, ...payload },
    });
  const original = session.snapshot();
  send('set-parameter', { key: 'keepOriginal', value: true });
  const changed = session.snapshot();
  send('undo');
  assert.deepEqual(session.snapshot(), original);
  send('redo');
  assert.deepEqual(session.snapshot(), changed);
  assert.throws(() => send('set-operation', { operation: 'invalid' }));
  assert.deepEqual(session.snapshot(), changed);
  send('set-parameter-lock', { key: 'center', locked: true });
  const locked = session.snapshot();
  assert.throws(() => send('set-parameter', { key: 'center', value: [0, 0] }), /lock/i);
  assert.deepEqual(session.snapshot(), locked);
  const lockedDoc = structuredClone(doc);
  lockedDoc.layers[0].locked = true;
  const lockedSession = new CommandSession(lockedDoc);
  assert.throws(
    () =>
      lockedSession.dispatch({
        ...lockedSession.state,
        type: 'set-parameter',
        payload: { layerId: l.id, nodeId: mirror.id, key: 'keepOriginal', value: true },
      }),
    /lock/i,
  );
});
