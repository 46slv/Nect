import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  blankLayer,
  node,
  connect,
  serialize,
  CommandSession,
} from '../src/core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

function fixture(options = {}) {
  const doc = blankDocument(),
    source = doc.layers[0],
    target = blankLayer('Paste target'),
    value = node('Value', 40, 70),
    math = node('Math', 340, 110);
  value.params.value.value = 1;
  connect({ nodes: [value, math] }, value.id, math.id, 'a');
  source.nodes.push(value, math);
  doc.layers.push(target);
  const project = migrateLegacyDocumentToCompositionProject(doc),
    session = createCompositionSessionOwner(project, options),
    send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  const edit = (next) =>
    send('set-parameter', { layerId: source.id, nodeId: value.id, key: 'value', value: next });
  const copy = (nodeIds = [value.id, math.id]) =>
    send('copy-nodes', { layerId: source.id, nodeIds });
  const paste = (layerId = target.id) => send('paste-nodes', { layerId });
  return { project, source, target, value, math, session, send, edit, copy, paste };
}

for (const redo of [false, true]) {
  test(`project Copy → Undo${redo ? ' → Redo' : ''} → Paste keeps its captured graph`, () => {
    const f = fixture();
    f.edit(7);
    const state = f.session.state,
      history = f.session.historyView.past.length,
      before = serialize(f.session.snapshot());
    f.copy();
    assert.deepEqual(f.session.state, state);
    assert.equal(f.session.historyView.past.length, history);
    assert.equal(serialize(f.session.snapshot()), before);
    f.send('undo');
    if (redo) f.send('redo');
    const beforePaste = serialize(f.session.snapshot()),
      past = f.session.historyView.past.length,
      ids = f.paste().nodeIds,
      target = f.session.snapshot().layers.find((layer) => layer.id === f.target.id),
      copies = ids.map((id) => target.nodes.find((item) => item.id === id));
    assert.equal(copies[0].params.value.value, 7);
    assert.equal(copies[1].params.a.source, copies[0].id);
    assert.ok(ids.every((id) => ![f.value.id, f.math.id].includes(id)));
    assert.deepEqual(
      copies.map((item) => item.position),
      [
        [70, 100],
        [370, 140],
      ],
    );
    assert.equal(target.output, f.target.output);
    assert.equal(f.session.historyView.past.length, past + 1);
    const pasted = serialize(f.session.snapshot());
    f.send('undo');
    assert.equal(serialize(f.session.snapshot()), beforePaste);
    f.send('redo');
    assert.equal(serialize(f.session.snapshot()), pasted);
    const next = f.paste().nodeIds;
    assert.ok(next.every((id) => !ids.includes(id)));
    assert.deepEqual(
      f.session
        .snapshot()
        .layers.find((layer) => layer.id === f.target.id)
        .nodes.find((item) => item.id === next[0]).position,
      [100, 130],
    );
  });
}

test('failed Paste after project Undo preserves Redo and the next Paste offset', () => {
  const f = fixture();
  f.edit(7);
  f.copy([f.math.id]);
  f.send('undo');
  const before = serialize(f.session.snapshot()),
    state = f.session.state,
    future = f.session.historyView.future;
  assert.throws(() => f.paste(), /unselected input/i);
  assert.equal(serialize(f.session.snapshot()), before);
  assert.deepEqual(f.session.state, state);
  assert.deepEqual(f.session.historyView.future, future);
  const id = f.paste(f.source.id).nodeIds[0],
    pasted = f.session.snapshot().layers[0].nodes.find((item) => item.id === id);
  assert.equal(pasted.params.a.source, f.value.id);
  assert.deepEqual(pasted.position, [370, 140]);
});

test('empty Undo/Redo keeps Copy, while a separate project owner starts empty', () => {
  const f = fixture();
  f.copy();
  assert.equal(f.send('undo').changed, false);
  assert.equal(f.send('redo').changed, false);
  assert.equal(f.paste().nodeIds.length, 2);
  const replacement = createCompositionSessionOwner(f.session.projectSnapshot());
  assert.throws(
    () =>
      replacement.dispatch({
        ...replacement.state,
        type: 'paste-nodes',
        payload: { layerId: f.target.id },
      }),
    /clipboard is empty/i,
  );
});

