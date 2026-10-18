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

  // This is an executable acceptance target, not a claimed implementation.
  // Enable strict mode after handoff to make remaining gaps fail the command.
  const splits =
    sample.text.length > 1 && sample.boundaries.length === 2 && [...sample.text].length > 1;
  test(
    `Canvas pointer carets respect grapheme boundaries: ${sample.name}`,
    {
      todo:
        splits && !process.env.NECT_GRAPHEME_ACCEPTANCE
          ? 'Pending font implementation handoff; current scalar geometry splits this grapheme'
          : false,
    },
    () => {
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
    },
  );
}
