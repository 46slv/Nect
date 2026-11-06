import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  blankLayer,
  node,
  connect,
  CommandSession,
  evaluate,
  serialize,
  deserialize,
  bindProjectDocumentContext,
} from '../src/core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { editKeyColorRecipe } from '../src/key-color-recipe.js';

function fixture(
  kind = 'document',
  { expression = true, paused = false, missing = false, schemaVersion } = {},
) {
  const doc = blankDocument(),
    target = doc.layers[0],
    source = blankLayer('Source'),
    sink = node('Fill'),
    color = node('Color'),
    local = node('Color'),
    other = blankLayer('Other');
  target.id = 'target-art';
  source.id = 'source-art';
  other.id = 'other-art';
  sink.id = 'sink';
  color.id = 'color';
  local.id = 'local';
  doc.schemaVersion = schemaVersion ?? (expression ? 64 : 63);
  doc.colorSlots = [{ id: 'brand', name: 'Brand', value: { hex: '#aBcD09', alpha: 0 } }];
  color.params.value.colorSlotId = 'brand';
  const ref = 'ref(colorArtwork, "source-art", "color", "value")';
  if (expression) sink.params.color.expression = ref;
  target.nodes.push(sink, local);
  if (!missing) source.nodes.push(color);
  doc.layers.push(source, other);
  if (paused) connect(target, local.id, sink.id, 'color');
  const session =
    kind === 'document'
      ? new CommandSession(doc)
      : createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc));
  const send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  const copy = () => send('copy-nodes', { layerId: target.id, nodeIds: [sink.id] });
  const paste = (layerId = target.id) => send('paste-nodes', { layerId });
  return { doc, target, source, sink, color, local, ref, session, send, copy, paste };
}

for (const kind of ['document', 'composition']) {
  test(`${kind}: Color Artwork clipboard keeps the same live source and one-step History`, () => {
    const f = fixture(kind),
      before = serialize(f.session.snapshot());
    assert.equal(f.copy().externalInputCount, 1);
    assert.equal(serialize(f.session.snapshot()), before);
    const next = { hex: '#1234aB', alpha: 0 };
    f.send('set-color-slot', { slotId: 'brand', value: next });
    const beforePaste = serialize(f.session.snapshot());
    const { nodeIds } = f.paste();
    const saved = f.session.snapshot(),
      pasted = saved.layers[0].nodes.find((n) => n.id === nodeIds[0]);
    assert.equal(pasted.params.color.expression, f.ref);
    assert.equal(pasted.params.color.colorSlotId, undefined);
    assert.deepEqual(
      evaluate(saved.layers[0], {}, {}, saved).parameterValue(nodeIds[0], 'color'),
      next,
    );
    assert.deepEqual(saved.colorSlots, [{ id: 'brand', name: 'Brand', value: next }]);
    assert.equal(saved.keyColorRecipe, undefined);
    const encoded = serialize(saved);
    assert.equal(serialize(deserialize(encoded)), encoded);
    f.send('undo');
    assert.equal(serialize(f.session.snapshot()), beforePaste);
    f.send('redo');
    assert.equal(serialize(f.session.snapshot()), encoded);
  });
}

test('a removed Color source permanently expires the clipboard and preserves Redo', () => {
  const f = fixture();
  f.copy();
  f.send('delete-node', { layerId: f.source.id, nodeId: f.color.id });
  assert.throws(() => f.paste(), /missing|changed|copy again/i);
  f.send('undo');
  const before = serialize(f.session.snapshot()),
    state = f.session.state,
    future = f.session.historyView.future;
  assert.throws(() => f.paste(), /missing|changed|copy again/i);
  assert.equal(serialize(f.session.snapshot()), before);
  assert.deepEqual(f.session.state, state);
  assert.deepEqual(f.session.historyView.future, future);
});

