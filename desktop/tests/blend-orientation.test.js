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
  CommandSession,
  activeParameters,
} from '../src/core.js';
import { toPaper } from '../src/paper-runtime.js';
import { blendSpineOrientations, orientBlendContour } from '../src/blend-spine.js';
import { evaluateBlend } from '../src/blend-node-binding.js';
import { paintedGroup } from '../src/composition-reference.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
} from '../src/composition-project-model.js';

const near = (a, b) => a.forEach((x, i) => assert.ok(Math.abs(x - b[i]) < 1e-6, `${a} != ${b}`));
function fixture(raw = 'M0 0L100 0') {
  const doc = document(),
    l = doc.layers[0],
    a = node('Rectangle'),
    b = node('Rectangle'),
    fa = node('Fill'),
    fb = node('Fill'),
    spine = node('Imported Path'),
    blend = node('Blend'),
    out = node('Output');
  a.params.center.value = [10, 20];
  b.params.center.value = [110, 20];
  a.params.size.value = b.params.size.value = [20, 40];
  spine.rawPath = raw;
  blend.operation = 'spine-tangent-steps';
  blend.params.steps.value = 1;
  l.nodes = [a, b, fa, fb, spine, blend, out];
  l.output = out.id;
  for (const [from, to, key] of [
    [a, fa, 'path'],
    [b, fb, 'path'],
    [fa, blend, 'a'],
    [fb, blend, 'b'],
    [spine, blend, 'spine'],
    [blend, out, 'path'],
  ])
    connect(l, from.id, to.id, key);
  return { doc, l, a, b, spine, blend };
}
const result = (doc) => evaluate(doc.layers[0], {}, doc.imageAssets, doc);
const contour = (positions) => ({
  closed: false,
  points: positions.map((position) => ({ position, handleA: [0, 0], handleB: [0, 0] })),
});
const shape = () => ({
  ...contour([
    [0, 0],
    [20, 0],
    [20, 40],
    [0, 40],
  ]),
  closed: true,
});

test('horizontal tangent is fixed identity, upward and reverse rotate endpoints and intermediates without author mutation', () => {
  for (const [raw, direction] of [
    ['M0 0L100 0', [1, 0]],
    ['M0 0L0 -100', [0, -1]],
    ['M100 0L0 0', [-1, 0]],
  ]) {
    const { doc, blend } = fixture(raw);
    const before = JSON.stringify(doc),
      r = result(doc);
    assert.equal(r.error, null);
    assert.equal(JSON.stringify(doc), before);
    blend.operation = 'spine-steps';
    const fixed = result(doc).path;
    blend.operation = 'spine-tangent-steps';
    for (let i = 0; i < 3; i++) {
      const item = toPaper({ contours: [fixed.children[i].contours[0]] });
      const pivot = [item.bounds.center.x, item.bounds.center.y];
      item.remove();
      r.path.children[i].contours[0].points.forEach((p, j) => {
        const q = fixed.children[i].contours[0].points[j],
          x = q.position[0] - pivot[0],
          y = q.position[1] - pivot[1];
        near(p.position, [
          pivot[0] + direction[0] * x - direction[1] * y,
          pivot[1] + direction[1] * x + direction[0] * y,
        ]);
      });
      assert.deepEqual(r.path.children[i].fill, fixed.children[i].fill);
    }
    if (direction[0] === 1)
      assert.equal(
        exportSVG(doc),
        (() => {
          blend.operation = 'spine-steps';
          return exportSVG(doc);
        })(),
      );
  }
});

test('curved spine rotates both relative handles and preserves exact endpoint placement and source bytes', () => {
  const a = shape(),
    b = shape();
  a.points[0].handleA = [-3, 4];
  a.points[0].handleB = [5, -6];
  const spine = {
    contours: [
      {
        closed: false,
        points: [
          { position: [0, 0], handleA: [0, 0], handleB: [0, 100] },
          { position: [100, 0], handleA: [0, 100], handleB: [0, 0] },
        ],
      },
    ],
  };
  const original = JSON.stringify({ a, b, spine }),
    frames = blendSpineOrientations(spine, a, b, 3);
  near(frames[0].position, [0, 0]);
  near(frames[1].position, [50, 75]);
  near(frames[2].position, [100, 0]);
  near(frames[0].tangent, [0, 1]);
  near(frames[1].tangent, [1, 0]);
  near(frames[2].tangent, [0, -1]);
  const rotated = orientBlendContour(a, frames[0]);
  near(rotated.points[0].handleA, [-4, -3]);
  near(rotated.points[0].handleB, [6, 5]);
  near(rotated.points[0].position, [frames[0].pivot[1], -frames[0].pivot[0]]);
  assert.equal(JSON.stringify({ a, b, spine }), original);
});

test('sampled corner and zero endpoint tangent diagnose, unsampled corners and collinear joins retain explicit semantics', () => {
  for (const raw of ['M0 0L100 0L100 100', 'M0 0L100 0L100 0']) {
    const { doc, blend } = fixture(raw);
    assert.match(result(doc).error, /tangent|discontinuous/);
    blend.operation = 'spine-steps';
    assert.equal(result(doc).error, null);
  }
  const { doc, blend } = fixture('M0 0L100 0L100 100');
  blend.params.steps.value = 0;
  assert.equal(result(doc).error, null);
  assert.equal(result(fixture('M0 0L100 0L200 0').doc).error, null);
});

