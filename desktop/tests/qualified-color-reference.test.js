import test from 'node:test';
import assert from 'node:assert/strict';
import { blankDocument, blankLayer, evaluate, node, previewColorExpression } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';

const send = (owner, type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });
const artworkRef = 'ref(colorArtwork, "source-art", "source-color", "value")';
const compositionRef =
  'ref(colorComposition, "source-comp", "foreign-art", "foreign-color", "value")';

function fixture({ sourceId = 'source-color', foreignId = 'foreign-color' } = {}) {
  const doc = blankDocument(),
    target = doc.layers[0],
    source = blankLayer('Source'),
    foreign = blankDocument(),
    fill = node('Fill'),
    localColor = node('Color'),
    foreignColor = node('Color');
  target.id = 'target-art';
  source.id = 'source-art';
  foreign.layers[0].id = 'foreign-art';
  fill.id = 'target-fill';
  localColor.id = sourceId;
  foreignColor.id = foreignId;
  localColor.params.value.value = { hex: '#aBcD09', alpha: 0 };
  foreignColor.params.value.value = { hex: '#2468ac', alpha: 0.25 };
  target.nodes.push(fill);
  source.nodes.push(localColor);
  foreign.layers[0].nodes.push(foreignColor);
  doc.layers.push(source);
  const project = migrateLegacyDocumentToCompositionProject(doc);
  project.compositions[0].id = 'target-comp';
  project.compositions.push({
    id: 'source-comp',
    name: 'Source',
    width: foreign.width,
    height: foreign.height,
    layers: foreign.layers,
    editor: foreign.editor,
  });
  return createCompositionSessionOwner(project, { activeCompositionId: 'target-comp' });
}
function read(owner) {
  const document = owner.snapshot(),
    layer = document.layers.find((entry) => entry.id === 'target-art');
  return evaluate(layer, {}, document.imageAssets, document).parameterValue('target-fill', 'color');
}
function apply(owner, expression) {
  return send(owner, 'set-parameter-expression', {
    layerId: 'target-art',
    nodeId: 'target-fill',
    key: 'color',
    expression,
  });
}

test('Artwork Color references retain exact source values, schema and Undo/Redo', () => {
  const owner = fixture(),
    initial = read(owner),
    before = owner.projectSnapshot();
  const doc = owner.snapshot(),
    layer = doc.layers.find((entry) => entry.id === 'target-art');
  assert.deepEqual(
    previewColorExpression(layer, 'target-fill', 'color', artworkRef, {}, doc.imageAssets, doc),
    { hex: '#aBcD09', alpha: 0 },
  );
  assert.deepEqual(owner.projectSnapshot(), before);
  apply(owner, artworkRef);
  assert.equal(owner.projectSnapshot().schemaVersion, 64);
  assert.deepEqual(read(owner), { hex: '#aBcD09', alpha: 0 });
  send(owner, 'set-parameter', {
    layerId: 'source-art',
    nodeId: 'source-color',
    key: 'value',
    value: { hex: '#f0E0d0', alpha: 0.7 },
  });
  assert.deepEqual(read(owner), { hex: '#f0E0d0', alpha: 0.7 });
  send(owner, 'undo');
  assert.deepEqual(read(owner), { hex: '#aBcD09', alpha: 0 });
  send(owner, 'undo');
  assert.deepEqual(read(owner), initial);
  assert.equal(owner.projectSnapshot().schemaVersion, before.schemaVersion);
  send(owner, 'redo');
  send(owner, 'redo');
  assert.deepEqual(read(owner), { hex: '#f0E0d0', alpha: 0.7 });
  const reopened = createCompositionSessionOwner(
    deserializeCompositionProject(serializeCompositionProject(owner.projectSnapshot())),
    { activeCompositionId: 'target-comp' },
  );
  assert.deepEqual(read(reopened), { hex: '#f0E0d0', alpha: 0.7 });
});

