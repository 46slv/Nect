import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { CommandSession, document, node, serialize, characterIdsInRange } from '../src/core.js';
import { parseFont } from '../src/fonts.js';

function fixture() {
  const doc = document(),
    layer = doc.layers[0],
    text = node('Text');
  text.text = 'ABA';
  layer.nodes.push(text);
  const fontData = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
  const font = parseFont(
    fontData.buffer.slice(fontData.byteOffset, fontData.byteOffset + fontData.byteLength),
  );
  const session = new CommandSession(doc, { fonts: () => ({ default: font }) });
  const send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  const target = { kind: 'text-font', layerId: layer.id, nodeId: text.id };
  const bytes = (name) => {
    const data = fs.readFileSync(new URL(name, import.meta.url));
    return data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength);
  };
  return { session, send, target, text, bytes };
}

test('font browsing owns asset IDs privately; commit is the last preview and exactly one Undo', () => {
  const { session: s, send, target, text, bytes } = fixture(),
    before = serialize(s.snapshot());
  let token = send('begin-gesture', { target });
  token = send('update-gesture', {
    ...token,
    update: { bytes: bytes('./fixtures/fonts/NectJPFixture-Regular.otf'), fileName: 'Japanese' },
  });
  const oldToken = token;
  token = send('update-gesture', {
    ...token,
    update: { bytes: bytes('../public/fonts/DejaVuSans.ttf'), fileName: 'Latin' },
  });
  assert.equal(serialize(s.snapshot()), before);
  assert.equal(s.historyView.past.length, 0);
  assert.throws(
    () => send('update-gesture', { ...oldToken, update: { font: 'default' } }),
    /STALE_GESTURE/,
  );
  assert.throws(() => send('undo'), /GESTURE_ACTIVE/);
  const preview = s.previewSnapshot(token.gestureId).document;
  assert.equal(Object.keys(preview.fontAssets).length, 1);
  const previewId = preview.layers[0].nodes.find((n) => n.id === text.id).font;
  send('commit-gesture', token);
  assert.equal(serialize(s.snapshot()), serialize(preview));
  assert.equal(s.snapshot().layers[0].nodes.find((n) => n.id === text.id).font, previewId);
  assert.equal(s.historyView.past.length, 1);
  send('undo');
  assert.equal(serialize(s.snapshot()), before);
  send('redo');
  assert.equal(serialize(s.snapshot()), serialize(preview));
});

test('font cancel/no-op preserve redo; invalid fonts and character IDs cannot partially commit', () => {
  const { session: s, send, target, bytes } = fixture();
  send('rename', { layerId: target.layerId, name: 'Next' });
  send('undo');
  const before = serialize(s.snapshot()),
    revision = s.state.expectedRevision;
  let token = send('begin-gesture', { target });
  token = send('update-gesture', {
    ...token,
    update: { bytes: bytes('./fixtures/fonts/NectJPFixture-Regular.otf'), fileName: 'Japanese' },
  });
  assert.throws(() =>
    send('update-gesture', { ...token, update: { bytes: new ArrayBuffer(5), fileName: 'bad' } }),
  );
  send('cancel-gesture', token);
  assert.equal(serialize(s.snapshot()), before);
  assert.equal(s.historyView.future.length, 1);
  token = send('begin-gesture', { target });
  token = send('update-gesture', { ...token, update: { font: 'default' } });
  send('commit-gesture', token);
  assert.equal(s.state.expectedRevision, revision);
  assert.equal(s.historyView.future.length, 1);
  assert.throws(() => send('begin-gesture', { target: { ...target, characterIds: ['foreign'] } }));
  token = send('begin-gesture', { target: { ...target, characterIds: ['c1'] } });
  token = send('update-gesture', {
    ...token,
    update: { bytes: bytes('./fixtures/fonts/NectJPFixture-Regular.otf'), fileName: 'Japanese' },
  });
  const preview = s.previewSnapshot(token.gestureId).document;
  const text = preview.layers[0].nodes.find((n) => n.id === target.nodeId);
  assert.equal(text.font, 'default');
  assert.ok(preview.fontAssets[text.characterPositions.styles.c1.font]);
  assert.equal(text.characterPositions.styles.c2, undefined);
  send('cancel-gesture', token);
  assert.equal(serialize(s.snapshot()), before);
});

test('font picker can open on a missing-glyph state without accepting an unavailable preview', () => {
  const { session: s, send, target } = fixture();
  send('set-text-content', { layerId: target.layerId, nodeId: target.nodeId, text: '漢' });
  const before = serialize(s.snapshot());
  const text = s.snapshot().layers[0].nodes.find((node) => node.id === target.nodeId);
  for (const characterIds of [
    undefined,
    characterIdsInRange(text.text, text.characterPositions, 0, text.text.length),
  ]) {
    const token = send('begin-gesture', {
      target: { ...target, ...(characterIds ? { characterIds } : {}) },
    });
    assert.throws(
      () => send('update-gesture', { ...token, update: { font: 'default' } }),
      /glyph|font/i,
    );
    send('cancel-gesture', token);
    assert.equal(serialize(s.snapshot()), before);
  }
});
