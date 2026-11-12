import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
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
import { isVector2 } from '../src/node-value-validation.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import { parseFont } from '../src/fonts.js';
import {
  createRemapSnapshotGuard,
  inspectFreshRemapOutput,
  inspectRemapOutput,
  remapOutputDelta,
  remapOutputValue,
} from '../src/remap-point-output.js';
import { remapFixture } from './helpers/remap-fingerprint-fixture.js';

test('Vector2 requires two own finite entries, including for Session input', () => {
  const inherited = Array(2);
  Object.setPrototypeOf(inherited, Object.assign(Object.create(Array.prototype), { 0: 1, 1: 2 }));
  for (const value of [Array(2), [0, ,], [, 0], inherited, [0, Infinity], [0, undefined]])
    assert.equal(isVector2(value), false);
  assert.equal(isVector2([0, -0.1]), true);
  const f = fixture('typed'),
    before = f.session.snapshot(),
    token = f.send('begin-gesture', { target: f.target });
  assert.throws(() => f.send('update-gesture', { ...token, update: { delta: Array(2) } }));
  f.send('cancel-gesture', token);
  assert.deepEqual(f.session.snapshot(), before);
});

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

test('Remap snapshot guard reuses unchanged geometry and rechecks before commit', () => {
  const f = fixture('raw'),
    doc = f.session.historyView.doc,
    font = {},
    fonts = { default: font },
    snapshot = inspectRemapOutput(doc, f.layer.id, f.remap.id, {}).get(f.first.id);
  let reads = 0,
    current = snapshot;
  const guard = createRemapSnapshotGuard(doc, fonts, snapshot, () => {
    reads++;
    return current;
  });
  assert.equal(guard(doc, { default: font }), true);
  assert.equal(guard(doc, { default: font }), true);
  assert.equal(reads, 1);
  assert.equal(guard(doc, { default: font }, true), true);
  assert.equal(reads, 2);
  current = { ...snapshot, offset: [9, 9] };
  assert.equal(guard(doc, { default: font }, true), false);
  current = snapshot;
  assert.equal(guard(doc, { default: font }), false, 'a stale scope stays invalid');

  const fontGuard = createRemapSnapshotGuard(doc, fonts, snapshot, () => snapshot);
  assert.equal(fontGuard(doc, { default: {} }), false);
  assert.equal(fontGuard(doc, { default: font }), false);
  const documentGuard = createRemapSnapshotGuard(doc, fonts, snapshot, () => snapshot);
  assert.equal(documentGuard(structuredClone(doc), fonts), false);
  const mutableFonts = { default: font },
    mapGuard = createRemapSnapshotGuard(doc, mutableFonts, snapshot, () => snapshot);
  assert.equal(mapGuard(doc, mutableFonts), true);
  mutableFonts.default = {};
  assert.equal(mapGuard(doc, mutableFonts), false, 'replacement in the same font map is stale');
});

test('Remap snapshot guard detects an in-place font change at the final fresh check', () => {
  const doc = {},
    font = { offset: 1 },
    fonts = { default: font },
    read = () => ({ position: [font.offset, 0] }),
    guard = createRemapSnapshotGuard(doc, fonts, read(), read);
  assert.equal(guard(doc, fonts), true);
  font.offset = 2;
  assert.equal(guard(doc, fonts), true, 'drag reuse does not claim a fresh geometry read');
  assert.equal(guard(doc, fonts, true), false, 'commit must reject changed geometry');
});

test('fresh Remap evaluation detects mutated glyphs without a cached Layer result', () => {
  const bytes = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url)),
    font = parseFont(bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength)),
    fonts = { default: font },
    doc = blankDocument(),
    layer = doc.layers[0],
    text = node('Text'),
    remap = node('Path Remap'),
    output = node('Output');
  text.text = 'A';
  layer.nodes = [text, remap, output];
  layer.output = output.id;
  connect(layer, text.id, remap.id, 'path');
  connect(layer, remap.id, output.id, 'path');
  const owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc), {
      fonts: () => fonts,
    }),
    committed = owner.historyView.doc,
    pointId = evaluate(committed.layers[0], fonts, committed.imageAssets, committed).run(text.id)
      .contours[0].points[0].id,
    cached = () =>
      inspectRemapOutput(committed, layer.id, remap.id, fonts, null, [pointId]).get(pointId),
    fresh = () =>
      inspectFreshRemapOutput(owner, owner.activeCompositionId, layer.id, remap.id, fonts, [
        pointId,
      ]).get(pointId),
    baseline = cached(),
    guard = createRemapSnapshotGuard(committed, fonts, baseline, (isFresh) =>
      isFresh ? fresh() : cached(),
    ),
    before = owner.projectSnapshot();
  assert.equal(guard(committed, fonts), true);
  assert.equal(guard(committed, fonts, true), true, 'an unchanged fresh Project matches');
  let gesture = owner.dispatch({
    ...owner.state,
    type: 'begin-gesture',
    payload: {
      target: {
        kind: 'remap-point',
        layerId: layer.id,
        nodeId: remap.id,
        pointId,
        key: 'position',
      },
    },
  });
  gesture = owner.dispatch({
    ...owner.state,
    type: 'update-gesture',
    payload: { ...gesture, update: { value: remapOutputDelta(baseline, 'position', 'y', 250) } },
  });
  for (const command of font.charToGlyph('A').path.commands)
    for (const key of ['y', 'y1', 'y2']) if (key in command) command[key] += 700;
  assert.deepEqual(cached(), baseline, 'the original frozen Layer can return its cached geometry');
  assert.notDeepEqual(fresh().source, baseline.source);
  assert.equal(guard(committed, fonts, true), false);
  owner.dispatch({ ...owner.state, type: 'cancel-gesture', payload: gesture });
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.equal(owner.historyView.past.length, 0);
});