test('a pasted expression remains missing after source deletion and Undo restores its value', () => {
  const f = fixture();
  f.copy();
  const id = f.paste().nodeIds[0];
  f.send('delete-node', { layerId: f.source.id, nodeId: f.color.id });
  let doc = f.session.snapshot();
  assert.equal(doc.layers[0].nodes.find((n) => n.id === id).params.color.expression, f.ref);
  assert.throws(() => evaluate(doc.layers[0], {}, {}, doc).parameterValue(id, 'color'), /missing/i);
  f.send('undo');
  doc = f.session.snapshot();
  assert.deepEqual(evaluate(doc.layers[0], {}, {}, doc).parameterValue(id, 'color'), {
    hex: '#aBcD09',
    alpha: 0,
  });
});

test('another Artwork paste refuses atomically without consuming Redo', () => {
  const f = fixture();
  f.copy();
  f.send('set-color-slot', { slotId: 'brand', value: { hex: '#345678', alpha: 1 } });
  f.send('undo');
  const before = serialize(f.session.snapshot()),
    state = f.session.state,
    future = f.session.historyView.future;
  assert.throws(() => f.paste('other-art'), /original Artwork/i);
  assert.equal(serialize(f.session.snapshot()), before);
  assert.deepEqual(f.session.state, state);
  assert.deepEqual(f.session.historyView.future, future);
});

test('a typed Color Artwork clipboard refuses an older destination without consuming Redo', () => {
  const f = fixture('document', { expression: false });
  f.send('set-parameter-expression', {
    layerId: f.target.id,
    nodeId: f.sink.id,
    key: 'color',
    expression: f.ref,
  });
  f.copy();
  f.send('undo');
  const before = serialize(f.session.snapshot()),
    state = f.session.state,
    future = f.session.historyView.future;
  assert.equal(f.session.snapshot().schemaVersion, 63);
  assert.throws(() => f.paste(), /schema|copy again|version/i);
  assert.equal(serialize(f.session.snapshot()), before);
  assert.deepEqual(f.session.state, state);
  assert.deepEqual(f.session.historyView.future, future);
});

test('an untrusted project rebind expires Color Artwork clipboard even when IDs match', () => {
  const f = fixture(),
    project = migrateLegacyDocumentToCompositionProject(f.doc);
  project.compositions[0].id = 'comp';
  const doc = structuredClone(f.doc);
  bindProjectDocumentContext(doc, project, 'comp');
  const session = new CommandSession(doc),
    send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  send('copy-nodes', { layerId: f.target.id, nodeIds: [f.sink.id] });
  const other = structuredClone(project);
  other.colorSlots[0].value = { hex: '#ff0000', alpha: 1 };
  session.setProjectContext(other, 'comp');
  const before = serialize(session.snapshot()),
    state = session.state;
  assert.throws(
    () => send('paste-nodes', { layerId: f.target.id }),
    /missing|changed|copy again|project/i,
  );
  assert.equal(serialize(session.snapshot()), before);
  assert.deepEqual(session.state, state);
});

test('directly rebinding a frozen History view cannot revive a Color clipboard', () => {
  const f = fixture(),
    project = migrateLegacyDocumentToCompositionProject(f.doc);
  project.compositions[0].id = 'comp';
  const doc = structuredClone(f.doc);
  bindProjectDocumentContext(doc, project, 'comp');
  const session = new CommandSession(doc),
    send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  send('copy-nodes', { layerId: f.target.id, nodeIds: [f.sink.id] });
  const foreign = structuredClone(project);
  foreign.colorSlots[0].value = { hex: '#ff0000', alpha: 1 };
  bindProjectDocumentContext(session.historyView.doc, foreign, 'comp');
  // Rebinding back without an intervening Paste must not revive the clipboard.
  bindProjectDocumentContext(session.historyView.doc, project, 'comp');
  const before = serialize(session.snapshot()),
    state = session.state;
  assert.throws(
    () => send('paste-nodes', { layerId: f.target.id }),
    /missing|changed|copy again|project/i,
  );
  assert.equal(serialize(session.snapshot()), before);
  assert.deepEqual(session.state, state);
});

