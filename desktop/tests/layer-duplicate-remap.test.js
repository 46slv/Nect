import test, { mock } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { Worker } from 'node:worker_threads';
import {
  blankDocument,
  blankLayer,
  connect,
  createVariableDocumentPreparer,
  enableRemapPoint,
  evaluate,
  exportSVG,
  node,
  point,
} from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { createCharacterPositions, setCharacterOffsets } from '../src/character-identity.js';
import { makeFontAsset, makeVariableFontAsset, parseFont, resolveFonts } from '../src/fonts.js';
import { duplicateLayerGeneric } from '../src/artwork-duplication.js';
import { createLayerClipboardOwner } from '../src/layer-clipboard-owner.js';
import { duplicationDocument, richDuplicationText } from './fixtures/artwork-duplication.js';
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

const fontBytes = (url) => {
  const data = fs.readFileSync(new URL(url, import.meta.url));
  return data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength);
};

function textRemapDocument() {
  const doc = blankDocument(),
    layer = doc.layers[0],
    text = node('Text'),
    remap = node('Path Remap'),
    fill = node('Fill'),
    output = layer.nodes[0];
  layer.nodes = [text, remap, fill, output];
  connect(layer, text.id, remap.id, 'path');
  connect(layer, remap.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  return { doc, layer, text, remap };
}

function makeTextEdit(f, fonts, suspended = false) {
  const input = evaluate(f.layer, fonts, f.doc.imageAssets, f.doc).run(f.text.id),
    target = input.contours[0].points[0],
    edit = enableRemapPoint(f.remap, input, target.id);
  edit.params.position.value = [36, 18];
  if (suspended) edit.pointId = 'missing-retained-point';
}

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

  test(`${type} snapshot preserves copy-time Path Remap geometry after the live source changes`, () => {
    const f = fixture(type),
      clipboard = createLayerClipboardOwner(),
      context = { owner: f.owner, generation: 1, compositionId: f.owner.activeCompositionId },
      original = f.owner.snapshot().layers.find((layer) => layer.id === f.layerId),
      expected = evaluate(original).path.contours[0].points[0].position;
    clipboard.copy({ ...context, layerId: f.layerId });
    f.send('set-parameter', {
      layerId: f.layerId,
      nodeId: f.remapId,
      key: `remap.${f.target.id}.position`,
      value: [99, 71],
    });
    const before = serializeCompositionProject(f.owner.projectSnapshot()),
      result = f.send('paste-layer-snapshot', { token: clipboard.token(context) }),
      document = f.owner.activeDocument(),
      live = document.layers.find((layer) => layer.id === f.layerId),
      copy = document.layers.find((layer) => layer.id === result.layerId),
      path = evaluate(copy).path;
    assert.deepEqual(path.contours[0].points[0].position, expected);
    assert.notDeepEqual(evaluate(live).path.contours[0].points[0].position, expected);
    assert.notEqual(path.remapSuspended, true);
    const saved = serializeCompositionProject(f.owner.projectSnapshot());
    f.send('undo');
    assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), before);
    f.send('redo');
    assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), saved);
    const reopened = createCompositionSessionOwner(deserializeCompositionProject(saved));
    assert.deepEqual(
      evaluate(reopened.activeDocument().layers.find((layer) => layer.id === copy.id)).path
        .contours[0].points[0].position,
      expected,
    );
  });
}

test('Text path snapshot checks the old contour against its independent copied contour', () => {
  const fonts = { default: parseFont(fontBytes('../public/fonts/DejaVuSans.ttf')) },
    doc = blankDocument(),
    layer = doc.layers[0],
    path = node('Circle'),
    text = node('Text'),
    fill = node('Fill'),
    output = layer.nodes[0];
  text.text = 'Snapshot';
  text.params.size.value = 10;
  layer.nodes = [path, text, fill, output];
  text.pathAttachment = {
    nodeId: path.id,
    contourId: evaluate(layer).run(path.id).contours[0].id,
    startMode: 'distance',
    start: 0,
    reversed: false,
  };
  connect(layer, text.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  const owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc), {
      fonts: () => fonts,
    }),
    send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload }),
    clipboard = createLayerClipboardOwner(),
    context = { owner, generation: 1, compositionId: owner.activeCompositionId },
    oldRadius = path.params.radius.value;
  clipboard.copy({ ...context, layerId: layer.id });
  send('set-parameter', {
    layerId: layer.id,
    nodeId: path.id,
    key: 'radius',
    value: oldRadius + 70,
  });
  const before = serializeCompositionProject(owner.projectSnapshot()),
    result = send('paste-layer-snapshot', { token: clipboard.token(context) }),
    after = owner.activeDocument(),
    copy = after.layers.find((item) => item.id === result.layerId),
    copiedPath = copy.nodes.find((item) => item.type === 'Circle'),
    copiedText = copy.nodes.find((item) => item.type === 'Text');
  assert.equal(copiedPath.params.radius.value, oldRadius);
  assert.equal(copiedText.pathAttachment.nodeId, copiedPath.id);
  assert.equal(
    copiedText.pathAttachment.contourId,
    evaluate(copy, fonts).run(copiedPath.id).contours[0].id,
  );
  const saved = serializeCompositionProject(owner.projectSnapshot());
  send('undo');
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), before);
  send('redo');
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), saved);
});

