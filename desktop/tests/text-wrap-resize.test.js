import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { parseFont } from '../src/fonts.js';
import {
  evaluate,
  CommandSession,
  serialize,
  exportSVG,
  setCharacterStyles,
  setCharacterOffsets,
  parameter,
} from '../src/core.js';
import { textWrapResizeContext, resizedTextWrapWidth } from '../src/text-wrap-resize.js';
import { textWrapCanvasFixture } from './fixtures/text-wrap-canvas.js';
import { transformCharacterPoint } from '../src/character-geometry.js';
const b = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
const fonts = { default: parseFont(b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength)) };
const context = (doc, sourceId = null) =>
  textWrapResizeContext(
    doc.layers[0],
    evaluate(doc.layers[0], fonts, doc.imageAssets, doc),
    fonts,
    sourceId,
  );

test('right width frame is source-local, fixed-origin and inverse-scaled with numeric bounds', () => {
  const f = textWrapCanvasFixture({ transformed: true }),
    saved = serialize(f.doc),
    c = context(f.doc);
  assert.equal(c.supported, true);
  assert.deepEqual(c.matrix, [1.5, 0, -0, 1.5, 20, 30]);
  assert.equal(c.handle[0], (140 + 320) * 1.5 + 20);
  assert.equal(resizedTextWrapWidth(c.width, c.matrix, [60, 100]), 360);
  assert.equal(resizedTextWrapWidth(c.width, c.matrix, [-10000, 0]), 0.01);
  assert.equal(resizedTextWrapWidth(c.width, c.matrix, [1000000, 0]), 100000);
  assert.equal(
    resizedTextWrapWidth(0.001, c.matrix, [0, 100]),
    0.001,
    'vertical/no width motion is a no-op',
  );
  assert.throws(() => resizedTextWrapWidth(320, c.matrix, [Infinity, 0]));
  assert.throws(() => resizedTextWrapWidth(320, c.matrix, [1, 2, 3]));
  assert.equal(serialize(f.doc), saved, 'display geometry is document-neutral');
  assert.deepEqual(
    context(f.doc, f.textId).matrix,
    [1, 0, 0, 1, 0, 0],
    'graph source overlay precedes downstream transforms',
  );
});

test('existing wrap Session gesture preserves IDs/position/policy, reflows, cancels and is one Undo', () => {
  const f = textWrapCanvasFixture(),
    session = new CommandSession(f.doc, { fonts: () => fonts });
  const before = session.snapshot(),
    initialSVG = exportSVG(before, fonts),
    first = context(before);
  const target = { kind: 'text-values', layerId: f.layerId, nodeId: f.textId, textKind: 'wrap' };
  let token = session.dispatch({ ...session.state, type: 'begin-gesture', payload: { target } });
  token = session.dispatch({
    ...session.state,
    type: 'update-gesture',
    payload: { ...token, update: { values: [180] } },
  });
  const preview = session.previewSnapshot(token.gestureId).document;
  assert.deepEqual(session.snapshot(), before);
  assert.deepEqual(
    preview.layers[0].nodes[0].characterPositions,
    before.layers[0].nodes[0].characterPositions,
  );
  assert.deepEqual(
    preview.layers[0].nodes[0].params.position,
    before.layers[0].nodes[0].params.position,
  );
  assert.equal(
    preview.layers[0].nodes[0].textWrap.policy,
    before.layers[0].nodes[0].textWrap.policy,
  );
  assert.deepEqual(context(preview).corners[0], first.corners[0], 'frame left/top fixed');
  assert.ok(context(preview).corners[2][1] > first.corners[2][1], 'height grows after reflow');
  session.dispatch({ ...session.state, type: 'cancel-gesture', payload: token });
  assert.deepEqual(session.snapshot(), before);
  assert.equal(exportSVG(session.snapshot(), fonts), initialSVG);
  assert.equal(session.historyView.past.length, 0);
  token = session.dispatch({ ...session.state, type: 'begin-gesture', payload: { target } });
  token = session.dispatch({
    ...session.state,
    type: 'update-gesture',
    payload: { ...token, update: { values: [180] } },
  });
  session.dispatch({ ...session.state, type: 'commit-gesture', payload: token });
  assert.equal(session.historyView.past.length, 1);
  const committed = session.snapshot();
  const expected = structuredClone(before);
  expected.layers[0].nodes[0].textWrap.width = 180;
  assert.deepEqual(committed, expected);
  assert.notEqual(exportSVG(committed, fonts), initialSVG);
  session.dispatch({ ...session.state, type: 'undo' });
  assert.deepEqual(session.snapshot(), before);
  session.dispatch({ ...session.state, type: 'redo' });
  assert.deepEqual(session.snapshot(), committed);
});