test('a nested project rebind cannot be overwritten by an older authority callback', () => {
  const f = fixture(),
    project = migrateLegacyDocumentToCompositionProject(f.doc);
  project.compositions[0].id = 'comp';
  const foreign = structuredClone(project),
    doc = structuredClone(f.doc);
  foreign.colorSlots[0].value = { hex: '#ff0000', alpha: 1 };
  bindProjectDocumentContext(doc, project, 'comp');
  let session,
    armed = false;
  const send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  session = new CommandSession(doc, {
    retainColorClipboardForProjectContext: () => {
      if (!armed) return false;
      armed = false;
      session.setProjectContext(foreign, 'comp');
      return true;
    },
  });
  send('copy-nodes', { layerId: f.target.id, nodeIds: [f.sink.id] });
  armed = true;
  assert.throws(() => session.setProjectContext(project, 'comp'), /STALE/);
  const before = serialize(session.snapshot()),
    state = session.state;
  assert.throws(
    () => send('paste-nodes', { layerId: f.target.id }),
    /missing|changed|copy again|project/i,
  );
  assert.equal(serialize(session.snapshot()), before);
  assert.deepEqual(session.state, state);
});

test('paused Color Artwork expressions remain linked when the copied socket is disconnected', () => {
  const f = fixture('document', { paused: true });
  f.copy();
  const { nodeIds } = f.paste(),
    id = nodeIds[0];
  const pasted = f.session.snapshot().layers[0].nodes.find((n) => n.id === id);
  assert.equal(pasted.params.color.source, f.local.id);
  assert.equal(pasted.params.color.expression, f.ref);
  f.send('disconnect', { layerId: f.target.id, nodeId: id, key: 'color' });
  const doc = f.session.snapshot();
  assert.deepEqual(evaluate(doc.layers[0], {}, {}, doc).parameterValue(id, 'color'), {
    hex: '#aBcD09',
    alpha: 0,
  });
});

test('schema 65 retains a mixed Color and numeric Artwork expression', () => {
  const f = fixture('document', { schemaVersion: 65 });
  const doc = f.session.snapshot(),
    layer = doc.layers[0],
    source = doc.layers[1],
    sink = layer.nodes.find((n) => n.id === f.sink.id),
    weight = node('Value');
  weight.id = 'weight';
  weight.params.value.value = 0.25;
  source.nodes.push(weight);
  sink.params.color.expression = `alpha(${f.ref}, ref(artwork, "source-art", "weight", "value"))`;
  const originalValue = evaluate(layer, {}, {}, doc).parameterValue(sink.id, 'color');
  assert.deepEqual(originalValue, { hex: '#abcd09', alpha: 0.25 });
  const session = new CommandSession(doc),
    send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  assert.equal(send('copy-nodes', { layerId: layer.id, nodeIds: [sink.id] }).externalInputCount, 2);
  const id = send('paste-nodes', { layerId: layer.id }).nodeIds[0],
    saved = session.snapshot();
  assert.equal(saved.schemaVersion, 65);
  assert.equal(
    saved.layers[0].nodes.find((n) => n.id === id).params.color.expression,
    sink.params.color.expression,
  );
  assert.deepEqual(
    evaluate(saved.layers[0], {}, {}, saved).parameterValue(id, 'color'),
    originalValue,
  );
});

test('a failed Color source copy preserves the previous clipboard', () => {
  const f = fixture('document', { missing: true });
  f.send('copy-nodes', { layerId: f.target.id, nodeIds: [f.local.id] });
  const before = serialize(f.session.snapshot()),
    state = f.session.state;
  assert.throws(() => f.copy(), /missing|changed/i);
  assert.equal(serialize(f.session.snapshot()), before);
  assert.deepEqual(f.session.state, state);
  const { nodeIds } = f.paste();
  assert.equal(f.session.snapshot().layers[0].nodes.find((n) => n.id === nodeIds[0]).type, 'Color');
});

