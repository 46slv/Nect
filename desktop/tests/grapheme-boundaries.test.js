import test from 'node:test';
import assert from 'node:assert/strict';
import {
  graphemeBoundaries,
  graphemeCaretIndex,
  graphemeDeletionRange,
  graphemeMovementCorrection,
} from '../src/grapheme-editing.js';
import {
  graphemeSamples,
  scalarSamples,
  expectedBoundaries,
  scalarGeometryFixture,
} from './fixtures/grapheme-editing.js';
import { textEditGeometry, nearestTextCaret } from '../src/canvas-text-edit.js';

for (const sample of [...graphemeSamples, ...scalarSamples]) {
  test(`raw boundaries and legal Canvas caret projection: ${sample.name}`, () => {
    const text = `A${sample.text}B`,
      expected = expectedBoundaries(sample);
    assert.deepEqual(graphemeBoundaries(text), expected);
    const f = scalarGeometryFixture(text),
      geometry = textEditGeometry(f.node, f.layout, f.occurrence);
    const projected = {
      ...geometry,
      carets: geometry.carets.filter(({ index }) => graphemeBoundaries(text).includes(index)),
    };
    assert.deepEqual(
      projected.carets.map(({ index }) => index),
      expected,
    );
    for (const caret of geometry.carets) {
      const midpoint = caret.points[0].map((value, i) => (value + caret.points[1][i]) / 2);
      assert.ok(expected.includes(nearestTextCaret(projected, midpoint)));
    }
  });
}

for (const sample of graphemeSamples) {
  test(`deletion and movement cannot expose an interior boundary: ${sample.name}`, () => {
    const text = `A${sample.text}B`,
      end = 1 + sample.text.length;
    assert.deepEqual(
      graphemeDeletionRange(text, { start: end, end, inputType: 'deleteContentBackward' }),
      { start: 1, end },
    );
    assert.deepEqual(
      graphemeDeletionRange(text, { start: 1, end: 1, inputType: 'deleteContentForward' }),
      { start: 1, end },
    );
    for (let index = 2; index < end; index++) {
      for (const inputType of ['deleteContentBackward', 'deleteContentForward'])
        assert.deepEqual(graphemeDeletionRange(text, { start: index, end: index, inputType }), {
          start: 1,
          end,
        });
      for (const [logicalDirection, target] of [
        ['backward', 1],
        ['forward', end],
      ])
        assert.deepEqual(
          graphemeMovementCorrection(text, { start: index, end: index, logicalDirection }),
          { start: target, end: target, selectionDirection: 'none' },
        );
      assert.deepEqual(
        graphemeDeletionRange(text, {
          start: index,
          end: end + 1,
          inputType: 'deleteContentForward',
        }),
        { start: 1, end: end + 1 },
      );
      assert.deepEqual(
        graphemeDeletionRange(text, { start: 0, end: index, inputType: 'deleteContentBackward' }),
        { start: 0, end },
      );
    }
    for (const index of [0, 1, end, end + 1])
      for (const logicalDirection of ['forward', 'backward'])
        assert.equal(
          graphemeMovementCorrection(text, { start: index, end: index, logicalDirection }),
          null,
        );
  });
}

test('selection extension preserves a legal anchor and direction across crossing it', () => {
  const text = 'Ax\u0301B';
  assert.deepEqual(
    graphemeMovementCorrection(text, {
      start: 0,
      end: 2,
      selectionDirection: 'forward',
      logicalDirection: 'forward',
      extend: true,
    }),
    { start: 0, end: 3, selectionDirection: 'forward' },
  );
  assert.deepEqual(
    graphemeMovementCorrection(text, {
      start: 2,
      end: 4,
      selectionDirection: 'backward',
      logicalDirection: 'backward',
      extend: true,
    }),
    { start: 1, end: 4, selectionDirection: 'backward' },
  );
  assert.deepEqual(
    graphemeMovementCorrection(text, {
      start: 1,
      end: 2,
      selectionDirection: 'forward',
      logicalDirection: 'backward',
      extend: true,
    }),
    { start: 1, end: 1, selectionDirection: 'none' },
  );
  assert.deepEqual(
    graphemeMovementCorrection(text, {
      start: 2,
      end: 3,
      selectionDirection: 'backward',
      logicalDirection: 'forward',
      extend: true,
    }),
    { start: 3, end: 3, selectionDirection: 'none' },
  );
  assert.deepEqual(
    graphemeMovementCorrection(text, {
      start: 2,
      end: 4,
      selectionDirection: 'forward',
      logicalDirection: 'forward',
      extend: true,
    }),
    { start: 1, end: 4, selectionDirection: 'forward' },
  );
  assert.equal(
    graphemeMovementCorrection(text, {
      start: 1,
      end: 4,
      selectionDirection: 'forward',
      logicalDirection: 'backward',
      extend: true,
    }),
    null,
  );
  assert.equal(
    graphemeMovementCorrection(text, { start: 1, end: 2, logicalDirection: 'forward' }),
    null,
    'a noncollapsed selection is not an unmodified native caret movement',
  );
});