for (const mode of ['Text path', 'Path Remap']) {
  test(`${mode} snapshot resolves a foreign reference back to the current source Layer`, () => {
    const fonts = { default: parseFont(fontBytes('../public/fonts/DejaVuSans.ttf')) },
      doc = blankDocument(),
      source = doc.layers[0],
      foreign = blankLayer('External'),
      localValue = node('Value'),
      foreignValue = node('Value'),
      path = node(mode === 'Text path' ? 'Circle' : 'Curve'),
      fill = node('Fill'),
      output = source.nodes[0];
    doc.schemaVersion = 62;
    source.id = 'artwork-a';
    foreign.id = 'artwork-b';
    localValue.id = 'value-a';
    foreignValue.id = 'value-b';
    localValue.params.value.value = 60;
    foreignValue.params.value.expression = 'ref(artwork, "artwork-a", "value-a", "value")';
    foreign.nodes.push(foreignValue);
    doc.layers.push(foreign);
    source.nodes = [localValue, path, fill, output];
    let text, remap;
    if (mode === 'Text path') {
      path.params.radius.expression = 'ref(artwork, "artwork-b", "value-b", "value")';
      text = node('Text');
      text.text = 'Snapshot';
      text.params.size.value = 10;
      source.nodes.splice(2, 0, text);
      text.pathAttachment = {
        nodeId: path.id,
        contourId: evaluate(source, fonts, undefined, doc).run(path.id).contours[0].id,
        startMode: 'distance',
        start: 0,
        reversed: false,
      };
      connect(source, text.id, fill.id, 'path');
    } else {
      path.points[0].position.expression = 'ref(artwork, "artwork-b", "value-b", "value"); 150';
      remap = node('Path Remap');
      source.nodes.splice(2, 0, remap);
      connect(source, path.id, remap.id, 'path');
      connect(source, remap.id, fill.id, 'path');
      const input = evaluate(source, fonts, undefined, doc).run(path.id);
      enableRemapPoint(remap, input, path.points[0].id).params.position.value = [8, 3];
    }
    connect(source, fill.id, output.id, 'path');
    const owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc), {
        fonts: () => fonts,
      }),
      send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload }),
      clipboard = createLayerClipboardOwner(),
      context = { owner, generation: 1, compositionId: owner.activeCompositionId };
    clipboard.copy({ ...context, layerId: source.id });
    send('set-parameter', { layerId: source.id, nodeId: localValue.id, key: 'value', value: 120 });
    const before = serializeCompositionProject(owner.projectSnapshot());
    const result = send('paste-layer-snapshot', { token: clipboard.token(context) });
    const current = owner.activeDocument(),
      copy = current.layers.find((layer) => layer.id === result.layerId),
      copiedValue = copy.nodes.find((item) => item.type === 'Value'),
      copiedPath = copy.nodes.find((item) => item.type === path.type),
      evaluation = evaluate(copy, fonts, current.imageAssets, current);
    assert.equal(copiedValue.params.value.value, 60, 'local authored value stays at copy time');
    if (mode === 'Text path') {
      assert.equal(evaluation.parameterValue(copiedPath.id, 'radius'), 120);
      assert.equal(
        copy.nodes.find((item) => item.type === 'Text').pathAttachment.nodeId,
        copiedPath.id,
      );
      assert.equal(evaluation.error, null);
    } else {
      assert.notEqual(evaluation.path.remapSuspended, true);
      assert.deepEqual(evaluation.path.contours[0].points[0].position, [128, 153]);
    }
    const after = serializeCompositionProject(owner.projectSnapshot());
    send('undo');
    assert.equal(serializeCompositionProject(owner.projectSnapshot()), before);
    send('redo');
    assert.equal(serializeCompositionProject(owner.projectSnapshot()), after);
  });
}

test('rich Text snapshot retains local character offsets, styles, transforms and kerning', () => {
  const f = duplicationDocument('Text');
  richDuplicationText(f.generator);
  const fonts = { default: parseFont(fontBytes('../public/fonts/DejaVuSans.ttf')) },
    owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(f.doc), {
      fonts: () => fonts,
    }),
    send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload }),
    clipboard = createLayerClipboardOwner(),
    context = { owner, generation: 1, compositionId: owner.activeCompositionId },
    beforeText = structuredClone(f.generator);
  clipboard.copy({ ...context, layerId: f.layer.id });
  send('set-parameter', { layerId: f.layer.id, nodeId: f.generator.id, key: 'size', value: 64 });
  const result = send('paste-layer-snapshot', { token: clipboard.token(context) }),
    copied = owner
      .activeDocument()
      .layers.find((layer) => layer.id === result.layerId)
      .nodes.find((item) => item.type === 'Text');
  assert.equal(copied.params.size.value, beforeText.params.size.value);
  assert.deepEqual(copied.characterPositions, beforeText.characterPositions);
  assert.deepEqual(copied.textWrap, beforeText.textWrap);
  assert.deepEqual(copied.leading, beforeText.leading);
  const saved = serializeCompositionProject(owner.projectSnapshot());
  const reopened = createCompositionSessionOwner(deserializeCompositionProject(saved), {
    fonts: () => fonts,
  });
  assert.deepEqual(
    reopened
      .activeDocument()
      .layers.find((layer) => layer.id === result.layerId)
      .nodes.find((item) => item.type === 'Text').characterPositions,
    beforeText.characterPositions,
  );
});

