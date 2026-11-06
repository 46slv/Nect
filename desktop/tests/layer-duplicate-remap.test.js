import test, { mock } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {
  blankDocument,
  blankLayer,
  connect,
  enableRemapPoint,
  evaluate,
  exportSVG,
  node,
  point,
} from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { parseFont } from '../src/fonts.js';
import { duplicateLayerGeneric } from '../src/artwork-duplication.js';
import {
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';

function fixture(type = 'Rectangle', configure = () => {}) {
  const doc = blankDocument(),
    layer = doc.layers[0],
    source = node(type),
    remap = node('Path Remap'),
    fill = node('Fill'),
    output = layer.nodes[0];
  if (type === 'Multi Path') {
    source.points = [point(10, 20), point(100, 20), point(50, 100)];
    source.contours = [
      { id: 'contour-test', closed: true, points: source.points.map((p) => p.id) },
    ];
    source.units = [{ id: 'unit-test', contours: ['contour-test'] }];
  }
  layer.nodes = [source, remap, fill, output];
  connect(layer, source.id, remap.id, 'path');
  connect(layer, remap.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  configure({ doc, layer, source, remap, fill, output });
  const target = evaluate(layer, {}, doc.imageAssets, doc).run(remap.params.path.source).contours[0]
      .points[0],
    owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc)),
    send = (command, payload = {}) => owner.dispatch({ ...owner.state, type: command, payload });
  send('edit-remap', { layerId: layer.id, nodeId: remap.id, action: 'enable', pointId: target.id });
  send('set-parameter', {
    layerId: layer.id,
    nodeId: remap.id,
    key: `remap.${target.id}.position`,
    value: [36, 18],
  });
  return { owner, send, layerId: layer.id, sourceId: source.id, remapId: remap.id, target };
}

const reopenOwner = (owner, mutate) => {
  const project = owner.projectSnapshot();
  mutate(project.compositions.find((composition) => composition.id === owner.activeCompositionId));
  const reopened = createCompositionSessionOwner(project);
  return {
    owner: reopened,
    send: (type, payload = {}) => reopened.dispatch({ ...reopened.state, type, payload }),
  };
};

test('generated Rectangle point edit cannot disappear into a committed duplicate', () => {
  const f = fixture();
  f.send('set-document-settings', { key: 'width', value: 1200 });
  f.send('undo');
  const before = serializeCompositionProject(f.owner.projectSnapshot()),
    state = f.owner.state,
    revision = f.owner.projectRevision,
    history = f.owner.historyView,
    entry = history.entryToken,
    redo = history.redoToken,
    active = f.owner.activeCompositionId;
  assert.throws(() => f.send('duplicate-layer', { layerId: f.layerId }), /Path Remap/i);
  assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), before);
  assert.deepEqual(f.owner.state, state);
  assert.equal(f.owner.projectRevision, revision);
  assert.equal(f.owner.activeCompositionId, active);
  assert.strictEqual(history.entryToken, entry);
  assert.strictEqual(history.redoToken, redo);
  f.send('redo');
  assert.equal(f.owner.activeDocument().width, 1200);
});

for (const type of ['Curve', 'Multi Path']) {
  test(`${type} saved points retain typed edits through duplicate, Undo, Redo and reopen`, () => {
    const f = fixture(type),
      before = serializeCompositionProject(f.owner.projectSnapshot()),
      result = f.send('duplicate-layer', { layerId: f.layerId }),
      document = f.owner.activeDocument(),
      source = document.layers.find((layer) => layer.id === f.layerId),
      copy = document.layers.find((layer) => layer.id === result.layerId),
      copiedRemap = copy.nodes.find((item) => item.type === 'Path Remap'),
      originalPoint = evaluate(source).path.contours[0].points[0],
      copiedPath = evaluate(copy).path,
      copiedPoint = copiedPath.contours[0].points[0];
    assert.notEqual(copiedPoint.id, originalPoint.id);
    assert.equal(copiedRemap.edits[0].pointId, copiedPoint.id);
    assert.deepEqual(copiedPoint.position, originalPoint.position);
    assert.notEqual(copiedPath.remapSuspended, true);
    const svg = (layer) =>
      exportSVG(
        { ...structuredClone(document), layers: [{ ...structuredClone(layer), name: 'same' }] },
        {},
      );
    assert.equal(svg(copy), svg(source));
    const after = serializeCompositionProject(f.owner.projectSnapshot());
    f.send('undo');
    assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), before);
    f.send('redo');
    assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), after);
    const reopened = createCompositionSessionOwner(deserializeCompositionProject(after)),
      reopenedCopy = reopened.activeDocument().layers.find((layer) => layer.id === copy.id),
      reopenedPath = evaluate(reopenedCopy).path;
    assert.deepEqual(reopenedPath.contours[0].points[0].position, originalPoint.position);
    assert.notEqual(reopenedPath.remapSuspended, true);
  });
}

