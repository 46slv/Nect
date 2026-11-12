import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import {
  blankDocument,
  blankLayer,
  node,
  CommandSession,
  evaluate,
  exportSVG,
  serialize,
} from '../src/core.js';
import { readSVGSource } from '../src/svg-tree.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

const tree = () =>
  readSVGSource(
    '<svg width="20" height="20"><rect width="20" height="20" fill="#ff0000"/></svg>',
    new Window().DOMParser,
  );

function fixture(project) {
  const document = blankDocument();
  const owner = project
    ? createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(document))
    : new CommandSession(document);
  const send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });
  const consumerId = document.layers[0].id;
  const { sourceLayerId } = send('add-svg-part', {
    layerId: consumerId,
    source: tree(),
    name: 'mark.svg',
  });
  return { owner, send, consumerId, sourceLayerId };
}

for (const project of [false, true]) {
  test(`${project ? 'project' : 'document'} Session rejects deleting a used SVG source without altering artwork or history`, () => {
    const { owner, send, consumerId, sourceLayerId } = fixture(project);
    send('create-blank-layer', { name: 'Other' });
    send('undo');
    const before = serialize(owner.snapshot()),
      state = owner.state,
      svg = exportSVG(owner.snapshot()),
      past = owner.historyView.past.length,
      future = owner.historyView.future.length;
    assert.equal(future, 1);
    assert.throws(() => send('delete-layer', { layerId: sourceLayerId }), /used by/i);
    assert.equal(serialize(owner.snapshot()), before);
    assert.deepEqual(owner.state, state);
    assert.equal(owner.historyView.past.length, past);
    assert.equal(owner.historyView.future.length, future);
    assert.equal(exportSVG(owner.snapshot()), svg);
    assert.equal(
      evaluate(
        owner.snapshot().layers.find((l) => l.id === consumerId),
        {},
        {},
        owner.snapshot(),
      ).error,
      null,
    );
    send('redo');
    assert.equal(owner.snapshot().layers.at(-1).name, 'Other');
  });

  test(`${project ? 'project' : 'document'} multi-delete rejects when any consumer survives`, () => {
    const { owner, send, consumerId, sourceLayerId } = fixture(project);
    send('create-blank-layer', { name: 'Other' });
    const otherId = owner.snapshot().layers.at(-1).id;
    const before = serialize(owner.snapshot()),
      state = owner.state;
    assert.throws(() => send('delete-layers', { layerIds: [sourceLayerId, otherId] }), /used by/i);
    assert.equal(serialize(owner.snapshot()), before);
    assert.deepEqual(owner.state, state);
    assert.ok(owner.snapshot().layers.some((l) => l.id === consumerId));
  });

  test(`${project ? 'project' : 'document'} Session protects a disconnected SVG asset reference`, () => {
    const document = blankDocument(),
      owner = project
        ? createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(document))
        : new CommandSession(document),
      send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });
    const { sourceLayerId } = send('insert-svg-asset', {
      route: 'graph',
      layerId: document.layers[0].id,
      source: tree(),
      name: 'mark.svg',
      provenance: {
        version: 1,
        mode: 'embedded',
        rootId: 'test-root',
        relativePath: 'mark.svg',
        sourceLocation: 'test/mark.svg',
        sha256: 'a'.repeat(64),
        author: 'Nect test',
        source: 'Self-authored',
        license: 'Test fixture',
      },
    });
    const before = serialize(owner.snapshot());
    assert.throws(() => send('delete-layer', { layerId: sourceLayerId }), /used by/i);
    assert.equal(serialize(owner.snapshot()), before);
  });

  test(`${project ? 'project' : 'document'} Session can delete source and all consumers together with one Undo/Redo`, () => {
    const { owner, send, consumerId, sourceLayerId } = fixture(project);
    const before = serialize(owner.snapshot()),
      past = owner.historyView.past.length;
    send('delete-layers', { layerIds: [sourceLayerId, consumerId] });
    const after = serialize(owner.snapshot());
    assert.equal(owner.historyView.past.length, past + 1);
    assert.equal(owner.snapshot().layers.length, 1);
    assert.notEqual(owner.snapshot().layers[0].id, sourceLayerId);
    send('undo');
    assert.equal(serialize(owner.snapshot()), before);
    send('redo');
    assert.equal(serialize(owner.snapshot()), after);
  });

  test(`${project ? 'project' : 'document'} multi-delete permits a source ordered before its consumer`, () => {
    const original = fixture(project),
      document = original.owner.snapshot();
    document.layers.reverse();
    const owner = project
      ? createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(document))
      : new CommandSession(document);
    const before = serialize(owner.snapshot());
    owner.dispatch({
      ...owner.state,
      type: 'delete-layers',
      payload: { layerIds: [original.sourceLayerId, original.consumerId] },
    });
    assert.equal(owner.snapshot().layers.length, 1);
    owner.dispatch({ ...owner.state, type: 'undo' });
    assert.equal(serialize(owner.snapshot()), before);
  });
}

test('missing or unbound references do not prevent unrelated deletion', () => {
  const document = blankDocument(),
    consumer = document.layers[0],
    unrelated = blankLayer('Unrelated');
  const missing = node('Composition Reference'),
    unset = node('Composition Reference');
  missing.compositionReference = { compositionId: 'already-missing' };
  consumer.nodes.push(missing, unset);
  document.schemaVersion = 31;
  document.layers.push(unrelated);
  const owner = new CommandSession(document);
  owner.dispatch({ ...owner.state, type: 'delete-layer', payload: { layerId: unrelated.id } });
  assert.equal(owner.snapshot().layers.length, 1);
  assert.deepEqual(
    owner.snapshot().layers[0].nodes.find((n) => n.id === missing.id).compositionReference,
    missing.compositionReference,
  );
});