for (const route of [['Convert to Path'], ['Fill', 'Convert to Path'], ['Text to Path']]) {
  test(`authored Curve point survives ${route.join(' → ')} before Remap`, () => {
    const f = fixture('Curve', ({ layer, source, remap }) => {
      let current = source;
      for (const type of route) {
        const next = node(type);
        layer.nodes.push(next);
        connect(layer, current.id, next.id, 'path');
        current = next;
      }
      connect(layer, current.id, remap.id, 'path');
    });
    const before = serializeCompositionProject(f.owner.projectSnapshot()),
      result = f.send('duplicate-layer', { layerId: f.layerId }),
      doc = f.owner.activeDocument(),
      original = doc.layers.find((layer) => layer.id === f.layerId),
      copy = doc.layers.find((layer) => layer.id === result.layerId),
      path = evaluate(copy).path,
      sourcePoint = evaluate(original).path.contours[0].points[0],
      copiedPoint = path.contours[0].points[0];
    assert.equal(copy.nodes.find((n) => n.type === 'Path Remap').edits[0].pointId, copiedPoint.id);
    assert.deepEqual(copiedPoint.position, sourcePoint.position);
    assert.notEqual(path.remapSuspended, true);
    const after = serializeCompositionProject(f.owner.projectSnapshot());
    f.send('undo');
    assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), before);
    f.send('redo');
    assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), after);
    const reopened = createCompositionSessionOwner(deserializeCompositionProject(after));
    assert.deepEqual(
      evaluate(reopened.activeDocument().layers[1]).path.contours[0].points[0].position,
      sourcePoint.position,
    );
  });
}

for (const route of [
  'bypassed painted Transform',
  'Polar Transform',
  'zero-gap Dash Path',
  'zero-radius Corner Round',
]) {
  test(`${route} retains authored Remap points through duplicate and history`, () => {
    const f = fixture('Curve', ({ doc, layer, source, remap }) => {
      if (route === 'bypassed painted Transform') {
        doc.schemaVersion = 31;
        const paint = node('Fill'),
          transform = node('Transform'),
          convert = node('Convert to Path');
        layer.nodes.push(paint, transform, convert);
        connect(layer, source.id, paint.id, 'path');
        connect(layer, paint.id, transform.id, 'path');
        transform.bypassed = true;
        connect(layer, transform.id, convert.id, 'path');
        connect(layer, convert.id, remap.id, 'path');
        assert.notEqual(evaluate(layer).run(transform.id).kind, 'painted-group');
      } else {
        const kind =
            route === 'Polar Transform'
              ? route
              : route === 'zero-gap Dash Path'
                ? 'Dash Path'
                : 'Corner Round',
          effect = node(kind);
        layer.nodes.push(effect);
        connect(layer, source.id, effect.id, 'path');
        if (kind !== 'Polar Transform') {
          const driver = node('Value');
          driver.params.value.value = 0;
          layer.nodes.push(driver);
          connect(layer, driver.id, effect.id, kind === 'Dash Path' ? 'gapLength' : 'radius');
        }
        connect(layer, effect.id, remap.id, 'path');
      }
    });
    const before = serializeCompositionProject(f.owner.projectSnapshot()),
      result = f.send('duplicate-layer', { layerId: f.layerId }),
      doc = f.owner.activeDocument(),
      original = doc.layers.find((layer) => layer.id === f.layerId),
      copy = doc.layers.find((layer) => layer.id === result.layerId),
      originalPath = evaluate(original).path,
      copiedPath = evaluate(copy).path;
    assert.notEqual(copiedPath.remapSuspended, true);
    assert.equal(
      copy.nodes.find((item) => item.type === 'Path Remap').edits[0].pointId,
      copy.nodes.find((item) => item.type === 'Curve').points[0].id,
    );
    assert.deepEqual(
      copiedPath.contours[0].points[0].position,
      originalPath.contours[0].points[0].position,
    );
    if (route === 'zero-gap Dash Path' || route === 'zero-radius Corner Round') {
      const effect = copy.nodes.find(
          (item) => item.type === (route === 'zero-gap Dash Path' ? 'Dash Path' : 'Corner Round'),
        ),
        driver = copy.nodes.find((item) => item.type === 'Value'),
        key = route === 'zero-gap Dash Path' ? 'gapLength' : 'radius';
      assert.equal(effect.params[key].source, driver.id);
      assert.notEqual(
        effect.params[key].value,
        0,
        'the effective zero comes from the copied driver',
      );
    }
    const after = serializeCompositionProject(f.owner.projectSnapshot());
    f.send('undo');
    assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), before);
    f.send('redo');
    assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), after);
    const reopened = createCompositionSessionOwner(deserializeCompositionProject(after));
    assert.deepEqual(
      evaluate(reopened.activeDocument().layers[1]).path.contours[0].points[0].position,
      originalPath.contours[0].points[0].position,
    );
  });
}

for (const kind of ['Dash Path', 'Corner Round']) {
  test(`${kind} nonzero generated points still refuse without history changes`, () => {
    const f = fixture('Curve', ({ layer, source, remap }) => {
      const effect = node(kind),
        driver = node('Value'),
        key = kind === 'Dash Path' ? 'gapLength' : 'radius';
      effect.params[key].value = 0;
      driver.params.value.value = 5;
      layer.nodes.push(effect, driver);
      connect(layer, source.id, effect.id, 'path');
      connect(layer, driver.id, effect.id, key);
      connect(layer, effect.id, remap.id, 'path');
    });
    f.send('set-document-settings', { key: 'width', value: 1200 });
    f.send('undo');
    const before = serializeCompositionProject(f.owner.projectSnapshot()),
      state = f.owner.state,
      redo = f.owner.historyView.redoToken;
    assert.throws(() => f.send('duplicate-layer', { layerId: f.layerId }), /Path Remap.*binding/);
    assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), before);
    assert.deepEqual(f.owner.state, state);
    assert.strictEqual(f.owner.historyView.redoToken, redo);
  });
}

