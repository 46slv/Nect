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
import { evaluateTrimPath } from '../src/trim-path-binding.js';
import { evaluateSetPathStart } from '../src/set-path-start-binding.js';
import { evaluateDashPath } from '../src/dash-path-binding.js';
import {
  activeCompositionDocument,
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

const hash = (value) => createHash('sha256').update(JSON.stringify(value)).digest('hex');
const bindings = {
  'Trim Path': evaluateTrimPath,
  'Set Path Start': evaluateSetPathStart,
  'Dash Path': evaluateDashPath,
};
const cases = [
  [
    'Trim Path',
    'trim',
    'M0 0 L50 0 L50 50',
    { start: 0.2, end: 0.8 },
    '16f1330a1dfe3bce118699ed64fb330dfaf314fa2c133c37f18d184f6d8176ea',
  ],
  [
    'Trim Path',
    'trim',
    'M0 0 C20 0 20 50 60 50',
    { start: 0.15, end: 0.85 },
    '01a070b19bf22d516f2ce4b57b378173f4203d7aba8820b359c99e21f25a3bfc',
  ],
  [
    'Trim Path',
    'trim',
    'M0 0 L100 0 L100 100 L0 100 Z',
    {},
    '596e4f1d9acb088f1488acf78db8f27b5ab40c3ce11740e9ec0954083c0c4f5c',
  ],
  [
    'Set Path Start',
    'start',
    'M0 0 L100 0 L100 100 L0 100 Z',
    { start: 0.25 },
    '6ae02b766b76cf4b6c146114761bddc29d2ea28dab547ce4481c4a631fe1b5c8',
  ],
  [
    'Set Path Start',
    'start',
    'M0 0 C20 0 20 50 60 50',
    { start: 0.5 },
    '74782bb08cb33c2efa06e494bd1560ac9cd576e6f0b86b90a1e699477ef1b241',
  ],
  [
    'Dash Path',
    'dash',
    'M0 0 L100 0',
    { dashLength: 12, gapLength: 6, offset: 0 },
    '93b20853cde1016720f8dd8cd9d2630e21a2c00d3cd625ca95a8752b1ad1177c',
  ],
  [
    'Dash Path',
    'dash',
    'M0 0 L100 0 L100 100 L0 100 Z',
    { dashLength: 30, gapLength: 10, offset: -7 },
    '5f35d6bb40653fddca0bbf1edd65dd65c3a7f61e842632dfe7685c3d42031ace',
  ],
  [
    'Dash Path',
    'dash',
    'M0 0 C20 0 20 50 60 50',
    { gapLength: 0 },
    'c85b55fade9ad6c9b27ceb6d0e05fa6bef10af21d4570626c3891f2807433dab',
  ],
];

for (const [type, id, raw, values, expected] of cases)
  test(`${type} preserves geometry and IDs for ${raw} / ${JSON.stringify(values)}`, () => {
    const doc = blankDocument(),
      layer = doc.layers[0],
      source = node('Imported Path'),
      effect = node(type);
    source.id = 'source';
    source.rawPath = raw;
    effect.id = id;
    for (const [key, value] of Object.entries(values)) effect.params[key].value = value;
    layer.nodes.push(source, effect);
    connect(layer, source.id, effect.id, 'path');
    const engine = evaluate(layer),
      input = engine.run(source.id),
      before = JSON.stringify(input),
      reads = [],
      bound = bindings[type](
        undefined,
        (key) => {
          reads.push(key);
          return key === 'path' ? input : effect.params[key].value;
        },
        { nodeId: effect.id },
      );
    assert.deepEqual(
      reads,
      type === 'Trim Path'
        ? ['path', 'start', 'end']
        : type === 'Set Path Start'
          ? ['path', 'start']
          : ['path', 'dashLength', 'gapLength', 'offset'],
    );
    assert.equal(hash(bound), expected);
    assert.deepEqual(engine.run(effect.id), bound);
    assert.equal(JSON.stringify(input), before);
    if (type === 'Dash Path' && values.gapLength === 0)
      assert.equal(bound.contours[0].points[0].id, input.contours[0].points[0].id);
    const saved = serialize(doc);
    assert.deepEqual(evaluate(deserialize(saved).layers[0]).run(effect.id), bound);
    assert.equal(serialize(doc), saved);
  });

for (const [type, key, value, invalid] of [
  ['Trim Path', 'start', 0.2, -1],
  ['Set Path Start', 'start', 0.25, -1],
  ['Dash Path', 'dashLength', 20, 0],
])
  test(`${type} Session preserves failure atomicity, bypass, history and reload`, () => {
    const doc = blankDocument(),
      layer = doc.layers[0],
      source = node('Imported Path'),
      effect = node(type);
    source.rawPath = 'M0 0 L100 0 L100 100 L0 100 Z';
    layer.nodes.push(source, effect);
    connect(layer, source.id, effect.id, 'path');
    const session = new CommandSession(doc),
      send = (command, payload = {}) =>
        session.dispatch({
          ...session.state,
          type: command,
          payload: { layerId: layer.id, nodeId: effect.id, ...payload },
        }),
      before = serialize(session.snapshot());
    send('set-parameter', { key, value });
    const edited = serialize(session.snapshot()),
      expected = evaluate(session.snapshot().layers[0]).run(effect.id);
    send('undo');
    assert.equal(serialize(session.snapshot()), before);
    const revision = session.state.expectedRevision,
      history = JSON.stringify(session.historyView);
    assert.throws(() => send('set-parameter', { key, value: invalid }));
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

for (const [type, id, expected] of [
  ['Trim Path', 'trim', 'e9ad4f1fa05e301ae4f167ad5a13cf6aafbb04f20f086ede73a5a38e34da3c0c'],
  ['Set Path Start', 'start', '5042dda8d5be534e2de03683d96b859158db39e6282707f464b6f110a06c12a6'],
  ['Dash Path', 'dash', '708e97a024fc261de75b212623242b37d22af27d9bc555e1d3ce20878d1df2aa'],
])
  test(`${type} preserves actual Text routing and generated identities`, () => {
    const bytes = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url)),
      fonts = {
        default: parseFont(
          bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength),
        ),
      },
      doc = blankDocument(),
      layer = doc.layers[0],
      text = node('Text'),
      effect = node(type);
    text.id = 'text-source';
    text.text = 'I';
    effect.id = id;
    layer.nodes.push(text, effect);
    connect(layer, text.id, effect.id, 'path');
    const saved = serialize(doc),
      result = evaluate(layer, fonts).run(effect.id);
    assert.equal(hash(result), expected);
    assert.equal(result.characterOccurrences[0].source, text.id);
    assert.equal(result.characterOccurrences[0].route, `${text.id}/${effect.id}`);
    assert.match(result.characterOccurrences[0].reason, /unavailable through/);
    assert.deepEqual(evaluate(deserialize(saved).layers[0], fonts).run(effect.id), result);
  });