test('a missing target stays missing even when its ID belongs to another copied Curve', () => {
  const original = fixture('Curve'),
    f = reopenOwner(original.owner, (composition) => {
      const layer = composition.layers[0],
        remap = layer.nodes.find((item) => item.id === original.remapId),
        unrelated = node('Curve');
      unrelated.points[0].id = 'unrelated-old-point';
      layer.nodes.push(unrelated);
      remap.edits[0].pointId = unrelated.points[0].id;
    });
  const source = f.owner.activeDocument().layers[0],
    edit = source.nodes.find((item) => item.id === original.remapId).edits[0];
  assert.equal(evaluate(source).path.remapSuspended, true);
  const copyId = f.send('duplicate-layer', { layerId: original.layerId }).layerId,
    copy = f.owner.activeDocument().layers.find((layer) => layer.id === copyId),
    copiedEdit = copy.nodes.find((item) => item.type === 'Path Remap').edits[0];
  assert.equal(copiedEdit.pointId, edit.pointId);
  assert.equal(copiedEdit.fingerprint, edit.fingerprint);
  assert.equal(evaluate(copy).path.remapSuspended, true);
});

test('a missing target cannot accidentally rebind to a generated point in the copy', () => {
  const original = fixture(),
    f = reopenOwner(original.owner, (composition) => {
      composition.layers[0].nodes.find((item) => item.id === original.remapId).edits[0].pointId =
        'copy-new-rectangle:c0:p0';
    }),
    before = serializeCompositionProject(f.owner.projectSnapshot()),
    state = f.owner.state,
    uuids = ['new-layer', 'new-rectangle', 'new-remap', 'new-fill', 'new-output'];
  let next = 0;
  mock.method(globalThis.crypto, 'randomUUID', () => uuids[next++]);
  try {
    assert.throws(
      () => f.send('duplicate-layer', { layerId: original.layerId }),
      /Path Remap.*binding/,
    );
  } finally {
    mock.restoreAll();
  }
  assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), before);
  assert.deepEqual(f.owner.state, state);
});

test('an already stale Multi Path fingerprint remains suspended without implicit rebind', () => {
  const original = fixture('Multi Path'),
    f = reopenOwner(original.owner, (composition) => {
      composition.layers[0].nodes.find(
        (item) => item.id === original.remapId,
      ).edits[0].fingerprint = 'old-fingerprint';
    });
  const source = f.owner.activeDocument().layers[0];
  assert.equal(evaluate(source).path.remapSuspended, true);
  const id = f.send('duplicate-layer', { layerId: original.layerId }).layerId,
    copy = f.owner.activeDocument().layers.find((layer) => layer.id === id),
    edit = copy.nodes.find((item) => item.type === 'Path Remap').edits[0];
  assert.equal(edit.fingerprint, 'old-fingerprint');
  assert.notEqual(edit.pointId, original.target.id);
  assert.equal(evaluate(copy).path.remapSuspended, true);
});