test('Composition Color references follow a shared slot without turning the destination into a slot binding', () => {
  const owner = fixture(),
    slotId = send(owner, 'create-color-slot', {
      name: 'Brand',
      value: { hex: '#1248aC', alpha: 0 },
    }).slotId;
  owner.switchComposition('source-comp');
  send(owner, 'bind-color-slot', {
    layerId: 'foreign-art',
    nodeId: 'foreign-color',
    key: 'value',
    slotId,
  });
  owner.switchComposition('target-comp');
  apply(owner, compositionRef);
  assert.deepEqual(read(owner), { hex: '#1248aC', alpha: 0 });
  const saved = owner.projectSnapshot(),
    destination = saved.compositions[0].layers[0].nodes.find((entry) => entry.id === 'target-fill');
  assert.equal(destination.params.color.colorSlotId, undefined);
  send(owner, 'set-color-slot', { slotId, value: { hex: '#7700aa', alpha: 0.5 } });
  assert.deepEqual(read(owner), { hex: '#7700aa', alpha: 0.5 });
  send(owner, 'undo');
  assert.deepEqual(read(owner), { hex: '#1248aC', alpha: 0 });
  send(owner, 'redo');
  assert.deepEqual(read(owner), { hex: '#7700aa', alpha: 0.5 });
  const reopened = createCompositionSessionOwner(
    deserializeCompositionProject(serializeCompositionProject(owner.projectSnapshot())),
    { activeCompositionId: 'target-comp' },
  );
  assert.deepEqual(read(reopened), { hex: '#7700aa', alpha: 0.5 });
});
import {
  expressionReferenceTargetsForProject,
  bindProjectDocumentContext,
  cloneDocument,
} from '../src/core.js';
import { directSharedColorUses } from '../src/shared-color-uses.js';
import { prepareProjectCompositionsImport } from '../src/composition-project-import.js';
import {
  activeCompositionDocument,
  applyActiveCompositionDocument,
} from '../src/composition-project-model.js';

test('Color picker emits typed local, Artwork and Composition expressions and excludes cycles', () => {
  const owner = fixture(),
    p = owner.projectSnapshot();
  const local = node('Color');
  local.id = 'local-color';
  p.compositions[0].layers[0].nodes.push(local);
  const targets = expressionReferenceTargetsForProject(
    p,
    'target-comp',
    'target-art',
    'target-fill',
    'Color',
    'color',
  );
  assert.ok(targets.some((t) => t.expression === 'ref("local-color", "value")'));
  assert.ok(targets.some((t) => t.expression === artworkRef && t.layerId === 'source-art'));
  assert.ok(
    targets.some((t) => t.expression === compositionRef && t.compositionId === 'source-comp'),
  );
  apply(owner, compositionRef);
  const back = expressionReferenceTargetsForProject(
    owner.projectSnapshot(),
    'source-comp',
    'foreign-art',
    'foreign-color',
    'Color',
    'value',
  );
  assert.ok(!back.some((t) => t.nodeId === 'target-fill'));
});

test('typed references work within Color operations and cannot enter Scalar or old numeric qualifiers', () => {
  const owner = fixture();
  apply(owner, `alpha(${compositionRef}, 0.5)`);
  assert.deepEqual(read(owner), { hex: '#2468ac', alpha: 0.5 });
  const before = owner.projectSnapshot(),
    state = owner.state;
  for (const bad of [
    'ref(artwork, "source-art", "source-color", "value")',
    'ref(composition, "source-comp", "foreign-art", "foreign-color", "value")',
    `alpha(value, ${artworkRef})`,
    'ref(colorArtwork, "source-art", "source-color", "value", "x")',
  ])
    assert.throws(() => apply(owner, bad), /Color|numeric|Scalar|component/i, bad);
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.state, state);
  const project = owner.projectSnapshot(),
    scalar = node('Value');
  scalar.id = 'sink-scalar';
  project.compositions[0].layers[0].nodes.push(scalar);
  const session = createCompositionSessionOwner(project);
  assert.throws(
    () =>
      send(session, 'set-parameter-expression', {
        layerId: 'target-art',
        nodeId: 'sink-scalar',
        key: 'value',
        expression: artworkRef,
      }),
    /Color/i,
  );
});

test('qualified Color slot remains a direct use only at its source, including projected slot previews', () => {
  const owner = fixture(),
    slotId = send(owner, 'create-color-slot', {
      name: 'Brand',
      value: { hex: '#773311', alpha: 0 },
    }).slotId;
  owner.switchComposition('source-comp');
  send(owner, 'bind-color-slot', {
    layerId: 'foreign-art',
    nodeId: 'foreign-color',
    key: 'value',
    slotId,
  });
  owner.switchComposition('target-comp');
  apply(owner, compositionRef);
  const uses = directSharedColorUses(owner.projectSnapshot(), slotId);
  assert.equal(uses.length, 1);
  assert.equal(uses[0].nodeId, 'foreign-color');
  const doc = cloneDocument(owner.snapshot()),
    source = doc.colorSlots.find((s) => s.id === slotId);
  source.value = { hex: '#AbCd01', alpha: 0.4 };
  const layer = doc.layers.find((l) => l.id === 'target-art');
  assert.deepEqual(
    evaluate(layer, {}, doc.imageAssets, doc).parameterValue('target-fill', 'color'),
    { hex: '#AbCd01', alpha: 0.4 },
  );
  assert.deepEqual(read(owner), { hex: '#773311', alpha: 0 });
});

