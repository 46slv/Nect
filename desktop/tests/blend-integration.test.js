import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  node,
  connect,
  evaluate,
  serialize,
  deserialize,
  exportSVG,
} from '../src/core.js';
test('Blend is available through node graph, serializes and exports colored geometry', () => {
  const doc = document(),
    l = doc.layers[0];
  const a = node('Rectangle'),
    b = node('Rectangle'),
    fa = node('Fill'),
    fb = node('Fill'),
    blend = node('Blend'),
    out = node('Output');
  a.params.center.value = [10, 10];
  b.params.center.value = [110, 10];
  fa.params.color.value = { hex: '#000000', alpha: 1 };
  fb.params.color.value = { hex: '#ffffff', alpha: 1 };
  blend.params.steps.value = 1;
  l.nodes = [a, b, fa, fb, blend, out];
  l.output = out.id;
  connect(l, a.id, fa.id, 'path');
  connect(l, b.id, fb.id, 'path');
  connect(l, fa.id, blend.id, 'a');
  connect(l, fb.id, blend.id, 'b');
  connect(l, blend.id, out.id, 'path');
  const result = evaluate(l, {}, doc.imageAssets, doc);
  assert.equal(result.error, null);
  assert.equal(result.path.children.length, 3);
  const svg = exportSVG(doc);
  assert.match(svg, /#bcbcbc/i);
  assert.equal(exportSVG(deserialize(serialize(doc))), svg);
  fb.params.color.value = { hex: '#ff0000', alpha: 1 };
  const next = evaluate(l, {}, doc.imageAssets, doc);
  assert.equal(next.error, null);
  assert.equal(next.path.children[1].fill.hex, '#bc0000');
  assert.match(exportSVG(doc), /#bc0000/i);
});
test('real Stroke nodes blend and export', () => {
  const doc = document(),
    l = doc.layers[0],
    a = node('Rectangle'),
    b = node('Rectangle'),
    fa = node('Fill'),
    fb = node('Fill'),
    sa = node('Stroke'),
    sb = node('Stroke'),
    blend = node('Blend'),
    out = node('Output');
  a.params.center.value = [10, 10];
  b.params.center.value = [110, 10];
  sa.params.color.value = { hex: '#000000', alpha: 1 };
  sb.params.color.value = { hex: '#ffffff', alpha: 1 };
  sa.params.width.value = 2;
  sb.params.width.value = 6;
  blend.params.steps.value = 1;
  l.nodes = [a, b, fa, fb, sa, sb, blend, out];
  l.output = out.id;
  connect(l, a.id, fa.id, 'path');
  connect(l, b.id, fb.id, 'path');
  connect(l, fa.id, sa.id, 'path');
  connect(l, fb.id, sb.id, 'path');
  connect(l, sa.id, blend.id, 'a');
  connect(l, sb.id, blend.id, 'b');
  connect(l, blend.id, out.id, 'path');
  const result = evaluate(l, {}, doc.imageAssets, doc);
  assert.equal(result.error, null);
  assert.equal(result.path.children[1].stroke.width, 4);
  assert.match(exportSVG(doc), /stroke-width="4"/);
});

test('Blend step edits update output and survive undo and redo', async () => {
  const { CommandSession } = await import('../src/core.js');
  const doc = document(),
    l = doc.layers[0];
  const a = node('Rectangle'),
    b = node('Rectangle'),
    fa = node('Fill'),
    fb = node('Fill'),
    blend = node('Blend'),
    out = node('Output');
  blend.params.steps.value = 1;
  l.nodes = [a, b, fa, fb, blend, out];
  l.output = out.id;
  connect(l, a.id, fa.id, 'path');
  connect(l, b.id, fb.id, 'path');
  connect(l, fa.id, blend.id, 'a');
  connect(l, fb.id, blend.id, 'b');
  connect(l, blend.id, out.id, 'path');
  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({ ...session.state, type, payload: { layerId: l.id, ...payload } });
  const count = () => {
    const current = session.snapshot();
    const result = evaluate(current.layers[0], {}, current.imageAssets, current);
    assert.equal(result.error, null);
    return result.path.children.length;
  };
  assert.equal(count(), 3);
  send('set-parameter', { nodeId: blend.id, key: 'steps', value: 3 });
  assert.equal(count(), 5);
  send('undo');
  assert.equal(count(), 3);
  send('redo');
  assert.equal(count(), 5);
});

test('Blend of different point counts evaluates and survives SVG and project roundtrip', async () => {
  const doc = document(),
    layer = doc.layers[0];
  const a = node('Curve'),
    b = node('Rectangle'),
    fa = node('Fill'),
    fb = node('Fill'),
    blend = node('Blend'),
    out = node('Output');
  blend.params.steps.value = 1;
  layer.nodes = [a, b, fa, fb, blend, out];
  layer.output = out.id;
  connect(layer, a.id, fa.id, 'path');
  connect(layer, b.id, fb.id, 'path');
  connect(layer, fa.id, blend.id, 'a');
  connect(layer, fb.id, blend.id, 'b');
  connect(layer, blend.id, out.id, 'path');
  const result = evaluate(layer, {}, doc.imageAssets, doc);
  assert.equal(result.error, null);
  assert.deepEqual(
    result.path.children.map((child) => child.contours[0].points.length),
    [3, 12, 4],
  );
  const svg = exportSVG(doc);
  assert.equal(exportSVG(deserialize(serialize(doc))), svg);

  // Editing the live mismatched-count graph must invalidate its geometry cache,
  // while history restores the exact export rather than only the copy count.
  const { CommandSession } = await import('../src/core.js');
  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({ ...session.state, type, payload: { layerId: layer.id, ...payload } });
  send('set-parameter', { nodeId: blend.id, key: 'steps', value: 3 });
  const edited = session.snapshot();
  const editedResult = evaluate(edited.layers[0], {}, edited.imageAssets, edited);
  assert.equal(editedResult.error, null);
  assert.deepEqual(
    editedResult.path.children.map((child) => child.contours[0].points.length),
    [3, 12, 12, 12, 4],
  );
  const editedSvg = exportSVG(edited);
  assert.notEqual(editedSvg, svg);
  assert.equal(exportSVG(deserialize(serialize(edited))), editedSvg);
  send('undo');
  assert.equal(exportSVG(session.snapshot()), svg);
  send('redo');
  assert.equal(exportSVG(session.snapshot()), editedSvg);
});