test('reentrant project-context authority retains the nested edit and does not overwrite it', () => {
  const f = fixture(),
    project = migrateLegacyDocumentToCompositionProject(f.doc);
  project.compositions[0].id = 'comp';
  const doc = structuredClone(f.doc);
  bindProjectDocumentContext(doc, project, 'comp');
  let session,
    armed = false;
  const send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  session = new CommandSession(doc, {
    retainColorClipboardForProjectContext: () => {
      if (armed) {
        armed = false;
        send('rename', { layerId: f.source.id, nodeId: f.color.id, name: 'Nested rename' });
      }
      return true;
    },
  });
  send('copy-nodes', { layerId: f.target.id, nodeIds: [f.sink.id] });
  armed = true;
  assert.throws(() => session.setProjectContext(project, 'comp'), /STALE/);
  assert.equal(
    session.snapshot().layers[1].nodes.find((n) => n.id === f.color.id).name,
    'Nested rename',
  );
  const { nodeIds } = send('paste-nodes', { layerId: f.target.id });
  assert.ok(nodeIds.length);
});

test('a source deleted inside project-context authority expires the clipboard before Undo', () => {
  const f = fixture(),
    project = migrateLegacyDocumentToCompositionProject(f.doc);
  project.compositions[0].id = 'comp';
  const doc = structuredClone(f.doc);
  bindProjectDocumentContext(doc, project, 'comp');
  let session,
    armed = false;
  const send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  session = new CommandSession(doc, {
    retainColorClipboardForProjectContext: () => {
      if (armed) {
        armed = false;
        send('delete-node', { layerId: f.source.id, nodeId: f.color.id });
      }
      return true;
    },
  });
  send('copy-nodes', { layerId: f.target.id, nodeIds: [f.sink.id] });
  armed = true;
  assert.throws(() => session.setProjectContext(project, 'comp'), /STALE/);
  assert.equal(
    session.snapshot().layers[1].nodes.some((n) => n.id === f.color.id),
    false,
  );
  send('undo');
  const before = serialize(session.snapshot()),
    state = session.state,
    future = session.historyView.future;
  assert.throws(() => send('paste-nodes', { layerId: f.target.id }), /missing|changed|copy again/i);
  assert.equal(serialize(session.snapshot()), before);
  assert.deepEqual(session.state, state);
  assert.deepEqual(session.historyView.future, future);
});

test('a source deleted during Color Artwork Paste cannot be overwritten by a stale candidate', () => {
  const f = fixture();
  let session,
    armed = false;
  const send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  session = new CommandSession(f.doc, {
    fonts: () => {
      if (armed) {
        armed = false;
        send('delete-node', { layerId: f.source.id, nodeId: f.color.id });
      }
      return {};
    },
  });
  send('copy-nodes', { layerId: f.target.id, nodeIds: [f.sink.id] });
  armed = true;
  assert.throws(() => send('paste-nodes', { layerId: f.target.id }), /STALE|retarget/i);
  assert.equal(
    session.snapshot().layers[1].nodes.some((n) => n.id === f.color.id),
    false,
  );
  assert.equal(session.snapshot().layers[0].nodes.filter((n) => n.type === 'Fill').length, 1);
  send('undo');
  const before = serialize(session.snapshot()),
    state = session.state,
    future = session.historyView.future;
  assert.throws(() => send('paste-nodes', { layerId: f.target.id }), /missing|changed|copy again/i);
  assert.equal(serialize(session.snapshot()), before);
  assert.deepEqual(session.state, state);
  assert.deepEqual(session.historyView.future, future);
});

test('a slot edited during Color Artwork Paste is not overwritten by a stale candidate', () => {
  const f = fixture();
  let session,
    armed = false;
  const notifications = [];
  const send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  session = new CommandSession(f.doc, {
    onCommitted: (event) => notifications.push(event),
    fonts: () => {
      if (armed) {
        armed = false;
        send('set-color-slot', { slotId: 'brand', value: { hex: '#ee2211', alpha: 0.4 } });
      }
      return {};
    },
  });
  send('copy-nodes', { layerId: f.target.id, nodeIds: [f.sink.id] });
  armed = true;
  assert.throws(() => send('paste-nodes', { layerId: f.target.id }), /STALE/);
  assert.deepEqual(session.snapshot().colorSlots[0].value, { hex: '#ee2211', alpha: 0.4 });
  assert.equal(session.snapshot().layers[0].nodes.filter((n) => n.type === 'Fill').length, 1);
  assert.equal(notifications.length, 1);
});