for (const kind of ['Polar Transform', 'Dash Path', 'Corner Round']) {
  test(`${kind} keeps an already stale fingerprint suspended`, () => {
    const original = fixture('Curve', ({ layer, source, remap }) => {
      const effect = node(kind);
      if (kind === 'Dash Path') effect.params.gapLength.value = 0;
      if (kind === 'Corner Round') effect.params.radius.value = 0;
      layer.nodes.push(effect);
      connect(layer, source.id, effect.id, 'path');
      connect(layer, effect.id, remap.id, 'path');
    });
    const f = reopenOwner(original.owner, (composition) => {
      composition.layers[0].nodes.find(
        (item) => item.id === original.remapId,
      ).edits[0].fingerprint = 'old-fingerprint';
    });
    assert.equal(evaluate(f.owner.activeDocument().layers[0]).path.remapSuspended, true);
    const result = f.send('duplicate-layer', { layerId: original.layerId }),
      copy = f.owner.activeDocument().layers.find((layer) => layer.id === result.layerId),
      edit = copy.nodes.find((item) => item.type === 'Path Remap').edits[0];
    assert.equal(edit.fingerprint, 'old-fingerprint');
    assert.equal(evaluate(copy).path.remapSuspended, true);
  });
}

test('bypassed Mask still generates a painted group and refuses a Remap copy', () => {
  const f = fixture('Curve', ({ doc, layer, source, remap }) => {
    doc.schemaVersion = 35;
    const paint = node('Fill'),
      mask = node('Mask'),
      convert = node('Convert to Path');
    layer.nodes.push(paint, mask, convert);
    connect(layer, source.id, paint.id, 'path');
    connect(layer, paint.id, mask.id, 'path');
    mask.bypassed = true;
    connect(layer, mask.id, convert.id, 'path');
    connect(layer, convert.id, remap.id, 'path');
    assert.equal(evaluate(layer).run(mask.id).kind, 'painted-group');
  });
  const before = serializeCompositionProject(f.owner.projectSnapshot());
  assert.throws(() => f.send('duplicate-layer', { layerId: f.layerId }), /Path Remap.*binding/);
  assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), before);
});

test('a painted-group conversion does not infer a generated point mapping', () => {
  const f = fixture('Curve', ({ doc, layer, source, remap }) => {
    doc.schemaVersion = 31;
    const paint = node('Fill'),
      transform = node('Transform'),
      convert = node('Convert to Path');
    layer.nodes.push(paint, transform, convert);
    connect(layer, source.id, paint.id, 'path');
    connect(layer, paint.id, transform.id, 'path');
    connect(layer, transform.id, convert.id, 'path');
    connect(layer, convert.id, remap.id, 'path');
    assert.equal(evaluate(layer).run(transform.id).kind, 'painted-group');
  });
  const before = serializeCompositionProject(f.owner.projectSnapshot()),
    state = f.owner.state;
  assert.throws(() => f.send('duplicate-layer', { layerId: f.layerId }), /Path Remap.*binding/);
  assert.equal(serializeCompositionProject(f.owner.projectSnapshot()), before);
  assert.deepEqual(f.owner.state, state);
});

for (const type of ['Curve', 'Multi Path', 'Circle', 'Polygon', 'Star']) {
  test(`a Text path attachment remaps the owned ${type} contour, preserving its layout`, () => {
    const fonts = { default: parseFont(fontBytes('../public/fonts/DejaVuSans.ttf')) },
      doc = blankDocument(),
      layer = doc.layers[0],
      path = node(type),
      text = node('Text'),
      fill = node('Fill'),
      output = layer.nodes[0];
    if (type === 'Curve') path.params.closed.value = false;
    if (type === 'Multi Path') {
      path.points = [point(10, 20), point(200, 20), point(200, 200)];
      path.contours = [{ id: 'path-contour', closed: false, points: path.points.map((p) => p.id) }];
      path.units = [{ id: 'path-unit', contours: ['path-contour'] }];
    }
    text.text = 'A';
    text.params.size.value = 10;
    layer.nodes = [path, text, fill, output];
    text.pathAttachment = {
      nodeId: path.id,
      contourId: evaluate(layer).run(path.id).contours[0].id,
      startMode: 'distance',
      start: 0,
      reversed: false,
    };
    connect(layer, text.id, fill.id, 'path');
    connect(layer, fill.id, output.id, 'path');
    const owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc), {
        fonts: () => fonts,
      }),
      send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload }),
      before = serializeCompositionProject(owner.projectSnapshot()),
      result = send('duplicate-layer', { layerId: layer.id }),
      after = owner.activeDocument(),
      copy = after.layers.find((item) => item.id === result.layerId),
      copiedPath = copy.nodes.find((item) => item.type === type),
      copiedText = copy.nodes.find((item) => item.type === 'Text');
    assert.equal(copiedText.pathAttachment.nodeId, copiedPath.id);
    assert.equal(
      copiedText.pathAttachment.contourId,
      evaluate(copy).run(copiedPath.id).contours[0].id,
    );
    const svg = (item) =>
      exportSVG(
        { ...structuredClone(after), layers: [{ ...structuredClone(item), name: 'same' }] },
        fonts,
      );
    if (type === 'Multi Path') {
      assert.match(evaluate(after.layers[0], fonts).error, /TEXT_PATH_UNSTABLE/);
      assert.equal(evaluate(copy, fonts).error, evaluate(after.layers[0], fonts).error);
    } else assert.equal(svg(copy), svg(after.layers[0]));
    const saved = serializeCompositionProject(owner.projectSnapshot());
    send('undo');
    assert.equal(serializeCompositionProject(owner.projectSnapshot()), before);
    send('redo');
    assert.equal(serializeCompositionProject(owner.projectSnapshot()), saved);
  });
}

