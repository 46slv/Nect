import test from 'node:test';
import assert from 'node:assert/strict';
import { blankDocument } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import { DocumentLifecycle } from '../src/document-lifecycle.js';
import { createLayerClipboardOwner } from '../src/layer-clipboard-owner.js';

function fixture() {
  const origin = migrateLegacyDocumentToCompositionProject(blankDocument());
  origin.compositions[0].id = 'origin';
  origin.compositions[0].name = 'Origin';
  origin.compositions[0].layers[0].id = 'source-layer';
  origin.compositions[0].layers[0].name = 'Original';
  const replacement = structuredClone(origin);
  replacement.compositions[0].name = 'Replacement';
  replacement.compositions[0].layers[0].name = 'Foreign';
  const project = structuredClone(origin),
    keeper = migrateLegacyDocumentToCompositionProject(blankDocument()).compositions[0];
  keeper.id = 'keeper';
  project.compositions.push(keeper);
  const clipboard = createLayerClipboardOwner();
  let owner, documents;
  const createOwner = (value) =>
    createCompositionSessionOwner(value, {
      onCommitted: (state) => {
        if (state.sessionId === owner.id && clipboard.hasCopy)
          clipboard.observe({
            owner,
            generation: documents.generation,
            operation: state.operation,
            history: owner.historyView,
          });
      },
    });
  owner = createOwner(project);
  documents = new DocumentLifecycle({
    read: () => JSON.stringify(owner.projectSnapshot()),
    write: async () => ({ token: 'saved' }),
    choose: async () => 'discard',
  });
  const send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });
  const copy = () =>
    clipboard.copy({
      owner,
      generation: documents.generation,
      compositionId: owner.activeCompositionId,
      layerId: 'source-layer',
    });
  const source = () =>
    clipboard.source({
      owner,
      generation: documents.generation,
      compositionId: owner.activeCompositionId,
    });
  const removeOrigin = () =>
    send('delete-composition', {
      compositionId: 'origin',
      expectedName: 'Origin',
      expectedProjectRevision: owner.projectRevision,
    });
  const importReplacement = () =>
    send('import-project-compositions', {
      sourceProject: replacement,
      compositionIds: ['origin'],
    });
  return {
    clipboard,
    documents,
    get owner() {
      return owner;
    },
    copy,
    source,
    send,
    removeOrigin,
    importReplacement,
    replace() {
      owner = createOwner(replacement);
      documents.replace({ saved: true });
    },
  };
}

test('live Layer copy follows normal edits and a round trip between Composition tabs', () => {
  const f = fixture(),
    revision = f.owner.projectRevision;
  f.copy();
  assert.equal(f.owner.projectRevision, revision, 'Copy does not add History');
  f.send('rename', { layerId: 'source-layer', expectedName: 'Original', name: 'Edited' });
  f.owner.switchComposition('keeper');
  assert.equal(f.source(), null);
  f.owner.switchComposition('origin');
  assert.equal(f.source(), 'source-layer');
  const before = JSON.stringify(f.owner.projectSnapshot());
  const result = f.send('duplicate-layer', { layerId: f.source() });
  assert.equal(
    f.owner.activeDocument().layers.find((item) => item.id === result.layerId).name,
    'Edited copy',
  );
  f.send('undo');
  assert.equal(JSON.stringify(f.owner.projectSnapshot()), before);
  assert.equal(f.source(), 'source-layer');
  f.send('redo');
  assert.equal(f.source(), 'source-layer');
});

test('the old copy cannot cross a project replacement with matching IDs', () => {
  const f = fixture();
  f.copy();
  f.replace();
  assert.equal(f.source(), null);
  assert.equal(f.clipboard.hasCopy, false);
});

test('deletion suspends Copy and exact Undo/Redo restores only its original History entry', () => {
  const f = fixture();
  f.copy();
  f.removeOrigin();
  assert.equal(f.source(), null);
  f.send('undo');
  f.owner.switchComposition('origin');
  assert.equal(f.source(), 'source-layer');
  f.send('redo');
  assert.equal(f.source(), null);
  f.send('undo');
  f.owner.switchComposition('origin');
  assert.equal(f.source(), 'source-layer');
});

test('a reused Composition and Layer ID is not the copied owner', () => {
  const f = fixture();
  f.copy();
  f.removeOrigin();
  f.importReplacement();
  assert.equal(f.owner.activeCompositionId, 'origin');
  assert.equal(f.owner.activeDocument().layers[0].name, 'Foreign');
  assert.equal(f.source(), null);
  f.send('undo');
  f.send('undo');
  f.owner.switchComposition('origin');
  assert.equal(f.source(), 'source-layer', 'returning to the exact prior state restores Copy');
});

test('copying the replacement does not revive that copy on the earlier owner', () => {
  const f = fixture();
  f.copy();
  f.removeOrigin();
  f.importReplacement();
  f.copy();
  assert.equal(f.source(), 'source-layer');
  f.send('undo');
  f.send('undo');
  f.owner.switchComposition('origin');
  assert.equal(f.source(), null);
  f.send('redo');
  f.send('redo');
  assert.equal(f.owner.activeCompositionId, 'keeper');
  f.owner.switchComposition('origin');
  assert.equal(f.source(), 'source-layer');
});

test('missing source across intervening edits stays suspended until the exact entry is restored', () => {
  const f = fixture();
  f.copy();
  f.removeOrigin();
  f.send('set-document-settings', { key: 'width', value: 1001 });
  f.importReplacement();
  assert.equal(f.source(), null);
  f.send('undo');
  f.send('undo');
  f.send('undo');
  f.owner.switchComposition('origin');
  assert.equal(f.source(), 'source-layer');
});

test('failed commands and Save As preserve Copy, History and its owner', async () => {
  const f = fixture();
  await f.documents.replace({ saved: true });
  f.copy();
  f.send('set-document-settings', { key: 'width', value: 1001 });
  f.send('undo');
  const redo = f.owner.historyView.redoToken,
    before = JSON.stringify(f.owner.projectSnapshot());
  assert.throws(() => f.send('delete-layer', { layerId: 'missing-layer' }));
  assert.equal(f.owner.historyView.redoToken, redo);
  assert.equal(JSON.stringify(f.owner.projectSnapshot()), before);
  assert.equal(f.source(), 'source-layer');
  const generation = f.documents.generation;
  assert.equal(await f.documents.save(true), true);
  assert.equal(f.documents.generation, generation);
  assert.equal(f.source(), 'source-layer');
});