test('cross-scope Color cycles reject atomically and retain Redo', () => {
  const owner = fixture();
  apply(owner, compositionRef);
  owner.switchComposition('source-comp');
  send(owner, 'set-parameter', {
    layerId: 'foreign-art',
    nodeId: 'foreign-color',
    key: 'value',
    value: { hex: '#010203', alpha: 0.4 },
  });
  send(owner, 'undo');
  const before = owner.projectSnapshot(),
    state = owner.state,
    history = owner.historyState;
  assert.throws(
    () =>
      send(owner, 'set-parameter-expression', {
        layerId: 'foreign-art',
        nodeId: 'foreign-color',
        key: 'value',
        expression: 'ref(colorComposition, "target-comp", "target-art", "target-fill", "color")',
      }),
    /cycle/i,
  );
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.state, state);
  assert.deepEqual(owner.historyState, history);
  send(owner, 'redo');
  owner.switchComposition('target-comp');
  assert.deepEqual(read(owner), { hex: '#010203', alpha: 0.4 });
});

test('deleted Color source is diagnosed, cannot rebind on ID reuse and Undo restores it', () => {
  const owner = fixture({ sourceId: 'node-color' });
  apply(owner, artworkRef.replace('source-color', 'node-color'));
  send(owner, 'delete-node', { layerId: 'source-art', nodeId: 'node-color' });
  assert.throws(() => read(owner), /missing/i);
  const before = owner.projectSnapshot(),
    state = owner.state,
    original = globalThis.crypto.randomUUID;
  Object.defineProperty(globalThis.crypto, 'randomUUID', {
    configurable: true,
    value: () => 'color',
  });
  try {
    assert.throws(
      () => send(owner, 'create-node', { layerId: 'source-art', nodeType: 'Color' }),
      /retarget|collid/i,
    );
  } finally {
    Object.defineProperty(globalThis.crypto, 'randomUUID', { configurable: true, value: original });
  }
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.state, state);
  send(owner, 'undo');
  assert.deepEqual(read(owner), { hex: '#aBcD09', alpha: 0 });
});

test('qualified Color imports and copies refuse atomically', () => {
  const owner = fixture();
  apply(owner, compositionRef);
  const before = owner.projectSnapshot(),
    state = owner.state;
  for (const [type, payload] of [
    ['copy-nodes', { layerId: 'target-art', nodeIds: ['target-fill'] }],
    ['duplicate-nodes', { layerId: 'target-art', nodeIds: ['target-fill'] }],
    ['duplicate-layer', { layerId: 'target-art' }],
  ]) {
    assert.throws(() => send(owner, type, payload), /not supported/i, type);
    assert.deepEqual(owner.projectSnapshot(), before);
    assert.deepEqual(owner.state, state);
  }
  for (const ids of [['target-comp'], ['target-comp', 'source-comp']])
    assert.throws(
      () => prepareProjectCompositionsImport(fixture().projectSnapshot(), before, ids),
      /not supported/i,
    );
});

test('a mixed Artwork and Composition Color copy is refused as a whole', () => {
  const owner = fixture();
  apply(owner, artworkRef);
  const second = send(owner, 'create-node', { layerId: 'target-art', nodeType: 'Fill' }).nodeId;
  send(owner, 'set-parameter-expression', {
    layerId: 'target-art',
    nodeId: second,
    key: 'color',
    expression: compositionRef,
  });
  const local = send(owner, 'create-node', { layerId: 'target-art', nodeType: 'Color' }).nodeId;
  send(owner, 'set-document-settings', { key: 'width', value: 961 });
  send(owner, 'undo');
  send(owner, 'copy-nodes', { layerId: 'target-art', nodeIds: [local] });
  const before = owner.projectSnapshot(),
    state = owner.state,
    history = owner.historyState;
  assert.throws(
    () =>
      send(owner, 'copy-nodes', {
        layerId: 'target-art',
        nodeIds: ['target-fill', second],
      }),
    /not supported/i,
  );
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.state, state);
  assert.deepEqual(owner.historyState, history);
  const id = send(owner, 'paste-nodes', { layerId: 'target-art' }).nodeIds[0];
  assert.equal(owner.snapshot().layers[0].nodes.find((entry) => entry.id === id).type, 'Color');
});

