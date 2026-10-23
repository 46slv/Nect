import test from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import {
  blankDocument,
  document,
  guideDefaults,
  gridAnchors,
  snapTranslation,
  exportSVG,
  serialize,
  deserialize,
  CommandSession,
} from '../src/core.js';
import {
  resolveGridBasis,
  gridLocalPoint,
  gridWorldPoint,
  gridLineLayout,
  MAX_GRID_LINES,
} from '../src/guides.js';
import { snapDirectionTranslation } from '../src/direction-snapping.js';
const near = (a, b) => a.forEach((v, i) => assert.ok(Math.abs(v - b[i]) < 1e-8, `${a} != ${b}`));
const fixture = (angle = 45) => {
  const doc = blankDocument();
  doc.width = 601;
  doc.height = 543;
  Object.assign(doc.editor, {
    gridAngle: angle,
    gridSize: 40,
    snap: true,
    snapGuides: false,
    snapArtboard: false,
  });
  return doc;
};
const send = (s, type, payload = {}) => s.dispatch({ ...s.state, type, payload });

test('shared orthonormal basis follows all nine origins, angle quadrants and resized canvases', () => {
  for (const angle of [0, 45, -30, 90])
    for (const anchor of gridAnchors)
      for (const [width, height] of [
        [601, 543],
        [960.5, 540.25],
      ]) {
        const doc = fixture(angle);
        Object.assign(doc, { width, height });
        doc.editor.gridAnchor = anchor;
        const b = resolveGridBasis(doc),
          r = (angle * Math.PI) / 180;
        near(gridWorldPoint(b, [40, -80]), [
          b.origin[0] + 40 * Math.cos(r) + 80 * Math.sin(r),
          b.origin[1] + 40 * Math.sin(r) - 80 * Math.cos(r),
        ]);
        near(gridLocalPoint(b, gridWorldPoint(b, [-123.5, 67.25])), [-123.5, 67.25]);
        assert.ok(Math.abs(b.cos * b.cos + b.sin * b.sin - 1) < 1e-12);
      }
  for (const angle of [undefined, null, '45', NaN, Infinity, 361, -361, 0, 360, -360]) {
    const d = fixture();
    d.editor.gridAngle = angle;
    assert.equal(resolveGridBasis(d).angle, 0);
  }
  assert.deepEqual([resolveGridBasis(fixture(90)).cos, resolveGridBasis(fixture(90)).sin], [0, 1]);
});

test('inverse-grid snapping handles negative coordinates, ties and per-local-axis screen thresholds', () => {
  for (const angle of [0, 45, -30, 90])
    for (const anchor of gridAnchors) {
      const doc = fixture(angle);
      doc.editor.gridAnchor = anchor;
      const b = resolveGridBasis(doc);
      for (const target of [
        [-80, -40],
        [0, 0],
        [80, 120],
      ]) {
        const p = gridWorldPoint(
          b,
          target.map((v, i) => v + (i ? 3 : -2)),
        );
        const result = snapTranslation(doc, [[p[0]], [p[1]]], [0, 0]);
        near(
          gridLocalPoint(
            b,
            p.map((v, i) => v + result.delta[i]),
          ),
          target,
        );
        for (const m of result.matches.filter((m) => m.gridLine && m.line))
          for (const endpoint of m.line)
            assert.ok(Math.abs(gridLocalPoint(b, endpoint)[m.axis] - m.position) < 1e-8);
      }
      for (const scale of [0.5, 1, 2]) {
        const p = gridWorldPoint(b, [80 + 5 / scale, 40]);
        const hit = snapTranslation(doc, [[p[0]], [p[1]]], [0, 0], scale);
        near(
          gridLocalPoint(
            b,
            p.map((v, i) => v + hit.delta[i]),
          ),
          [80, 40],
        );
        const miss = gridWorldPoint(b, [80 + 7 / scale, 40]);
        const result = snapTranslation(doc, [[miss[0]], [miss[1]]], [0, 0], scale);
        near(
          gridLocalPoint(
            b,
            miss.map((v, i) => v + result.delta[i]),
          ),
          [80 + 7 / scale, 40],
        );
        assert.deepEqual(snapTranslation(doc, [[p[0]], [p[1]]], [0, 0], scale, true).matches, []);
      }
    }
  const d = fixture(90);
  d.editor.gridSize = 8;
  const b = resolveGridBasis(d),
    p = gridWorldPoint(b, [-4, 4]);
  const result = snapTranslation(d, [[p[0]], [p[1]]], [0, 0]);
  near(
    gridLocalPoint(
      b,
      p.map((v, i) => v + result.delta[i]),
    ),
    [0, 8],
  );
});