test('visibility/lock/driven/reference/transform guards reject without mutation', () => {
  const mutations = [
    (d) => (d.layers[0].locked = true),
    (d) => (d.layers[0].visible = false),
    (d) => (d.layers[0].nodes[0].locked = true),
    (d) => (d.layers[0].nodes[0].bypassed = true),
    (d) => (d.layers[0].nodes[0].textWrap = undefined),
    (d) => {
      d.layers[0].nodes[0].writingMode = 'vertical-rl';
      d.layers[0].nodes[0].textWrap = { height: 320, policy: 'japanese-basic-v1' };
    },
    (d) => (d.layers[0].nodes[0].alignment = 'unknown'),
    (d) => (d.layers[0].nodes[0].crossAlignment = 'unknown'),
    (d) => (d.layers[0].nodes[0].params.size.expression = 'value * 2'),
    (d) => {
      const n = d.layers[0].nodes[1];
      n.params.skew = { ...parameter(n, 'skew'), value: 10 };
    },
    (d) => (d.layers[0].nodes[1].params.scale.value = -1),
    (d) => (d.layers[0].nodes[1].params.scale.value = 0),
  ];
  for (const mutate of mutations) {
    const f = textWrapCanvasFixture({ transformed: true });
    mutate(f.doc);
    const before = JSON.stringify(f.doc);
    assert.equal(context(f.doc).supported, false);
    assert.equal(JSON.stringify(f.doc), before);
  }
});

test('source-wide ascent stays fixed and overflowing groups retain warnings and outlines', () => {
  const f = textWrapCanvasFixture(),
    n = f.doc.layers[0].nodes[0];
  n.characterPositions = setCharacterStyles(n.text, n.characterPositions, {
    [n.characterPositions.units[7].id]: { size: 50, baselineShift: 12 },
  });
  const a = context(f.doc);
  n.textWrap.width = 100;
  const c = context(f.doc);
  assert.equal(a.supported, true);
  assert.equal(c.supported, true);
  assert.deepEqual(c.corners[0], a.corners[0]);
  n.textWrap.width = 0.01;
  const r = evaluate(f.doc.layers[0], fonts, f.doc.imageAssets, f.doc);
  assert.ok(r.warnings.some((w) => /Wrap overflow/.test(w)));
  assert.ok(r.path.contours.length > 0);
  assert.ok(exportSVG(f.doc, fonts).includes('<path'));
});

test('legacy width-only Text keeps its schema and wrap representation after resize', () => {
  const f = textWrapCanvasFixture();
  f.doc.schemaVersion = 33;
  f.doc.layers[0].nodes[0].textWrap = { width: 320 };
  f.doc.layers[0].nodes[0].crossAlignment = 'start';
  const before = structuredClone(f.doc),
    session = new CommandSession(f.doc, { fonts: () => fonts });
  assert.equal(context(before).supported, true);
  let token = session.dispatch({
    ...session.state,
    type: 'begin-gesture',
    payload: {
      target: { kind: 'text-values', layerId: f.layerId, nodeId: f.textId, textKind: 'wrap' },
    },
  });
  token = session.dispatch({
    ...session.state,
    type: 'update-gesture',
    payload: { ...token, update: { values: [220] } },
  });
  session.dispatch({ ...session.state, type: 'commit-gesture', payload: token });
  before.layers[0].nodes[0].textWrap.width = 220;
  assert.deepEqual(session.snapshot(), before);
  assert.equal(context(session.snapshot()).corners[0][1], 160);
});