test('Artwork Color expression follows a connected source, expression and legacy Fill fallback', () => {
  const owner = fixture();
  const project = owner.projectSnapshot();
  const src = project.compositions[0].layers.find((l) => l.id === 'source-art');
  const fallback = node('Fill');
  fallback.id = 'legacy-fill';
  fallback.params.color.value = null;
  src.fill = '#aBc123';
  src.nodes.push(fallback);
  const nested = node('Color');
  nested.id = 'nested';
  nested.params.value.value = { hex: '#A0b0C0', alpha: 0.2 };
  src.nodes.push(nested);
  const session = createCompositionSessionOwner(project);
  apply(session, 'ref(colorArtwork, "source-art", "legacy-fill", "color")');
  assert.deepEqual(read(session), { hex: '#aBc123', alpha: 1 });
  send(session, 'set-parameter-expression', {
    layerId: 'source-art',
    nodeId: 'source-color',
    key: 'value',
    expression: 'ref("nested", "value")',
  });
  apply(session, artworkRef);
  assert.deepEqual(read(session), { hex: '#A0b0C0', alpha: 0.2 });
  send(session, 'connect', {
    layerId: 'source-art',
    nodeId: 'source-color',
    key: 'value',
    sourceId: 'nested',
  });
  send(session, 'set-parameter', {
    layerId: 'source-art',
    nodeId: 'nested',
    key: 'value',
    value: { hex: '#1200FF', alpha: 0 },
  });
  assert.deepEqual(read(session), { hex: '#1200FF', alpha: 0 });
});

test('cross-Artwork Color cycles and a resumed invalid expression reject without consuming Redo', () => {
  const owner = fixture();
  apply(owner, artworkRef);
  let before = owner.projectSnapshot();
  assert.throws(
    () =>
      send(owner, 'set-parameter-expression', {
        layerId: 'source-art',
        nodeId: 'source-color',
        key: 'value',
        expression: 'ref(colorArtwork, "target-art", "target-fill", "color")',
      }),
    /cycle/i,
  );
  assert.deepEqual(owner.projectSnapshot(), before);
  const p = owner.projectSnapshot(),
    local = node('Color'),
    scalar = node('Value');
  local.id = 'connected-color';
  scalar.id = 'scalar';
  scalar.params.value.value = 0.5;
  p.compositions[0].layers[0].nodes.push(local);
  p.compositions[0].layers[1].nodes.push(scalar);
  const session = createCompositionSessionOwner(p);
  apply(
    session,
    'alpha(ref(colorArtwork, "source-art", "source-color", "value"), ref(artwork, "source-art", "scalar", "value"))',
  );
  send(session, 'connect', {
    layerId: 'target-art',
    nodeId: 'target-fill',
    key: 'color',
    sourceId: 'connected-color',
  });
  send(session, 'set-parameter', {
    layerId: 'source-art',
    nodeId: 'scalar',
    key: 'value',
    value: 2,
  });
  send(session, 'set-parameter', {
    layerId: 'target-art',
    nodeId: 'connected-color',
    key: 'value',
    value: { hex: '#abcdef', alpha: 1 },
  });
  send(session, 'undo');
  before = session.projectSnapshot();
  const history = session.historyState,
    state = session.state;
  assert.throws(
    () =>
      send(session, 'disconnect', { layerId: 'target-art', nodeId: 'target-fill', key: 'color' }),
    /alpha|finite/i,
  );
  assert.deepEqual(session.projectSnapshot(), before);
  assert.deepEqual(session.state, state);
  assert.deepEqual(session.historyState, history);
  send(session, 'redo');
  assert.deepEqual(read(session), { hex: '#abcdef', alpha: 1 });
});

test('legacy opaque typed Color reference text never becomes active through promotion or import', () => {
  const template = fixture().projectSnapshot();
  template.schemaVersion = 63;
  template.compositionSchemaVersion = 2;
  for (const field of ['connected', 'unsupported', 'point', 'edit', 'preset']) {
    const p = structuredClone(template),
      source = p.compositions[1].layers[0];
    if (field === 'connected') {
      const q = source.nodes.find((n) => n.id === 'foreign-color').params.value;
      q.expression = compositionRef;
      q.source = 'foreign-color';
    }
    if (field === 'unsupported')
      source.nodes.find((n) => n.id === 'foreign-color').params.opaque = {
        type: 'String',
        value: 'x',
        expression: artworkRef,
      };
    if (field === 'point') {
      const n = node('Curve');
      n.points[0].position.expression = compositionRef;
      source.nodes.push(n);
    }
    if (field === 'edit') {
      const n = node('Path Remap');
      n.edits.push({
        params: {
          position: { type: 'Vector2', value: [0, 0], expression: artworkRef, locked: false },
        },
      });
      source.nodes.push(n);
    }
    if (field === 'preset')
      p.nodePresets = [
        {
          id: 'opaque-preset',
          nodes: [
            {
              params: {
                payload: {
                  type: 'Boolean',
                  value: false,
                  expression: compositionRef,
                  locked: false,
                },
              },
            },
          ],
        },
      ];
    assert.deepEqual(deserializeCompositionProject(serializeCompositionProject(p)), p, field);
    const edited = activeCompositionDocument(p, 'target-comp');
    edited.schemaVersion = 64;
    assert.throws(
      () => applyActiveCompositionDocument(p, 'target-comp', edited),
      /opaque.*Color/i,
      field,
    );
  }
  const source = structuredClone(template);
  source.compositions[0].layers[0].nodes.find(
    (n) => n.id === 'target-fill',
  ).params.color.expression = artworkRef;
  const current = fixture();
  apply(current, compositionRef);
  assert.throws(
    () => prepareProjectCompositionsImport(current.projectSnapshot(), source, ['target-comp']),
    /opaque.*Color/i,
  );
});
import { connect, exportSVG } from '../src/core.js';