test('all three path edits keep their authored connections and results in a Composition project', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    source = node('Imported Path');
  source.id = 'source';
  source.rawPath = 'M0 0 L100 0 L100 100 L0 100 Z';
  layer.nodes.push(source);
  let previous = source;
  for (const [type, id] of [
    ['Trim Path', 'trim'],
    ['Set Path Start', 'start'],
    ['Dash Path', 'dash'],
  ]) {
    const effect = node(type);
    effect.id = id;
    layer.nodes.push(effect);
    connect(layer, previous.id, effect.id, 'path');
    previous = effect;
  }
  const expected = evaluate(layer).run(previous.id),
    project = migrateLegacyDocumentToCompositionProject(doc),
    owner = createCompositionSessionOwner(project),
    saved = serializeCompositionProject(owner.projectSnapshot()),
    reopened = deserializeCompositionProject(saved),
    restored = activeCompositionDocument(reopened, reopened.compositions[0].id);
  assert.equal(serializeCompositionProject(reopened), saved);
  assert.deepEqual(evaluate(restored.layers[0]).run(previous.id), expected);
  assert.equal(
    restored.layers[0].nodes.find((item) => item.id === 'trim').params.path.source,
    'source',
  );
  assert.equal(
    restored.layers[0].nodes.find((item) => item.id === 'start').params.path.source,
    'trim',
  );
  assert.equal(
    restored.layers[0].nodes.find((item) => item.id === 'dash').params.path.source,
    'start',
  );
  assert.doesNotMatch(saved, /implementationId|capabilities|legacyDefault/);
});
