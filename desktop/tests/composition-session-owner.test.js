import test from 'node:test';
import assert from 'node:assert/strict';
import { CompositionSessionOwner } from '../src/composition-session-owner.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';

let idSequence = 0;
const clone = (value) => structuredClone(value);
const uid = (prefix) => `${prefix}-test-${++idSequence}`;

function blankDocument() {
  return {
    schemaVersion: 1,
    name: 'Test project',
    width: 960,
    height: 540,
    editor: { guides: [] },
    layers: [{ id: uid('layer'), name: 'Artwork', nodes: [] }],
  };
}

class TestHistory {
  constructor(doc) {
    this.doc = clone(doc);
    this.past = [];
    this.future = [];
    this.revision = 0;
  }
  apply(edit) {
    const before = clone(this.doc);
    edit(this.doc);
    if (JSON.stringify(before) === JSON.stringify(this.doc)) return;
    this.revision++;
    this.past.push(before);
    this.future = [];
  }
  undo() {
    if (!this.past.length) return;
    this.revision++;
    this.future.push(this.doc);
    this.doc = this.past.pop();
  }
  redo() {
    if (!this.future.length) return;
    this.revision++;
    this.past.push(this.doc);
    this.doc = this.future.pop();
  }
}

let sessionSequence = 0;
class TestCommandSession {
  constructor(doc) {
    this.doc = clone(doc);
    this.id = `test-session-${++sessionSequence}`;
    this.revision = 0;
    this.hasGesture = false;
    this.hasTextComposition = false;
    this.hasTextDrafts = false;
    this.prepared = null;
  }
  get state() {
    return { sessionId: this.id, expectedRevision: this.revision };
  }
  snapshot() {
    return clone(this.doc);
  }
  async prepareCommand({ sessionId, expectedRevision, type, payload = {} }) {
    if (sessionId !== this.id || expectedRevision !== this.revision)
      throw Error('STALE_REVISION: document changed');
    if (type !== 'set-document-settings') throw Error('Unsupported prepared test command');
    const candidate = clone(this.doc);
    candidate[payload.key] = payload.value;
    const token = Object.freeze({});
    this.prepared = { token, candidate };
    return token;
  }
  preparedSnapshot(token) {
    if (!this.prepared || this.prepared.token !== token) throw Error('STALE_REVISION');
    return { ...this.state, document: clone(this.prepared.candidate), fonts: {} };
  }
  commitPrepared(token) {
    if (!this.prepared || this.prepared.token !== token) throw Error('STALE_REVISION');
    this.doc = clone(this.prepared.candidate);
    this.prepared = null;
    this.revision++;
    return { ...this.state, changed: true };
  }
  cancelPrepared() {
    this.prepared = null;
  }
  dispatch({ sessionId, expectedRevision, type, payload = {} }) {
    if (sessionId !== this.id) throw Error('STALE_SESSION: document changed');
    if (expectedRevision !== this.revision) throw Error('STALE_REVISION: document changed');
    const before = JSON.stringify(this.doc);
    if (type === 'set-document-settings') {
      if (!['width', 'height'].includes(payload.key)) throw Error('Invalid document setting');
      this.doc[payload.key] = payload.value;
    } else if (type === 'add-shared-assets') {
      this.doc.fontAssets = { ...this.doc.fontAssets, ...payload.fontAssets };
      this.doc.imageAssets = { ...this.doc.imageAssets, ...payload.imageAssets };
    } else throw Error(`Unsupported test command: ${type}`);
    if (JSON.stringify(this.doc) !== before) this.revision++;
    return { ...this.state };
  }
}

const runtime = {
  History: TestHistory,
  CommandSession: TestCommandSession,
  blankDocument,
  cloneDocument: clone,
  sameDocument: (left, right) => JSON.stringify(left) === JSON.stringify(right),
  uid,
  validateDocument(document) {
    if (
      !Number.isFinite(document.width) ||
      document.width <= 0 ||
      !Number.isFinite(document.height) ||
      document.height <= 0 ||
      !Array.isArray(document.layers) ||
      !document.layers.length
    )
      throw Error('Invalid test Document');
    return document;
  },
};