test('Color reference drives rendered Fill and export without baking source or slot identity', () => {
  const owner = fixture(),
    project = owner.projectSnapshot(),
    layer = project.compositions[0].layers[0],
    circle = node('Circle');
  circle.id = 'circle';
  layer.nodes.push(circle);
  connect(layer, 'circle', 'target-fill', 'path');
  connect(layer, 'target-fill', layer.output, 'path');
  const session = createCompositionSessionOwner(project);
  apply(session, artworkRef);
  let doc = session.snapshot(),
    result = evaluate(doc.layers[0], {}, doc.imageAssets, doc);
  assert.equal(result.error, null);
  assert.deepEqual(result.path.fill, { hex: '#aBcD09', alpha: 0 });
  const first = exportSVG(doc);
  assert.match(first, /fill-opacity="0"/);
  send(session, 'set-parameter', {
    layerId: 'source-art',
    nodeId: 'source-color',
    key: 'value',
    value: { hex: '#010203', alpha: 0.6 },
  });
  doc = session.snapshot();
  result = evaluate(doc.layers[0], {}, doc.imageAssets, doc);
  assert.deepEqual(result.path.fill, { hex: '#010203', alpha: 0.6 });
  assert.notEqual(exportSVG(doc), first);
  assert.equal(
    doc.layers[0].nodes.find((n) => n.id === 'target-fill').params.color.colorSlotId,
    undefined,
  );
});

test('Composition Color source deletion remains missing and rejects exact ID reuse', () => {
  const owner = fixture({ foreignId: 'node-color' });
  apply(owner, compositionRef.replace('foreign-color', 'node-color'));
  owner.switchComposition('source-comp');
  send(owner, 'delete-node', { layerId: 'foreign-art', nodeId: 'node-color' });
  owner.switchComposition('target-comp');
  assert.throws(() => read(owner), /missing/i);
  owner.switchComposition('source-comp');
  const before = owner.projectSnapshot(),
    state = owner.state,
    old = globalThis.crypto.randomUUID;
  Object.defineProperty(globalThis.crypto, 'randomUUID', {
    configurable: true,
    value: () => 'color',
  });
  try {
    assert.throws(
      () => send(owner, 'create-node', { layerId: 'foreign-art', nodeType: 'Color' }),
      /retarget|collid/i,
    );
  } finally {
    Object.defineProperty(globalThis.crypto, 'randomUUID', { configurable: true, value: old });
  }
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.state, state);
  send(owner, 'undo');
  owner.switchComposition('target-comp');
  assert.deepEqual(read(owner), { hex: '#2468ac', alpha: 0.25 });
});

test('mixed Color and numeric references share cycle, preview and evaluation semantics', () => {
  const owner = fixture(),
    p = owner.projectSnapshot();
  const scalar = node('Value');
  scalar.id = 'fraction';
  scalar.params.value.value = 0.5;
  p.compositions[1].layers[0].nodes.push(scalar);
  const session = createCompositionSessionOwner(p);
  const text =
    'alpha(ref(colorArtwork, "source-art", "source-color", "value"), ref(composition, "source-comp", "foreign-art", "fraction", "value"))';
  const doc = session.snapshot(),
    layer = doc.layers.find((l) => l.id === 'target-art');
  const result = previewColorExpression(
    layer,
    'target-fill',
    'color',
    text,
    {},
    doc.imageAssets,
    doc,
  );
  assert.equal(result.alpha, 0.5);
  apply(session, text);
  assert.deepEqual(read(session), result);
  session.switchComposition('source-comp');
  const before = session.projectSnapshot(),
    state = session.state;
  assert.throws(
    () =>
      send(session, 'set-parameter', {
        layerId: 'foreign-art',
        nodeId: 'fraction',
        key: 'value',
        value: 2,
      }),
    /alpha/i,
  );
  assert.deepEqual(session.projectSnapshot(), before);
  assert.deepEqual(session.state, state);
  send(session, 'set-parameter', {
    layerId: 'foreign-art',
    nodeId: 'fraction',
    key: 'value',
    value: 0.7,
  });
  session.switchComposition('target-comp');
  assert.equal(read(session).alpha, 0.7);
});

