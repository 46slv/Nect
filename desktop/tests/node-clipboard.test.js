import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import {
  document,
  blankDocument,
  blankLayer,
  node,
  connect,
  CommandSession,
  serialize,
  deserialize,
  evaluate,
  exportSVG,
  createCharacterPositions,
} from '../src/core.js';
import { makeFontAsset } from '../src/fonts.js';
import { encodeRgbaPng } from '../src/image-keyer.js';
import { encodeImage } from '../src/images.js';
import { Window } from 'happy-dom';
import { readSVGSource } from '../src/svg-tree.js';
function fixture() {
  const doc = document(),
    src = doc.layers[0],
    dst = blankLayer('Paste target'),
    a = node('Value', 40, 70),
    b = node('Math', 340, 110);
  a.params.value.value = 7;
  connect({ nodes: [a, b] }, a.id, b.id, 'a');
  src.nodes.push(a, b);
  doc.layers.push(dst);
  const session = new CommandSession(doc),
    send = (type, payload = {}, layerId = src.id) =>
      session.dispatch({ ...session.state, type, payload: { layerId, ...payload } });
  return { doc, src, dst, a, b, session, send };
}
test('copy is a snapshot with no authored/history change; cross-Composition paste retains internal edges and relative positions in one Undo', () => {
  const f = fixture(),
    before = serialize(f.session.snapshot()),
    revision = f.session.state.expectedRevision;
  assert.equal(f.send('copy-nodes', { nodeIds: [f.a.id, f.b.id, f.src.output] }).skippedOutputs, 1);
  assert.equal(serialize(f.session.snapshot()), before);
  assert.equal(f.session.state.expectedRevision, revision);
  f.send('set-parameter', { nodeId: f.a.id, key: 'value', value: 99 });
  const base = serialize(f.session.snapshot()),
    past = f.session.historyView.past.length;
  const ids = f.send('paste-nodes', {}, f.dst.id).nodeIds,
    dest = f.session.snapshot().layers.find((l) => l.id === f.dst.id),
    copies = ids.map((id) => dest.nodes.find((n) => n.id === id));
  assert.equal(copies[0].params.value.value, 7);
  assert.equal(copies[1].params.a.source, copies[0].id);
  assert.deepEqual(
    copies.map((n) => n.position),
    [
      [70, 100],
      [370, 140],
    ],
  );
  assert.equal(dest.output, f.dst.output);
  assert.equal(f.session.historyView.past.length, past + 1);
  const saved = serialize(f.session.snapshot());
  assert.equal(serialize(deserialize(saved)), saved);
  f.send('undo');
  assert.equal(serialize(f.session.snapshot()), base);
  f.send('redo');
  assert.equal(serialize(f.session.snapshot()), saved);
  const next = f.send('paste-nodes', {}, f.dst.id).nodeIds;
  assert.ok(!ids.includes(next[0]));
  assert.deepEqual(
    f.session
      .snapshot()
      .layers.find((l) => l.id === f.dst.id)
      .nodes.find((n) => n.id === next[0]).position,
    [100, 130],
  );
});
test('external inputs remain original only in the source Composition; cross-Composition and missing references refuse atomically', () => {
  const f = fixture();
  assert.equal(f.send('copy-nodes', { nodeIds: [f.b.id] }).externalInputCount, 1);
  const id = f.send('paste-nodes').nodeIds[0];
  assert.equal(
    f.session.snapshot().layers[0].nodes.find((n) => n.id === id).params.a.source,
    f.a.id,
  );
  let before = serialize(f.session.snapshot());
  assert.throws(() => f.send('paste-nodes', {}, f.dst.id), /unselected input/);
  assert.equal(serialize(f.session.snapshot()), before);
  f.send('delete-node', { nodeId: f.a.id });
  before = serialize(f.session.snapshot());
  assert.throws(() => f.send('paste-nodes'), /unselected input/);
  assert.equal(serialize(f.session.snapshot()), before);
});
test('expressions, position References and internal sockets remap together without changing fallback settings', () => {
  const f = fixture(),
    v = node('Vector'),
    r = node('Reference'),
    expr = node('Math');
  r.reference = { nodeId: v.id, portId: 'output' };
  expr.params.a.expression = `ref("${f.a.id}", "value") + 2`;
  expr.params.a.value = 31;
  f.doc.layers[0].nodes.push(v, r, expr);
  const s = new CommandSession(f.doc),
    send = (type, payload, layerId = f.src.id) =>
      s.dispatch({ ...s.state, type, payload: { layerId, ...payload } });
  send('copy-nodes', { nodeIds: [f.a.id, v.id, r.id, expr.id] });
  const ids = send('paste-nodes', {}, f.dst.id).nodeIds,
    l = s.snapshot().layers.find((l) => l.id === f.dst.id),
    copies = ids.map((id) => l.nodes.find((n) => n.id === id));
  assert.equal(copies[2].reference.nodeId, copies[1].id);
  assert.equal(copies[3].params.a.value, 31);
  assert.match(copies[3].params.a.expression, new RegExp(copies[0].id));
  assert.equal(evaluate(l).run(copies[3].id), 9);
});
test('image, SVG Composition Reference, Keyer, Mask and Text retain same-document assets and saved data', async () => {
  const f = fixture(),
    image = node('Image'),
    key = node('Keyer'),
    shape = node('Ellipse'),
    mask = node('Mask'),
    ref = node('Composition Reference'),
    text = node('Text');
  const bytes = await readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url)),
    font = makeFontAsset(
      bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength),
      'DejaVuSans.ttf',
    );
  f.doc.schemaVersion = 35;
  f.doc.fontAssets = { [font.id]: font.asset };
  text.font = font.id;
  text.text = 'Copied Text';
  text.characterPositions = createCharacterPositions(text.text);
  f.doc.imageAssets = {
    'image-test': {
      name: 'synthetic.png',
      mime: 'image/png',
      width: 1,
      height: 1,
      data: encodeImage(encodeRgbaPng(1, 1, new Uint8Array([255, 0, 0, 128]))),
    },
  };
  image.asset = 'image-test';
  ref.compositionReference = { compositionId: f.src.id };
  const third = blankLayer('Destination');
  f.doc.layers.push(third);
  f.doc.layers[0].nodes.push(image, key, shape, mask, text);
  f.doc.layers[1].nodes.push(ref);
  connect(f.doc.layers[0], image.id, key.id, 'path');
  connect(f.doc.layers[0], key.id, mask.id, 'path');
  connect(f.doc.layers[0], shape.id, mask.id, 'mask');
  const s = new CommandSession(f.doc),
    send = (type, payload = {}, layerId = f.src.id) =>
      s.dispatch({ ...s.state, type, payload: { layerId, ...payload } });
  send('copy-nodes', { nodeIds: [image.id, key.id, shape.id, mask.id, text.id] });
  const ids = send('paste-nodes', {}, third.id).nodeIds,
    d = s.snapshot(),
    l = d.layers.find((l) => l.id === third.id),
    copies = ids.map((id) => l.nodes.find((n) => n.id === id));
  assert.equal(copies[0].asset, image.asset);
  assert.equal(copies[4].font, font.id);
  assert.deepEqual(copies[4].characterPositions, text.characterPositions);
  assert.equal(evaluate(l, {}, d.imageAssets, d).run(copies[3].id).children[0].kind, 'image');
  assert.equal(Object.keys(d.imageAssets).length, 1);
  assert.equal(Object.keys(d.fontAssets).length, 1);
  send('copy-nodes', { nodeIds: [ref.id] }, f.dst.id);
  const refId = send('paste-nodes', {}, third.id).nodeIds[0];
  assert.equal(
    s
      .snapshot()
      .layers.find((l) => l.id === third.id)
      .nodes.find((n) => n.id === refId).compositionReference.compositionId,
    f.src.id,
  );
  const before = serialize(s.snapshot());
  assert.throws(() => send('paste-nodes', {}, f.src.id), /cycle/);
  assert.equal(serialize(s.snapshot()), before);
});
test('Curve and Multi Path identities and Text path attachments copy to the new generator', async () => {
  const f = fixture(),
    curve = node('Curve'),
    multi = node('Multi Path'),
    text = node('Text');
  multi.points = node('Curve').points;
  multi.contours = [
    { id: 'contour-original', closed: false, points: multi.points.map((p) => p.id) },
  ];
  multi.units = [{ id: 'unit-original', contours: ['contour-original'] }];
  text.text = 'A';
  const bytes = await readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url)),
    asset = makeFontAsset(
      bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength),
      'DejaVuSans.ttf',
    );
  f.doc.fontAssets = { [asset.id]: asset.asset };
  text.font = asset.id;
  text.pathAttachment = {
    nodeId: curve.id,
    contourId: curve.id,
    start: 0,
    startMode: 'distance',
    reversed: false,
  };
  f.doc.schemaVersion = 27;
  f.doc.layers[0].nodes.push(curve, multi, text);
  const s = new CommandSession(f.doc);
  s.dispatch({
    ...s.state,
    type: 'copy-nodes',
    payload: { layerId: f.src.id, nodeIds: [curve.id, multi.id, text.id] },
  });
  const ids = s.dispatch({
      ...s.state,
      type: 'paste-nodes',
      payload: { layerId: f.dst.id },
    }).nodeIds,
    copies = s.snapshot().layers[1].nodes.filter((n) => ids.includes(n.id));
  assert.ok(copies[0].points.every((p, i) => p.id !== curve.points[i].id));
  assert.ok(copies[1].points.every((p, i) => p.id !== multi.points[i].id));
  assert.equal(copies[2].pathAttachment.nodeId, copies[0].id);
  assert.equal(copies[2].pathAttachment.contourId, copies[0].id);
  assert.deepEqual(
    copies[1].contours[0].points,
    copies[1].points.map((p) => p.id),
  );
});
test('empty clipboard/new Session, Output-only selection, locked destination and active gestures refuse safely', () => {
  const f = fixture();
  assert.throws(() => f.send('paste-nodes'), /clipboard is empty/);
  assert.throws(() => f.send('copy-nodes', { nodeIds: [f.src.output] }), /own Output/);
  f.send('copy-nodes', { nodeIds: [f.a.id] });
  f.send('set-layer-state', { key: 'locked', value: true }, f.dst.id);
  const before = serialize(f.session.snapshot());
  assert.throws(() => f.send('paste-nodes', {}, f.dst.id), /locked/);
  assert.equal(serialize(f.session.snapshot()), before);
  const foreign = new CommandSession(f.session.snapshot());
  assert.throws(
    () =>
      foreign.dispatch({ ...foreign.state, type: 'paste-nodes', payload: { layerId: f.src.id } }),
    /OS\/other-document/,
  );
  const g = f.send('begin-gesture', {
    target: { kind: 'parameter', layerId: f.src.id, nodeId: f.a.id, key: 'value' },
    value: 7,
  });
  assert.throws(() => f.send('copy-nodes', { nodeIds: [f.a.id] }), /GESTURE_ACTIVE/);
  f.send('cancel-gesture', { gestureId: g.gestureId });
});
test('copied assets removed by Undo cannot be silently rebound; dangling Composition targets refuse', () => {
  const f = fixture(),
    ref = node('Composition Reference');
  ref.compositionReference = { compositionId: 'layer-missing' };
  f.doc.schemaVersion = 31;
  f.doc.layers[0].nodes.push(ref);
  const s = new CommandSession(f.doc);
  s.dispatch({ ...s.state, type: 'copy-nodes', payload: { layerId: f.src.id, nodeIds: [ref.id] } });
  const before = serialize(s.snapshot());
  assert.throws(
    () => s.dispatch({ ...s.state, type: 'paste-nodes', payload: { layerId: f.dst.id } }),
    /reference is missing/,
  );
  assert.equal(serialize(s.snapshot()), before);
  const imported = f.send('adopt-image', {
    name: 'temporary.png',
    png: encodeImage(encodeRgbaPng(1, 1, new Uint8Array([255, 0, 0, 128]))),
  });
  f.send('copy-nodes', { nodeIds: [imported.nodeId] }, imported.layerId);
  f.send('undo');
  const unchanged = serialize(f.session.snapshot());
  assert.throws(() => f.send('paste-nodes', {}, f.dst.id), /image is no longer present/);
  assert.equal(serialize(f.session.snapshot()), unchanged);
});
test('actual SVG part references paste without flattening or losing their source Composition', async () => {
  const win = new Window();
  try {
    const f = fixture(),
      tree = readSVGSource(
        '<svg viewBox="0 0 200 200"><path d="M0 0H100V100H0Z" fill="#123456"/></svg>',
        win.DOMParser,
      ),
      imported = f.send('add-svg-part', { source: tree, name: 'copy.svg' });
    f.send('copy-nodes', { nodeIds: [imported.nodeId] });
    const ids = f.send('paste-nodes', {}, f.dst.id).nodeIds,
      d = f.session.snapshot(),
      copied = d.layers.find((l) => l.id === f.dst.id).nodes.find((n) => n.id === ids[0]);
    assert.equal(copied.type, 'Composition Reference');
    assert.ok(d.layers.some((l) => l.id === copied.compositionReference.compositionId));
    f.send('preview-node', { nodeId: ids[0] }, f.dst.id);
    assert.match(exportSVG(f.session.snapshot()), /#123456/);
    const saved = serialize(f.session.snapshot());
    assert.equal(exportSVG(deserialize(saved)), exportSVG(f.session.snapshot()));
  } finally {
    await win.happyDOM.abort();
  }
});
test('replacing the document through SVG import clears the private clipboard', async () => {
  const win = new Window();
  try {
    const f = fixture(),
      tree = readSVGSource(
        '<svg viewBox="0 0 200 200"><path d="M0 0H100V100H0Z"/></svg>',
        win.DOMParser,
      );
    f.send('copy-nodes', { nodeIds: [f.a.id] });
    f.send('import-svg', { source: tree });
    const current = f.session.snapshot(),
      before = serialize(current);
    assert.throws(() => f.send('paste-nodes', {}, current.layers[0].id), /clipboard is empty/);
    assert.equal(serialize(f.session.snapshot()), before);
  } finally {
    await win.happyDOM.abort();
  }
});
