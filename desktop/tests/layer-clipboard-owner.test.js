import test, { mock } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { blankDocument, blankLayer, evaluate, node } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import { DocumentLifecycle } from '../src/document-lifecycle.js';
import {
  createLayerClipboardOwner,
  resolveLayerClipboardToken,
} from '../src/layer-clipboard-owner.js';
import { parseFont } from '../src/fonts.js';

function fixture(configure = () => {}, options = {}) {
  const origin = migrateLegacyDocumentToCompositionProject(blankDocument());
  origin.compositions[0].id = 'origin';
  origin.compositions[0].name = 'Origin';
  origin.compositions[0].layers[0].id = 'source-layer';
  origin.compositions[0].layers[0].name = 'Original';
  configure(origin);
  const replacement = structuredClone(origin);
  replacement.compositions[0].name = 'Replacement';
  replacement.compositions[0].layers[0].name = 'Foreign';
  const project = structuredClone(origin),
    keeper = migrateLegacyDocumentToCompositionProject(blankDocument()).compositions[0];
  keeper.id = 'keeper';
  project.compositions.push(keeper);
  let owner, documents;
  const clipboard = createLayerClipboardOwner({ currentGeneration: () => documents?.generation });
  const createOwner = (value) =>
    createCompositionSessionOwner(value, {
      ...options,
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
  const token = () =>
    clipboard.token({
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
    token,
    send,
    removeOrigin,
    importReplacement,
    replace() {
      owner = createOwner(replacement);
      documents.replace({ saved: true });
    },
  };
}

test('Layer Copy retains its snapshot through edits and a round trip between Composition tabs', () => {
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
  const result = f.send('paste-layer-snapshot', { token: f.token() });
  assert.equal(
    f.owner.activeDocument().layers.find((item) => item.id === result.layerId).name,
    'Original copy',
  );
  f.send('undo');
  assert.equal(JSON.stringify(f.owner.projectSnapshot()), before);
  assert.equal(f.source(), 'source-layer');
  f.send('redo');
  assert.equal(f.source(), 'source-layer');
});

test('snapshot Paste uses the source Layer’s current Folder and position as its anchor', () => {
  const f = fixture();
  f.copy();
  f.send('rename', { layerId: 'source-layer', name: 'Edited' });
  const { folderId } = f.send('create-layer-folder', {
    name: 'Now here',
    layerIds: ['source-layer'],
  });
  const before = f.owner.projectSnapshot(),
    result = f.send('paste-layer-snapshot', { token: f.token() }),
    document = f.owner.activeDocument(),
    sourceIndex = document.layers.findIndex((layer) => layer.id === 'source-layer');
  assert.equal(document.layers[sourceIndex + 1].id, result.layerId);
  assert.equal(document.layers[sourceIndex + 1].name, 'Original copy');
  assert.deepEqual(document.editor.layerFolders.find((folder) => folder.id === folderId).members, [
    { layerId: 'source-layer' },
    { layerId: result.layerId },
  ]);
  f.send('undo');
  assert.deepEqual(f.owner.projectSnapshot(), before);
});

for (const nested of ['edit', 'copy']) {
  test(`a reentrant ${nested} cannot publish an obsolete Layer snapshot`, () => {
    let f,
      armed = false;
    f = fixture(() => {}, {
      fonts: () => {
        if (armed) {
          armed = false;
          if (nested === 'copy') f.copy();
          else f.send('rename', { layerId: 'source-layer', name: 'Nested edit' });
        }
        return {};
      },
    });
    f.copy();
    const token = f.token();
    armed = true;
    assert.throws(() => f.send('paste-layer-snapshot', { token }), /STALE|unavailable/i);
    assert.equal(f.owner.activeDocument().layers.length, 1);
    if (nested === 'copy') assert.notEqual(f.token(), token);
    else assert.equal(f.owner.activeDocument().layers[0].name, 'Nested edit');
    const result = f.send('paste-layer-snapshot', { token: f.token() });
    assert.equal(
      f.owner.activeDocument().layers.find((layer) => layer.id === result.layerId).name,
      'Original copy',
    );
  });
}

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

test('opaque snapshot tokens cannot inject authored Layers or cross owners', () => {
  const f = fixture(),
    foreign = fixture();
  f.copy();
  const token = f.token(),
    capture = resolveLayerClipboardToken(token, f.owner, 'origin'),
    before = f.owner.projectSnapshot();
  assert.ok(Object.isFrozen(capture.snapshot));
  assert.throws(() => {
    capture.snapshot.name = 'Injected';
  }, TypeError);
  assert.throws(
    () => f.send('paste-layer-snapshot', { token: {}, snapshot: { name: 'Injected' } }),
    /clipboard|unavailable/i,
  );
  assert.throws(() => foreign.send('paste-layer-snapshot', { token }), /clipboard|unavailable/i);
  assert.deepEqual(f.owner.projectSnapshot(), before);
  const result = f.send('paste-layer-snapshot', { token, snapshot: { name: 'Injected' } });
  assert.equal(
    f.owner.activeDocument().layers.find((layer) => layer.id === result.layerId).name,
    'Original copy',
  );
});

test('failed locked Copy and Paste retain the prior snapshot and Redo', () => {
  const f = fixture();
  f.copy();
  const token = f.token();
  f.send('set-layer-state', { layerId: 'source-layer', key: 'locked', value: true });
  f.send('set-document-settings', { key: 'width', value: 1001 });
  f.send('undo');
  const before = f.owner.projectSnapshot(),
    redo = f.owner.historyView.redoToken;
  assert.throws(() => f.copy(), /Unlock/);
  assert.equal(f.token(), token);
  assert.throws(() => f.send('paste-layer-snapshot', { token }), /Unlock/);
  assert.deepEqual(f.owner.projectSnapshot(), before);
  assert.equal(f.owner.historyView.redoToken, redo);
  f.send('undo');
  assert.equal(f.token(), token);
  const result = f.send('paste-layer-snapshot', { token });
  assert.equal(
    f.owner.activeDocument().layers.find((layer) => layer.id === result.layerId).name,
    'Original copy',
  );
});

test('a snapshot cannot cross below its captured schema, and Redo restores eligibility', () => {
  const f = fixture();
  f.send('create-color-slot', { name: 'Temporary', value: { hex: '#112233', alpha: 1 } });
  f.copy();
  const token = f.token();
  f.send('undo');
  const before = f.owner.projectSnapshot(),
    redo = f.owner.historyView.redoToken;
  assert.throws(() => f.send('paste-layer-snapshot', { token }), /schema/i);
  assert.deepEqual(f.owner.projectSnapshot(), before);
  assert.equal(f.owner.historyView.redoToken, redo);
  f.send('redo');
  const result = f.send('paste-layer-snapshot', { token });
  assert.equal(
    f.owner.activeDocument().layers.find((layer) => layer.id === result.layerId).name,
    'Original copy',
  );
});

test('Image snapshot keeps its asset identity and is suspended while its source is absent', () => {
  const f = fixture(),
    png =
      'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP4z8DwHwAFAAH/iZk9HQAAAABJRU5ErkJggg==',
    image = f.send('adopt-image', { name: 'red.png', png }),
    context = {
      owner: f.owner,
      generation: f.documents.generation,
      compositionId: f.owner.activeCompositionId,
    };
  f.clipboard.copy({ ...context, layerId: image.layerId });
  const token = f.clipboard.token(context);
  f.send('rename', { layerId: image.layerId, name: 'Edited image' });
  const pasted = f.send('paste-layer-snapshot', { token }),
    copy = f.owner.activeDocument().layers.find((layer) => layer.id === pasted.layerId);
  assert.equal(copy.nodes.find((item) => item.type === 'Image').asset, image.assetId);
  assert.equal(copy.name, 'red.png copy');
  f.send('undo');
  f.send('undo');
  f.send('undo');
  assert.equal(f.clipboard.token(context), null);
  assert.throws(() => f.send('paste-layer-snapshot', { token }), /unavailable/i);
  f.send('redo');
  assert.equal(f.clipboard.token(context), token);
  const restored = f.send('paste-layer-snapshot', { token });
  assert.equal(
    f.owner
      .activeDocument()
      .layers.find((layer) => layer.id === restored.layerId)
      .nodes.find((item) => item.type === 'Image').asset,
    image.assetId,
  );
});

test('an embedded font snapshot suspends when Undo removes the font and resumes on exact Redo', () => {
  const f = fixture((project) => {
    const text = node('Text');
    text.id = 'source-text';
    text.text = 'A';
    project.compositions[0].layers[0].nodes.push(text);
  });
  const buffer = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url)),
    bytes = buffer.buffer.slice(buffer.byteOffset, buffer.byteOffset + buffer.byteLength),
    adopted = f.send('adopt-font', {
      layerId: 'source-layer',
      nodeId: 'source-text',
      bytes,
      fileName: 'DejaVuSans.ttf',
    });
  f.copy();
  const token = f.token();
  f.send('undo');
  assert.equal(f.token(), null);
  assert.throws(() => f.send('paste-layer-snapshot', { token }), /unavailable/i);
  f.send('redo');
  assert.equal(f.token(), token);
  const result = f.send('paste-layer-snapshot', { token }),
    copy = f.owner.activeDocument().layers.find((layer) => layer.id === result.layerId);
  assert.equal(copy.nodes.find((item) => item.type === 'Text').font, adopted.fontId);
  assert.ok(f.owner.projectSnapshot().fontAssets[adopted.fontId].data);
});

test('a copied Composition Reference suspends on source removal and revives only on exact Undo', () => {
  const f = fixture((project) => {
    project.schemaVersion = 31;
    const target = blankLayer('Referenced source'),
      reference = node('Composition Reference');
    target.id = 'referenced-layer';
    reference.id = 'source-reference';
    reference.compositionReference = { compositionId: target.id };
    project.compositions[0].layers[0].nodes.push(reference);
    project.compositions[0].layers.push(target);
  });
  f.copy();
  const token = f.token();
  f.send('delete-node', { layerId: 'source-layer', nodeId: 'source-reference' });
  const beforeDelete = f.owner.projectSnapshot();
  f.send('delete-layer', { layerId: 'referenced-layer' });
  assert.equal(f.token(), null);
  assert.throws(() => f.send('paste-layer-snapshot', { token }), /unavailable/i);
  f.send('undo');
  assert.deepEqual(f.owner.projectSnapshot(), beforeDelete);
  assert.equal(f.token(), token);
  const result = f.send('paste-layer-snapshot', { token });
  assert.equal(
    f.owner
      .activeDocument()
      .layers.find((layer) => layer.id === result.layerId)
      .nodes.find((item) => item.type === 'Composition Reference').compositionReference
      .compositionId,
    'referenced-layer',
  );
});

test('numeric Artwork references stay live and suspend when the referenced node disappears', () => {
  const f = fixture((project) => {
    project.schemaVersion = 62;
    project.compositionSchemaVersion = 2;
    const target = blankLayer('Driver layer'),
      driver = node('Value'),
      sink = node('Value');
    target.id = 'driver-layer';
    driver.id = 'node-reused';
    driver.params.value.value = 7;
    target.nodes.push(driver);
    sink.id = 'sink-node';
    sink.params.value.expression = 'ref(artwork, "driver-layer", "node-reused", "value")';
    project.compositions[0].layers[0].nodes.push(sink);
    project.compositions[0].layers.push(target);
  });
  f.copy();
  const token = f.token();
  f.send('rename', { layerId: 'source-layer', name: 'Edited' });
  f.send('set-parameter', {
    layerId: 'driver-layer',
    nodeId: 'node-reused',
    key: 'value',
    value: 9,
  });
  const pasted = f.send('paste-layer-snapshot', { token }),
    document = f.owner.activeDocument(),
    copy = document.layers.find((layer) => layer.id === pasted.layerId),
    sink = copy.nodes.find((item) => item.type === 'Value');
  assert.equal(copy.name, 'Original copy');
  assert.equal(
    sink.params.value.expression,
    'ref(artwork, "driver-layer", "node-reused", "value")',
  );
  assert.equal(
    evaluate(copy, {}, document.imageAssets, document).parameterValue(sink.id, 'value'),
    9,
  );
  f.send('undo');
  f.send('delete-node', { layerId: 'source-layer', nodeId: 'sink-node' });
  const beforeDelete = f.owner.projectSnapshot();
  f.send('delete-node', { layerId: 'driver-layer', nodeId: 'node-reused' });
  assert.equal(f.token(), null);
  assert.throws(() => f.send('paste-layer-snapshot', { token }), /unavailable/i);
  const random = mock.method(globalThis.crypto, 'randomUUID', () => 'reused');
  try {
    const replacement = f.send('create-node', { layerId: 'driver-layer', nodeType: 'Value' });
    assert.equal(replacement.nodeId, 'node-reused');
  } finally {
    random.mock.restore();
  }
  assert.equal(f.token(), null, 'a new node with the same ID does not revive the copy');
  f.send('undo');
  f.send('undo');
  assert.deepEqual(f.owner.projectSnapshot(), beforeDelete);
  assert.equal(f.token(), token);
  const restored = f.send('paste-layer-snapshot', { token });
  assert.equal(
    f.owner
      .activeDocument()
      .layers.find((layer) => layer.id === restored.layerId)
      .nodes.find((item) => item.type === 'Value').params.value.expression,
    sink.params.value.expression,
  );
});

test('an unresolved Artwork reference refuses a new Copy without replacing the prior one', () => {
  const f = fixture((project) => {
    project.schemaVersion = 62;
    project.compositionSchemaVersion = 2;
    const other = blankLayer('Safe'),
      sink = node('Value');
    other.id = 'safe-layer';
    sink.params.value.expression = 'ref(artwork, "missing-layer", "missing-node", "value")';
    project.compositions[0].layers[0].nodes.push(sink);
    project.compositions[0].layers.push(other);
  });
  const context = { owner: f.owner, generation: f.documents.generation, compositionId: 'origin' };
  f.clipboard.copy({ ...context, layerId: 'safe-layer' });
  const token = f.clipboard.token(context),
    before = f.owner.projectSnapshot();
  assert.throws(() => f.copy(), /resource|unavailable/i);
  assert.equal(f.clipboard.token(context), token);
  assert.deepEqual(f.owner.projectSnapshot(), before);
});

test('a same-ID change of referenced node type suspends the snapshot until exact Undo', () => {
  const bytes = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url)),
    font = parseFont(bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength)),
    f = fixture(
      (project) => {
        project.schemaVersion = 62;
        project.compositionSchemaVersion = 2;
        const target = blankLayer('Text source'),
          text = node('Text'),
          sink = node('Value');
        target.id = 'text-layer';
        text.id = 'text-node';
        text.text = 'A';
        target.nodes.push(text);
        sink.id = 'sink-node';
        sink.params.value.expression = 'ref(artwork, "text-layer", "text-node", "position", "x")';
        project.compositions[0].layers[0].nodes.push(sink);
        project.compositions[0].layers.push(target);
      },
      { fonts: () => ({ default: font }) },
    );
  f.copy();
  const token = f.token();
  f.send('delete-node', { layerId: 'source-layer', nodeId: 'sink-node' });
  f.send('bake-text-outline', { layerId: 'text-layer', nodeId: 'text-node' });
  assert.equal(
    f.owner
      .activeDocument()
      .layers.find((layer) => layer.id === 'text-layer')
      .nodes.find((item) => item.id === 'text-node').type,
    'Multi Path',
  );
  assert.equal(f.token(), null);
  assert.throws(() => f.send('paste-layer-snapshot', { token }), /unavailable/i);
  f.send('undo');
  assert.equal(f.token(), token);
});