test('invalid spines and finite-overflow rotation fail without guessed orientation', () => {
  for (const raw of ['M0 0L0 0', 'M0 0L100 0Z', 'M0 0L100 0M200 0L300 0'])
    assert.ok(result(fixture(raw).doc).error);
  const s = shape();
  assert.throws(
    () =>
      blendSpineOrientations(
        {
          contours: [
            contour([
              [0, 0],
              [Infinity, 0],
            ]),
          ],
        },
        s,
        s,
        3,
      ),
    /finite/,
  );
  assert.throws(
    () =>
      orientBlendContour(s, { position: [1e308, 1e308], pivot: [-1e308, -1e308], tangent: [1, 1] }),
    /finite/,
  );
});

test('Session mode/edits, Undo/Redo, both codecs and Expand preserve tangent appearance', () => {
  const { doc, l, blend } = fixture('M0 0C0 100 100 100 100 0'),
    session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({ ...session.state, type, payload: { layerId: l.id, ...payload } });
  const initial = exportSVG(session.snapshot());
  send('set-parameter', { nodeId: blend.id, key: 'steps', value: 3 });
  const edited = exportSVG(session.snapshot());
  assert.notEqual(edited, initial);
  send('undo');
  assert.equal(exportSVG(session.snapshot()), initial);
  send('redo');
  assert.equal(exportSVG(session.snapshot()), edited);
  assert.equal(exportSVG(deserialize(serialize(session.snapshot()))), edited);
  const p = migrateLegacyDocumentToCompositionProject(session.snapshot());
  assert.deepEqual(deserializeCompositionProject(serializeCompositionProject(p)), p);
  send('expand-blend', { nodeId: blend.id });
  assert.equal(exportSVG(session.snapshot()), edited);
  send('undo');
  assert.equal(exportSVG(session.snapshot()), edited);
  assert.ok(
    activeParameters(session.snapshot().layers[0].nodes.find((n) => n.id === blend.id)).some(
      ([key]) => key === 'spine',
    ),
  );
});

test('tangent evaluation preserves source/spine blockers and asymmetric virtual pivot', () => {
  const make = (points) => ({
    contours: [{ ...contour(points), closed: true }],
    fill: { hex: '#123456', alpha: 0.8 },
  });
  const a = make([
      [0, 0],
      [20, 0],
      [0, 10],
    ]),
    b = make([
      [100, 0],
      [120, 50],
      [130, 10],
      [110, 30],
    ]);
  const spine = {
    contours: [
      contour([
        [0, 0],
        [0, 100],
      ]),
    ],
    exportBlockers: ['spine', 'same'],
  };
  a.exportBlockers = ['source', 'same'];
  b.exportBlockers = ['source'];
  const inputs = { a, b, spine, steps: 1 },
    before = JSON.stringify(inputs),
    context = { nodeId: 'fixture', groupAppearance: paintedGroup };
  const actual = evaluateBlend('spine-tangent-steps', (key) => inputs[key], context);
  const plain = evaluateBlend('steps', (key) => inputs[key], context);
  const ca = [10, 5],
    cb = [115, 25];
  for (let i = 0; i < 3; i++) {
    const t = i / 2,
      pivot = ca.map((x, j) => (1 - t) * x + t * cb[j]);
    actual.children[i].contours[0].points.forEach((point, j) => {
      const p = plain.children[i].contours[0].points[j].position;
      near(point.position, [-(p[1] - pivot[1]), 100 * t + p[0] - pivot[0]]);
    });
    assert.deepEqual(actual.children[i].exportBlockers, ['source', 'same', 'spine']);
  }
  assert.equal(JSON.stringify(inputs), before);
});

test('source edit diagnoses, explicit fixed mode and Undo recover without discarding the wire', () => {
  const { doc, l, blend, spine } = fixture();
  assert.equal(result(doc).error, null);
  spine.rawPath = 'M0 0L100 0L100 100';
  assert.match(result(doc).error, /discontinuous/);
  blend.operation = 'spine-steps';
  const session = new CommandSession(doc),
    before = session.snapshot();
  const send = (type, payload) =>
    session.dispatch({ ...session.state, type, payload: { layerId: l.id, ...payload } });
  assert.equal(result(before).error, null);
  send('set-operation', { nodeId: blend.id, operation: 'spine-tangent-steps' });
  assert.match(result(session.snapshot()).error, /discontinuous/);
  const invalid = session.snapshot();
  assert.throws(() => send('expand-blend', { nodeId: blend.id }), /discontinuous/);
  assert.deepEqual(session.snapshot(), invalid);
  send('undo', {});
  assert.deepEqual(session.snapshot(), before);
  assert.equal(
    session.snapshot().layers[0].nodes.find((n) => n.id === blend.id).params.spine.source,
    spine.id,
  );
});