test('a slot edited by the precommit hook is not overwritten by Color Artwork Paste', () => {
  const f = fixture();
  let session,
    armed = false;
  const send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  session = new CommandSession(f.doc, {
    beforeCandidateCommit: () => {
      if (armed) {
        armed = false;
        send('set-color-slot', { slotId: 'brand', value: { hex: '#ee2211', alpha: 0.4 } });
      }
    },
  });
  send('copy-nodes', { layerId: f.target.id, nodeIds: [f.sink.id] });
  armed = true;
  assert.throws(() => send('paste-nodes', { layerId: f.target.id }), /STALE/);
  assert.deepEqual(session.snapshot().colorSlots[0].value, { hex: '#ee2211', alpha: 0.4 });
  assert.equal(session.snapshot().layers[0].nodes.filter((n) => n.type === 'Fill').length, 1);
  const id = send('paste-nodes', { layerId: f.target.id }).nodeIds[0],
    saved = session.snapshot();
  assert.deepEqual(evaluate(saved.layers[0], {}, {}, saved).parameterValue(id, 'color'), {
    hex: '#ee2211',
    alpha: 0.4,
  });
});

test('the project owner keeps a nested live slot edit and its valid clipboard', () => {
  const f = fixture(),
    project = migrateLegacyDocumentToCompositionProject(f.doc);
  let owner,
    armed = false;
  const notifications = [];
  const send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });
  owner = createCompositionSessionOwner(project, {
    fonts: () => {
      if (armed) {
        armed = false;
        send('set-color-slot', { slotId: 'brand', value: { hex: '#ee2211', alpha: 0.4 } });
      }
      return {};
    },
    onCommitted: (event) => notifications.push(event.projectRevision),
  });
  send('copy-nodes', { layerId: f.target.id, nodeIds: [f.sink.id] });
  armed = true;
  assert.throws(() => send('paste-nodes', { layerId: f.target.id }), /STALE/);
  const afterNested = owner.snapshot();
  assert.deepEqual(afterNested.colorSlots[0].value, { hex: '#ee2211', alpha: 0.4 });
  assert.equal(afterNested.layers[0].nodes.filter((n) => n.type === 'Fill').length, 1);
  assert.deepEqual(notifications, [1]);
  const id = send('paste-nodes', { layerId: f.target.id }).nodeIds[0],
    saved = owner.snapshot();
  assert.deepEqual(evaluate(saved.layers[0], {}, {}, saved).parameterValue(id, 'color'), {
    hex: '#ee2211',
    alpha: 0.4,
  });
});

test('a project switch during Color Artwork Paste cannot commit through the retired Session', () => {
  const f = fixture(),
    project = migrateLegacyDocumentToCompositionProject(f.doc),
    other = blankDocument();
  project.compositions[0].id = 'original';
  project.compositions.push({
    id: 'other',
    name: 'Other',
    width: other.width,
    height: other.height,
    layers: other.layers,
    editor: other.editor,
  });
  let owner,
    armed = false;
  const send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });
  owner = createCompositionSessionOwner(project, {
    fonts: () => {
      if (armed) {
        armed = false;
        owner.switchComposition('other');
        send('set-color-slot', { slotId: 'brand', value: { hex: '#ee2211', alpha: 0.4 } });
        owner.switchComposition('original');
      }
      return {};
    },
  });
  send('copy-nodes', { layerId: f.target.id, nodeIds: [f.sink.id] });
  armed = true;
  assert.throws(() => send('paste-nodes', { layerId: f.target.id }), /STALE/);
  const saved = owner.snapshot();
  assert.deepEqual(saved.colorSlots[0].value, { hex: '#ee2211', alpha: 0.4 });
  assert.equal(saved.layers[0].nodes.filter((n) => n.type === 'Fill').length, 1);
  assert.throws(() => send('paste-nodes', { layerId: f.target.id }), /clipboard is empty/i);
});

