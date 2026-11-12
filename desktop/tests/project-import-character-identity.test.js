import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {
  blankDocument,
  node,
  setCharacterOffsets,
  setCharacterTransforms,
  setCharacterStyles,
  setCharacterKerningPairs,
} from '../src/core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { prepareProjectCompositionsImport } from '../src/composition-project-import.js';
import { DocumentLifecycle } from '../src/document-lifecycle.js';
import { imageData } from './fixtures/artwork-deletion.js';

const send = (owner, type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });

function project(prefix, withText = true) {
  const doc = blankDocument();
  doc.schemaVersion = 65;
  const layer = doc.layers[0];
  layer.id = `${prefix}-layer`;
  layer.nodes[0].id = `${prefix}-output`;
  layer.output = layer.nodes[0].id;
  if (withText) {
    const text = node('Text');
    text.id = `${prefix}-text`;
    text.text = 'AV';
    text.characterPositions = setCharacterOffsets(text.text, undefined, { c1: [5, -3] });
    text.characterPositions = setCharacterTransforms(text.text, text.characterPositions, {
      c2: { rotation: 15, scale: 1.2 },
    });
    text.characterPositions = setCharacterStyles(text.text, text.characterPositions, {
      c1: { size: 45, width: 1.1 },
    });
    text.characterPositions = setCharacterKerningPairs(text.text, text.characterPositions, {
      'c1:c2': -80,
    });
    layer.nodes.push(text);
  }
  const result = migrateLegacyDocumentToCompositionProject(doc);
  result.compositions[0].id = `${prefix}-composition`;
  return result;
}

