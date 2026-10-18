import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { document, node, connect, evaluate } from '../src/core.js';
import { parseFont } from '../src/fonts.js';
import { textEditGeometry, nearestTextCaret } from '../src/canvas-text-edit.js';
const b = fs.readFileSync('public/fonts/DejaVuSans.ttf');
const fonts = { default: parseFont(b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength)) };
for (const vertical of [false, true])
  test(`evaluated ${vertical ? 'vertical' : 'horizontal'} carets cover glyphs and empty lines under a downstream Transform`, () => {
    const doc = document(),
      l = doc.layers[0],
      text = node('Text'),
      transform = node('Transform'),
      fill = node('Fill'),
      out = node('Output');
    text.text = 'AB\n\nCD\n';
    if (vertical) text.writingMode = 'vertical-rl';
    transform.params.rotation.value = 23;
    transform.params.scale.value = 1.4;
    l.nodes = [text, transform, fill, out];
    l.output = out.id;
    connect(l, text.id, transform.id, 'path');
    connect(l, transform.id, fill.id, 'path');
    connect(l, fill.id, out.id, 'path');
    const r = evaluate(l, fonts),
      occurrence = r.path.characterOccurrences[0];
    const geometry = textEditGeometry(text, r.textLayout(text.id), occurrence, fonts, {
      ...r.textDefaults(text.id),
      position: text.params.position.value,
    });
    assert.deepEqual(
      geometry.carets.map((c) => c.index),
      [0, 1, 2, 3, 4, 5, 6, 7],
    );
    assert.equal(geometry.selections.length, 4);
    for (const caret of geometry.carets) {
      assert.ok(caret.points.flat().every(Number.isFinite));
      const center = caret.points[0].map((v, i) => (v + caret.points[1][i]) / 2);
      assert.equal(nearestTextCaret(geometry, center), caret.index);
    }
  });
test('UTF16 caret positions never split a surrogate pair', () => {
  const n = node('Text');
  n.text = 'A😀B';
  const characters = Array.from(n.text).map((text, i) => ({
    text,
    sourceIndex: i,
    base: [i * 50, 100],
    bounds: [i * 50, 20, i * 50 + 45, 100],
    pivot: [i * 50 + 25, 60],
    offset: [0, 0],
    scale: 1,
    rotation: 0,
    size: 80,
    width: 1,
    height: 1,
    font: 'default',
  }));
  const layout = {
    characters,
    glyphs: characters.map((g) => ({ ...g, advance: 50 })),
    lines: [{ sourceStart: 0, sourceEnd: 3 }],
    lineOffsets: [0],
  };
  const geometry = textEditGeometry(n, layout, { matrix: [1, 0, 0, 1, 0, 0] }, fonts);
  assert.deepEqual(
    geometry.carets.map((c) => c.index),
    [0, 1, 3, 4],
  );
});