test('zero/full-turn/fallback snap results exactly match the actual PR63 implementation', async () => {
  const source = execFileSync(
    'git',
    ['show', '49283bca1c90e83aec3bb8c901017d854e526dc3:desktop/src/guides.js'],
    { encoding: 'utf8' },
  );
  const old = await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const d = fixture(0);
  d.editor.snapGuides = true;
  d.editor.snapArtboard = true;
  d.editor.snapArtwork = true;
  d.editor.guides = [{ id: 'guide', axis: 'x', position: 52, locked: false }];
  for (const angle of [undefined, 0, 360, -360, 'bad', null])
    for (const scale of [0.5, 1, 2])
      for (const delta of [
        [0, 0],
        [2, -1],
        [-7, 4],
      ]) {
        d.editor.gridAngle = angle;
        const args = [
          d,
          [
            [-13, 50, 97],
            [-9, 56, 110],
          ],
          delta,
          scale,
          false,
          null,
          [],
          [
            [
              [50, 70, 90],
              [30, 50, 70],
            ],
          ],
        ];
        assert.deepEqual(snapTranslation(...args), old.snapTranslation(...args));
      }
});

test('finite display segments cover clipped lattice ranges with a strict density budget', () => {
  for (const angle of [45, -30, 90])
    for (const anchor of ['top-left', 'center', 'bottom-right']) {
      const d = fixture(angle);
      d.editor.gridAnchor = anchor;
      for (const view of [
        [0, 0, d.width, d.height],
        [100, 150, 500, 450],
      ]) {
        const layout = gridLineLayout(d, d.editor, view);
        assert.ok(layout.lines.length > 0);
        assert.equal(layout.stride, 1);
        for (const line of layout.lines) {
          assert.ok(Number.isInteger(line.index));
          for (const p of line.segment) {
            assert.ok(p.every(Number.isFinite));
            assert.ok(p[0] >= view[0] && p[0] <= view[2] && p[1] >= view[1] && p[1] <= view[3]);
            assert.ok(Math.abs(gridLocalPoint(layout.basis, p)[line.axis] - line.position) < 1e-8);
          }
        }
      }
      d.width = d.height = 100000;
      d.editor.gridSize = 1;
      const before = serialize(d),
        dense = gridLineLayout(d);
      assert.ok(dense.stride > 1);
      assert.ok(dense.lines.length <= MAX_GRID_LINES);
      assert.ok(dense.lines.every((line) => line.index % dense.stride === 0));
      assert.equal(serialize(d), before);
      assert.equal(d.editor.gridSize, 1);
      assert.equal(gridLineLayout(d, d.editor, [-100, -100, 0, 0]).lines.length, 0);
    }
});

test('explicit guide/object targets retain priority and axis-only edits never acquire a second coordinate', () => {
  const d = fixture(45);
  Object.assign(d.editor, {
    snapGuides: true,
    guides: [{ id: 'x', axis: 'x', position: 301, locked: false }],
  });
  const r = snapTranslation(d, [[300], [300]], [0, 0]);
  assert.deepEqual(r.delta, [1, 0]);
  assert.equal(r.matches[0].kind, 'guide');
  assert.ok(!r.matches.some((m) => m.gridLine));
  d.editor.snapGuides = false;
  d.editor.snapArtwork = true;
  const object = snapTranslation(
    d,
    [[300], [300]],
    [0, 0],
    1,
    false,
    null,
    [],
    [
      [
        [302, 352, 402],
        [399, 449, 499],
      ],
    ],
  );
  assert.deepEqual(object.delta, [2, 0]);
  assert.equal(object.matches[0].kind, 'artwork');
  d.editor.snapArtwork = false;
  for (const sources of [
    [[300], []],
    [[], [300]],
    [Array(257).fill(300), [300]],
  ])
    assert.deepEqual(snapTranslation(d, sources, [3, 4]), { delta: [3, 4], matches: [] });
});

