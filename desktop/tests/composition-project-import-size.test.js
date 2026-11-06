import test from 'node:test';
import assert from 'node:assert/strict';
import { blankDocument, node } from '../src/core.js';
import { MAX_PROJECT_BYTES } from '../src/fonts.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { prepareProjectCompositionsImport } from '../src/composition-project-import.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

const pixel = {
  name: 'pixel.png',
  mime: 'image/png',
  width: 1,
  height: 1,
  data: 'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=',
};
const send = (owner, type, payload) => owner.dispatch({ ...owner.state, type, payload });
const bytes = (project) => Buffer.byteLength(serializeCompositionProject(project), 'utf8');

function project(prefix, imageIds) {
  const doc = blankDocument();
  doc.layers[0].id = `${prefix}-layer`;
  doc.layers[0].nodes[0].id = `${prefix}-output`;
  doc.layers[0].output = `${prefix}-output`;
  doc.imageAssets = {};
  for (const id of imageIds) {
    doc.imageAssets[id] = { ...pixel, name: `${id}.png` };
    const image = node('Image');
    image.id = `${prefix}-${id}`;
    image.asset = id;
    doc.layers[0].nodes.push(image);
  }
  const result = migrateLegacyDocumentToCompositionProject(doc);
  result.compositions[0].id = `${prefix}-composition`;
  return result;
}

test('oversized merged project with multiple images is rejected without changing owner history', () => {
  const current = project('current', ['image-shared']);
  const source = project('source', ['image-shared', 'image-other']);
  // Unknown Composition fields are legal JSON and must survive import intact.
  current.compositions[0].opaque = '界'.repeat(8_333_334);
  source.compositions[0].opaque = '界'.repeat(8_333_334);
  assert.ok(bytes(current) < MAX_PROJECT_BYTES);
  assert.ok(bytes(source) < MAX_PROJECT_BYTES);
  // The source is a valid independently openable project, not just a codec fixture.
  createCompositionSessionOwner(source);
  const sourceBefore = serializeCompositionProject(source);
  const owner = createCompositionSessionOwner(current);
  const active = owner.activeCompositionId;
  const initial = owner.projectSnapshot();
  owner.renameComposition(active, 'Current name');
  owner.renameComposition(active, 'Future name');
  const future = owner.projectSnapshot();
  send(owner, 'undo');
  const before = owner.projectSnapshot();
  const state = owner.state;
  const history = owner.historyState;
  const entryToken = owner.historyView.entryToken;
  const undoToken = owner.historyView.undoToken;
  const redoToken = owner.historyView.redoToken;
  assert.equal(history.canUndo, true);
  assert.equal(history.canRedo, true);

  assert.throws(
    () => send(owner, 'import-project-compositions', { sourceProject: source }),
    /Project exceeds 50 MB/,
  );
  assert.equal(serializeCompositionProject(source), sourceBefore);
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.state, state);
  assert.deepEqual(owner.historyState, history);
  assert.equal(owner.activeCompositionId, active);
  assert.equal(owner.historyView.entryToken, entryToken);
  assert.equal(owner.historyView.undoToken, undoToken);
  assert.equal(owner.historyView.redoToken, redoToken);
  send(owner, 'redo');
  assert.deepEqual(owner.projectSnapshot(), future);
  send(owner, 'undo');
  assert.deepEqual(owner.projectSnapshot(), before);
  send(owner, 'undo');
  assert.deepEqual(owner.projectSnapshot(), initial);
  send(owner, 'redo');
  assert.deepEqual(owner.projectSnapshot(), before);
});

test('merged UTF-8 size accepts exactly 50 MB, rejects one byte more, and counts deduplicated assets', () => {
  const current = project('current', ['image-shared']);
  const source = project('source', ['image-shared', 'image-other']);
  current.compositions[0].opaque = '';
  source.compositions[0].opaque = '';
  const base = prepareProjectCompositionsImport(current, source).project;
  assert.deepEqual(Object.keys(base.imageAssets).sort(), ['image-other', 'image-shared']);
  const currentPadding = 24_000_000;
  const remaining = MAX_PROJECT_BYTES - bytes(base) - currentPadding;
  current.compositions[0].opaque = 'x'.repeat(currentPadding);
  source.compositions[0].opaque =
    '界'.repeat(Math.floor(remaining / 3)) + 'x'.repeat(remaining % 3);
  assert.ok(bytes(current) < MAX_PROJECT_BYTES);
  assert.ok(bytes(source) < MAX_PROJECT_BYTES);
  const imported = prepareProjectCompositionsImport(current, source).project;
  const saved = serializeCompositionProject(imported);
  assert.equal(Buffer.byteLength(saved, 'utf8'), MAX_PROJECT_BYTES);
  assert.ok(saved.length < MAX_PROJECT_BYTES);
  assert.equal(imported.compositions[0].opaque, current.compositions[0].opaque);
  assert.equal(imported.compositions[1].opaque, source.compositions[0].opaque);
  assert.deepEqual(Object.keys(imported.imageAssets).sort(), ['image-other', 'image-shared']);
  const acceptedSource = source.compositions[0].opaque;
  source.compositions[0].opaque += 'x';
  assert.throws(() => prepareProjectCompositionsImport(current, source), /Project exceeds 50 MB/);
  assert.equal(source.compositions[0].opaque, `${acceptedSource}x`);
  assert.equal(current.compositions[0].opaque, imported.compositions[0].opaque);
  source.compositions[0].opaque = acceptedSource;
  const owner = createCompositionSessionOwner(current);
  const originalId = owner.activeCompositionId;
  const result = send(owner, 'import-project-compositions', { sourceProject: source });
  assert.equal(result.importedCompositionIds.length, 1);
  assert.equal(bytes(owner.projectSnapshot()), MAX_PROJECT_BYTES);
  send(owner, 'undo');
  assert.equal(owner.activeCompositionId, originalId);
  assert.equal(owner.projectSnapshot().compositions.length, 1);
  send(owner, 'redo');
  assert.equal(bytes(owner.projectSnapshot()), MAX_PROJECT_BYTES);
});

test('asset deduplication can keep a merged project under the limit even when input sizes sum over it', () => {
  const current = project('current', ['image-shared']);
  const source = project('source', ['image-shared', 'image-other']);
  current.imageAssets['image-shared'].opaque = '界'.repeat(8_500_000);
  source.imageAssets['image-shared'].opaque = current.imageAssets['image-shared'].opaque;
  assert.ok(bytes(current) < MAX_PROJECT_BYTES);
  assert.ok(bytes(source) < MAX_PROJECT_BYTES);
  assert.ok(bytes(current) + bytes(source) > MAX_PROJECT_BYTES);
  const imported = prepareProjectCompositionsImport(current, source).project;
  assert.ok(bytes(imported) < MAX_PROJECT_BYTES);
  assert.deepEqual(Object.keys(imported.imageAssets).sort(), ['image-other', 'image-shared']);
  assert.equal(
    imported.imageAssets['image-shared'].opaque,
    current.imageAssets['image-shared'].opaque,
  );
  assert.equal(
    source.imageAssets['image-shared'].opaque,
    current.imageAssets['image-shared'].opaque,
  );
});
