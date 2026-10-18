import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { CommandSession, document, node, connect, evaluate, pathBounds } from '../src/core.js';
import { parseFont } from '../src/fonts.js';
import { resolveNodeOutputTargets } from '../src/node-output-alignment.js';
const bytes = fs.readFileSync('public/fonts/DejaVuSans.ttf');
const fonts = {
  default: parseFont(bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength)),
};
function fixture() {
  const doc = document(),
    l = doc.layers[0];
  const text = node('Text'),
    fill = node('Fill'),
    out = node('Output'),
    vector = node('Vector'),
    nil = node('Null'),
    curve = node('Curve'),
    value = node('Value');
  text.text = 'Nect';
  vector.params.value.value = [650, 430];
  l.nodes = [text, fill, out, vector, nil, curve, value];
  l.output = out.id;
  connect(l, text.id, fill.id, 'path');
  connect(l, fill.id, out.id, 'path');
  const session = new CommandSession(doc, { fonts: () => fonts });
  const align = (nodes, action = 'left', reference = 'artboard') =>
    session.dispatch({
      ...session.state,
      type: 'align-node-outputs',
      payload: { layerId: l.id, nodeIds: nodes.map((n) => n.id), action, reference },
    });
  const read = (n) => session.snapshot().layers[0].nodes.find((x) => x.id === n.id);
  return { doc, l, text, fill, out, vector, nil, curve, value, session, align, read };
}
test('Vector2 and Null align as positions using the common translate command, with one Undo/Redo', () => {
  const f = fixture(),
    before = f.session.snapshot();
  f.align([f.vector], 'center-x');
  assert.deepEqual(f.read(f.vector).params.value.value, [480, 430]);
  const after = f.session.snapshot();
  f.session.dispatch({ ...f.session.state, type: 'undo' });
  assert.deepEqual(f.session.snapshot(), before);
  f.session.dispatch({ ...f.session.state, type: 'redo' });
  assert.deepEqual(f.session.snapshot(), after);
  f.align([f.nil], 'top');
  assert.equal(f.read(f.nil).params.position.value[1], 0);
});
test('Text + Vector mixed selection aligns output bounds, preserving graph boxes and text formatting', () => {
  const f = fixture(),
    before = f.session.snapshot();
  f.align([f.text, f.vector], 'right', 'selection');
  const after = f.session.snapshot(),
    r = evaluate(after.layers[0], fonts);
  assert.ok(
    Math.abs(pathBounds(r.run(f.text.id))[0][2] - f.read(f.vector).params.value.value[0]) < 0.00002,
  );
  assert.deepEqual(
    after.layers[0].nodes.map((n) => n.position),
    before.layers[0].nodes.map((n) => n.position),
  );
  const t = f.read(f.text);
  assert.equal(t.text, f.text.text);
  assert.equal(t.font, f.text.font);
  assert.deepEqual(t.params.size, f.text.params.size);
  f.session.dispatch({ ...f.session.state, type: 'undo' });
  assert.deepEqual(f.session.snapshot(), before);
});
test('selected Text/Fill/Output deduplicate; a selected driving Vector moves the text once without detaching it', () => {
  const f = fixture();
  const resolved = resolveNodeOutputTargets(f.doc, f.l.id, [f.text.id, f.fill.id, f.out.id], {
    fonts,
  });
  assert.equal(resolved.members.length, 1);
  connect(f.l, f.vector.id, f.text.id, 'position');
  const s = new CommandSession(f.doc, { fonts: () => fonts });
  const before = s.snapshot();
  s.dispatch({
    ...s.state,
    type: 'align-node-outputs',
    payload: {
      layerId: f.l.id,
      nodeIds: [f.text.id, f.vector.id, f.fill.id],
      action: 'left',
      reference: 'artboard',
    },
  });
  const after = s.snapshot(),
    text = after.layers[0].nodes[0];
  assert.deepEqual(text.params.position, before.layers[0].nodes[0].params.position);
  assert.ok(Math.abs(pathBounds(evaluate(after.layers[0], fonts).path)[0][0]) < 0.00002);
  s.dispatch({ ...s.state, type: 'undo' });
  assert.deepEqual(s.snapshot(), before);
});
test('Curve alignment translates every anchor by one delta, preserving relative handles and topology', () => {
  const f = fixture(),
    before = structuredClone(f.curve);
  f.align([f.curve], 'bottom');
  const after = f.read(f.curve),
    delta = after.points[0].position.value.map((v, i) => v - before.points[0].position.value[i]);
  assert.deepEqual(after.params.closed, before.params.closed);
  for (let i = 0; i < after.points.length; i++) {
    assert.equal(after.points[i].id, before.points[i].id);
    assert.deepEqual(after.points[i].handleA, before.points[i].handleA);
    assert.deepEqual(after.points[i].handleB, before.points[i].handleB);
    assert.deepEqual(
      after.points[i].position.value,
      before.points[i].position.value.map((v, axis) => v + delta[axis]),
    );
  }
});
test('unsupported/locked members reject the entire selection; aligned no-op preserves an existing redo branch', () => {
  const f = fixture(),
    before = f.session.snapshot(),
    state = f.session.state;
  assert.throws(() => f.align([f.vector, f.value]), /Value.*No selected output moved/);
  assert.deepEqual(f.session.snapshot(), before);
  assert.deepEqual(f.session.state, state);
  f.align([f.vector], 'left');
  const aligned = f.session.snapshot();
  f.align([f.vector], 'top');
  f.session.dispatch({ ...f.session.state, type: 'undo' });
  const revision = f.session.state.expectedRevision;
  f.align([f.vector], 'left');
  assert.equal(f.session.state.expectedRevision, revision);
  f.session.dispatch({ ...f.session.state, type: 'redo' });
  assert.equal(f.read(f.vector).params.value.value[1], 0);
  assert.notDeepEqual(f.session.snapshot(), aligned);
  f.session.dispatch({
    ...f.session.state,
    type: 'set-parameter-lock',
    payload: { layerId: f.l.id, nodeId: f.vector.id, key: 'value', locked: true },
  });
  const locked = f.session.snapshot();
  assert.throws(() => f.align([f.vector, f.text]), /locked/);
  assert.deepEqual(f.session.snapshot(), locked);
});