test('Composition switching and History fallback retain a self-contained Copy', () => {
  const f = fixture(),
    original = f.session.activeCompositionId,
    added = f.session.createComposition({ name: 'Other' });
  f.session.switchComposition(original);
  f.copy();
  f.session.switchComposition(added);
  f.session.switchComposition(original);
  assert.equal(f.paste().nodeIds.length, 2);
  f.send('undo');
  f.session.switchComposition(added);
  const layer = f.session.snapshot().layers[0];
  const addedNode = f.send('create-node', { layerId: layer.id, nodeType: 'Value' });
  f.send('copy-nodes', { layerId: layer.id, nodeIds: [addedNode.nodeId] });
  f.send('undo');
  f.send('undo');
  assert.equal(f.session.activeCompositionId, original);
  assert.equal(f.paste().nodeIds.length, 1);
});

test('prepared project Undo/Redo and cancellation retain the same Copy snapshot', async () => {
  const f = fixture(),
    bridge = {
      async prepareDocument(document) {
        return { document: structuredClone(document) };
      },
      assertFresh(ticket) {
        assert.ok(ticket.document);
      },
      preparedFonts: () => ({}),
      activate() {},
      stageActivation() {},
      assertFreshForPublication() {},
      publishActivation() {},
      discardActivation() {},
      cancel() {},
    };
  f.edit(7);
  f.copy();
  const cancelled = await f.session.prepareCommand({ ...f.session.state, type: 'undo' }, bridge);
  f.session.cancelPrepared(bridge);
  assert.throws(() => f.session.commitPrepared(cancelled), /STALE/);
  assert.equal(
    f.session.snapshot().layers[0].nodes.find((item) => item.id === f.value.id).params.value.value,
    7,
  );
  for (const type of ['undo', 'redo']) {
    const token = await f.session.prepareCommand({ ...f.session.state, type }, bridge);
    f.session.commitPrepared(token);
  }
  assert.equal(f.paste().nodeIds.length, 2);
});

test('history inheritance requires its non-null owner and isolates mutable clipboard metadata', () => {
  const doc = blankDocument(),
    value = node('Value', 40, 70),
    owner = Object.freeze({});
  doc.layers[0].nodes.push(value);
  const source = new CommandSession(doc, { nodeClipboardOwner: owner });
  source.dispatch({
    ...source.state,
    type: 'copy-nodes',
    payload: { layerId: doc.layers[0].id, nodeIds: [value.id] },
  });
  for (const options of [
    { nodeClipboardFrom: source },
    { nodeClipboardOwner: {}, nodeClipboardFrom: source },
    { nodeClipboardOwner: owner, nodeClipboardFrom: {} },
  ])
    assert.throws(() => new CommandSession(doc, options), /clipboard owner/);
  const first = new CommandSession(doc, {
    nodeClipboardOwner: owner,
    nodeClipboardFrom: source,
  });
  const firstId = first.dispatch({
    ...first.state,
    type: 'paste-nodes',
    payload: { layerId: doc.layers[0].id },
  }).nodeIds[0];
  const second = new CommandSession(doc, {
    nodeClipboardOwner: owner,
    nodeClipboardFrom: source,
  });
  const secondId = second.dispatch({
    ...second.state,
    type: 'paste-nodes',
    payload: { layerId: doc.layers[0].id },
  }).nodeIds[0];
  for (const [session, id] of [
    [first, firstId],
    [second, secondId],
  ])
    assert.deepEqual(
      session.snapshot().layers[0].nodes.find((item) => item.id === id).position,
      [70, 100],
    );
});

test('a nested project-only rename during Undo cannot overwrite newer History or Copy', () => {
  let f,
    armed = false;
  f = fixture({
    busy: () => {
      if (armed) {
        armed = false;
        f.session.renameComposition(f.session.activeCompositionId, 'Nested rename');
      }
      return false;
    },
  });
  f.edit(7);
  f.copy();
  armed = true;
  assert.throws(() => f.send('undo'), /STALE/);
  assert.equal(f.session.projectSnapshot().compositions[0].name, 'Nested rename');
  assert.equal(
    f.session.snapshot().layers[0].nodes.find((item) => item.id === f.value.id).params.value.value,
    7,
  );
  assert.equal(f.session.historyView.past.length, 2);
  assert.equal(f.paste().nodeIds.length, 2);
});
