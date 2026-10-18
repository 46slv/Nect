import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  blankLayer,
  node,
  connect,
  CommandSession,
  serialize,
  deserialize,
  evaluate,
} from '../src/core.js';
import { encodeImage } from '../src/images.js';
import { encodeRgbaPng } from '../src/image-keyer.js';
import { readFile } from 'node:fs/promises';
import { makeFontAsset } from '../src/fonts.js';

function fixture() {
  const doc = blankDocument(),
    src = doc.layers[0],
    dst = blankLayer('Target');
  const a = node('Value'),
    b = node('Math');
  a.params.value.value = 7;
  connect({ nodes: [a, b] }, a.id, b.id, 'a');
  src.nodes.push(a, b);
  doc.layers.push(dst);
  const session = new CommandSession(doc);
  const send = (type, payload = {}, layerId = src.id) =>
    session.dispatch({ ...session.state, type, payload: { layerId, ...payload } });
  return { doc, src, dst, a, b, session, send };
}
function atomic(f, action, pattern) {
  const before = serialize(f.session.snapshot()),
    state = f.session.state,
    past = f.session.historyView.past.length;
  assert.throws(action, pattern);
  assert.equal(serialize(f.session.snapshot()), before);
  assert.deepEqual(f.session.state, state);
  assert.equal(f.session.historyView.past.length, past);
}
test('named detached snapshot saves, applies fresh IDs across Compositions, remains ordinary editable nodes and preserves Output', () => {
  const f = fixture(),
    original = serialize(f.session.snapshot());
  const presetId = f.send('save-node-preset', {
    nodeIds: [f.a.id, f.b.id],
    name: '  Seven  ',
  }).presetId;
  assert.equal(f.session.snapshot().schemaVersion, 39);
  assert.equal(f.session.snapshot().nodePresets[0].name, 'Seven');
  assert.equal(f.session.historyView.past.length, 1);
  const saved = serialize(f.session.snapshot());
  f.send('undo');
  assert.equal(serialize(f.session.snapshot()), original);
  f.send('redo');
  assert.equal(serialize(f.session.snapshot()), saved);
  f.send('set-parameter', { nodeId: f.a.id, key: 'value', value: 99 });
  const beforeApply = serialize(f.session.snapshot()),
    count = f.session.historyView.past.length;
  const ids = f.send('apply-node-preset', { presetId }, f.dst.id).nodeIds;
  let d = f.session.snapshot(),
    dest = d.layers[1];
  assert.equal(dest.output, f.dst.output);
  assert.deepEqual(dest.nodes[0], f.dst.nodes[0]);
  assert.equal(dest.nodes.find((n) => n.id === ids[0]).params.value.value, 7);
  assert.equal(dest.nodes.find((n) => n.id === ids[1]).params.a.source, ids[0]);
  assert.ok(ids.every((id) => ![f.a.id, f.b.id].includes(id)));
  assert.equal(evaluate(dest).run(ids[1]), 7);
  assert.equal(f.session.historyView.past.length, count + 1);
  const afterApply = serialize(d);
  f.send('undo');
  assert.equal(serialize(f.session.snapshot()), beforeApply);
  f.send('redo');
  assert.equal(serialize(f.session.snapshot()), afterApply);
  f.send('set-parameter', { nodeId: ids[0], key: 'value', value: 12 }, f.dst.id);
  assert.equal(f.session.snapshot().nodePresets[0].nodes[0].params.value.value, 7);
  const next = f.send('apply-node-preset', { presetId }, f.dst.id).nodeIds;
  assert.ok(next.every((id) => !ids.includes(id)));
  const encoded = serialize(f.session.snapshot());
  assert.equal(serialize(deserialize(encoded)), encoded);
  const reloaded = new CommandSession(deserialize(encoded));
  const applied = reloaded.dispatch({
    ...reloaded.state,
    type: 'apply-node-preset',
    payload: { layerId: f.dst.id, presetId },
  });
  assert.equal(
    reloaded.snapshot().layers[1].nodes.find((n) => n.id === applied.nodeIds[0]).params.value.value,
    7,
  );
});
test('name and ID collisions refuse; update replaces snapshot with stable preset ID in one Undo', () => {
  const f = fixture(),
    p = { nodeIds: [f.a.id, f.b.id], name: 'One', presetId: 'preset-one' };
  f.send('save-node-preset', p);
  atomic(f, () => f.send('save-node-preset', { ...p, presetId: 'preset-two' }), /name already/);
  atomic(f, () => f.send('save-node-preset', { ...p, name: 'Two' }), /ID already/);
  atomic(
    f,
    () => f.send('save-node-preset', { ...p, presetId: f.a.id, name: 'Two' }),
    /duplicate ID/,
  );
  for (const name of ['', ' ', 'x'.repeat(101)])
    atomic(f, () => f.send('save-node-preset', { ...p, presetId: 'preset-two', name }), /name/);
  f.send('save-node-preset', { ...p, presetId: 'preset-two', name: 'Two' });
  atomic(f, () => f.send('update-node-preset', { ...p, name: 'Two' }), /name already/);
  f.send('set-parameter', { nodeId: f.a.id, key: 'value', value: 42 });
  const before = serialize(f.session.snapshot()),
    past = f.session.historyView.past.length;
  assert.equal(
    f.send('update-node-preset', { presetId: p.presetId, nodeIds: [f.a.id, f.b.id] }).presetId,
    p.presetId,
  );
  assert.equal(f.session.historyView.past.length, past + 1);
  const updated = serialize(f.session.snapshot());
  f.send('undo');
  assert.equal(serialize(f.session.snapshot()), before);
  f.send('redo');
  assert.equal(serialize(f.session.snapshot()), updated);
  const ids = f.send('apply-node-preset', { presetId: p.presetId }, f.dst.id).nodeIds;
  assert.equal(
    f.session.snapshot().layers[1].nodes.find((n) => n.id === ids[0]).params.value.value,
    42,
  );
});
test('invalid, external and inactive dependencies, Outputs and Composition References refuse atomically', () => {
  const f = fixture();
  for (const nodeIds of [[f.b.id], [f.src.output], [f.a.id, f.a.id], ['missing'], []])
    atomic(
      f,
      () => f.send('save-node-preset', { name: 'Bad', nodeIds }),
      /external|Output|distinct/,
    );
  const expr = node('Math'),
    ref = node('Reference'),
    cr = node('Composition Reference');
  expr.params.a.expression = `ref("${f.a.id}", "value") + 2`;
  expr.bypassed = true;
  ref.reference = { nodeId: 'missing-vector', portId: 'output' };
  f.doc.layers[0].nodes.push(expr, ref, cr);
  f.doc.schemaVersion = 31;
  const session = new CommandSession(f.doc),
    g = { ...f, session };
  for (const n of [expr, ref, cr])
    atomic(
      g,
      () =>
        session.dispatch({
          ...session.state,
          type: 'save-node-preset',
          payload: { layerId: f.src.id, nodeIds: [n.id], name: 'Bad' },
        }),
      /external|Composition Reference/,
    );
  atomic(
    f,
    () => f.send('update-node-preset', { presetId: 'missing', nodeIds: [f.a.id] }),
    /Missing preset/,
  );
  atomic(f, () => f.send('apply-node-preset', { presetId: 'missing' }, f.dst.id), /Missing preset/);
});
test('document assets are captured and checked; missing/changed asset and malformed persisted snapshots refuse', () => {
  const f = fixture(),
    image = node('Image');
  image.asset = 'image-test';
  f.doc.imageAssets = {
    'image-test': {
      name: 'pixel.png',
      mime: 'image/png',
      width: 1,
      height: 1,
      data: encodeImage(encodeRgbaPng(1, 1, new Uint8Array([255, 0, 0, 255]))),
    },
  };
  f.doc.layers[0].nodes.push(image);
  const s = new CommandSession(f.doc),
    send = (type, payload) =>
      s.dispatch({ ...s.state, type, payload: { layerId: f.src.id, ...payload } });
  const p = send('save-node-preset', { nodeIds: [image.id], name: 'Pixel' }).presetId;
  send('apply-node-preset', { presetId: p, layerId: f.dst.id });
  const good = s.snapshot();
  for (const mutate of [
    (d) => delete d.imageAssets['image-test'],
    (d) => (d.imageAssets['image-test'].name = 'Changed'),
    (d) => (d.nodePresets[0].nodes[0].params.position.source = 'external'),
    (d) => (d.nodePresets[0].nodes[0].position = [Infinity, 0]),
    (d) => d.nodePresets.push(structuredClone(d.nodePresets[0])),
    (d) => (d.schemaVersion = 38),
  ]) {
    const d = structuredClone(good);
    mutate(d);
    assert.throws(() => deserialize(serialize(d)));
  }
  const missing = node('Image');
  missing.asset = 'not-present';
  f.doc.layers[0].nodes.push(missing);
  // Missing assets already refuse at the common document boundary.
  assert.throws(() => new CommandSession(f.doc), /Missing image asset/);
});
test('locked destination, invalid offset, stale revision and active gesture refuse with no history change', () => {
  const f = fixture(),
    presetId = f.send('save-node-preset', { nodeIds: [f.a.id], name: 'Value' }).presetId;
  atomic(f, () => f.send('apply-node-preset', { presetId, offset: [NaN, 0] }, f.dst.id), /offset/);
  f.send('set-layer-state', { key: 'locked', value: true }, f.dst.id);
  atomic(f, () => f.send('apply-node-preset', { presetId }, f.dst.id), /locked/);
  atomic(
    f,
    () =>
      f.session.dispatch({
        ...f.session.state,
        expectedRevision: 0,
        type: 'apply-node-preset',
        payload: { layerId: f.src.id, presetId },
      }),
    /STALE/,
  );
  const g = f.send('begin-gesture', {
    target: { kind: 'parameter', layerId: f.src.id, nodeId: f.a.id, key: 'value' },
    value: 7,
  });
  atomic(
    f,
    () => f.send('save-node-preset', { nodeIds: [f.a.id], name: 'Busy' }),
    /GESTURE_ACTIVE/,
  );
  f.send('cancel-gesture', { gestureId: g.gestureId });
});