for (const alignment of ['center', 'end'])
  for (const referenceBounds of ['advance', 'visible']) {
    test(`${alignment}/${referenceBounds} frame fixes Position and matches numeric reflow at smaller/larger widths`, () => {
      const f = textWrapCanvasFixture({ transformed: true, alignment, referenceBounds });
      const original = structuredClone(f.doc),
        c = context(original);
      const anchor = alignment === 'center' ? 0.5 : 1;
      const deltaFactor = alignment === 'center' ? 2 : -1;
      assert.equal(c.reference, alignment);
      assert.equal(c.handle[0], c.corners[alignment === 'end' ? 0 : 1][0]);
      const fixedX = c.corners[0][0] + (c.corners[1][0] - c.corners[0][0]) * anchor;
      assert.equal(fixedX, 140 * 1.5 + 20);
      const underflow = resizedTextWrapWidth(
        320,
        c.matrix,
        [(-330 * 1.5) / deltaFactor, 0],
        alignment,
      );
      assert.equal(underflow, 0.01, 'crossing the Position clamps without a reference flip');
      assert.equal(
        resizedTextWrapWidth(320, c.matrix, [(200000 * 1.5) / deltaFactor, 0], alignment),
        100000,
      );
      assert.throws(() => resizedTextWrapWidth(320, c.matrix, [1, 0], 'unknown'));
      for (const width of [220, 420]) {
        const mapped = resizedTextWrapWidth(
          320,
          c.matrix,
          [((width - 320) * 1.5) / deltaFactor, 0],
          alignment,
        );
        assert.equal(mapped, width);
        const session = new CommandSession(original, { fonts: () => fonts });
        const numeric = new CommandSession(original, { fonts: () => fonts });
        const target = {
          kind: 'text-values',
          layerId: f.layerId,
          nodeId: f.textId,
          textKind: 'wrap',
        };
        const preview = () => {
          let token = session.dispatch({
            ...session.state,
            type: 'begin-gesture',
            payload: { target },
          });
          return session.dispatch({
            ...session.state,
            type: 'update-gesture',
            payload: { ...token, update: { values: [mapped] } },
          });
        };
        let token = preview();
        const displayed = session.previewSnapshot(token.gestureId).document;
        const frame = context(displayed);
        assert.equal(
          frame.corners[0][0] + (frame.corners[1][0] - frame.corners[0][0]) * anchor,
          fixedX,
        );
        assert.equal(frame.corners[0][1], c.corners[0][1]);
        assert.deepEqual(session.snapshot(), original);
        session.dispatch({ ...session.state, type: 'cancel-gesture', payload: token });
        assert.deepEqual(session.snapshot(), original);
        assert.equal(session.historyView.past.length, 0);
        token = preview();
        session.dispatch({ ...session.state, type: 'commit-gesture', payload: token });
        numeric.dispatch({
          ...numeric.state,
          type: 'set-text-settings',
          payload: {
            layerId: f.layerId,
            nodeId: f.textId,
            settings: { textWrap: { ...original.layers[0].nodes[0].textWrap, width } },
          },
        });
        const committed = session.snapshot(),
          expected = structuredClone(original);
        expected.layers[0].nodes[0].textWrap.width = width;
        assert.deepEqual(committed, expected);
        assert.deepEqual(committed, numeric.snapshot());
        assert.equal(exportSVG(committed, fonts), exportSVG(numeric.snapshot(), fonts));
        assert.equal(session.historyView.past.length, 1);
        session.dispatch({ ...session.state, type: 'undo' });
        assert.deepEqual(session.snapshot(), original);
        session.dispatch({ ...session.state, type: 'redo' });
        assert.deepEqual(session.snapshot(), committed);
      }
    });
  }

test('all nine X/Y frame references retain Position across reflow and equal numeric width commands', () => {
  const factor = { start: 0, center: 0.5, end: 1 };
  for (const alignment of ['start', 'center', 'end'])
    for (const crossAlignment of ['start', 'center', 'end']) {
      const f = textWrapCanvasFixture({
        transformed: true,
        alignment,
        crossAlignment,
        referenceBounds: alignment === 'center' ? 'visible' : 'advance',
      });
      const before = structuredClone(f.doc),
        first = context(before),
        saved = serialize(f.doc);
      const pin = (c) => [
        c.corners[0][0] + (c.corners[1][0] - c.corners[0][0]) * factor[alignment],
        c.corners[0][1] + (c.corners[3][1] - c.corners[0][1]) * factor[crossAlignment],
      ];
      assert.equal(first.supported, true);
      assert.equal(first.crossReference, crossAlignment);
      assert.deepEqual(pin(first), [230, 270]);
      for (const width of [200, 640]) {
        const session = new CommandSession(before, { fonts: () => fonts }),
          numeric = new CommandSession(before, { fonts: () => fonts });
        let token = session.dispatch({
          ...session.state,
          type: 'begin-gesture',
          payload: {
            target: { kind: 'text-values', layerId: f.layerId, nodeId: f.textId, textKind: 'wrap' },
          },
        });
        token = session.dispatch({
          ...session.state,
          type: 'update-gesture',
          payload: { ...token, update: { values: [width] } },
        });
        const preview = session.previewSnapshot(token.gestureId).document,
          frame = context(preview);
        numeric.dispatch({
          ...numeric.state,
          type: 'set-text-settings',
          payload: {
            layerId: f.layerId,
            nodeId: f.textId,
            settings: { textWrap: { ...before.layers[0].nodes[0].textWrap, width } },
          },
        });
        assert.deepEqual(preview, numeric.snapshot());
        assert.equal(exportSVG(preview, fonts), exportSVG(numeric.snapshot(), fonts));
        assert.deepEqual(pin(frame), pin(first));
        const heightChange =
          frame.corners[3][1] - frame.corners[0][1] - (first.corners[3][1] - first.corners[0][1]);
        assert.ok(
          Math.abs(
            frame.corners[0][1] - first.corners[0][1] + heightChange * factor[crossAlignment],
          ) < 1e-9,
        );
        assert.equal(frame.handle[0], frame.corners[alignment === 'end' ? 0 : 1][0]);
        assert.equal(frame.handle[1], (frame.corners[0][1] + frame.corners[3][1]) / 2);
        // Actual glyphs are already anchored by layout; do not shift their baselines again.
        const glyphs = evaluate(preview.layers[0], fonts, preview.imageAssets, preview)
          .textLayout(f.textId)
          .characters.filter((g) => !g.whitespace);
        const min = Math.min(...glyphs.map((g) => g.bounds[1])),
          max = Math.max(...glyphs.map((g) => g.bounds[3]));
        assert.ok(Math.abs(min + (max - min) * factor[crossAlignment] - 160) < 1e-9);
        session.dispatch({ ...session.state, type: 'cancel-gesture', payload: token });
        assert.deepEqual(session.snapshot(), before);
        assert.equal(session.historyView.past.length, 0);
      }
      assert.equal(serialize(f.doc), saved, 'frame inspection is document neutral');
    }
});