test('mixed Artwork and Composition Color cycle rejects while independent values stay intact', () => {
  const owner = fixture();
  send(owner, 'set-parameter-expression', {
    layerId: 'source-art',
    nodeId: 'source-color',
    key: 'value',
    expression: compositionRef,
  });
  apply(owner, artworkRef);
  owner.switchComposition('source-comp');
  const before = owner.projectSnapshot(),
    state = owner.state;
  assert.throws(
    () =>
      send(owner, 'set-parameter-expression', {
        layerId: 'foreign-art',
        nodeId: 'foreign-color',
        key: 'value',
        expression: 'ref(colorComposition, "target-comp", "target-art", "target-fill", "color")',
      }),
    /cycle/i,
  );
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.state, state);
});

test('malformed old reserved Color text is retained as opaque but cannot be promoted', () => {
  const p = fixture().projectSnapshot();
  p.schemaVersion = 63;
  p.compositionSchemaVersion = 2;
  p.compositions[1].layers[0].nodes.find((n) => n.id === 'foreign-color').params.value.expression =
    'ref(colorComposition, "unfinished"';
  assert.deepEqual(deserializeCompositionProject(serializeCompositionProject(p)), p);
  const edited = activeCompositionDocument(p, 'target-comp');
  edited.schemaVersion = 64;
  assert.throws(() => applyActiveCompositionDocument(p, 'target-comp', edited), /opaque.*Color/i);
  const lied = structuredClone(p);
  lied.schemaVersion = 64;
  assert.throws(() => serializeCompositionProject(lied), /Color|reference|Expected|Invalid/i);
});

test('Color preview and Apply agree when an inactive schema 11 source requires migration', () => {
  const p = fixture().projectSnapshot();
  p.schemaVersion = 11;
  p.compositionSchemaVersion = 1;
  for (const composition of p.compositions) delete composition.editor;
  const old = node('Resample Path');
  old.id = 'old-resample';
  delete old.params.spacing;
  p.compositions[1].layers[0].nodes.push(old);
  const owner = createCompositionSessionOwner(p),
    before = owner.projectSnapshot(),
    doc = owner.snapshot();
  const layer = doc.layers.find((l) => l.id === 'target-art');
  assert.deepEqual(
    previewColorExpression(layer, 'target-fill', 'color', compositionRef, {}, doc.imageAssets, doc),
    { hex: '#2468ac', alpha: 0.25 },
  );
  assert.deepEqual(owner.projectSnapshot(), before);
  apply(owner, compositionRef);
  assert.deepEqual(read(owner), { hex: '#2468ac', alpha: 0.25 });
  const reopened = createCompositionSessionOwner(
    deserializeCompositionProject(serializeCompositionProject(owner.projectSnapshot())),
  );
  assert.deepEqual(read(reopened), { hex: '#2468ac', alpha: 0.25 });
});
import { prepareCompositionImport } from '../src/project-composition-import.js';
import { serialize } from '../src/core.js';

test('importing an unrelated Artwork from schema 64 does not unnecessarily promote the destination', () => {
  const owner = fixture();
  apply(owner, compositionRef);
  const source = owner.snapshot(),
    destination = blankDocument();
  destination.schemaVersion = 62;
  const imported = prepareCompositionImport(destination, serialize(source), 'source-art');
  assert.ok(imported.schemaVersion < 63);
  assert.ok(imported.layers.some((l) => l.name === 'Source'));
});

test('schema 64 wrapper rejects malformed Color arithmetic even in a connected descriptor', () => {
  const owner = fixture();
  apply(owner, artworkRef);
  const p = owner.projectSnapshot();
  const q = p.compositions[0].layers[0].nodes.find((n) => n.id === 'target-fill').params.color;
  q.expression = `${artworkRef} + 1`;
  q.source = 'source-local';
  assert.throws(() => serializeCompositionProject(p), /Color|reference/i);
});

test('Color Composition removal retains missing expression and Undo restores the source', () => {
  const owner = fixture();
  apply(owner, compositionRef);
  owner.deleteComposition('source-comp');
  assert.throws(() => read(owner), /missing/i);
  const saved = deserializeCompositionProject(serializeCompositionProject(owner.projectSnapshot()));
  const reopened = createCompositionSessionOwner(saved);
  assert.throws(() => read(reopened), /missing/i);
  send(owner, 'undo');
  assert.deepEqual(read(owner), { hex: '#2468ac', alpha: 0.25 });
  send(owner, 'redo');
  assert.throws(() => read(owner), /missing/i);
  send(owner, 'undo');
  send(owner, 'delete-composition', {
    compositionId: 'source-comp',
    expectedName: 'Source',
    expectedProjectRevision: owner.projectRevision,
  });
  assert.throws(() => read(owner), /missing/i);
  send(owner, 'undo');
  assert.deepEqual(read(owner), { hex: '#2468ac', alpha: 0.25 });
});

