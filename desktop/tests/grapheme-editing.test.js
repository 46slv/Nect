import test from 'node:test';
import assert from 'node:assert/strict';
import { textEditGeometry, nearestTextCaret } from '../src/canvas-text-edit.js';
import {
  graphemeSamples,
  scalarSamples,
  expectedBoundaries,
  scalarGeometryFixture,
} from './fixtures/grapheme-editing.js';

const segmenter = new Intl.Segmenter(undefined, { granularity: 'grapheme' });
const offsets = (text) =>
  [...segmenter.segment(text)].map((part) => part.index).concat(text.length);

for (const sample of [...graphemeSamples, ...scalarSamples]) {
  test(`fixture has explicit extended-grapheme UTF-16 boundaries: ${sample.name}`, () => {
    assert.deepEqual(offsets(sample.text), sample.boundaries);
    assert.deepEqual(offsets(`A${sample.text}B`), expectedBoundaries(sample));
  });

  test(`Canvas pointer carets respect grapheme boundaries: ${sample.name}`, () => {
    const f = scalarGeometryFixture(`A${sample.text}B`);
    const geometry = textEditGeometry(f.node, f.layout, f.occurrence);
    assert.deepEqual(
      geometry.carets.map((caret) => caret.index),
      expectedBoundaries(sample),
    );
    for (const caret of geometry.carets) {
      const midpoint = caret.points[0].map((value, i) => (value + caret.points[1][i]) / 2);
      assert.ok(expectedBoundaries(sample).includes(nearestTextCaret(geometry, midpoint)));
    }
  });
}

test('NFC layout indices map to original decomposed UTF-16 caret and selection edges', () => {
  for (const raw of ['Ae\u0301B', 'A\u1100\u1161\u11a8B']) {
    const fixture = scalarGeometryFixture(raw.normalize('NFC'));
    fixture.node.text = raw;
    const geometry = textEditGeometry(fixture.node, fixture.layout, fixture.occurrence);
    assert.deepEqual(
      geometry.carets.map((caret) => caret.index),
      offsets(raw),
    );
    assert.deepEqual(geometry.selections[1], {
      start: 1,
      end: raw.length - 1,
      points: geometry.selections[1].points,
    });
    assert.equal(
      geometry.carets.at(-1).points[0][0],
      150,
      'terminal caret follows three NFC glyphs',
    );
  }
});
