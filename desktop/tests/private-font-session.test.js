import test from 'node:test';
import assert from 'node:assert/strict';
import { CommandSession, document, node, serialize, deserialize } from '../src/core.js';
import { desktopTextIntent } from '../src/desktop-font-text.js';

const identity = { kind: 'file-native-v1', sha256: 'a'.repeat(64), faceIndex: 1 };
const face = {
  family: 'Same family', name: 'Regular', weight: 400,
  style: 0, stretch: 5, format: 1, color: false,
};
function fixture() {
  const doc = document(), layer = doc.layers[0], n = node('Text');
  n.text = 'ABC';
  n.params.size.value = 100;
  layer.nodes.push(n);
  const owner = new CommandSession(doc);
  const send = (type, payload = {}) => owner.dispatch({
    ...owner.state, type, payload: { layerId: layer.id, nodeId: n.id, ...payload },
  });
  const raw = {
    engine: 'directwrite-v1', ...desktopTextIntent(n, face, { ligatures: false }),
    points: [[0, 0], [20, 0], [20, 10], [0, 10]], types: [0, 1, 1, 129],
    ascent: 80, advanceWidth: 20, lineCount: 1, glyphCount: 3,
  };
  return { owner, send, raw, live: () => owner.snapshot().layers[0].nodes.find(x => x.id === n.id) };
}

test('private native adoption saves only exact authored identity and is one undoable command', () => {
  const f = fixture(), before = serialize(f.owner.snapshot()), count = f.owner.historyView.past.length;
  f.send('set-os-font-text', { source: identity, outline: f.raw });
  assert.equal(f.owner.historyView.past.length, count + 1);
  assert.equal(f.owner.snapshot().schemaVersion, 35);
  assert.deepEqual(f.live().osFont.source, identity);
  const after = serialize(f.owner.snapshot());
  assert.equal(serialize(deserialize(after)), after);
  assert.ok(!after.includes('"token"'));
  assert.ok(!after.includes('"sha256":"C:'));
  f.send('undo');
  assert.equal(serialize(f.owner.snapshot()), before);
  f.send('redo');
  assert.equal(serialize(f.owner.snapshot()), after);
});

test('missing or malformed source admission never drops the retained private identity', () => {
  const f = fixture();
  f.send('set-os-font-text', { source: identity, outline: f.raw });
  const before = serialize(f.owner.snapshot()), revision = f.owner.state.expectedRevision;
  for (const source of [undefined, { ...identity, path: 'C:/font.otf' }, { ...identity, token: 'secret' },
    { ...identity, kind: 'unknown-source' }, { ...identity, faceIndex: 256 }]) {
    assert.throws(() => f.send('set-os-font-text', { source, outline: f.raw }), /source/i);
    assert.equal(serialize(f.owner.snapshot()), before);
    assert.equal(f.owner.state.expectedRevision, revision);
  }
  const older = JSON.parse(before);
  older.schemaVersion = 34;
  assert.throws(() => deserialize(JSON.stringify(older)), /schema 35/);
});

test('an explicit installed choice can replace a private source, even with the same face label', () => {
  const f = fixture();
  f.send('set-os-font-text', { source: identity, outline: f.raw });
  const retained = serialize(f.owner.snapshot());
  f.send('set-os-font-text', { source: null, outline: f.raw });
  assert.ok(!Object.hasOwn(f.live().osFont, 'source'));
  f.send('undo');
  assert.equal(serialize(f.owner.snapshot()), retained);
});