test('chained Remaps keep each independently authored correction', () => {
  const f = fixture('Curve', ({ layer, remap, fill }) => {
    const second = node('Path Remap');
    layer.nodes.push(second);
    connect(layer, remap.id, second.id, 'path');
    connect(layer, second.id, fill.id, 'path');
  });
  const second = f.owner
    .activeDocument()
    .layers[0].nodes.filter((item) => item.type === 'Path Remap')[1];
  f.send('edit-remap', {
    layerId: f.layerId,
    nodeId: second.id,
    action: 'enable',
    pointId: f.target.id,
  });
  f.send('set-parameter', {
    layerId: f.layerId,
    nodeId: second.id,
    key: `remap.${f.target.id}.position`,
    value: [-4, 7],
  });
  const id = f.send('duplicate-layer', { layerId: f.layerId }).layerId,
    doc = f.owner.activeDocument(),
    original = doc.layers.find((layer) => layer.id === f.layerId),
    copy = doc.layers.find((layer) => layer.id === id),
    copiedPoint = copy.nodes.find((item) => item.type === 'Curve').points[0].id;
  assert.deepEqual(
    copy.nodes.filter((item) => item.type === 'Path Remap').map((item) => item.edits[0].pointId),
    [copiedPoint, copiedPoint],
  );
  assert.deepEqual(
    evaluate(copy).path.contours[0].points[0].position,
    evaluate(original).path.contours[0].points[0].position,
  );
  assert.notEqual(evaluate(copy).path.remapSuspended, true);
});

test('a Remap correction expression follows the copied local driver', () => {
  const original = fixture('Curve'),
    f = reopenOwner(original.owner, (composition) => {
      const layer = composition.layers[0],
        driver = node('Vector');
      driver.id = 'remap-driver';
      driver.params.value.value = [9, -6];
      layer.nodes.push(driver);
      layer.nodes.find((item) => item.id === original.remapId).edits[0].params.position.expression =
        'ref("remap-driver", "value", "x"); ref("remap-driver", "value", "y")';
    }),
    id = f.send('duplicate-layer', { layerId: original.layerId }).layerId,
    doc = f.owner.activeDocument(),
    source = doc.layers.find((layer) => layer.id === original.layerId),
    copy = doc.layers.find((layer) => layer.id === id),
    driver = copy.nodes.find((item) => item.type === 'Vector'),
    expression = copy.nodes.find((item) => item.type === 'Path Remap').edits[0].params.position
      .expression;
  assert.ok(expression.includes(`"${driver.id}"`));
  assert.deepEqual(
    evaluate(copy).path.contours[0].points[0].position,
    evaluate(source).path.contours[0].points[0].position,
  );
});

for (const amount of [0, 10]) {
  test(`Closed Path Offset amount ${amount}: use actual output identity`, () => {
    const f = fixture('Curve', ({ layer, source, remap }) => {
      const offset = node('Closed Path Offset');
      offset.params.amount.value = amount;
      layer.nodes.push(offset);
      connect(layer, source.id, offset.id, 'path');
      connect(layer, offset.id, remap.id, 'path');
    });
    if (amount) {
      const before = serializeCompositionProject(f.owner.projectSnapshot());
      assert.throws(() => f.send('duplicate-layer', { layerId: f.layerId }), /Path Remap.*binding/);
      assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), before);
    } else {
      const id = f.send('duplicate-layer', { layerId: f.layerId }).layerId,
        copy = f.owner.activeDocument().layers.find((layer) => layer.id === id);
      assert.notEqual(evaluate(copy).path.remapSuspended, true);
      assert.deepEqual(
        evaluate(copy).path.contours[0].points[0].position,
        evaluate(f.owner.activeDocument().layers[0]).path.contours[0].points[0].position,
      );
    }
  });
}

test('an empty Path Remap permits ordinary Rectangle duplication', () => {
  const f = fixture();
  f.send('undo');
  f.send('undo');
  assert.equal(
    f.owner.activeDocument().layers[0].nodes.find((n) => n.id === f.remapId).edits.length,
    0,
  );
  assert.doesNotThrow(() => f.send('duplicate-layer', { layerId: f.layerId }));
});