test('logical movement is independent of visual arrow direction in bidi text', () => {
  const text = '\u05d0\u05b7\u05d1';
  assert.deepEqual(
    graphemeMovementCorrection(text, { start: 1, end: 1, logicalDirection: 'forward' }),
    { start: 2, end: 2, selectionDirection: 'none' },
  );
  assert.deepEqual(
    graphemeMovementCorrection(text, { start: 1, end: 1, logicalDirection: 'backward' }),
    { start: 0, end: 0, selectionDirection: 'none' },
  );
});

test('composition/history/word and line intents bypass deletion planning', () => {
  for (const inputType of [
    'insertCompositionText',
    'deleteCompositionText',
    'historyUndo',
    'historyRedo',
    'deleteWordBackward',
    'deleteSoftLineForward',
    'deleteByCut',
    'insertText',
    undefined,
  ])
    assert.equal(graphemeDeletionRange('Ax\u0301B', { start: 2, end: 2, inputType }), null);
  assert.equal(
    graphemeDeletionRange(null, {
      start: -1,
      end: 100,
      inputType: 'deleteContentBackward',
      composing: true,
    }),
    null,
    'do not inspect a transient IME string',
  );
  assert.equal(graphemeMovementCorrection(null, { composing: true }), null);
});

test('raw Unicode, empty text, line endings and normalization remain untouched', () => {
  assert.deepEqual(graphemeBoundaries(''), [0]);
  assert.deepEqual(graphemeBoundaries('e\u0301\r\n\u00e9'), [0, 2, 4, 5]);
  assert.deepEqual(
    graphemeDeletionRange('A\r\nB', { start: 3, end: 3, inputType: 'deleteContentBackward' }),
    { start: 1, end: 3 },
  );
  assert.deepEqual(
    graphemeDeletionRange('', { start: 0, end: 0, inputType: 'deleteContentBackward' }),
    { start: 0, end: 0 },
  );
  assert.deepEqual(
    graphemeDeletionRange('A', { start: 0, end: 0, inputType: 'deleteContentBackward' }),
    { start: 0, end: 0 },
  );
  assert.deepEqual(
    graphemeDeletionRange('A', { start: 1, end: 1, inputType: 'deleteContentForward' }),
    { start: 1, end: 1 },
  );
  assert.equal(graphemeCaretIndex('Ax\u0301B', 2, 'nearest'), 3);
  assert.equal(graphemeCaretIndex('Ax\u0301B', 2, 'backward'), 1);
  assert.equal(graphemeCaretIndex('Ax\u0301B', 2, 'forward'), 3);
});

test('invalid settled ranges are rejected without a partial result', () => {
  for (const [start, end] of [
    [-1, 1],
    [2, 1],
    [0, 6],
    [NaN, 1],
    [1.5, 2],
  ])
    assert.throws(
      () => graphemeDeletionRange('abc', { start, end, inputType: 'deleteContentBackward' }),
      /range/,
    );
  assert.throws(() => graphemeCaretIndex('abc', 4), /range/);
  assert.throws(() => graphemeCaretIndex('abc', 1, 'left'), /bias/);
  assert.throws(
    () => graphemeMovementCorrection('abc', { start: 1, end: 1, logicalDirection: 'left' }),
    /logical/,
  );
  assert.throws(() => graphemeBoundaries(null), /text/);
});