test('node Duplicate remains explicitly unsupported for Color Artwork references', () => {
  const f = fixture();
  const before = serialize(f.session.snapshot()),
    state = f.session.state;
  assert.throws(
    () => f.send('duplicate-nodes', { layerId: f.target.id, nodeIds: [f.sink.id] }),
    /qualified Color|not supported/i,
  );
  assert.equal(serialize(f.session.snapshot()), before);
  assert.deepEqual(f.session.state, state);
});

test('a reentrant copy payload cannot replace the previous clipboard after an edit', () => {
  const f = fixture();
  f.send('copy-nodes', { layerId: f.target.id, nodeIds: [f.local.id] });
  const command = {
    ...f.session.state,
    type: 'copy-nodes',
    payload: {
      layerId: f.target.id,
      get nodeIds() {
        f.send('set-color-slot', { slotId: 'brand', value: { hex: '#223344', alpha: 1 } });
        return [f.sink.id];
      },
    },
  };
  assert.throws(() => f.session.dispatch(command), /STALE/);
  assert.deepEqual(f.session.snapshot().colorSlots[0].value, { hex: '#223344', alpha: 1 });
  const { nodeIds } = f.paste();
  assert.equal(
    f.session.snapshot().layers[0].nodes.find((n) => n.id === nodeIds[0]).id,
    nodeIds[0],
  );
  assert.equal(f.session.snapshot().layers[0].nodes.find((n) => n.id === nodeIds[0]).type, 'Color');
});

test('a live palette update by the same project owner retains the copied Color reference', () => {
  const f = fixture('composition', { expression: false });
  const request = {
    version: 1,
    modelVersion: 2,
    solverVersion: 'experimental-affine-v1',
    colorSemanticsVersion: 'linear-srgb-straight-v1',
    paletteId: 'fixture-palette',
    intent: {
      keyColor: {
        id: 'fresh-key',
        role: 'Identity',
        value: { hex: '#aBcD09', alpha: 0 },
        hueHint: 17,
      },
      preset: {
        id: 'explicit-fixture-only',
        version: 1,
        harmony: 'triadic',
        hueSteps: 12,
        slots: [
          {
            id: 'fresh-derived',
            role: 'Background',
            position: 1,
            lightnessScale: 0,
            lightnessOffset: 0.83,
            chromaScale: 0,
            chromaOffset: 0,
            alpha: 1,
          },
        ],
      },
      slots: [{ id: 'fresh-derived', role: 'Background', deltaL: 0.03, deltaRatioCmax: 0 }],
      settings: { areaAware: false },
    },
    allocations: [
      { slotId: 'fresh-derived', name: 'Background' },
      { slotId: 'fresh-key', name: 'C1' },
    ],
  };
  f.send('initialize-live-key-color-recipe', request);
  f.send('bind-color-slot', {
    layerId: f.source.id,
    nodeId: f.color.id,
    key: 'value',
    slotId: 'fresh-key',
  });
  f.send('set-parameter-expression', {
    layerId: f.target.id,
    nodeId: f.sink.id,
    key: 'color',
    expression: f.ref,
  });
  f.copy();
  const before = f.session.snapshot(),
    previous = before.keyColorRecipe.committedSnapshot;
  const next = editKeyColorRecipe(previous, {
    type: 'key-color',
    expectedRevision: previous.revision,
    value: { hex: '#d03270', alpha: 0 },
  });
  f.send('apply-live-key-color-recipe-snapshot', {
    snapshot: next,
    expectedPaletteRevision: previous.revision,
  });
  const changed = f.session.snapshot(),
    id = f.paste().nodeIds[0],
    pasted = f.session.snapshot();
  assert.deepEqual(pasted.colorSlots, changed.colorSlots);
  assert.deepEqual(pasted.keyColorRecipe, changed.keyColorRecipe);
  assert.equal(pasted.layers[0].nodes.find((n) => n.id === id).params.color.colorSlotId, undefined);
  assert.deepEqual(evaluate(pasted.layers[0], {}, {}, pasted).parameterValue(id, 'color'), {
    hex: '#d03270',
    alpha: 0,
  });
});
