import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {
  document,
  node,
  evaluate,
  serialize,
  deserialize,
  exportSVG,
  CommandSession,
} from '../src/core.js';
import { makeFontAsset } from '../src/fonts.js';
import {
  prepareLuminanceMask,
  clipLuminanceMarks,
  LUMINANCE_MASK_LIMITS,
} from '../src/luminance-mask.js';
const bytes = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
const font = makeFontAsset(
  bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength),
  'DejaVuSans.ttf',
);
const ring = (count) => ({
  contours: [
    {
      id: 'ring',
      closed: true,
      points: Array.from({ length: count }, (_, i) => ({
        id: 'p' + i,
        position: [
          64 + 32 * Math.cos((2 * Math.PI * i) / count),
          64 + 32 * Math.sin((2 * Math.PI * i) / count),
        ],
        handleA: [0, 0],
        handleB: [0, 0],
      })),
    },
  ],
});
function scene(type, text = 'Nect Dither Preview') {
  const doc = document(),
    layer = doc.layers[0],
    source = node('Text'),
    fill = node('Fill'),
    effect = node(type),
    out = node('Output');
  source.text = text;
  source.font = font.id;
  source.params.size.value = 24;
  source.params.position.value = [20, 50];
  fill.params.path.source = source.id;
  fill.params.color.value = { hex: '#808080', alpha: 1 };
  effect.params.path.source = fill.id;
  effect.params.pitch.value = 24;
  out.params.path.source = effect.id;
  layer.nodes = [source, fill, effect, out];
  layer.output = out.id;
  doc.fontAssets = { [font.id]: font.asset };
  return { doc, layer, source, effect, fonts: { [font.id]: font.font } };
}
for (const type of ['Ordered Dither', 'Error Diffusion Dither']) {
  test(
    type + ' evaluates ordinary live Text above 256 points without changing authored content',
    () => {
      const f = scene(type),
        before = serialize(f.doc),
        input = evaluate(f.layer, f.fonts).run(f.source.id);
      assert.ok(input.contours.reduce((n, c) => n + c.points.length, 0) > 256);
      const result = evaluate(f.layer, f.fonts).run(f.effect.id);
      assert.ok(result.contours.length > 0);
      assert.equal(serialize(f.doc), before);
      assert.deepEqual(evaluate(f.layer, f.fonts).run(f.effect.id), result);
      const loaded = deserialize(before);
      assert.deepEqual(evaluate(loaded.layers[0], f.fonts).run(f.effect.id), result);
      assert.ok(exportSVG(loaded, f.fonts).includes('fill="#000000"'));
    },
  );
  test(type + ' clips a 1200-point vector through the real modifier route', () => {
    const f = scene(type),
      shape = node('Imported Path'),
      points = ring(1200).contours[0].points;
    shape.rawPath = points.map((p, i) => (i ? 'L' : 'M') + p.position.join(' ')).join(' ') + ' Z';
    f.layer.nodes[0] = shape;
    f.layer.nodes[1].params.path.source = shape.id;
    const before = serialize(f.doc),
      result = evaluate(f.layer, f.fonts).run(f.effect.id);
    assert.ok(result.contours.length > 0);
    assert.equal(serialize(f.doc), before);
  });
  test(
    type +
      ' accepts multi-glyph contours and preserves atomic rollback when density exceeds work budget',
    () => {
      const f = scene(type, 'Nect Dither Preview '.repeat(3)),
        input = evaluate(f.layer, f.fonts).run(f.source.id);
      assert.ok(input.contours.length > 32);
      const session = new CommandSession(f.doc),
        before = serialize(session.snapshot());
      const send = (type, payload = {}) =>
        session.dispatch({
          ...session.state,
          type,
          payload: { layerId: f.layer.id, nodeId: f.effect.id, ...payload },
        });
      send('set-parameter', { key: 'pitch', value: 32 });
      const after = serialize(session.snapshot());
      assert.equal(session.historyView.past.length, 1);
      send('undo');
      assert.equal(serialize(session.snapshot()), before);
      send('redo');
      assert.equal(serialize(session.snapshot()), after);
      assert.throws(() => send('set-parameter', { key: 'pitch', value: 1 }));
      assert.equal(serialize(session.snapshot()), after);
    },
  );
}
test('complex closed paths reach the bounded 10000-point input boundary; denser clipping stays guarded', () => {
  const source = ring(1200),
    before = JSON.stringify(source),
    mask = prepareLuminanceMask(source);
  assert.equal(mask.polygons[0].length, 1200);
  assert.equal(JSON.stringify(source), before);
  const boundary = prepareLuminanceMask(ring(10000));
  assert.equal(boundary.polygons[0].length, 10000);
  assert.throws(
    () => prepareLuminanceMask(ring(10001)),
    (e) => e.code === 'LUMINANCE_MASK_LIMIT' && /10000/.test(e.message),
  );
  const marks = Array.from({ length: 501 }, (_, i) => {
    const x = i % 25;
    return [
      [x, 40],
      [x + 0.5, 40],
      [x + 0.5, 41],
      [x, 41],
    ];
  });
  assert.throws(
    () => clipLuminanceMarks(mask, marks, 'budget'),
    (e) => e.code === 'LUMINANCE_MASK_LIMIT' && /spacing/.test(e.message),
  );
  assert.equal(LUMINANCE_MASK_LIMITS.pairWork, 2000000);
  assert.equal(LUMINANCE_MASK_LIMITS.generatedVertices, 100000);
  assert.equal(LUMINANCE_MASK_LIMITS.outputVertices, 100000);
});
