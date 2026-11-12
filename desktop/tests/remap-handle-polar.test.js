import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  CommandSession,
  connect,
  enableRemapPoint,
  evaluate,
  node,
  point,
} from '../src/core.js';
import * as core from '../src/core.js';
import { remapFixture } from './helpers/remap-fingerprint-fixture.js';

function fixture(kind = 'raw') {
  const doc = blankDocument(),
    layer = doc.layers[0],
    curve = node('Curve'),
    remap = node('Path Remap'),
    fill = node('Fill'),
    out = node('Output'),
    first = point(100, 100);
  first.handleB.value = [0.1, 0];
  curve.points = [first, point(200, 100), point(100, 200)];
  layer.nodes = [curve, remap, fill, out];
  layer.output = out.id;
  connect(layer, curve.id, remap.id, 'path');
  connect(layer, remap.id, fill.id, 'path');
  connect(layer, fill.id, out.id, 'path');
  if (kind === 'raw') remap.edits = [{ pointId: first.id, handleB: [0.2, 0] }];
  if (kind === 'typed') {
    const source = evaluate(layer, {}, doc.imageAssets, doc).run(curve.id);
    enableRemapPoint(remap, source, first.id).params.handleB.value = [0.2, 0];
  }
  const session = new CommandSession(doc),
    send = (type, payload) => session.dispatch({ ...session.state, type, payload }),
    target = {
      kind: 'remap-point',
      layerId: layer.id,
      nodeId: remap.id,
      pointId: first.id,
      key: 'handleB',
    };
  const output = (document) =>
    evaluate(document.layers[0], {}, document.imageAssets, document)
      .run(remap.id)
      .contours.flatMap((contour) => contour.points)
      .find((entry) => entry.id === first.id).handleB;
  return { session, send, target, output, remap, first, layer };
}

for (const kind of ['raw', 'typed'])
  test(`absolute Remap handle target creates exact zero in one Undo (${kind})`, () => {
    const f = fixture(kind),
      before = f.session.snapshot();
    assert.deepEqual(f.output(before), [0.30000000000000004, 0]);
    let token = f.send('begin-gesture', { target: f.target });
    token = f.send('update-gesture', { ...token, update: { value: [-0.1, 0] } });
    assert.deepEqual(f.output(f.session.previewSnapshot(token.gestureId).document), [0, 0]);
    assert.deepEqual(f.session.snapshot(), before, 'preview must not mutate authored state');
    f.send('commit-gesture', token);
    const after = f.session.snapshot();
    assert.deepEqual(f.output(after), [0, 0]);
    assert.equal(f.session.historyView.past.length, 1);
    f.send('undo');
    assert.deepEqual(f.session.snapshot(), before);
    f.send('redo');
    assert.deepEqual(f.session.snapshot(), after);
  });

test('absolute Remap no-op preserves sparse state, Undo and Redo', () => {
  const f = fixture('none');
  let token = f.send('begin-gesture', { target: f.target });
  token = f.send('update-gesture', { ...token, update: { delta: [1, 0] } });
  f.send('commit-gesture', token);
  f.send('undo');
  const before = f.session.historyView.doc;
  token = f.send('begin-gesture', { target: f.target });
  token = f.send('update-gesture', { ...token, update: { value: [0, 0] } });
  f.send('commit-gesture', token);
  assert.equal(f.session.historyView.doc, before);
  assert.equal(f.session.historyView.past.length, 0);
  assert.equal(f.session.historyView.future.length, 1);
  assert.deepEqual(f.session.snapshot().layers[0].nodes[1].edits, []);
});

test('absolute Remap preview returning to origin preserves raw authored values', () => {
  const f = fixture('raw'),
    before = f.session.historyView.doc;
  let token = f.send('begin-gesture', { target: f.target });
  token = f.send('update-gesture', { ...token, update: { value: [-0.1, 0] } });
  token = f.send('update-gesture', { ...token, update: { value: [0.2, 0] } });
  f.send('commit-gesture', token);
  assert.equal(f.session.historyView.doc, before);
  assert.deepEqual(f.session.snapshot().layers[0].nodes[1].edits, [
    { pointId: f.first.id, handleB: [0.2, 0] },
  ]);
});

test('absolute Remap rejects invalid and ambiguous updates without partial commit', () => {
  const f = fixture('typed'),
    before = f.session.snapshot();
  let token = f.send('begin-gesture', { target: f.target });
  for (const update of [{ value: [NaN, 0] }, { value: [0] }, { value: [0, 0], delta: [1, 1] }])
    assert.throws(() => f.send('update-gesture', { ...token, update }));
  f.send('cancel-gesture', token);
  assert.deepEqual(f.session.snapshot(), before);
  assert.equal(f.session.historyView.past.length, 0);
});

test('absolute Remap respects locked, driven and suspended targets', () => {
  for (const key of ['locked', 'expression', 'source']) {
    const original = fixture('typed'),
      doc = original.session.snapshot(),
      q = doc.layers[0].nodes[1].edits[0].params.handleB;
    if (key === 'locked') q.locked = true;
    else if (key === 'expression') q.expression = '1; 2';
    else {
      const vector = node('Vector');
      doc.layers[0].nodes.push(vector);
      q.source = vector.id;
    }
    const session = new CommandSession(doc),
      before = session.snapshot();
    assert.throws(
      () =>
        session.dispatch({
          ...session.state,
          type: 'begin-gesture',
          payload: { target: original.target },
        }),
      /locked or driven/,
    );
    assert.deepEqual(session.snapshot(), before);
  }
  const f = remapFixture(core),
    source = evaluate(f.doc.layers[0], {}, f.doc.imageAssets, f.doc).run(f.source.id),
    pointId = source.contours[0].points[0].id,
    edit = enableRemapPoint(f.remap, source, pointId);
  assert.equal(source.unstable, true);
  edit.fingerprint = 'not-the-source';
  const session = new CommandSession(f.doc),
    before = session.snapshot(),
    target = {
      kind: 'remap-point',
      layerId: f.doc.layers[0].id,
      nodeId: f.remap.id,
      pointId,
      key: 'handleB',
    };
  assert.throws(
    () => session.dispatch({ ...session.state, type: 'begin-gesture', payload: { target } }),
    /suspended/,
  );
  assert.deepEqual(session.snapshot(), before);
});

test('absolute Remap value cannot address multiple selected points', () => {
  const f = fixture('none'),
    second = f.session.snapshot().layers[0].nodes[0].points[1].id,
    before = f.session.snapshot();
  let token = f.send('begin-gesture', {
    target: { ...f.target, key: 'position', pointIds: [f.first.id, second] },
  });
  assert.throws(
    () => f.send('update-gesture', { ...token, update: { value: [5, 6] } }),
    /requires one point/,
  );
  f.send('cancel-gesture', token);
  assert.deepEqual(f.session.snapshot(), before);
});
