import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import opentype from 'opentype.js';
import {
  document,
  node,
  connect,
  evaluate,
  CommandSession,
  serialize,
  deserialize,
  exportSVG,
} from '../src/core.js';

function scene(rawPath = 'M0 0L100 0L100 100L0 100Z M50 50L75 50') {
  const doc = document(),
    layer = doc.layers[0],
    shape = node('Imported Path'),
    twist = node('Twist Points'),
    fill = node('Fill'),
    output = node('Output');
  shape.rawPath = rawPath;
  twist.params.angle.value = 90;
  layer.nodes = [shape, twist, fill, output];
  layer.output = output.id;
  connect(layer, shape.id, twist.id, 'path');
  connect(layer, twist.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  return { doc, layer, shape, twist };
}

test('Twist Points evaluates Path geometry with unstable provenance, bypass, JSON and SVG', () => {
  const { doc, layer, shape, twist } = scene(),
    before = JSON.stringify(shape),
    evaluation = evaluate(layer),
    source = evaluation.run(shape.id),
    result = evaluation.run(twist.id);
  assert.equal(evaluation.error, null);
  assert.equal(result.unstable, true);
  assert.equal(typeof result.fingerprint, 'string');
  assert.notDeepEqual(
    result.contours.map((contour) => contour.points.map((item) => item.position)),
    source.contours.map((contour) => contour.points.map((item) => item.position)),
  );
  assert.deepEqual(evaluate(deserialize(serialize(doc)).layers[0]).run(twist.id), result);
  assert.match(exportSVG(doc), /<path /);
  assert.equal(JSON.stringify(shape), before);
  twist.bypassed = true;
  assert.deepEqual(evaluate(layer).run(twist.id), evaluate(layer).run(shape.id));
});

test('Twist angle is bounded, undoable and locked atomically', () => {
  const { doc, layer, twist } = scene(),
    session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: layer.id, nodeId: twist.id, ...payload },
    });
  const before = session.snapshot();
  for (const value of [361, -361, NaN]) {
    assert.throws(() => send('set-parameter', { key: 'angle', value }), /finite|from -360 to 360/);
    assert.deepEqual(session.snapshot(), before);
  }
  send('set-parameter', { key: 'angle', value: -270 });
  const changed = session.snapshot();
  send('undo');
  assert.deepEqual(session.snapshot(), before);
  send('redo');
  assert.deepEqual(session.snapshot(), changed);
  send('set-parameter-lock', { key: 'angle', locked: true });
  const locked = session.snapshot();
  assert.throws(() => send('set-parameter', { key: 'angle', value: 0 }), /lock/i);
  assert.deepEqual(session.snapshot(), locked);
});

test('Twist preflight rolls back geometry arithmetic errors', () => {
  const { doc, layer, shape, twist } = scene('M0 0L1e308 0L0 1e308');
  shape.rawPath =
    'M4.49423283715579e307 -1.7976931348623157e308L1.3482698511467367e308 1.7976931348623157e308L8.98846567431158e307 1.3482698511467367e308';
  const session = new CommandSession(doc);
  const before = session.snapshot();
  assert.throws(
    () =>
      session.dispatch({
        ...session.state,
        type: 'set-parameter',
        payload: { layerId: layer.id, nodeId: twist.id, key: 'angle', value: -360 },
      }),
    /finite-coordinate arithmetic/,
  );
  assert.deepEqual(session.snapshot(), before);
});

test('Twist Points restricts direct canvas text edits while preserving upstream Text provenance', () => {
  const bytes = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url)),
    font = opentype.parse(
      bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength),
    ),
    doc = document(),
    layer = doc.layers[0],
    text = node('Text'),
    twist = node('Twist Points');
  text.text = 'Nect';
  twist.params.angle.value = 45;
  layer.nodes = [text, twist];
  connect(layer, text.id, twist.id, 'path');
  const output = evaluate(layer, { default: font }).run(twist.id);
  assert.equal(output.unstable, true);
  assert.equal(output.characterOccurrences.length, 1);
  assert.equal(output.characterOccurrences[0].source, text.id);
  assert.match(
    output.characterOccurrences[0].reason,
    /Canvas character editing is unavailable through Twist Points; edit the source Text/,
  );
  assert.equal(
    evaluate(layer, { default: font }).run(text.id).characterOccurrences[0].reason,
    null,
  );
});