test('alpha operation retains hidden RGB from a zero-alpha Color source', () => {
  const owner = fixture();
  apply(owner, `alpha(${artworkRef}, 0)`);
  assert.deepEqual(read(owner), { hex: '#abcd09', alpha: 0 });
});

test('qualified Color expression preserves editable Base and one-step Undo/Redo', () => {
  const owner = fixture();
  apply(owner, artworkRef);
  const before = owner.snapshot().layers[0].nodes.find((n) => n.id === 'target-fill').params
    .color.value;
  send(owner, 'set-parameter-base', {
    layerId: 'target-art',
    nodeId: 'target-fill',
    key: 'color',
    value: { hex: '#112233', alpha: 0.2 },
  });
  let q = owner.snapshot().layers[0].nodes.find((n) => n.id === 'target-fill').params.color;
  assert.deepEqual(q.value, { hex: '#112233', alpha: 0.2 });
  assert.equal(q.expression, artworkRef);
  assert.deepEqual(read(owner), { hex: '#aBcD09', alpha: 0 });
  send(owner, 'undo');
  q = owner.snapshot().layers[0].nodes.find((n) => n.id === 'target-fill').params.color;
  assert.deepEqual(q.value, before);
  assert.equal(q.expression, artworkRef);
  send(owner, 'redo');
  q = owner.snapshot().layers[0].nodes.find((n) => n.id === 'target-fill').params.color;
  assert.deepEqual(q.value, { hex: '#112233', alpha: 0.2 });
  assert.equal(q.expression, artworkRef);
});

import { CommandSession } from '../src/core.js';

test('copy after qualified Color edit does not promote an unrelated node after Undo', () => {
  const doc = blankDocument();
  doc.schemaVersion = 63;
  const target = doc.layers[0],
    source = blankLayer('Source');
  target.id = 'copy-target';
  source.id = 'copy-source';
  const fill = node('Fill'),
    color = node('Color'),
    value = node('Value');
  fill.id = 'copy-fill';
  color.id = 'copy-color';
  value.id = 'copy-value';
  target.nodes.push(fill, value);
  source.nodes.push(color);
  doc.layers.push(source);
  const session = new CommandSession(doc),
    send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  send('set-parameter-expression', {
    layerId: target.id,
    nodeId: fill.id,
    key: 'color',
    expression: 'ref(colorArtwork, "copy-source", "copy-color", "value")',
  });
  assert.equal(session.snapshot().schemaVersion, 64);
  send('copy-nodes', { layerId: target.id, nodeIds: [value.id] });
  send('undo');
  assert.equal(session.snapshot().schemaVersion, 63);
  const before = session.snapshot();
  const id = send('paste-nodes', { layerId: target.id }).nodeIds[0];
  const pasted = session.snapshot();
  assert.equal(pasted.schemaVersion, 63);
  assert.equal(pasted.layers[0].nodes.find((n) => n.id === id).type, 'Value');
  send('undo');
  assert.deepEqual(session.snapshot(), before);
  send('redo');
  assert.deepEqual(session.snapshot(), pasted);
});

test('numeric Artwork references copied from a Color schema 64 document retain their required version', () => {
  const doc = blankDocument();
  doc.schemaVersion = 61;
  const target = doc.layers[0],
    source = blankLayer('Source');
  target.id = 'copy-target';
  source.id = 'copy-source';
  const fill = node('Fill'),
    color = node('Color'),
    sink = node('Value'),
    driver = node('Value');
  fill.id = 'copy-fill';
  color.id = 'copy-color';
  sink.id = 'copy-sink';
  driver.id = 'copy-driver';
  driver.params.value.value = 17;
  target.nodes.push(fill, sink);
  source.nodes.push(color, driver);
  doc.layers.push(source);
  const session = new CommandSession(doc),
    send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  const numeric = 'ref(artwork, "copy-source", "copy-driver", "value")';
  send('set-parameter-expression', {
    layerId: target.id,
    nodeId: sink.id,
    key: 'value',
    expression: numeric,
  });
  assert.equal(session.snapshot().schemaVersion, 62);
  send('set-parameter-expression', {
    layerId: target.id,
    nodeId: fill.id,
    key: 'color',
    expression: 'ref(colorArtwork, "copy-source", "copy-color", "value")',
  });
  send('copy-nodes', { layerId: target.id, nodeIds: [sink.id] });
  send('undo');
  send('undo');
  assert.equal(session.snapshot().schemaVersion, 61);
  const id = send('paste-nodes', { layerId: target.id }).nodeIds[0],
    after = session.snapshot();
  assert.equal(after.schemaVersion, 62);
  assert.equal(after.layers[0].nodes.find((n) => n.id === id).params.value.expression, numeric);
  assert.equal(
    evaluate(after.layers[0], {}, after.imageAssets, after).parameterValue(id, 'value'),
    17,
  );
  send('undo');
  assert.equal(session.snapshot().schemaVersion, 61);
  send('redo');
  assert.equal(session.snapshot().schemaVersion, 62);
});

