import test from 'node:test';
import assert from 'node:assert/strict';
import {
  evaluate,
  serialize,
  deserialize,
  exportSVG,
  CommandSession,
  node,
  connect,
} from '../src/core.js';
import { repeatGeometry } from '../src/repeater.js';
import { alternateRepeaterFixture } from './fixtures/repeater-alternate-scale.js';

const near = (a, b) => a.forEach((v, i) => assert.ok(Math.abs(v - b[i]) < 1e-8, `${a} != ${b}`));
const resolvedPoints = (source) =>
  source.points.map((p) => ({
    id: p.id,
    position: p.position.value,
    handleA: p.handleA.value,
    handleB: p.handleB.value,
  }));
const settings = (repeat) =>
  Object.fromEntries(Object.entries(repeat.params).map(([k, q]) => [k, q.value]));
const send = (session, layer, repeat, type, payload = {}) =>
  session.dispatch({
    ...session.state,
    type,
    payload: { layerId: layer.id, nodeId: repeat.id, ...payload },
  });

for (const kind of ['linear', 'radial']) {
  test(`24 ${kind} copies scale odd geometry/handles around copied anchors without changing placement or IDs`, () => {
    const { doc, layer, source, repeat, anchor } = alternateRepeaterFixture(kind);
    const before = structuredClone(source),
      options = settings(repeat);
    const input = { contours: [{ id: 'motif', closed: true, points: resolvedPoints(source) }] };
    const run = (extra) => repeatGeometry(input, { ...options, prefix: 'seed', ...extra });
    const baseline = run({ alternateScale: 100 });
    const omitted = { ...options, prefix: 'seed' };
    delete omitted.alternateScale;
    assert.deepEqual(
      repeatGeometry(input, omitted),
      baseline,
      'identity takes the original transform path',
    );
    for (const percent of [60, 150]) {
      const scaled = run({ alternateScale: percent });
      assert.equal(scaled.contours.length, 24);
      for (let i = 0; i < 24; i++) {
        const original = baseline.contours[i],
          actual = scaled.contours[i];
        const center = original.points.reduce(
          (p, pt) => p.map((v, axis) => v + pt.position[axis] / 4),
          [0, 0],
        );
        const ratio = i % 2 ? percent / 100 : 1;
        assert.deepEqual({ ...actual, points: [] }, { ...original, points: [] });
        for (let p = 0; p < 4; p++) {
          assert.equal(actual.points[p].id, original.points[p].id);
          near(
            actual.points[p].position,
            original.points[p].position.map((v, axis) => center[axis] + (v - center[axis]) * ratio),
          );
          for (const key of ['handleA', 'handleB'])
            near(
              actual.points[p][key],
              original.points[p][key].map((v) => v * ratio),
            );
        }
        near(
          actual.points.reduce((p, pt) => p.map((v, axis) => v + pt.position[axis] / 4), [0, 0]),
          center,
        );
        if (!(i % 2)) assert.deepEqual(actual, original);
      }
      repeat.params.alternateScale.value = percent;
      doc.schemaVersion = 45;
      const output = evaluate(layer);
      assert.equal(output.error, null);
      assert.deepEqual(evaluate(deserialize(serialize(doc)).layers[0]).path, output.path);
      assert.equal(exportSVG(deserialize(serialize(doc))), exportSVG(doc));
    }
    assert.deepEqual(source, before);
    assert.deepEqual(repeat.params.anchor.value, anchor);
    // Copies Above reverses paint order only; odd/even identity stays in IDs.
    assert.deepEqual(
      run({ alternateScale: 60, copiesAbove: false }).contours,
      run({ alternateScale: 60 }).contours.toReversed(),
    );
  });
}

test('fractional offset, affine step, holes and partial-tail opacity keep their existing contracts', () => {
  const { repeat, source } = alternateRepeaterFixture('radial');
  const input = {
    contours: [
      { id: 'outer', group: 'g', closed: true, points: resolvedPoints(source) },
      { id: 'hole', group: 'g', closed: false, points: resolvedPoints(source).toReversed() },
    ],
  };
  for (const offset of [-1.5, 0.5, 2]) {
    const opts = {
      ...settings(repeat),
      count: 2.5,
      offset,
      scale: [110, 90],
      startOpacity: 20,
      endOpacity: 80,
      prefix: 'fraction',
    };
    const baseline = repeatGeometry(input, { ...opts, alternateScale: 100 });
    const scaled = repeatGeometry(input, { ...opts, alternateScale: 50 });
    assert.deepEqual(
      scaled.contours.map((c) => c.repeaterOpacity),
      [0.2, 0.2, 0.5, 0.5, 0.4, 0.4],
    );
    for (const i of [0, 1, 4, 5]) assert.deepEqual(scaled.contours[i], baseline.contours[i]);
    for (const i of [2, 3]) {
      const center = baseline.contours[i].points.reduce(
        (a, p) => a.map((v, axis) => v + p.position[axis] / 4),
        [0, 0],
      );
      scaled.contours[i].points.forEach((p, j) =>
        near(
          p.position,
          baseline.contours[i].points[j].position.map(
            (v, axis) => center[axis] + (v - center[axis]) / 2,
          ),
        ),
      );
    }
    assert.equal(scaled.contours[2].group, scaled.contours[3].group);
    assert.equal(scaled.contours[3].closed, false);
  }
});