test('rich manual-leading frame does not chase post-layout character offsets or double-apply Y anchoring', () => {
  for (const crossAlignment of ['center', 'end']) {
    const f = textWrapCanvasFixture({ alignment: 'end', crossAlignment });
    const n = f.doc.layers[0].nodes[0];
    n.leading = { mode: 'manual', value: 64 };
    n.characterPositions = setCharacterStyles(n.text, n.characterPositions, {
      [n.characterPositions.units[7].id]: { size: 50, baselineShift: 12 },
    });
    for (const width of [200, 640]) {
      n.textWrap.width = width;
      const frame = context(f.doc),
        adjusted = structuredClone(f.doc),
        t = adjusted.layers[0].nodes[0];
      t.characterPositions = setCharacterOffsets(
        t.text,
        t.characterPositions,
        Object.fromEntries(t.characterPositions.units.map((u) => [u.id, [0, 70]])),
      );
      assert.deepEqual(
        context(adjusted).corners,
        frame.corners,
        'logical frame ignores later character translations',
      );
      const factor = crossAlignment === 'center' ? 0.5 : 1;
      assert.equal(frame.corners[0][1] + (frame.corners[3][1] - frame.corners[0][1]) * factor, 160);
      const glyphs = evaluate(adjusted.layers[0], fonts, adjusted.imageAssets, adjusted)
        .textLayout(f.textId)
        .characters.filter((g) => !g.whitespace);
      const min = Math.min(...glyphs.map((g) => g.bounds[1])),
        max = Math.max(...glyphs.map((g) => g.bounds[3]));
      assert.ok(
        Math.abs(min + (max - min) * factor - 230) < 1e-9,
        'visible ink contains the later offset while Position remains fixed',
      );
      assert.deepEqual(t.params.position.value, [140, 160]);
    }
  }
});