test('internal expressions, Reference and Curve/Multi Path identities reuse duplicateNodes remapping after originals are deleted', () => {
  const f = fixture(),
    curve = node('Curve'),
    multi = node('Multi Path'),
    vector = node('Vector'),
    ref = node('Reference');
  multi.points = structuredClone(curve.points);
  multi.points.forEach((p, i) => {
    p.id = `multi-point-${i}`;
  });
  multi.contours = [{ id: 'multi-contour', closed: false, points: multi.points.map((p) => p.id) }];
  multi.units = [{ id: 'multi-unit', contours: ['multi-contour'] }];
  ref.reference = { nodeId: vector.id, portId: 'output' };
  f.b.params.b.expression = `ref("${f.a.id}", "value") + 2`;
  f.doc.layers[0].nodes.push(curve, multi, vector, ref);
  const s = new CommandSession(f.doc),
    send = (type, payload = {}, layerId = f.src.id) =>
      s.dispatch({ ...s.state, type, payload: { layerId, ...payload } });
  const originals = [f.a, f.b, curve, multi, vector, ref];
  const ids = originals.map((n) => n.id);
  const presetId = send('save-node-preset', { nodeIds: ids, name: 'Internal references' }).presetId;
  const stable = serialize(s.snapshot());
  const count = s.historyView.past.length;
  assert.equal(send('update-node-preset', { presetId, nodeIds: ids }).changed, false);
  assert.equal(s.historyView.past.length, count);
  assert.equal(serialize(s.snapshot()), stable);
  // A failed update cannot replace the good snapshot.
  atomic(
    { ...f, session: s },
    () => send('update-node-preset', { presetId, nodeIds: [f.b.id] }),
    /external/,
  );
  send('delete-nodes', { nodeIds: ids });
  const added = send('apply-node-preset', { presetId }, f.dst.id).nodeIds;
  const d = s.snapshot(),
    l = d.layers[1],
    copies = added.map((id) => l.nodes.find((n) => n.id === id));
  assert.equal(copies[1].params.a.source, copies[0].id);
  assert.match(copies[1].params.b.expression, new RegExp(copies[0].id));
  assert.equal(evaluate(l).run(copies[1].id), 16);
  assert.equal(copies[5].reference.nodeId, copies[4].id);
  assert.ok(copies[2].points.every((p, i) => p.id !== curve.points[i].id));
  assert.ok(copies[3].points.every((p, i) => p.id !== multi.points[i].id));
  assert.notEqual(copies[3].contours[0].id, multi.contours[0].id);
  assert.notEqual(copies[3].units[0].id, multi.units[0].id);
  assert.deepEqual(
    copies[3].contours[0].points,
    copies[3].points.map((p) => p.id),
  );
  assert.equal(serialize(deserialize(serialize(d))), serialize(d));
});