for (const route of [
  'driven zero Offset',
  'plain Fill and Convert',
  'bypassed painted Transform',
]) {
  test(`Text attachment retains its contour through ${route}`, () => {
    const fonts = { default: parseFont(fontBytes('../public/fonts/DejaVuSans.ttf')) },
      doc = blankDocument(),
      layer = doc.layers[0],
      curve = node('Curve'),
      text = node('Text'),
      fill = node('Fill'),
      output = layer.nodes[0];
    curve.params.closed.value = false;
    text.text = 'A';
    text.params.size.value = 10;
    layer.nodes = [curve, text, fill, output];
    let target;
    if (route === 'driven zero Offset') {
      const driver = node('Value'),
        offset = node('Closed Path Offset');
      driver.params.value.value = 0;
      layer.nodes.push(driver, offset);
      connect(layer, curve.id, offset.id, 'path');
      connect(layer, driver.id, offset.id, 'amount');
      target = offset;
    } else {
      const paint = node('Fill'),
        convert = node('Convert to Path');
      layer.nodes.push(paint, convert);
      connect(layer, curve.id, paint.id, 'path');
      let input = paint;
      if (route === 'bypassed painted Transform') {
        doc.schemaVersion = 31;
        const transform = node('Transform');
        layer.nodes.push(transform);
        connect(layer, paint.id, transform.id, 'path');
        transform.bypassed = true;
        input = transform;
      }
      connect(layer, input.id, convert.id, 'path');
      target = convert;
    }
    text.pathAttachment = {
      nodeId: target.id,
      contourId: curve.id,
      startMode: 'distance',
      start: 0,
      reversed: false,
    };
    connect(layer, text.id, fill.id, 'path');
    connect(layer, fill.id, output.id, 'path');
    const owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc), {
        fonts: () => fonts,
      }),
      send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload }),
      before = serializeCompositionProject(owner.projectSnapshot()),
      result = send('duplicate-layer', { layerId: layer.id }),
      after = owner.activeDocument(),
      copy = after.layers.find((item) => item.id === result.layerId),
      copiedCurve = copy.nodes.find((item) => item.type === 'Curve'),
      copiedText = copy.nodes.find((item) => item.type === 'Text');
    assert.equal(copiedText.pathAttachment.contourId, copiedCurve.id);
    const svg = (item) =>
      exportSVG(
        { ...structuredClone(after), layers: [{ ...structuredClone(item), name: 'same' }] },
        fonts,
      );
    assert.equal(svg(copy), svg(after.layers[0]));
    const saved = serializeCompositionProject(owner.projectSnapshot());
    send('undo');
    assert.equal(serializeCompositionProject(owner.projectSnapshot()), before);
    send('redo');
    assert.equal(serializeCompositionProject(owner.projectSnapshot()), saved);
  });
}

test('Text attachment through an edited Path Remap retains both the contour and correction', () => {
  const fonts = { default: parseFont(fontBytes('../public/fonts/DejaVuSans.ttf')) },
    doc = blankDocument(),
    layer = doc.layers[0],
    curve = node('Curve'),
    remap = node('Path Remap'),
    text = node('Text'),
    fill = node('Fill'),
    output = layer.nodes[0];
  curve.params.closed.value = false;
  text.text = 'A';
  text.params.size.value = 10;
  layer.nodes = [curve, remap, text, fill, output];
  connect(layer, curve.id, remap.id, 'path');
  connect(layer, text.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  const input = evaluate(layer).run(curve.id);
  enableRemapPoint(remap, input, curve.points[0].id).params.position.value = [8, 3];
  text.pathAttachment = {
    nodeId: remap.id,
    contourId: curve.id,
    startMode: 'distance',
    start: 0,
    reversed: false,
  };
  const owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc), {
      fonts: () => fonts,
    }),
    result = owner.dispatch({
      ...owner.state,
      type: 'duplicate-layer',
      payload: { layerId: layer.id },
    }),
    after = owner.activeDocument(),
    copy = after.layers.find((item) => item.id === result.layerId),
    copiedCurve = copy.nodes.find((item) => item.type === 'Curve'),
    copiedRemap = copy.nodes.find((item) => item.type === 'Path Remap'),
    copiedText = copy.nodes.find((item) => item.type === 'Text');
  assert.equal(copiedText.pathAttachment.contourId, copiedCurve.id);
  assert.equal(copiedRemap.edits[0].pointId, copiedCurve.points[0].id);
  const svg = (item) =>
    exportSVG(
      { ...structuredClone(after), layers: [{ ...structuredClone(item), name: 'same' }] },
      fonts,
    );
  assert.equal(svg(copy), svg(after.layers[0]));
});

