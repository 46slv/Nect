import test from 'node:test';
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import {
  blankDocument,
  CommandSession,
  deserialize,
  evaluate,
  node,
  serialize,
} from '../src/core.js';
import { radialPath, radialSettings } from '../src/radial-geometry.js';

const hash = (value) => createHash('sha256').update(JSON.stringify(value)).digest('hex');
const cases = [
  {
    type: 'Polygon',
    id: 'polygon',
    key: 'sides',
    defaultHash: 'f28d4b67ad1ec42288b74111031d96adc0aef8876866188057b5127bb2e6b3cb',
    custom: { center: [120, 90], sides: 7, radius: 50, rotation: 37 },
    reference: 'left-top',
    referenceHash: 'd1e849a9f2bf7d4a33260a07136d18c877640e6b483f1d47db34aa686da00d34',
  },
  {
    type: 'Star',
    id: 'star',
    key: 'points',
    defaultHash: '8d0409ce5329499b15f27fe9b5995d112daf13857360f49ea629cd5294316d3a',
    custom: { center: [180, 140], points: 6, outerRadius: 80, innerRadius: 35, rotation: -28 },
    reference: 'right-bottom',
    referenceHash: '1063ae40090dfbf90ac5e0469fbe84101a23f7f8d54b96e6a09f91eb97e74609',
  },
];

for (const example of cases) {
  test(`${example.type} keeps geometry, stable IDs, position reference and saved data`, () => {
    const doc = blankDocument(),
      layer = doc.layers[0],
      shape = node(example.type);
    shape.id = example.id;
    layer.nodes.push(shape);
    const read = (key) => shape.params[key].value,
      initial = evaluate(layer).run(shape.id);
    assert.equal(hash(initial), example.defaultHash);
    assert.deepEqual(radialPath(example.type, shape.id, read), initial);
    const count = shape.params[example.key].value;
    assert.equal(initial.contours[0].id, `${shape.id}:${example.type}:${count}`);
    assert.ok(
      initial.contours[0].points.every(
        (point, i) => point.id === `${shape.id}:${example.type}:${count}:${i}`,
      ),
    );
    for (const [key, value] of Object.entries(example.custom)) shape.params[key].value = value;
    shape.positionReference = example.reference;
    const saved = serialize(doc),
      result = evaluate(layer).run(shape.id);
    assert.equal(hash(result), example.referenceHash);
    assert.equal(serialize(deserialize(saved)), saved);
    assert.deepEqual(evaluate(deserialize(saved).layers[0]).run(shape.id), result);
  });

  test(`${example.type} Session refuses invalid controls without losing history`, () => {
    const doc = blankDocument(),
      layer = doc.layers[0],
      shape = node(example.type);
    layer.nodes.push(shape);
    const session = new CommandSession(doc),
      send = (type, payload = {}) =>
        session.dispatch({
          ...session.state,
          type,
          payload: { layerId: layer.id, nodeId: shape.id, ...payload },
        }),
      before = serialize(session.snapshot());
    send('set-parameter', { key: example.key, value: 7 });
    const edited = serialize(session.snapshot()),
      expected = evaluate(session.snapshot().layers[0]).run(shape.id);
    send('undo');
    assert.equal(serialize(session.snapshot()), before);
    const revision = session.state.expectedRevision,
      history = JSON.stringify(session.historyView);
    assert.throws(
      () => send('set-parameter', { key: example.key, value: 2 }),
      /integer from 3 to 256/,
    );
    assert.equal(serialize(session.snapshot()), before);
    assert.equal(session.state.expectedRevision, revision);
    assert.equal(JSON.stringify(session.historyView), history);
    send('redo');
    assert.equal(serialize(session.snapshot()), edited);
    assert.deepEqual(evaluate(session.snapshot().layers[0]).run(shape.id), expected);
    assert.equal(serialize(deserialize(edited)), edited);
  });
}

test('radial settings preserve their finite, range and radius validation', () => {
  const values = { sides: 6, radius: 20, points: 5, outerRadius: 20, innerRadius: 10 };
  assert.deepEqual(
    radialSettings('Polygon', (key) => values[key]),
    { count: 6, outer: 20, inner: 20 },
  );
  assert.deepEqual(
    radialSettings('Star', (key) => values[key]),
    { count: 5, outer: 20, inner: 10 },
  );
  assert.throws(
    () => radialSettings('Polygon', (key) => (key === 'sides' ? 2.5 : values[key])),
    /integer from 3 to 256/,
  );
  assert.throws(
    () => radialSettings('Star', (key) => (key === 'innerRadius' ? 21 : values[key])),
    /innerRadius must not exceed outerRadius/,
  );
});