test('embedded Text font and source content persist in a detached snapshot', async () => {
  const f = fixture(),
    text = node('Text');
  const bytes = await readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
  const font = makeFontAsset(
    bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength),
    'DejaVuSans.ttf',
  );
  f.doc.fontAssets = { [font.id]: font.asset };
  text.font = font.id;
  text.text = 'Saved text';
  f.doc.layers[0].nodes.push(text);
  const s = new CommandSession(f.doc);
  const send = (type, payload) =>
    s.dispatch({ ...s.state, type, payload: { layerId: f.src.id, ...payload } });
  const presetId = send('save-node-preset', { nodeIds: [text.id], name: 'Text' }).presetId;
  send('set-text-content', { nodeId: text.id, text: 'Changed text' });
  const { nodeIds } = send('apply-node-preset', { layerId: f.dst.id, presetId });
  const copy = s.snapshot().layers[1].nodes.find((n) => n.id === nodeIds[0]);
  assert.equal(copy.text, 'Saved text');
  assert.equal(copy.font, font.id);
  assert.equal(serialize(deserialize(serialize(s.snapshot()))), serialize(s.snapshot()));
  const missing = s.snapshot();
  delete missing.fontAssets[font.id];
  assert.throws(() => deserialize(serialize(missing)), /font asset|Preset asset/);
});

test('old documents retain version until preset save; schema 39 does not weaken older feature floors', () => {
  const f = fixture(),
    encoded = serialize(f.doc);
  assert.equal(deserialize(encoded).schemaVersion, 27);
  assert.equal(f.session.snapshot().schemaVersion, 27);
  f.send('save-node-preset', { nodeIds: [f.a.id], name: 'Promote' });
  assert.equal(f.session.snapshot().schemaVersion, 39);
  f.send('undo');
  assert.equal(f.session.snapshot().schemaVersion, 27);
  const d = structuredClone(f.doc);
  d.schemaVersion = 32;
  d.layers[0].solo = true;
  assert.throws(() => deserialize(serialize(d)), /layer state/);
  d.schemaVersion = 33;
  assert.equal(deserialize(serialize(d)).schemaVersion, 33);
});