test('a dangling Reference is not revived by fresh copied IDs', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    driver = node('Vector'),
    reference = node('Reference');
  reference.reference = { nodeId: 'copy-future-dangling', portId: 'output' };
  layer.nodes.push(driver, reference);
  const owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc)),
    uuids = ['new-layer', 'new-output', 'future-dangling', 'new-driver', 'new-reference'];
  let next = 0;
  mock.method(globalThis.crypto, 'randomUUID', () => uuids[next++]);
  try {
    const result = owner.dispatch({
        ...owner.state,
        type: 'duplicate-layer',
        payload: { layerId: layer.id },
      }),
      copy = owner.activeDocument().layers.find((item) => item.id === result.layerId),
      target = copy.nodes.find((item) => item.type === 'Reference').reference.nodeId;
    assert.equal(target, 'copy-future-dangling');
    assert.ok(copy.nodes.every((item) => item.id !== target));
  } finally {
    mock.restoreAll();
  }
});

test('snapshot-only dangling Reference IDs cannot be rebound by a newly generated copy ID', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    reference = node('Reference');
  reference.reference = { nodeId: 'copy-future-dangling', portId: 'output' };
  layer.nodes.push(reference);
  const owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc)),
    send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload }),
    clipboard = createLayerClipboardOwner(),
    context = { owner, generation: 1, compositionId: owner.activeCompositionId };
  clipboard.copy({ ...context, layerId: layer.id });
  send('delete-node', { layerId: layer.id, nodeId: reference.id });
  let next = 0;
  const uuids = ['future-dangling', 'fresh-layer', 'fresh-output', 'fresh-reference'];
  const random = mock.method(globalThis.crypto, 'randomUUID', () => uuids[next++]);
  try {
    const result = send('paste-layer-snapshot', { token: clipboard.token(context) }),
      copy = owner.activeDocument().layers.find((item) => item.id === result.layerId),
      copiedReference = copy.nodes.find((item) => item.type === 'Reference');
    assert.equal(copiedReference.reference.nodeId, 'copy-future-dangling');
    assert.ok(copy.nodes.every((item) => item.id !== 'copy-future-dangling'));
    assert.notEqual(copy.id, 'copy-future-dangling');
  } finally {
    random.mock.restore();
  }
});

test('fresh layer identities avoid existing shared Color slot IDs', () => {
  const doc = blankDocument();
  doc.schemaVersion = 54;
  doc.colorSlots = [{ id: 'copy-color-slot', name: 'Shared', value: { hex: '#ff0000', alpha: 1 } }];
  const uuids = ['color-slot', 'new-layer', 'new-output'];
  let next = 0;
  mock.method(globalThis.crypto, 'randomUUID', () => uuids[next++]);
  try {
    const result = duplicateLayerGeneric(doc, doc.layers[0].id);
    assert.equal(result.layerId, 'copy-new-layer');
    assert.equal(doc.colorSlots[0].id, 'copy-color-slot');
  } finally {
    mock.restoreAll();
  }
});

test('Multi Path contour IDs are remapped in their own scope, including group members', () => {
  const f = fixture('Multi Path', ({ doc, layer, source }) => {
    const marker = node('Vector');
    marker.id = 'scoped-collision';
    source.contours[0].id = marker.id;
    source.units[0].contours = [marker.id];
    layer.nodes.push(marker);
    doc.schemaVersion = 44;
    layer.nodeGroups = [{ id: 'test-group', name: marker.id, nodeIds: [source.id, marker.id] }];
  });
  const result = f.send('duplicate-layer', { layerId: f.layerId }),
    copy = f.owner.activeDocument().layers.find((layer) => layer.id === result.layerId),
    multi = copy.nodes.find((item) => item.type === 'Multi Path'),
    marker = copy.nodes.find((item) => item.type === 'Vector'),
    group = copy.nodeGroups[0];
  assert.notEqual(multi.contours[0].id, marker.id);
  assert.deepEqual(multi.units[0].contours, [multi.contours[0].id]);
  assert.deepEqual(
    multi.contours[0].points,
    multi.points.map((item) => item.id),
  );
  assert.deepEqual(group.nodeIds, [multi.id, marker.id]);
  assert.equal(group.name, 'scoped-collision');
  assert.notEqual(evaluate(copy).path.remapSuspended, true);
});

test('generic layer duplication preserves content, names and opaque strings equal to node IDs', () => {
  const fonts = { default: parseFont(fontBytes('../public/fonts/DejaVuSans.ttf')) },
    doc = blankDocument(),
    layer = doc.layers[0],
    text = node('Text'),
    vector = node('Vector'),
    reference = node('Reference'),
    fill = node('Fill'),
    output = layer.nodes[0];
  vector.id = 'c1';
  vector.params.value.value = [200, 200];
  text.text = vector.id;
  text.name = vector.id;
  text.characterPositions = setCharacterOffsets(text.text, createCharacterPositions(text.text), {
    c1: [3, 1],
  });
  text.extra = { id: vector.id, source: vector.id, nested: [vector.id] };
  text.contours = { id: vector.id, nested: { source: vector.id } };
  text.params.size.extra = { id: vector.id, source: vector.id };
  reference.reference = { nodeId: vector.id, portId: 'output' };
  layer.nodes = [text, vector, reference, fill, output];
  connect(layer, vector.id, text.id, 'position');
  connect(layer, text.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  const owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc), {
      fonts: () => fonts,
    }),
    send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload }),
    before = serializeCompositionProject(owner.projectSnapshot()),
    result = send('duplicate-layer', { layerId: layer.id }),
    afterDoc = owner.activeDocument(),
    original = afterDoc.layers.find((item) => item.id === layer.id),
    copy = afterDoc.layers.find((item) => item.id === result.layerId),
    copiedText = copy.nodes.find((item) => item.type === 'Text'),
    copiedVector = copy.nodes.find((item) => item.type === 'Vector'),
    copiedReference = copy.nodes.find((item) => item.type === 'Reference');
  assert.equal(copiedText.text, vector.id);
  assert.equal(copiedText.name, vector.id);
  assert.deepEqual(copiedText.extra, text.extra);
  assert.deepEqual(copiedText.contours, text.contours);
  assert.deepEqual(copiedText.params.size.extra, text.params.size.extra);
  assert.deepEqual(copiedText.characterPositions, text.characterPositions);
  assert.equal(copiedText.params.position.source, copiedVector.id);
  assert.equal(copiedReference.reference.nodeId, copiedVector.id);
  const svg = (item) =>
    exportSVG(
      { ...structuredClone(afterDoc), layers: [{ ...structuredClone(item), name: 'same' }] },
      fonts,
    );
  assert.equal(svg(copy), svg(original));
  const after = serializeCompositionProject(owner.projectSnapshot());
  send('undo');
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), before);
  send('redo');
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), after);
});

