import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { createHash } from 'node:crypto';
import {
  blankDocument,
  CommandSession,
  connect,
  deserialize,
  evaluate,
  node,
  serialize,
} from '../src/core.js';
import { parseFont } from '../src/fonts.js';
import { evaluateSubdividePath } from '../src/subdivide-path-binding.js';

const hash = (value) => createHash('sha256').update(JSON.stringify(value)).digest('hex');
const cases = [
  ['M0 0 L50 0 L50 50', 1, 'd3345b971d8d7924913aca78f131819afee423949b13d9a02a5610defdf147ce'],
  ['M0 0 L50 0 L50 50', 3, '9994393ca63b11e715bfe6bd8ccea546096c4d8524d41c2f812d81f03b746a15'],
  ['M0 0 C20 0 20 50 60 50', 2, 'cc6e6036d6740a1f4eb9730d9275bffc1e18649bcd021c74db78b0909d620efd'],
  [
    'M0 0 L100 0 L100 100 L0 100 Z',
    2,
    'd41e40fb51889db9a36f09c39aa3a821ebf3adf177ad7a34e69dc65d6b9fb196',
  ],
];

for (const [raw, segments, expected] of cases)
  test(`Subdivide preserves ${segments} segments for ${raw}`, () => {
    const doc = blankDocument(),
      layer = doc.layers[0],
      source = node('Imported Path'),
      effect = node('Subdivide Path');
    source.id = 'source';
    source.rawPath = raw;
    effect.id = 'subdivide';
    effect.params.segments.value = segments;
    layer.nodes.push(source, effect);
    connect(layer, source.id, effect.id, 'path');
    const engine = evaluate(layer),
      input = engine.run(source.id),
      before = JSON.stringify(input),
      reads = [],
      bound = evaluateSubdividePath(
        undefined,
        (key) => {
          reads.push(key);
          return key === 'path' ? input : segments;
        },
        { nodeId: effect.id },
      );
    assert.deepEqual(reads, ['path', 'segments']);
    assert.equal(hash(bound), expected);
    assert.deepEqual(engine.run(effect.id), bound);
    assert.equal(JSON.stringify(input), before);
    assert.notEqual(bound.contours[0].points[0].id, input.contours[0].points[0].id);
    const saved = serialize(doc);
    assert.deepEqual(evaluate(deserialize(saved).layers[0]).run(effect.id), bound);
    assert.equal(serialize(doc), saved);
  });

test('Subdivide Session rejects invalid values and preserves bypass and history', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    source = node('Imported Path'),
    effect = node('Subdivide Path');
  source.rawPath = 'M0 0 C20 0 20 50 60 50';
  layer.nodes.push(source, effect);
  connect(layer, source.id, effect.id, 'path');
  const session = new CommandSession(doc),
    send = (type, payload = {}) =>
      session.dispatch({
        ...session.state,
        type,
        payload: { layerId: layer.id, nodeId: effect.id, ...payload },
      }),
    before = serialize(session.snapshot());
  send('set-parameter', { key: 'segments', value: 3 });
  const edited = serialize(session.snapshot()),
    expected = evaluate(session.snapshot().layers[0]).run(effect.id);
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  const revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  assert.throws(() => send('set-parameter', { key: 'segments', value: 0 }), /integer from 1 to 32/);
  assert.equal(serialize(session.snapshot()), before);
  assert.equal(session.state.expectedRevision, revision);
  assert.equal(JSON.stringify(session.historyView), history);
  send('redo');
  assert.equal(serialize(session.snapshot()), edited);
  assert.deepEqual(evaluate(session.snapshot().layers[0]).run(effect.id), expected);
  send('set-node-bypassed', { bypassed: true });
  const bypassed = serialize(session.snapshot()),
    evaluation = evaluate(session.snapshot().layers[0]);
  assert.deepEqual(evaluation.run(effect.id), evaluation.run(source.id));
  send('undo');
  assert.equal(serialize(session.snapshot()), edited);
  send('redo');
  assert.equal(serialize(session.snapshot()), bypassed);
  assert.equal(serialize(deserialize(bypassed)), bypassed);
});

test('actual Text keeps current routed occurrence and generated point identities', () => {
  const bytes = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url)),
    fonts = {
      default: parseFont(bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength)),
    },
    doc = blankDocument(),
    layer = doc.layers[0],
    text = node('Text'),
    effect = node('Subdivide Path');
  text.id = 'text-source';
  text.text = 'I';
  effect.id = 'subdivide-text';
  effect.params.segments.value = 2;
  layer.nodes.push(text, effect);
  connect(layer, text.id, effect.id, 'path');
  const saved = serialize(doc),
    result = evaluate(layer, fonts).run(effect.id);
  assert.equal(hash(result), '68425cc700ecaad2ca1ca82d07970bfcf284fa8ce9b533b500ea0be5b2ada2df');
  assert.equal(result.characterOccurrences[0].source, text.id);
  assert.equal(result.characterOccurrences[0].route, `${text.id}/${effect.id}`);
  assert.match(result.characterOccurrences[0].reason, /unavailable through Subdivide Path/);
  assert.deepEqual(evaluate(deserialize(saved).layers[0], fonts).run(effect.id), result);
});