function fixture() {
  const project = migrateLegacyDocumentToCompositionProject(blankDocument());
  const owner = new CompositionSessionOwner(project, runtime);
  const firstId = project.compositions[0].id;
  const secondId = owner.createComposition({ name: 'Tall', width: 320, height: 960 });
  owner.switchComposition(firstId);
  return { owner, firstId, secondId };
}

function send(owner, type, payload = {}) {
  return owner.dispatch({ ...owner.state, type, payload });
}

function dimensions(owner) {
  return owner
    .projectSnapshot()
    .compositions.map(({ id, width, height }) => ({ id, width, height }));
}

test('project-wide Undo/Redo follows edits across Compositions without crossing their data', () => {
  const { owner, firstId, secondId } = fixture();
  const firstOriginal = owner.activeDocument().width;
  send(owner, 'set-document-settings', { key: 'width', value: 1280 });
  owner.switchComposition(secondId);
  send(owner, 'set-document-settings', { key: 'height', value: 1440 });
  const afterBoth = dimensions(owner);

  send(owner, 'undo');
  assert.equal(owner.activeCompositionId, secondId);
  assert.deepEqual(dimensions(owner), [
    { id: firstId, width: 1280, height: 540 },
    { id: secondId, width: 320, height: 960 },
  ]);
  send(owner, 'undo');
  assert.deepEqual(dimensions(owner), [
    { id: firstId, width: firstOriginal, height: 540 },
    { id: secondId, width: 320, height: 960 },
  ]);
  send(owner, 'redo');
  send(owner, 'redo');
  assert.deepEqual(dimensions(owner), afterBoth);
});

test('Composition switching never adds a history step or changes undo/redo availability', () => {
  const { owner, firstId, secondId } = fixture();
  send(owner, 'set-document-settings', { key: 'width', value: 1200 });
  send(owner, 'undo');
  const before = {
    projectRevision: owner.projectRevision,
    historyState: owner.historyState,
    project: serializeCompositionProject(owner.projectSnapshot()),
  };

  owner.switchComposition(secondId);
  owner.switchComposition(firstId);
  owner.switchComposition(secondId);

  assert.equal(owner.projectRevision, before.projectRevision);
  assert.deepEqual(owner.historyState, before.historyState);
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), before.project);
  assert.equal(owner.activeCompositionId, secondId);
});

test('session-compatible History views stay stable across project edits and Composition switches', () => {
  const { owner, firstId, secondId } = fixture(),
    history = owner.historyView,
    firstDocument = history.doc;

  assert.strictEqual(owner.historyView, history);
  assert.equal(owner.id, owner.state.sessionId);
  assert.equal(firstDocument.width, 960);
  assert.strictEqual(history.doc, firstDocument);
  assert.ok(Object.isFrozen(firstDocument));

  send(owner, 'set-document-settings', { key: 'width', value: 1280 });
  assert.equal(history.doc.width, 1280);
  assert.strictEqual(history.past.at(-1), firstDocument);
  assert.strictEqual(history.past.at(-1), history.past.at(-1));

  owner.switchComposition(secondId);
  const secondDocument = history.doc;
  assert.equal(secondDocument.width, 320);
  send(owner, 'set-document-settings', { key: 'width', value: 480 });
  assert.strictEqual(history.past.at(-1), secondDocument);
  assert.equal(history.past.at(-1).width, 320);
  assert.equal(owner.activeCompositionId, secondId);
  assert.equal(owner.projectSnapshot().compositions.find((item) => item.id === firstId).width, 1280);
});