test('an Image asset identifier equal to a copied node ID stays external', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    image = node('Image'),
    vector = node('Vector'),
    output = node('Image Output'),
    data =
      'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP4z8DwHwAFAAH/iZk9HQAAAABJRU5ErkJggg==';
  vector.id = 'image-shared';
  image.asset = 'image-shared';
  doc.imageAssets = {
    'image-shared': { name: 'red.png', mime: 'image/png', width: 1, height: 1, data },
  };
  layer.nodes = [vector, image, output];
  layer.output = output.id;
  delete layer.allowEmptyOutput;
  connect(layer, image.id, output.id, 'path');
  const owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc)),
    result = owner.dispatch({
      ...owner.state,
      type: 'duplicate-layer',
      payload: { layerId: layer.id },
    }),
    copy = owner.activeDocument().layers.find((item) => item.id === result.layerId);
  assert.equal(copy.nodes.find((item) => item.type === 'Image').asset, 'image-shared');
  assert.equal(evaluate(copy, {}, doc.imageAssets).error, null);
});

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
  const fonts = { default: parseFont(fontBytes('../public/fonts/DejaVuSans.ttf')) },
    f = textRemapDocument();
  makeTextEdit(f, fonts);
  const owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(f.doc)),
    before = serializeCompositionProject(owner.projectSnapshot()),
    state = owner.state,
    revision = owner.projectRevision;
  assert.throws(
    () =>
      owner.dispatch({ ...owner.state, type: 'duplicate-layer', payload: { layerId: f.layer.id } }),
    /Path Remap.*unable to verify its input.*font/i,
  );
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), before);
  assert.deepEqual(owner.state, state);
  assert.equal(owner.projectRevision, revision);
});

test('a Session-provided static font verifies both suspended inputs and allows the copy', () => {
  const fonts = { default: parseFont(fontBytes('../public/fonts/DejaVuSans.ttf')) },
    f = textRemapDocument();
  makeTextEdit(f, fonts, true);
  const direct = structuredClone(f.doc);
  assert.throws(
    () => duplicateLayerGeneric(direct, f.layer.id),
    /unable to verify its input.*font/i,
  );
  assert.equal(direct.layers.length, 1);
  const directCopy = duplicateLayerGeneric(direct, f.layer.id, { fonts });
  assert.equal(
    evaluate(
      direct.layers.find((layer) => layer.id === directCopy.layerId),
      fonts,
    ).path.remapSuspended,
    true,
  );
  const owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(f.doc), {
      fonts: () => fonts,
    }),
    send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload }),
    before = serializeCompositionProject(owner.projectSnapshot()),
    result = send('duplicate-layer', { layerId: f.layer.id }),
    after = serializeCompositionProject(owner.projectSnapshot()),
    copy = owner.activeDocument().layers.find((layer) => layer.id === result.layerId),
    edit = copy.nodes.find((item) => item.type === 'Path Remap').edits[0];
  assert.equal(edit.pointId, 'missing-retained-point');
  assert.equal(evaluate(copy, fonts).path.remapSuspended, true);
  send('undo');
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), before);
  send('redo');
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), after);
  const reopened = createCompositionSessionOwner(deserializeCompositionProject(after), {
    fonts: () => fonts,
  });
  assert.equal(evaluate(reopened.activeDocument().layers[1], fonts).path.remapSuspended, true);
});

test('a loaded font does not bypass the guard for an active generated Text point', () => {
  const fonts = { default: parseFont(fontBytes('../public/fonts/DejaVuSans.ttf')) },
    f = textRemapDocument();
  makeTextEdit(f, fonts);
  const owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(f.doc), {
      fonts: () => fonts,
    }),
    before = serializeCompositionProject(owner.projectSnapshot()),
    state = owner.state;
  assert.throws(
    () =>
      owner.dispatch({ ...owner.state, type: 'duplicate-layer', payload: { layerId: f.layer.id } }),
    /Path Remap.*binding/,
  );
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), before);
  assert.deepEqual(owner.state, state);
});

test('a static embedded font is resolved for the duplicate command', () => {
  const f = textRemapDocument(),
    asset = makeFontAsset(fontBytes('../public/fonts/DejaVuSans.ttf'), 'DejaVuSans.ttf');
  f.doc.fontAssets = { [asset.id]: asset.asset };
  f.text.font = asset.id;
  makeTextEdit(f, resolveFonts(f.doc), true);
  const owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(f.doc)),
    result = owner.dispatch({
      ...owner.state,
      type: 'duplicate-layer',
      payload: { layerId: f.layer.id },
    }),
    doc = owner.activeDocument(),
    copy = doc.layers.find((layer) => layer.id === result.layerId);
  assert.equal(evaluate(copy, resolveFonts(doc)).path.remapSuspended, true);
  assert.equal(doc.fontAssets[asset.id].data, asset.asset.data);
});

