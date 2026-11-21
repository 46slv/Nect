import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { parseFont } from '../src/fonts.js';
import test from 'node:test';
import { deserialize, exportSVG } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  deserializeCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createLiveTextContent } from '../src/live-text-content.js';
import { createLiveEditorProtocol } from '../src/live-editor-protocol.js';

test('live text writes the exact addressed node and shares the existing undo history', async () => {
  const raw = readFileSync(new URL('../examples/blend-distance.json', import.meta.url), 'utf8');
  const owner = createCompositionSessionOwner(deserializeCompositionProject(raw, deserialize));
  const { layerId, nodeId } = owner.dispatch({
    ...owner.state,
    type: 'create-artwork',
    payload: { artworkType: 'Text', at: [250, 300] },
  });
  const target = () => ({
    documentGeneration: 1,
    compositionId: owner.activeCompositionId,
    sessionId: owner.state.sessionId,
    sessionRevision: owner.state.expectedRevision,
    projectRevision: owner.projectRevision,
  });
  const snapshot = () => serializeCompositionProject(owner.projectSnapshot());
  const readText = (l, n) =>
    owner
      .snapshot()
      .layers.find(({ id }) => id === l)
      ?.nodes.find(({ id, type }) => id === n && type === 'Text')?.text;
  let blocked = false;
  let changeWhilePreparing = false;
  const setText = createLiveTextContent({
    readTarget: target,
    isEditBlocked: () => blocked,
    readText,
    async commit(command, isCurrent) {
      await Promise.resolve();
      if (changeWhilePreparing) blocked = true;
      if (!isCurrent()) return { status: 'edit-in-progress' };
      owner.dispatch(command);
      return { status: 'changed' };
    },
  });
  const initial = snapshot();
  const edit = { target: target(), layerId, nodeId, text: 'Nect \u30c6\u30b9\u30c8' };
  assert.equal((await setText({ ...edit, nodeId: 'missing' })).status, 'unsupported-target');
  assert.equal((await setText({ ...edit, text: 'x'.repeat(1025) })).status, 'invalid-arguments');
  assert.equal((await setText({ ...edit, text: readText(layerId, nodeId) })).status, 'unchanged');
  changeWhilePreparing = true;
  assert.equal((await setText(edit)).status, 'edit-in-progress');
  assert.equal(snapshot(), initial);
  changeWhilePreparing = blocked = false;
  const packet = { version: 1, operation: 'text.setContent', arguments: edit };
  const handle = createLiveEditorProtocol({ readTarget: target, setTextContent: setText });
  assert.equal(
    createLiveEditorProtocol({ readTarget: target })(packet).status,
    'unsupported-operation',
  );
  assert.equal(
    handle({ ...packet, arguments: { ...edit, path: 'not allowed' } }).status,
    'invalid-arguments',
  );
  assert.equal(
    handle({ ...packet, arguments: { ...edit, text: 'x'.repeat(1025) } }).status,
    'invalid-arguments',
  );
  assert.deepEqual(await handle(packet), { status: 'changed', target: target() });
  assert.equal(readText(layerId, nodeId), edit.text);
  assert.equal((await setText(edit)).status, 'stale-target');
  const changed = snapshot();
  owner.dispatch({ ...owner.state, type: 'undo', payload: {} });
  assert.equal(snapshot(), initial);
  owner.dispatch({ ...owner.state, type: 'redo', payload: {} });
  assert.equal(snapshot(), changed);
});

test('Japanese static fixture writes and reopens as text with equal GUI and live SVG geometry', async () => {
  const fixtureBytes = (url) => {
    const b = readFileSync(new URL(url, import.meta.url));
    return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
  };
  const bytes = fixtureBytes('./fixtures/fonts/NectJPFixture-Regular.otf');
  const selectedFont = parseFont(bytes);
  const value = '\u65e5\u672c\u8a9e A1\uff0812\uff09';
  for (const ch of value) assert.notEqual(selectedFont.charToGlyphIndex(ch), 0, ch);
  const create = (raw) =>
    createCompositionSessionOwner(deserializeCompositionProject(raw, deserialize));
  const doc = create(
    readFileSync(new URL('../examples/blend-distance.json', import.meta.url), 'utf8'),
  );
  const send = (owner, type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });
  const { layerId, nodeId } = send(doc, 'create-artwork', { artworkType: 'Text', at: [550, 200] });
  let fontGesture = send(doc, 'begin-gesture', { target: { kind: 'text-font', layerId, nodeId } });
  fontGesture = send(doc, 'update-gesture', {
    ...fontGesture,
    update: { bytes, fileName: 'NectJPFixture-Regular.otf' },
  });
  send(doc, 'commit-gesture', fontGesture);
  const serialized = (owner) => serializeCompositionProject(owner.projectSnapshot());
  const seed = serialized(doc);
  const gui = create(seed),
    live = create(seed);
  const getNode = (owner, l, n) =>
    owner
      .snapshot()
      .layers.find(({ id }) => id === l)
      ?.nodes.find(({ id, type }) => id === n && type === 'Text');
  const fontId = getNode(live, layerId, nodeId).font;
  assert.notEqual(fontId, 'default');
  const target = () => ({
    documentGeneration: 1,
    compositionId: live.activeCompositionId,
    sessionId: live.state.sessionId,
    sessionRevision: live.state.expectedRevision,
    projectRevision: live.projectRevision,
  });
  const write = createLiveTextContent({
    readTarget: target,
    isEditBlocked: () => false,
    readText: (l, n) => getNode(live, l, n)?.text,
    commit(command, isCurrent) {
      assert.ok(isCurrent());
      live.dispatch(command);
      return { status: 'changed' };
    },
  });
  send(gui, 'set-text-content', { layerId, nodeId, text: value });
  assert.equal((await write({ target: target(), layerId, nodeId, text: value })).status, 'changed');
  const output = serialized(live);
  assert.equal(output, serialized(gui));
  send(live, 'undo');
  assert.equal(serialized(live), seed);
  send(live, 'redo');
  assert.equal(serialized(live), output);
  const reopened = create(output);
  assert.equal(getNode(reopened, layerId, nodeId).text, value);
  assert.equal(getNode(reopened, layerId, nodeId).font, fontId);
  const fonts = { [fontId]: selectedFont };
  assert.equal(exportSVG(reopened.snapshot(), fonts), exportSVG(gui.snapshot(), fonts));
  assert.notEqual(exportSVG(reopened.snapshot(), fonts), exportSVG(doc.snapshot(), fonts));
});