test('prepared edits enter project History and variable-aware Undo restores the project snapshot', async () => {
  const { owner, firstId } = fixture(),
    bridge = {
      async prepareDocument(document) {
        return { document: clone(document) };
      },
      assertFresh(ticket) {
        if (!ticket?.document) throw Error('STALE_PREPARED');
      },
      preparedFonts: () => ({}),
      activate() {},
      cancel() {},
    };

  const editToken = await owner.prepareCommand(
    { ...owner.state, type: 'set-document-settings', payload: { key: 'width', value: 1280 } },
    bridge,
  );
  assert.equal(owner.preparedSnapshot(editToken).document.width, 1280);
  owner.commitPrepared(editToken);
  assert.equal(owner.projectSnapshot().compositions[0].width, 1280);
  assert.equal(owner.historyState.canUndo, true);

  const undoToken = await owner.prepareCommand({ ...owner.state, type: 'undo' }, bridge);
  assert.equal(owner.preparedSnapshot(undoToken).document.width, 960);
  owner.commitPrepared(undoToken);
  assert.equal(owner.activeCompositionId, firstId);
  assert.equal(owner.projectSnapshot().compositions[0].width, 960);
  assert.equal(owner.historyState.canRedo, true);
});

test('Undo in another Composition retains shared font and image assets', () => {
  const { owner, firstId, secondId } = fixture();
  const firstLayerId = owner.activeDocument().layers[0].id;
  send(owner, 'add-shared-assets', {
    fontAssets: { sharedFont: { name: 'Fixture', data: 'font-bytes' } },
    imageAssets: { sharedImage: { name: 'pixel.png', data: 'image-bytes' } },
  });

  owner.switchComposition(secondId);
  send(owner, 'set-document-settings', { key: 'width', value: 500 });
  send(owner, 'undo');
  const afterUndo = owner.projectSnapshot();
  assert.equal(owner.activeCompositionId, secondId);
  assert.equal(afterUndo.compositions.find((entry) => entry.id === secondId).width, 320);
  assert.ok(
    afterUndo.compositions
      .find((entry) => entry.id === firstId)
      .layers.some((layer) => layer.id === firstLayerId),
  );
  assert.ok(afterUndo.fontAssets.sharedFont, 'shared font remains in project history');
  assert.ok(afterUndo.imageAssets.sharedImage, 'shared image remains in project history');

  send(owner, 'redo');
  const afterRedo = owner.projectSnapshot();
  assert.equal(afterRedo.compositions.find((entry) => entry.id === secondId).width, 500);
  assert.ok(afterRedo.fontAssets.sharedFont);
  assert.ok(afterRedo.imageAssets.sharedImage);
});

test('invalid switch and invalid resize preserve the full project and session state', () => {
  const { owner, firstId, secondId } = fixture();
  const projectBefore = serializeCompositionProject(owner.projectSnapshot());
  const stateBefore = owner.state;
  const historyBefore = owner.historyState;

  assert.throws(() => owner.switchComposition('missing-composition'), /Unknown active Composition/);
  assert.throws(() => owner.resizeComposition(secondId, 0, 700), /Invalid Composition width/);

  assert.equal(owner.activeCompositionId, firstId);
  assert.deepEqual(owner.state, stateBefore);
  assert.deepEqual(owner.historyState, historyBefore);
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), projectBefore);
});

test('Composition create, rename, resize and delete are project Undo steps', () => {
  const project = migrateLegacyDocumentToCompositionProject(blankDocument());
  const owner = new CompositionSessionOwner(project, runtime);
  const original = project.compositions[0].id;
  const added = owner.createComposition({ name: 'Mobile', width: 375, height: 812 });
  owner.renameComposition(added, 'Phone');
  owner.resizeComposition(added, 390, 844);
  assert.deepEqual(
    owner.projectSnapshot().compositions.map(({ name, width, height }) => [name, width, height]),
    [
      ['Composition 1', 960, 540],
      ['Phone', 390, 844],
    ],
  );
  owner.deleteComposition(added);
  assert.deepEqual(owner.projectSnapshot().compositions.map(({ id }) => id), [original]);
  send(owner, 'undo');
  assert.deepEqual(owner.projectSnapshot().compositions.map(({ id }) => id), [original, added]);
  assert.equal(owner.projectSnapshot().compositions[1].name, 'Phone');
  assert.equal(owner.projectSnapshot().compositions[1].width, 390);
});