test('Text-local IDs coexist with colliding global IDs, assets, references, history and Save', async () => {
  const destination = project('destination'),
    source = project('source');
  const destLayer = destination.compositions[0].layers[0],
    sourceLayer = source.compositions[0].layers[0];
  for (const layer of [destLayer, sourceLayer]) {
    const driver = node('Value'),
      curve = node('Curve');
    driver.id = 'c1';
    driver.params.value.value = 37;
    curve.id = `curve-${layer.id}`;
    curve.points[0].id = 'c2';
    layer.nodes.push(driver, curve);
  }
  const sink = node('Value'),
    external = node('Value'),
    secondText = node('Text');
  sink.id = 'sink';
  sink.params.value.source = 'c1';
  sink.params.value.expression = 'ref("c1","value") + ref("external","value")';
  external.id = 'external';
  destLayer.nodes.push(external);
  const firstText = sourceLayer.nodes.find((n) => n.type === 'Text');
  Object.assign(secondText, structuredClone(firstText), { id: 'source-text-2' });
  secondText.characterPositions.offsets.c1 = [-2, 7];
  secondText.characterPositions.kerningPairs['c1:c2'] = 12;
  firstText.metadata = { note: 'c1', nested: { character: 'c2' } };
  sourceLayer.nodes.push(sink, secondText);

  const fontData = fs.readFileSync(
    new URL('../public/fonts/DejaVuSans.ttf', import.meta.url),
    'base64',
  );
  const font = (name) => ({ name, fileName: 'DejaVuSans.ttf', data: fontData });
  const sharedFont = font('Shared font');
  destination.fontAssets = {
    'font-shared': sharedFont,
    'font-collision': font('Destination font'),
  };
  source.fontAssets = {
    'font-shared': structuredClone(sharedFont),
    'font-collision': font('Source font'),
  };
  firstText.font = 'font-shared';
  firstText.characterPositions.styles.c1.font = 'font-collision';
  secondText.font = 'font-collision';
  secondText.characterPositions.styles.c1.font = 'font-shared';
  const image = (name) => ({ name, mime: 'image/png', width: 1, height: 1, data: imageData });
  const sharedImage = image('shared.png');
  destination.imageAssets = {
    'image-shared': sharedImage,
    'image-collision': image('destination.png'),
  };
  source.imageAssets = {
    'image-shared': structuredClone(sharedImage),
    'image-collision': image('source.png'),
  };
  for (const [id, asset] of [
    ['shared-image-node', 'image-shared'],
    ['collision-image-node', 'image-collision'],
  ]) {
    const n = node('Image');
    n.id = id;
    n.asset = asset;
    sourceLayer.nodes.push(n);
  }
  const sourceBefore = structuredClone(source),
    destinationBefore = structuredClone(destination),
    owner = createCompositionSessionOwner(structuredClone(destination));
  prepareProjectCompositionsImport(destination, source);
  assert.deepEqual(destination, destinationBefore);
  assert.deepEqual(source, sourceBefore);
  let saved;
  const lifecycle = new DocumentLifecycle({
    read: () => serializeCompositionProject(owner.projectSnapshot()),
    write: async ({ contents }) => {
      saved = contents;
      return { token: 'saved' };
    },
    choose: async () => 'cancel',
  });
  await lifecycle.replace({ saved: true });
  assert.equal(lifecycle.dirty, false);
  const before = lifecycle.read(),
    revision = owner.projectRevision;
  const result = send(owner, 'import-project-compositions', { sourceProject: source });
  assert.equal(result.changed, true);
  assert.equal(owner.projectRevision, revision + 1);
  assert.equal(owner.historyView.past.length, 1);
  assert.equal(lifecycle.dirty, true);
  assert.equal(lifecycle.revision, 1);
  const after = lifecycle.read();
  const imported = owner
    .projectSnapshot()
    .compositions.find((c) => c.id === result.importedCompositionIds[0]);
  const nodes = imported.layers[0].nodes;
  const copiedTexts = nodes.filter((n) => n.type === 'Text');
  assert.equal(copiedTexts.length, 2);
  assert.deepEqual(
    copiedTexts.map((n) => [n.id, n.text]),
    [
      ['source-text', 'AV'],
      ['source-text-2', 'AV'],
    ],
  );
  for (let i = 0; i < copiedTexts.length; i++) {
    const expected = structuredClone([firstText, secondText][i].characterPositions);
    if (i === 0) expected.styles.c1.font = 'font-collision-import-1';
    assert.deepEqual(copiedTexts[i].characterPositions, expected);
    assert.deepEqual(
      copiedTexts[i].characterPositions.units.map((u) => u.id),
      ['c1', 'c2'],
    );
  }
  assert.equal(copiedTexts[0].font, 'font-shared');
  assert.equal(copiedTexts[1].font, 'font-collision-import-1');
  assert.deepEqual(copiedTexts[0].metadata, firstText.metadata);
  assert.equal(nodes.find((n) => n.id === 'c1-import-1').params.value.value, 37);
  assert.equal(nodes.find((n) => n.type === 'Curve').points[0].id, 'c2-import-1');
  const copiedSink = nodes.find((n) => n.id === 'sink');
  assert.equal(copiedSink.params.value.source, 'c1-import-1');
  assert.equal(
    copiedSink.params.value.expression,
    'ref("c1-import-1","value") + ref("external","value")',
  );
  const snapshot = owner.projectSnapshot();
  assert.deepEqual(snapshot.fontAssets['font-shared'], sourceBefore.fontAssets['font-shared']);
  assert.deepEqual(
    snapshot.fontAssets['font-collision'],
    destinationBefore.fontAssets['font-collision'],
  );
  assert.deepEqual(
    snapshot.fontAssets['font-collision-import-1'],
    sourceBefore.fontAssets['font-collision'],
  );
  assert.equal(nodes.find((n) => n.id === 'shared-image-node').asset, 'image-shared');
  assert.equal(
    nodes.find((n) => n.id === 'collision-image-node').asset,
    'image-collision-import-1',
  );
  assert.deepEqual(snapshot.imageAssets['image-shared'], sourceBefore.imageAssets['image-shared']);
  assert.deepEqual(
    snapshot.imageAssets['image-collision-import-1'],
    sourceBefore.imageAssets['image-collision'],
  );
  assert.deepEqual(source, sourceBefore);
  assert.deepEqual(destination, destinationBefore);
  send(owner, 'undo');
  assert.equal(owner.projectRevision, revision + 2);
  assert.equal(lifecycle.read(), before);
  assert.equal(lifecycle.dirty, false);
  assert.equal(lifecycle.revision, 2);
  send(owner, 'redo');
  assert.equal(owner.projectRevision, revision + 3);
  assert.equal(lifecycle.read(), after);
  assert.equal(lifecycle.dirty, true);
  assert.equal(lifecycle.revision, 3);
  assert.equal(await lifecycle.save(), true);
  assert.equal(saved, after);
  assert.equal(lifecycle.dirty, false);
  assert.equal(lifecycle.savedRevision, 3);
  const reopened = createCompositionSessionOwner(deserializeCompositionProject(saved), {
    activeCompositionId: result.importedCompositionIds[0],
  });
  assert.deepEqual(reopened.projectSnapshot(), snapshot);
});