test('schema 64 clipboard still refuses a dormant numeric Composition reference', () => {
  const base = fixture().projectSnapshot(),
    target = base.compositions[0].layers[0],
    foreign = base.compositions[1].layers[0];
  const sink = node('Value'),
    input = node('Value'),
    driver = node('Value');
  sink.id = 'scalar-sink';
  input.id = 'scalar-input';
  driver.id = 'scalar-driver';
  target.nodes.push(sink, input);
  foreign.nodes.push(driver);
  const owner = createCompositionSessionOwner(base);
  send(owner, 'set-parameter-expression', {
    layerId: 'target-art',
    nodeId: 'scalar-sink',
    key: 'value',
    expression: 'ref(composition, "source-comp", "foreign-art", "scalar-driver", "value")',
  });
  send(owner, 'connect', {
    layerId: 'target-art',
    nodeId: 'scalar-sink',
    key: 'value',
    sourceId: 'scalar-input',
  });
  apply(owner, artworkRef);
  assert.equal(owner.projectSnapshot().schemaVersion, 64);
  const before = owner.projectSnapshot(),
    state = owner.state,
    history = owner.historyState;
  assert.throws(
    () => send(owner, 'copy-nodes', { layerId: 'target-art', nodeIds: ['scalar-sink'] }),
    /Composition property references/i,
  );
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.state, state);
  assert.deepEqual(owner.historyState, history);
});

test('Curve handle reference copied from schema 64 retains its supported version and value', () => {
  const doc = blankDocument();
  doc.schemaVersion = 60;
  const target = doc.layers[0],
    source = blankLayer('Source');
  target.id = 'copy-target';
  source.id = 'copy-source';
  const fill = node('Fill'),
    color = node('Color'),
    sink = node('Value'),
    curve = node('Curve');
  fill.id = 'copy-fill';
  color.id = 'copy-color';
  sink.id = 'copy-sink';
  curve.id = 'copy-curve';
  curve.points[0].id = 'copy-point';
  curve.points[0].handleA.value = [3, 4];
  target.nodes.push(fill, sink, curve);
  source.nodes.push(color);
  doc.layers.push(source);
  const session = new CommandSession(doc),
    send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  const numeric = 'ref("copy-curve", "copy-point.handleA", "length")';
  send('set-parameter-expression', {
    layerId: target.id,
    nodeId: sink.id,
    key: 'value',
    expression: numeric,
  });
  assert.equal(session.snapshot().schemaVersion, 61);
  send('set-parameter-expression', {
    layerId: target.id,
    nodeId: fill.id,
    key: 'color',
    expression: 'ref(colorArtwork, "copy-source", "copy-color", "value")',
  });
  send('copy-nodes', { layerId: target.id, nodeIds: [sink.id] });
  send('undo');
  send('undo');
  assert.equal(session.snapshot().schemaVersion, 60);
  const id = send('paste-nodes', { layerId: target.id }).nodeIds[0],
    after = session.snapshot();
  assert.ok(after.schemaVersion >= 61 && after.schemaVersion <= 62);
  assert.equal(
    evaluate(after.layers[0], {}, after.imageAssets, after).parameterValue(id, 'value'),
    5,
  );
});

test('copy after Undo keeps inactive schema 63 opaque Color text uninterpreted', () => {
  const project = fixture().projectSnapshot();
  project.schemaVersion = 63;
  project.compositionSchemaVersion = 2;
  const opaque = project.compositions[1].layers[0].nodes.find((n) => n.id === 'foreign-color');
  opaque.params.metadata = {
    type: 'String',
    value: 'opaque',
    expression: 'ref(colorArtwork, "missing", "missing", "value")',
  };
  const active = activeCompositionDocument(project, 'target-comp'),
    value = node('Value');
  value.id = 'plain-copy';
  active.layers[0].nodes.push(value);
  const session = new CommandSession(active),
    send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  send('set-parameter-expression', {
    layerId: 'target-art',
    nodeId: 'target-fill',
    key: 'color',
    expression: artworkRef,
  });
  send('copy-nodes', { layerId: 'target-art', nodeIds: [value.id] });
  send('undo');
  send('paste-nodes', { layerId: 'target-art' });
  assert.equal(session.snapshot().schemaVersion, 63);
  const updated = applyActiveCompositionDocument(project, 'target-comp', session.snapshot());
  assert.equal(updated.schemaVersion, 63);
  assert.deepEqual(updated.compositions[1], project.compositions[1]);
  const lied = session.snapshot();
  lied.schemaVersion = 64;
  assert.throws(
    () => applyActiveCompositionDocument(project, 'target-comp', lied),
    /opaque.*Color/i,
  );
});
