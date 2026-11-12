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

const hash = (value) => createHash('sha256').update(JSON.stringify(value)).digest('hex');
const cases = [
  [
    'Circle',
    'circle',
    {},
    undefined,
    'e83a375529374c5999a2cdba8be6f9199099c6b13d1abc34652eb61c9e24421c',
  ],
  [
    'Circle',
    'circle',
    { center: [90, 110], radius: -20 },
    undefined,
    'ee7b285b6b903c1045329121397f8ac3f8b0584d1b893ec3536e82dca6cdd101',
  ],
  [
    'Circle',
    'circle',
    { center: [90, 110], radius: 40 },
    'left-top',
    '4b483f0d85834076225ff2cf8006494cb7fc9fb9948b3e8ecf35c4ae861c030d',
  ],
  [
    'Ellipse',
    'ellipse',
    {},
    undefined,
    '1ba0f631b00f3fed01e546a5bbff5c37322d386ecc43de1d21b417c317efb421',
  ],
  [
    'Ellipse',
    'ellipse',
    { center: [110, 90], size: [-80, 40] },
    'right-bottom',
    '712dc962f621a8e14b6008be4950e535446bc2b08ff4d1b43b576b92c6e7278c',
  ],
  [
    'Ellipse',
    'ellipse',
    { size: [0, 0] },
    undefined,
    'f16c5f5b8c93d7f5595acc699c39b05a818dd4c403a289c3f835a3dda413c155',
  ],
  [
    'Rectangle',
    'rectangle',
    {},
    undefined,
    '57086a3343e6ffa68a0786f105eac1d218cda68c15b127d5691f1834b5bfedd3',
  ],
  [
    'Rectangle',
    'rectangle',
    { center: [80, 70], size: [-100, 0] },
    'right-bottom',
    'e443227fde72766bec02aecec1e5fe78f798f02ee6a5fe13a235589df68f1f14',
  ],
  [
    'Rectangle',
    'rectangle',
    { size: [0, 0] },
    undefined,
    '416a3ffa45c4fa89d5f83b9141cc80e0f3475d921c8f65c3c7b87f589c2a5836',
  ],
];

for (const [type, id, values, reference, expected] of cases)
  test(`${type} keeps current geometry and point IDs for ${JSON.stringify(values)} / ${reference ?? 'origin'}`, () => {
    const doc = blankDocument(),
      layer = doc.layers[0],
      shape = node(type);
    shape.id = id;
    for (const [key, value] of Object.entries(values)) shape.params[key].value = value;
    if (reference) shape.positionReference = reference;
    layer.nodes.push(shape);
    const saved = serialize(doc),
      result = evaluate(layer).run(id);
    assert.equal(hash(result), expected);
    assert.equal(result.contours[0].id, `${id}:c0`);
    assert.ok(result.contours[0].points.every((point, i) => point.id === `${id}:c0:p${i}`));
    assert.equal(serialize(deserialize(saved)), saved);
    assert.deepEqual(evaluate(deserialize(saved).layers[0]).run(id), result);
  });

for (const type of ['Circle', 'Ellipse', 'Rectangle'])
  test(`${type} Session preserves valid edits, rejection and Undo/Redo`, () => {
    const doc = blankDocument(),
      layer = doc.layers[0],
      shape = node(type),
      key = type === 'Circle' ? 'radius' : 'size',
      editedValue = type === 'Circle' ? 80 : [-80, 40],
      invalidValue = type === 'Circle' ? 'invalid' : [1, Infinity];
    layer.nodes.push(shape);
    const session = new CommandSession(doc),
      send = (type, payload = {}) =>
        session.dispatch({
          ...session.state,
          type,
          payload: { layerId: layer.id, nodeId: shape.id, ...payload },
        }),
      before = serialize(session.snapshot());
    send('set-parameter', { key, value: editedValue });
    const edited = serialize(session.snapshot()),
      expected = evaluate(session.snapshot().layers[0]).run(shape.id);
    send('undo');
    assert.equal(serialize(session.snapshot()), before);
    const revision = session.state.expectedRevision,
      history = JSON.stringify(session.historyView);
    assert.throws(() => send('set-parameter', { key, value: invalidValue }), /Expected/);
    assert.equal(serialize(session.snapshot()), before);
    assert.equal(session.state.expectedRevision, revision);
    assert.equal(JSON.stringify(session.historyView), history);
    send('redo');
    assert.equal(serialize(session.snapshot()), edited);
    assert.deepEqual(evaluate(session.snapshot().layers[0]).run(shape.id), expected);
    assert.equal(serialize(deserialize(edited)), edited);
  });