test('zero,90,negative and arbitrary rotation reuse the renderer affine and project only local X', () => {
  for (const angle of [0, 90, -45, 37]) {
    const alignment = angle < 0 ? 'end' : 'center',
      crossAlignment = angle === 37 ? 'end' : 'center';
    const f = textWrapCanvasFixture({
      transformed: true,
      alignment,
      crossAlignment,
      rotation: angle,
      transformAnchor: [70, 90],
      transformPosition: [300, 180],
    });
    const before = structuredClone(f.doc),
      c = context(before),
      local = context(before, f.textId);
    const renderer = evaluate(before.layers[0], fonts, before.imageAssets, before).transformValues(
      f.transformId,
    );
    assert.equal(c.supported, true);
    assert.deepEqual(c.matrix, renderer.matrix);
    assert.deepEqual(
      c.corners,
      local.corners.map((p) => transformCharacterPoint(renderer.matrix, p)),
    );
    assert.deepEqual(c.handle, transformCharacterPoint(renderer.matrix, local.handle));
    assert.equal(
      c.cursor,
      angle === 90
        ? 'ns-resize'
        : angle === -45
          ? 'nesw-resize'
          : angle === 37
            ? 'nwse-resize'
            : 'ew-resize',
    );
    const [a, b, x, y] = c.matrix,
      factor = alignment === 'center' ? 2 : -1;
    assert.equal(
      resizedTextWrapWidth(320, c.matrix, [x * 30, y * 30], alignment),
      320,
      'local Y is width neutral',
    );
    if (angle === 90) {
      assert.equal(
        resizedTextWrapWidth(320, c.matrix, [30, 0], alignment),
        320,
        'screen X is not local width at90',
      );
      assert.equal(
        resizedTextWrapWidth(320, c.matrix, [0, 30], alignment),
        360,
        'vertical screen motion changes width at90',
      );
    }
    assert.equal(
      resizedTextWrapWidth(320, c.matrix, [(a * 1e8) / factor, (b * 1e8) / factor], alignment),
      100000,
    );
    assert.equal(
      resizedTextWrapWidth(320, c.matrix, [(-a * 1e8) / factor, (-b * 1e8) / factor], alignment),
      0.01,
    );
    for (const width of [200, 640]) {
      const dx = (width - 320) / factor,
        dy = 23;
      const mapped = resizedTextWrapWidth(
        320,
        c.matrix,
        [a * dx + x * dy, b * dx + y * dy],
        alignment,
      );
      assert.equal(mapped, width);
      const session = new CommandSession(before, { fonts: () => fonts }),
        numeric = new CommandSession(before, { fonts: () => fonts });
      let token = session.dispatch({
        ...session.state,
        type: 'begin-gesture',
        payload: {
          target: { kind: 'text-values', layerId: f.layerId, nodeId: f.textId, textKind: 'wrap' },
        },
      });
      token = session.dispatch({
        ...session.state,
        type: 'update-gesture',
        payload: { ...token, update: { values: [mapped] } },
      });
      const preview = session.previewSnapshot(token.gestureId).document,
        expected = structuredClone(before);
      expected.layers[0].nodes[0].textWrap.width = width;
      numeric.dispatch({
        ...numeric.state,
        type: 'set-text-settings',
        payload: {
          layerId: f.layerId,
          nodeId: f.textId,
          settings: { textWrap: { ...before.layers[0].nodes[0].textWrap, width } },
        },
      });
      assert.deepEqual(preview, expected);
      assert.deepEqual(preview, numeric.snapshot());
      assert.equal(exportSVG(preview, fonts), exportSVG(numeric.snapshot(), fonts));
      session.dispatch({ ...session.state, type: 'cancel-gesture', payload: token });
      assert.deepEqual(session.snapshot(), before);
      assert.equal(session.historyView.past.length, 0);
    }
  }
});

test('skew/reflection/collapsed and noninvertible rotations refuse without document writes', () => {
  for (const mutate of [
    (n) => (n.params.scale.value = 0),
    (n) => (n.params.scale.value = 1e-8),
    (n) => (n.params.scale.value = -1),
    (n) => (n.params.skew = { ...parameter(n, 'skew'), value: 10 }),
    (n) => {
      n.params.uniformScale = { ...parameter(n, 'uniformScale'), value: false };
      n.params.scaleWidth = { ...parameter(n, 'scaleWidth'), value: 0 };
    },
  ]) {
    const f = textWrapCanvasFixture({ transformed: true, rotation: 37 });
    mutate(f.doc.layers[0].nodes[1]);
    const before = JSON.stringify(f.doc);
    assert.equal(context(f.doc).supported, false);
    assert.equal(JSON.stringify(f.doc), before);
  }
  const f = textWrapCanvasFixture({ transformed: true, rotation: 37 }),
    result = evaluate(f.doc.layers[0], fonts, f.doc.imageAssets, f.doc),
    saved = serialize(f.doc);
  for (const matrix of [
    [1, 0, 0, 0, 0, 0],
    [1e-9, 0, 0, 1e-9, 0, 0],
    [Infinity, 0, 0, 1, 0, 0],
  ]) {
    const invalid = {
      ...result,
      transformValues: () => ({ ...result.transformValues(f.transformId), matrix }),
    };
    assert.equal(textWrapResizeContext(f.doc.layers[0], invalid, fonts).supported, false);
    assert.throws(() => resizedTextWrapWidth(320, matrix, [1, 1]));
  }
  assert.throws(() => resizedTextWrapWidth(320, context(f.doc).matrix, [NaN, 0]));
  assert.equal(serialize(f.doc), saved);
});