test('Session promotion, one edit/Undo/Redo, JSON and locked or invalid literal/expression/wire updates are atomic', () => {
  const { doc, layer, repeat } = alternateRepeaterFixture();
  const driver = node('Value');
  driver.params.value.value = 60;
  layer.nodes.push(driver);
  const session = new CommandSession(doc);
  const before = serialize(session.snapshot()),
    svg = exportSVG(session.snapshot());
  for (const boundary of [0.01, 1000]) {
    send(session, layer, repeat, 'set-parameter', { key: 'alternateScale', value: boundary });
    assert.equal(evaluate(session.snapshot().layers[0]).error, null);
    send(session, layer, repeat, 'undo');
    assert.equal(serialize(session.snapshot()), before);
  }
  send(session, layer, repeat, 'set-parameter', { key: 'alternateScale', value: 60 });
  const saved = serialize(session.snapshot()),
    changedSVG = exportSVG(session.snapshot());
  assert.equal(session.snapshot().schemaVersion, 45);
  assert.notEqual(changedSVG, svg);
  assert.equal(serialize(deserialize(saved)), saved);
  send(session, layer, repeat, 'undo');
  assert.equal(serialize(session.snapshot()), before);
  assert.equal(exportSVG(session.snapshot()), svg);
  assert.equal(session.historyView.past.length, 0);
  send(session, layer, repeat, 'redo');
  assert.equal(serialize(session.snapshot()), saved);
  for (const value of [0, -1, 0.001, 1001, Infinity, NaN]) {
    const state = session.state,
      snapshot = serialize(session.snapshot());
    assert.throws(
      () => send(session, layer, repeat, 'set-parameter', { key: 'alternateScale', value }),
      /finite|Alternate Scale|Scalar/i,
    );
    assert.deepEqual(session.state, state);
    assert.equal(serialize(session.snapshot()), snapshot);
  }
  assert.throws(
    () =>
      send(session, layer, repeat, 'set-parameter-expression', {
        key: 'alternateScale',
        expression: '-10',
      }),
    /Alternate Scale/,
  );
  assert.equal(serialize(session.snapshot()), saved);
  send(session, layer, repeat, 'connect', { sourceId: driver.id, key: 'alternateScale' });
  const wired = serialize(session.snapshot()),
    state = session.state;
  assert.throws(
    () => send(session, layer, driver, 'set-parameter', { key: 'value', value: 0 }),
    /Alternate Scale/,
  );
  assert.equal(serialize(session.snapshot()), wired);
  assert.deepEqual(session.state, state);
  send(session, layer, repeat, 'disconnect', { key: 'alternateScale' });
  send(session, layer, repeat, 'set-parameter-expression', {
    key: 'alternateScale',
    expression: 'value / 2',
  });
  assert.equal(
    evaluate(session.snapshot().layers[0]).parameterValue(repeat.id, 'alternateScale'),
    30,
  );
  const expr = session.snapshot().layers[0].nodes.find((n) => n.id === repeat.id).params
    .alternateScale.expression;
  send(session, layer, repeat, 'set-parameter-base', { key: 'alternateScale', value: 100 });
  assert.equal(
    session.snapshot().layers[0].nodes.find((n) => n.id === repeat.id).params.alternateScale
      .expression,
    expr,
  );
  send(session, layer, repeat, 'set-parameter-lock', { key: 'alternateScale', locked: true });
  const locked = serialize(session.snapshot());
  assert.throws(
    () => send(session, layer, repeat, 'set-parameter', { key: 'alternateScale', value: 70 }),
    /locked/,
  );
  assert.equal(serialize(session.snapshot()), locked);
});

test('schema37 backfills identity without promotion; schema45 requires the control and older retags reject authored variation', () => {
  const { doc, layer, repeat } = alternateRepeaterFixture();
  const svg = exportSVG(doc);
  delete repeat.params.alternateScale;
  const restored = deserialize(serialize(doc));
  assert.equal(restored.schemaVersion, 37);
  assert.equal(exportSVG(restored), svg);
  assert.equal(restored.layers[0].nodes[1].params.alternateScale.value, 100);
  const changed = structuredClone(restored);
  changed.layers[0].nodes[1].params.alternateScale.value = 60;
  assert.throws(() => deserialize(serialize(changed)), /schemaVersion 45/);
  changed.schemaVersion = 45;
  assert.equal(serialize(deserialize(serialize(changed))), serialize(changed));
  delete changed.layers[0].nodes[1].params.alternateScale;
  assert.throws(() => deserialize(serialize(changed)), /parameter|params/i);
  // An old legacy mode keeps its output and version while gaining identity fallback.
  repeat.operation = 'linear';
  doc.schemaVersion = 36;
  const oldSVG = exportSVG(doc),
    old = deserialize(serialize(doc));
  assert.equal(old.schemaVersion, 36);
  assert.equal(exportSVG(old), oldSVG);
  assert.equal(old.layers[0].nodes[1].params.alternateScale.value, 100);
});