test('a copied Color slot stays live, suspends on removal and only exact Undo restores it', () => {
  const f = fixture((project) => {
    project.schemaVersion = 54;
    project.colorSlots = [{ id: 'slot-a', name: 'Shared', value: { hex: '#112233', alpha: 1 } }];
    const fill = node('Fill');
    fill.id = 'source-fill';
    fill.params.color.colorSlotId = 'slot-a';
    project.compositions[0].layers[0].nodes.push(fill);
  });
  f.copy();
  const token = f.token();
  f.send('set-color-slot', { slotId: 'slot-a', value: { hex: '#aabbcc', alpha: 0.7 } });
  const pasted = f.send('paste-layer-snapshot', { token });
  const copied = f.owner.activeDocument().layers.find((layer) => layer.id === pasted.layerId);
  assert.equal(
    copied.nodes.find((item) => item.id !== 'source-fill' && item.type === 'Fill').params.color
      .colorSlotId,
    'slot-a',
  );
  assert.equal(f.owner.projectSnapshot().colorSlots[0].value.hex, '#aabbcc');
  f.send('undo');
  f.send('unbind-color-slot', { layerId: 'source-layer', nodeId: 'source-fill', key: 'color' });
  const beforeDelete = f.owner.projectSnapshot();
  f.send('delete-color-slot', { slotId: 'slot-a' });
  assert.equal(f.token(), null);
  assert.throws(() => f.send('paste-layer-snapshot', { token }), /unavailable/i);
  f.importReplacement();
  f.owner.switchComposition('origin');
  assert.ok(f.owner.projectSnapshot().colorSlots.some((slot) => slot.id === 'slot-a'));
  assert.equal(f.token(), null, 'a reused slot ID must not revive the copy');
  f.send('undo');
  f.send('undo');
  assert.deepEqual(f.owner.projectSnapshot(), beforeDelete);
  assert.equal(f.token(), token, 'exact Undo restores the original slot identity');
  const restored = f.send('paste-layer-snapshot', { token });
  assert.equal(
    f.owner
      .activeDocument()
      .layers.find((layer) => layer.id === restored.layerId)
      .nodes.find((item) => item.type === 'Fill').params.color.colorSlotId,
    'slot-a',
  );
});