test('invalid character input rejects atomically while retaining the redo branch', async () => {
  const owner = createCompositionSessionOwner(project('destination'));
  owner.createComposition({ name: 'Undoable' });
  send(owner, 'undo');
  const source = project('source');
  source.compositions[0].layers[0].nodes.find(
    (n) => n.type === 'Text',
  ).characterPositions.units[1].id = 'c1';
  const sourceBefore = structuredClone(source),
    before = owner.projectSnapshot(),
    state = owner.state,
    history = owner.historyState,
    active = owner.activeCompositionId;
  const lifecycle = new DocumentLifecycle({
    read: () => serializeCompositionProject(owner.projectSnapshot()),
    write: async () => null,
    choose: async () => 'cancel',
  });
  await lifecycle.replace({ saved: true });
  assert.throws(
    () => send(owner, 'import-project-compositions', { sourceProject: source }),
    /Invalid or duplicate character unit/,
  );
  assert.deepEqual(source, sourceBefore);
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.state, state);
  assert.deepEqual(owner.historyState, history);
  assert.equal(owner.activeCompositionId, active);
  assert.equal(lifecycle.dirty, false);
  send(owner, 'redo');
  assert.equal(owner.projectSnapshot().compositions.length, 2);
});

test('a Text-local ID never consumes an unrelated global node ID', () => {
  for (const globalOnSource of [false, true]) {
    const destination = project('destination'),
      source = project('source');
    const globalProject = globalOnSource ? source : destination;
    const value = node('Value');
    value.id = 'c1';
    globalProject.compositions[0].layers[0].nodes.push(value);
    const owner = createCompositionSessionOwner(destination);
    const result = send(owner, 'import-project-compositions', { sourceProject: source });
    const imported = owner
      .projectSnapshot()
      .compositions.find((c) => c.id === result.importedCompositionIds[0]);
    const text = imported.layers[0].nodes.find((n) => n.type === 'Text');
    assert.deepEqual(
      text.characterPositions.units.map((unit) => unit.id),
      ['c1', 'c2'],
    );
    if (globalOnSource) assert.ok(imported.layers[0].nodes.some((n) => n.id === 'c1'));
  }
});

test('multiple imported Compositions retain independent matching Text-local IDs', () => {
  const source = project('source-a');
  source.compositions.push(project('source-b').compositions[0]);
  const sourceBefore = structuredClone(source),
    owner = createCompositionSessionOwner(project('destination'));
  const result = send(owner, 'import-project-compositions', { sourceProject: source });
  assert.equal(result.importedCompositionIds.length, 2);
  for (const id of result.importedCompositionIds) {
    const composition = owner.projectSnapshot().compositions.find((c) => c.id === id);
    const text = composition.layers[0].nodes.find((n) => n.type === 'Text');
    assert.deepEqual(
      text.characterPositions,
      sourceBefore.compositions
        .find((c) => c.id === id)
        .layers[0].nodes.find((n) => n.type === 'Text').characterPositions,
    );
  }
  assert.deepEqual(source, sourceBefore);
  send(owner, 'undo');
  assert.equal(owner.projectSnapshot().compositions.length, 1);
  send(owner, 'redo');
  assert.equal(owner.projectSnapshot().compositions.length, 3);
});

for (const collision of [false, true]) {
  test(`real Session Project Import retains Text-local adjustments with${collision ? '' : 'out'} collision`, () => {
    const source = project('source');
    const owner = createCompositionSessionOwner(project('destination', collision));
    const before = owner.projectSnapshot(),
      state = owner.state,
      history = owner.historyState,
      originalSource = structuredClone(source);
    let result;
    try {
      result = owner.dispatch({
        ...owner.state,
        type: 'import-project-compositions',
        payload: { sourceProject: source },
      });
    } catch (error) {
      assert.deepEqual(source, originalSource);
      assert.deepEqual(owner.projectSnapshot(), before);
      assert.deepEqual(owner.state, state);
      assert.deepEqual(owner.historyState, history);
      throw error;
    }
    assert.equal(result.changed, true);
    const imported = owner
      .projectSnapshot()
      .compositions.find((c) => c.id === result.importedCompositionIds[0]);
    assert.deepEqual(
      imported.layers[0].nodes.find((n) => n.type === 'Text').characterPositions,
      source.compositions[0].layers[0].nodes.find((n) => n.type === 'Text').characterPositions,
    );
  });
}