test('external Artwork expressions and opaque metadata stay unchanged', () => {
  const f = fixture('Curve', ({ doc, layer, source }) => {
    const other = blankLayer('Other'),
      driver = node('Value'),
      unused = node('Text');
    other.id = 'other-artwork';
    driver.id = 'external-driver';
    driver.params.value.value = 123;
    other.nodes.push(driver);
    doc.layers.push(other);
    doc.schemaVersion = 62;
    source.points[0].position.expression =
      'ref(artwork, "other-artwork", "external-driver", "value"); 20';
    source.metadata = {
      id: source.id,
      point: source.points[0].id,
      nested: { id: 'metadata-only' },
    };
    source.points[0].position.metadata = { id: source.id, source: 'external-driver' };
    // This unrelated node cannot resolve its font, and must not be evaluated.
    layer.nodes.push(unused);
  });
  const before = f.owner.activeDocument(),
    original = before.layers.find((layer) => layer.id === f.layerId),
    expected = original.nodes.find((item) => item.id === f.sourceId),
    result = f.send('duplicate-layer', { layerId: f.layerId }),
    after = f.owner.activeDocument(),
    copy = after.layers.find((layer) => layer.id === result.layerId),
    copiedCurve = copy.nodes.find((item) => item.type === 'Curve');
  assert.deepEqual(
    after.layers.find((layer) => layer.id === f.layerId),
    original,
  );
  assert.deepEqual(copiedCurve.metadata, expected.metadata);
  assert.deepEqual(copiedCurve.points[0].position.metadata, expected.points[0].position.metadata);
  assert.equal(copiedCurve.points[0].position.expression, expected.points[0].position.expression);
  assert.deepEqual(
    evaluate(copy, {}, after.imageAssets, after).path.contours[0].points[0].position,
    evaluate(original, {}, before.imageAssets, before).path.contours[0].points[0].position,
  );
});

test('an edited Text path that needs unavailable runtime fonts is refused without state change', () => {
  const data = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url)),
    buffer = data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength),
    fonts = { default: parseFont(buffer) },
    doc = blankDocument(),
    layer = doc.layers[0],
    text = node('Text'),
    remap = node('Path Remap'),
    fill = node('Fill'),
    output = layer.nodes[0];
  layer.nodes = [text, remap, fill, output];
  connect(layer, text.id, remap.id, 'path');
  connect(layer, remap.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  const input = evaluate(layer, fonts).run(text.id),
    target = input.contours[0].points[0];
  enableRemapPoint(remap, input, target.id).params.position.value = [36, 18];
  const owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc), {
      fonts: () => fonts,
    }),
    before = serializeCompositionProject(owner.projectSnapshot()),
    state = owner.state,
    revision = owner.projectRevision;
  assert.throws(
    () =>
      owner.dispatch({ ...owner.state, type: 'duplicate-layer', payload: { layerId: layer.id } }),
    /Path Remap.*unable to verify its input.*font/i,
  );
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), before);
  assert.deepEqual(owner.state, state);
  assert.equal(owner.projectRevision, revision);
});

for (const type of ['Curve', 'Rectangle']) {
  test(`${type} duplicate guard does not freeze or mutate unrelated image assets`, () => {
    const f = fixture(type),
      doc = structuredClone(f.owner.activeDocument()),
      data =
        'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP4z8DwHwAFAAH/iZk9HQAAAABJRU5ErkJggg==';
    doc.imageAssets = {
      'image-shared': { name: 'red.png', mime: 'image/png', width: 1, height: 1, data },
    };
    const source = doc.layers[0],
      before = structuredClone(source),
      assetTable = doc.imageAssets,
      asset = assetTable['image-shared'];
    if (type === 'Curve') duplicateLayerGeneric(doc, source.id);
    else assert.throws(() => duplicateLayerGeneric(doc, source.id), /Path Remap/);
    assert.strictEqual(doc.layers[0], source);
    assert.deepEqual(source, before);
    assert.strictEqual(doc.imageAssets, assetTable);
    assert.strictEqual(doc.imageAssets['image-shared'], asset);
    assert.equal(Object.isFrozen(asset), false);
    assert.equal(asset.data, data);
  });
}