test('fresh Remap evaluation keeps external Composition context', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    circle = node('Circle'),
    remap = node('Path Remap'),
    output = node('Output'),
    project = migrateLegacyDocumentToCompositionProject(doc),
    other = blankDocument(),
    driver = node('Value');
  layer.nodes = [circle, remap, output];
  layer.output = output.id;
  connect(layer, circle.id, remap.id, 'path');
  connect(layer, remap.id, output.id, 'path');
  project.compositions[0].layers = [layer];
  project.compositions[0].id = 'active';
  other.layers[0].id = 'other-layer';
  driver.id = 'other-value';
  driver.params.value.value = 7;
  other.layers[0].nodes.push(driver);
  project.compositions.push({
    id: 'other',
    name: 'Other',
    width: other.width,
    height: other.height,
    layers: other.layers,
    editor: other.editor,
  });
  const owner = createCompositionSessionOwner(project, { activeCompositionId: 'active' });
  owner.dispatch({
    ...owner.state,
    type: 'set-parameter-expression',
    payload: {
      layerId: layer.id,
      nodeId: circle.id,
      key: 'radius',
      expression: 'ref(composition, "other", "other-layer", "other-value", "value")',
    },
  });
  const document = owner.historyView.doc,
    pointId = evaluate(document.layers[0], {}, document.imageAssets, document).run(circle.id)
      .contours[0].points[0].id,
    regular = inspectRemapOutput(document, layer.id, remap.id, {}, null, [pointId]).get(pointId),
    fresh = inspectFreshRemapOutput(owner, 'active', layer.id, remap.id, {}, [pointId]).get(
      pointId,
    );
  assert.deepEqual(fresh, regular);
  assert.ok(fresh.source.position.every(Number.isFinite));
});

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
  for (const update of [
    { value: [NaN, 0] },
    { value: [0] },
    { value: Array(2) },
    { value: [0, ,] },
    { value: [, 0] },
    { value: [0, 0], delta: [1, 1] },
  ])
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

test('Remap output values use source geometry and offset, preserving the other position delta', () => {
  const f = fixture('raw'),
    doc = f.session.snapshot(),
    remap = doc.layers[0].nodes[1];
  remap.params.offset.value = [15.3, -8];
  remap.edits[0].position = [7, 9];
  const snapshot = inspectRemapOutput(doc, f.layer.id, f.remap.id, {}).get(f.first.id);
  assert.equal(remapOutputValue(snapshot, 'position', 'x'), 122.3);
  assert.equal(remapOutputValue(snapshot, 'position', 'y'), 101);
  assert.equal(remapOutputValue(snapshot, 'handleB', 'length'), 0.30000000000000004);
  assert.deepEqual(remapOutputDelta(snapshot, 'position', 'x', 130), [14.700000000000003, 9]);
  assert.deepEqual(remapOutputDelta(snapshot, 'handleB', 'length', 0), [-0.1, 0]);
  assert.deepEqual(
    remapOutputDelta(
      snapshot,
      'handleB',
      'length',
      remapOutputValue(snapshot, 'handleB', 'length'),
    ),
    [0.2, 0],
    'an exactly unchanged output retains the authored delta without float drift',
  );
});

test('Remap output rejects duplicate point identity and locks only the driven target', () => {
  const f = fixture('typed'),
    doc = f.session.snapshot(),
    remap = doc.layers[0].nodes[1];
  remap.edits[0].params.handleB.expression = '1; 2';
  const snapshot = inspectRemapOutput(doc, f.layer.id, f.remap.id, {}).get(f.first.id);
  assert.equal(snapshot.editable.handleB, false);
  assert.equal(snapshot.editable.handleA, true);
  assert.equal(snapshot.editable.position, true);
  assert.throws(() => remapOutputDelta(snapshot, 'handleB', 'length', 20));
  const geometry = { id: f.first.id, position: [1, 2], handleA: [0, 0], handleB: [1, 0] },
    path = { contours: [{ id: 'same', points: [geometry, geometry] }] },
    evaluation = { run: () => path, parameterValue: () => [0, 0] };
  assert.equal(inspectRemapOutput(doc, f.layer.id, f.remap.id, {}, evaluation).size, 0);
});