test('a prepared variable font is passed through, while missing preparation refuses atomically', async () => {
  const f = textRemapDocument(),
    asset = makeVariableFontAsset(
      fontBytes('./fixtures/variable-font/Roboto-variable.ttf'),
      'Roboto.ttf',
    ),
    workerFactory = () => {
      const worker = new Worker(new URL('./variable-font-worker-node.mjs', import.meta.url)),
        adapter = {
          postMessage: (data) => worker.postMessage(data),
          terminate: () => worker.terminate(),
        };
      worker.on('message', (data) => adapter.onmessage?.({ data }));
      worker.on('error', (error) => adapter.onerror?.(error));
      return adapter;
    },
    bridge = createVariableDocumentPreparer({ workerFactory });
  f.doc.fontAssets = { [asset.id]: asset.asset };
  f.text.font = asset.id;
  f.text.text = 'AV';
  f.text.variation = { version: 1, fontId: asset.id, axes: {} };
  try {
    bridge.activate(await bridge.prepareDocument(f.doc));
    makeTextEdit(f, resolveFonts(f.doc, bridge.fonts()), true);
    let prepared = true;
    const owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(f.doc), {
        fonts: () => (prepared ? bridge.fonts() : {}),
      }),
      send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });
    send('set-document-settings', { key: 'width', value: 1200 });
    send('undo');
    const before = serializeCompositionProject(owner.projectSnapshot()),
      state = owner.state,
      redo = owner.historyView.redoToken,
      revision = owner.projectRevision;
    prepared = false;
    assert.throws(
      () => send('duplicate-layer', { layerId: f.layer.id }),
      /unable to verify its input.*PREPARATION_REQUIRED/,
    );
    assert.equal(serializeCompositionProject(owner.projectSnapshot()), before);
    assert.deepEqual(owner.state, state);
    assert.equal(owner.projectRevision, revision);
    assert.strictEqual(owner.historyView.redoToken, redo);
    prepared = true;
    const result = send('duplicate-layer', { layerId: f.layer.id }),
      doc = owner.activeDocument(),
      copy = doc.layers.find((layer) => layer.id === result.layerId);
    assert.equal(evaluate(copy, resolveFonts(doc, bridge.fonts())).path.remapSuspended, true);
    assert.equal(doc.fontAssets[asset.id].data, asset.asset.data);
    const after = serializeCompositionProject(owner.projectSnapshot());
    send('undo');
    assert.equal(serializeCompositionProject(owner.projectSnapshot()), before);
    send('redo');
    assert.equal(serializeCompositionProject(owner.projectSnapshot()), after);
  } finally {
    bridge.dispose();
  }
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

test('duplicate without any Remap also leaves nested caller assets unfrozen', () => {
  const doc = blankDocument(),
    data =
      'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP4z8DwHwAFAAH/iZk9HQAAAABJRU5ErkJggg==',
    provenance = {
      version: 1,
      mode: 'embedded',
      rootId: 'folder-fixture',
      relativePath: 'image.png',
      sourceLocation: 'owned/image.png',
      sha256: 'a'.repeat(64),
      author: 'Fixture',
      source: 'Self-authored',
      license: 'Fixture',
    },
    asset = { name: 'red.png', mime: 'image/png', width: 1, height: 1, data, provenance };
  doc.schemaVersion = 53;
  doc.imageAssets = { 'image-shared': asset };
  duplicateLayerGeneric(doc, doc.layers[0].id);
  assert.equal(Object.isFrozen(asset), false);
  assert.equal(Object.isFrozen(provenance), false);
  assert.strictEqual(doc.imageAssets['image-shared'], asset);
});

for (const type of ['Curve', 'Rectangle']) {
  for (const frozenWrapper of [false, true]) {
    test(`${type} ${frozenWrapper ? 'shallow-frozen' : 'mutable'} asset provenance remains mutable after duplicate`, () => {
      const f = fixture(type),
        doc = structuredClone(f.owner.activeDocument()),
        data =
          'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP4z8DwHwAFAAH/iZk9HQAAAABJRU5ErkJggg==',
        provenance = {
          version: 1,
          mode: 'embedded',
          rootId: 'folder-fixture',
          relativePath: 'image.png',
          sourceLocation: 'owned/image.png',
          sha256: 'a'.repeat(64),
          author: 'Fixture',
          source: 'Self-authored',
          license: 'Fixture',
        },
        asset = { name: 'red.png', mime: 'image/png', width: 1, height: 1, data, provenance };
      doc.schemaVersion = 53;
      if (frozenWrapper) Object.freeze(asset);
      doc.imageAssets = { 'image-shared': asset };
      if (type === 'Curve') duplicateLayerGeneric(doc, doc.layers[0].id);
      else assert.throws(() => duplicateLayerGeneric(doc, doc.layers[0].id), /Path Remap/);
      assert.strictEqual(doc.imageAssets['image-shared'], asset);
      assert.strictEqual(asset.provenance, provenance);
      assert.equal(Object.isFrozen(provenance), false);
      provenance.author = 'Updated';
      assert.equal(provenance.author, 'Updated');
    });
  }
}