test('constrained world-axis/diagonal movement intersects the actual rotated line without false feedback', () => {
  for (const angle of [45, -30, 90])
    for (const direction of [
      [1, 0],
      [0, 1],
      [1, 1],
      [1, -1],
    ]) {
      const d = fixture(angle),
        b = resolveGridBasis(d),
        target = gridWorldPoint(b, [40, 80]);
      const p = target.map((v, a) => v - 10 * direction[a]);
      const result = snapDirectionTranslation(
        (sources, delta, scale, options) =>
          snapTranslation(d, sources, delta, scale, false, null, [], [], [], options),
        [[p[0]], [p[1]]],
        direction.map((v) => 8 * v),
        { direction },
      );
      near(
        result.delta,
        direction.map((v) => 10 * v),
      );
      assert.ok(Math.abs(result.delta[0] * direction[1] - result.delta[1] * direction[0]) < 1e-8);
      const local = gridLocalPoint(
        b,
        p.map((v, a) => v + result.delta[a]),
      );
      assert.ok(result.matches.some((m) => m.gridLine));
      for (const m of result.matches.filter((m) => m.gridLine))
        assert.ok(Math.abs(local[m.axis] - m.position) < 1e-8);
    }
});

test('angle edits retain geometry/output, schema/JSON/history and numeric gesture cancellation contracts', () => {
  const d = document();
  d.editor = guideDefaults();
  d.editor.guides = [{ id: 'absolute', axis: 'y', position: 123.5, locked: true }];
  const s = new CommandSession(d),
    before = s.snapshot(),
    svg = exportSVG(before);
  send(s, 'set-guide-setting', { key: 'gridAngle', value: 45 });
  const changed = s.snapshot();
  assert.equal(changed.schemaVersion, 49);
  assert.deepEqual(changed.layers, before.layers);
  assert.deepEqual(changed.editor.guides, before.editor.guides);
  assert.equal(exportSVG(changed), svg);
  assert.deepEqual(deserialize(serialize(changed)), changed);
  send(s, 'set-guide-setting', { key: 'gridAngle', value: 45 });
  assert.equal(s.historyView.past.length, 1);
  send(s, 'undo');
  assert.deepEqual(s.snapshot(), before);
  send(s, 'redo');
  assert.deepEqual(s.snapshot(), changed);
  for (const value of [361, -361, NaN, Infinity, null, '45']) {
    const snapshot = s.snapshot(),
      state = s.state,
      past = s.historyView.past.length;
    assert.throws(() => send(s, 'set-guide-setting', { key: 'gridAngle', value }), /Grid angle/);
    assert.deepEqual(s.snapshot(), snapshot);
    assert.deepEqual(s.state, state);
    assert.equal(s.historyView.past.length, past);
  }
  const stale = s.state;
  send(s, 'set-guide-setting', { key: 'gridAngle', value: 90 });
  assert.throws(
    () =>
      s.dispatch({
        ...stale,
        type: 'set-guide-setting',
        payload: { key: 'gridAngle', value: -30 },
      }),
    /revision|stale/i,
  );
  const old = document();
  old.editor = guideDefaults();
  delete old.editor.gridAngle;
  const saved = serialize(old);
  assert.equal(serialize(deserialize(saved)), saved);
  old.editor.gridAngle = 45;
  assert.throws(() => deserialize(serialize(old)), /schemaVersion 49/);
  old.editor.gridAngle = 0;
  assert.equal(deserialize(serialize(old)).schemaVersion, old.schemaVersion);
  const session = new CommandSession(old),
    original = session.snapshot();
  const token = send(session, 'begin-gesture', {
    target: {
      kind: 'numeric-command',
      command: 'set-guide-setting',
      payload: { key: 'gridAngle' },
      initial: 0,
    },
  });
  const updated = send(session, 'update-gesture', { ...token, update: { value: -30 } });
  assert.equal(session.previewSnapshot(updated.gestureId).document.editor.gridAngle, -30);
  assert.deepEqual(session.snapshot(), original);
  send(session, 'cancel-gesture', { gestureId: updated.gestureId });
  assert.deepEqual(session.snapshot(), original);
  assert.equal(session.historyView.past.length, 0);
});
