import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  node,
  evaluate,
  expressionReferenceTargetsForProject,
  previewColorExpression,
} from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
} from '../src/composition-project-model.js';

function fixture() {
  const a = blankDocument(),
    b = blankDocument();
  a.layers[0].id = 'art-a';
  b.layers[0].id = 'art-b';
  const sink = node('Value'),
    driver = node('Value');
  sink.id = 'sink';
  driver.id = 'driver';
  driver.params.value.value = 7;
  a.layers[0].nodes.push(sink);
  b.layers[0].nodes.push(driver);
  const project = migrateLegacyDocumentToCompositionProject(a);
  project.compositions[0].id = 'comp-a';
  project.compositions.push({
    id: 'comp-b',
    name: 'Source',
    width: b.width,
    height: b.height,
    layers: b.layers,
    editor: b.editor,
  });
  return project;
}
const send = (session, type, payload = {}) => session.dispatch({ ...session.state, type, payload });
const apply = (session, expression) =>
  send(session, 'set-parameter-expression', {
    layerId: 'art-a',
    nodeId: 'sink',
    key: 'value',
    expression,
  });
const read = (session) => {
  const d = session.snapshot();
  return evaluate(d.layers[0], {}, d.imageAssets, d).parameterValue('sink', 'value');
};
const ref = 'ref(composition, "comp-b", "art-b", "driver", "value")';

test('live Composition Scalar reference follows edits and project history across save and reopen', () => {
  const owner = createCompositionSessionOwner(fixture(), { activeCompositionId: 'comp-a' });
  apply(owner, ref);
  assert.equal(read(owner), 7);
  assert.equal(owner.projectSnapshot().schemaVersion, 63);
  const picker = expressionReferenceTargetsForProject(
    owner.projectSnapshot(),
    'comp-a',
    'art-a',
    'sink',
    'Scalar',
    'value',
  );
  assert.ok(picker.some((t) => t.compositionId === 'comp-b' && t.expression === ref));
  owner.switchComposition('comp-b');
  send(owner, 'set-parameter', { layerId: 'art-b', nodeId: 'driver', key: 'value', value: 12 });
  owner.switchComposition('comp-a');
  assert.equal(read(owner), 12);
  send(owner, 'undo');
  assert.equal(read(owner), 7);
  send(owner, 'redo');
  assert.equal(read(owner), 12);
  const reopened = createCompositionSessionOwner(
    deserializeCompositionProject(serializeCompositionProject(owner.projectSnapshot())),
    { activeCompositionId: 'comp-a' },
  );
  assert.equal(read(reopened), 12);
});

test('mixed cycles reject atomically and do not destroy Redo', () => {
  const owner = createCompositionSessionOwner(fixture());
  apply(owner, ref);
  owner.switchComposition('comp-b');
  send(owner, 'set-parameter', { layerId: 'art-b', nodeId: 'driver', key: 'value', value: 9 });
  send(owner, 'undo');
  const before = owner.projectSnapshot(),
    state = owner.state,
    history = owner.historyState;
  assert.throws(
    () =>
      send(owner, 'set-parameter-expression', {
        layerId: 'art-b',
        nodeId: 'driver',
        key: 'value',
        expression: 'ref(composition, "comp-a", "art-a", "sink", "value")',
      }),
    /cycle/i,
  );
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.state, state);
  assert.deepEqual(owner.historyState, history);
  send(owner, 'redo');
  owner.switchComposition('comp-a');
  assert.equal(read(owner), 9);
});

test('inactive dependent range and gesture previews use the complete project candidate', () => {
  const project = fixture();
  project.schemaVersion = 37;
  const repeater = node('Geometry Repeater');
  repeater.id = 'repeat';
  repeater.operation = 'ae-2d';
  project.compositions[0].layers[0].nodes.push(repeater);
  const owner = createCompositionSessionOwner(project);
  send(owner, 'set-parameter-expression', {
    layerId: 'art-a',
    nodeId: 'repeat',
    key: 'count',
    expression: ref,
  });
  const beforeView = owner.snapshot();
  owner.switchComposition('comp-b');
  const before = owner.projectSnapshot(),
    state = owner.state;
  assert.throws(
    () =>
      send(owner, 'set-parameter', {
        layerId: 'art-b',
        nodeId: 'driver',
        key: 'value',
        value: 1000,
      }),
    /Repeater/i,
  );
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.state, state);
  const started = send(owner, 'begin-gesture', {
    target: { kind: 'parameter', layerId: 'art-b', nodeId: 'driver', key: 'value' },
  });
  const preview = send(owner, 'update-gesture', { ...started, update: { value: 8 } });
  const previewDoc = owner.previewSnapshot(preview.gestureId).document;
  assert.equal(
    evaluate(previewDoc.layers[0], {}, previewDoc.imageAssets, previewDoc).parameterValue(
      'driver',
      'value',
    ),
    8,
  );
  assert.throws(
    () => send(owner, 'update-gesture', { ...preview, update: { value: 1000 } }),
    /Repeater/i,
  );
  send(owner, 'cancel-gesture', preview);
  send(owner, 'set-parameter', { layerId: 'art-b', nodeId: 'driver', key: 'value', value: 8 });
  assert.equal(
    evaluate(beforeView.layers[0], {}, beforeView.imageAssets, beforeView).parameterValue(
      'repeat',
      'count',
    ),
    7,
  );
});

test('Color Scalar preview and committed values use explicit Composition scope', () => {
  const project = fixture();
  project.compositions[1].layers[0].nodes.find((n) => n.id === 'driver').params.value.value = 0.25;
  const color = node('Color');
  color.id = 'color';
  project.compositions[0].layers[0].nodes.push(color);
  const owner = createCompositionSessionOwner(project);
  const doc = owner.snapshot(),
    text = `alpha(value, ${ref})`;
  assert.equal(
    previewColorExpression(doc.layers[0], 'color', 'value', text, {}, doc.imageAssets, doc).alpha,
    0.25,
  );
  send(owner, 'set-parameter-expression', {
    layerId: 'art-a',
    nodeId: 'color',
    key: 'value',
    expression: text,
  });
  const committed = owner.snapshot();
  assert.equal(
    evaluate(committed.layers[0], {}, committed.imageAssets, committed).parameterValue(
      'color',
      'value',
    ).alpha,
    0.25,
  );
  assert.throws(
    () =>
      evaluate(
        committed.layers[0],
        {},
        committed.imageAssets,
        structuredClone(committed),
      ).parameterValue('color', 'value'),
    /project context/i,
  );
});

test('deleted source is diagnosed and Undo restores; identity reuse is rejected', () => {
  const project = fixture();
  project.compositions[1].layers[0].nodes.find((n) => n.id === 'driver').id = 'node-collision';
  const owner = createCompositionSessionOwner(project);
  apply(owner, ref.replace('driver', 'node-collision'));
  owner.switchComposition('comp-b');
  send(owner, 'delete-node', { layerId: 'art-b', nodeId: 'node-collision' });
  owner.switchComposition('comp-a');
  assert.throws(() => read(owner), /missing/i);
  owner.switchComposition('comp-b');
  const before = owner.projectSnapshot();
  const old = globalThis.crypto.randomUUID;
  Object.defineProperty(globalThis.crypto, 'randomUUID', {
    configurable: true,
    value: () => 'collision',
  });
  try {
    assert.throws(
      () => send(owner, 'create-node', { layerId: 'art-b', nodeType: 'Value' }),
      /retarget|collid/i,
    );
  } finally {
    Object.defineProperty(globalThis.crypto, 'randomUUID', { configurable: true, value: old });
  }
  assert.deepEqual(owner.projectSnapshot(), before);
  send(owner, 'undo');
  owner.switchComposition('comp-a');
  assert.equal(read(owner), 7);
});

test('new syntax cannot be smuggled through old opaque descriptors or component references', () => {
  const project = fixture();
  project.compositions[1].layers[0].nodes.find((n) => n.id === 'driver').params.value.expression =
    ref;
  const owner = createCompositionSessionOwner(fixture());
  const before = owner.projectSnapshot();
  const mutated = fixture();
  mutated.compositions[1].layers[0].nodes.find((n) => n.id === 'driver').params.value.expression =
    'ref(composition, "opaque", "x", "y", "value")';
  assert.throws(() => createCompositionSessionOwner(mutated), /version|reference|same|different/i);
  assert.throws(
    () => apply(owner, 'ref(composition, "comp-b", "art-b", "driver", "value", "x")'),
    /Scalar property/i,
  );
  assert.deepEqual(owner.projectSnapshot(), before);
});

import { prepareProjectCompositionsImport } from '../src/composition-project-import.js';
import {
  activeCompositionDocument,
  applyActiveCompositionDocument,
} from '../src/composition-project-model.js';
import { exportSVG, connect, cloneDocument } from '../src/core.js';

test('source Composition uses its own Artwork resolver and projects are stable snapshots', () => {
  const project = fixture(),
    b = project.compositions[1],
    other = blankDocument().layers[0],
    inner = node('Value');
  other.id = 'art-c';
  inner.id = 'inner';
  inner.params.value.value = 11;
  other.nodes.push(inner);
  b.layers.push(other);
  project.schemaVersion = 62;
  project.compositionSchemaVersion = 2;
  b.layers[0].nodes.find((n) => n.id === 'driver').params.value.expression =
    'ref(artwork, "art-c", "inner", "value")';
  const owner = createCompositionSessionOwner(project);
  apply(owner, ref);
  const original = owner.snapshot();
  assert.equal(read(owner), 11);
  owner.switchComposition('comp-b');
  send(owner, 'set-parameter', { layerId: 'art-c', nodeId: 'inner', key: 'value', value: 13 });
  owner.switchComposition('comp-a');
  assert.equal(read(owner), 13);
  assert.equal(
    evaluate(original.layers[0], {}, original.imageAssets, original).parameterValue(
      'sink',
      'value',
    ),
    11,
  );
  assert.match(exportSVG(owner.snapshot()), /<svg/);
});

test('connected expressions pause and reject invalid values when resumed without losing Redo', () => {
  const project = fixture(),
    local = node('Value');
  local.id = 'local';
  local.params.value.value = 8;
  project.compositions[0].layers[0].nodes.push(local);
  const owner = createCompositionSessionOwner(project);
  apply(owner, `1 / ${ref}`);
  const target = { layerId: 'art-a', nodeId: 'sink', key: 'value' };
  send(owner, 'connect', { ...target, sourceId: 'local' });
  owner.switchComposition('comp-b');
  send(owner, 'set-parameter', { layerId: 'art-b', nodeId: 'driver', key: 'value', value: 0 });
  owner.switchComposition('comp-a');
  assert.equal(read(owner), 8);
  send(owner, 'set-parameter', { layerId: 'art-a', nodeId: 'local', key: 'value', value: 9 });
  send(owner, 'undo');
  const before = owner.projectSnapshot(),
    state = owner.state,
    history = owner.historyState;
  assert.throws(() => send(owner, 'disconnect', target), /finite|zero|divide/i);
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.state, state);
  assert.deepEqual(owner.historyState, history);
  send(owner, 'redo');
  assert.equal(read(owner), 9);
  owner.switchComposition('comp-b');
  send(owner, 'set-parameter', { layerId: 'art-b', nodeId: 'driver', key: 'value', value: 4 });
  owner.switchComposition('comp-a');
  send(owner, 'disconnect', target);
  assert.equal(read(owner), 0.25);
});

test('Composition deletion is dangling and import with one or both referenced Compositions refuses atomically', () => {
  const owner = createCompositionSessionOwner(fixture());
  apply(owner, ref);
  const source = owner.projectSnapshot();
  for (const selection of [['comp-a'], ['comp-a', 'comp-b']])
    assert.throws(
      () => prepareProjectCompositionsImport(fixture(), source, selection),
      /not supported/i,
    );
  const before = owner.projectSnapshot(),
    state = owner.state;
  assert.throws(
    () =>
      send(owner, 'import-project-compositions', {
        sourceProject: source,
        compositionIds: ['comp-a', 'comp-b'],
      }),
    /not supported/i,
  );
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.state, state);
  owner.deleteComposition('comp-b');
  assert.throws(() => read(owner), /missing/i);
  send(owner, 'undo');
  assert.equal(read(owner), 7);
});

test('v62 opaque syntax is preserved by wrapper but blocks promotion even on connected or unsupported descriptors', () => {
  for (const descriptor of ['connected', 'unsupported', 'string', 'point', 'edit', 'preset']) {
    const project = fixture();
    project.schemaVersion = 62;
    project.compositionSchemaVersion = 2;
    const layer = project.compositions[1].layers[0];
    if (descriptor === 'connected') {
      const driver = layer.nodes.find((n) => n.id === 'driver');
      driver.params.value.expression = ref;
      driver.params.value.source = layer.output;
    }
    if (descriptor === 'string')
      layer.nodes.find((n) => n.id === 'driver').params.extra = {
        type: 'String',
        value: 'opaque',
        expression: ref,
      };
    if (descriptor === 'unsupported') {
      const output = layer.nodes.find((n) => n.id === layer.output);
      output.params.path.expression = ref;
    }
    if (descriptor === 'point') {
      const curve = node('Curve');
      curve.points[0].position.expression = ref;
      layer.nodes.push(curve);
    }
    if (descriptor === 'edit') {
      const remap = node('Path Remap');
      remap.edits.push({
        params: { position: { type: 'Vector2', value: [0, 0], expression: ref, locked: false } },
      });
      layer.nodes.push(remap);
    }
    if (descriptor === 'preset')
      project.nodePresets = [
        {
          id: 'preset-opaque',
          nodes: [
            {
              params: {
                payload: { type: 'Boolean', value: false, expression: ref, locked: false },
              },
            },
          ],
        },
      ];
    assert.deepEqual(deserializeCompositionProject(serializeCompositionProject(project)), project);
    const edited = activeCompositionDocument(project, 'comp-a');
    edited.schemaVersion = 63;
    assert.throws(
      () => applyActiveCompositionDocument(project, 'comp-a', edited),
      /opaque Composition/i,
      descriptor,
    );
  }
});

test('live cross-Composition Scalar drives rendered scene and SVG export', () => {
  const project = fixture(),
    layer = project.compositions[0].layers[0];
  const circle = node('Circle'),
    fill = node('Fill');
  circle.id = 'circle';
  fill.id = 'fill';
  layer.nodes.push(circle, fill);
  connect(layer, circle.id, fill.id, 'path');
  connect(layer, fill.id, layer.output, 'path');
  const owner = createCompositionSessionOwner(project);
  send(owner, 'set-parameter-expression', {
    layerId: 'art-a',
    nodeId: 'circle',
    key: 'radius',
    expression: ref,
  });
  const first = owner.snapshot(),
    result = evaluate(first.layers[0], {}, first.imageAssets, first);
  assert.equal(result.parameterValue('circle', 'radius'), 7);
  assert.equal(result.error, null);
  const firstSvg = exportSVG(first);
  assert.match(firstSvg, /<path/);
  owner.switchComposition('comp-b');
  send(owner, 'set-parameter', { layerId: 'art-b', nodeId: 'driver', key: 'value', value: 12 });
  owner.switchComposition('comp-a');
  const second = owner.snapshot(),
    secondSvg = exportSVG(second);
  assert.notEqual(secondSvg, firstSvg);
  assert.equal(exportSVG(first), firstSvg);
});

test('applying an older projection into a modern project cannot activate opaque text', () => {
  const owner = createCompositionSessionOwner(fixture());
  apply(owner, ref);
  const modern = owner.projectSnapshot(),
    old = activeCompositionDocument(fixture(), 'comp-a');
  old.schemaVersion = 62;
  old.layers[0].nodes.find((n) => n.id === 'sink').params.value.expression = ref;
  assert.throws(() => applyActiveCompositionDocument(modern, 'comp-a', old), /opaque Composition/i);
});

test('independent project owners never reuse another project scope through shared documents, clones or history', () => {
  const project1 = fixture(),
    source2 = structuredClone(project1.compositions[1]);
  source2.layers[0].nodes.find((n) => n.id === 'driver').params.value.value = 19;
  const project2 = { ...project1, compositions: [project1.compositions[0], source2] };
  assert.strictEqual(project1.compositions[0], project2.compositions[0]);
  const first = createCompositionSessionOwner(project1),
    second = createCompositionSessionOwner(project2);
  apply(first, ref);
  apply(second, ref);
  const oldFirst = first.snapshot(),
    oldSecond = second.snapshot(),
    cloned = cloneDocument(oldFirst);
  assert.equal(read(first), 7);
  assert.equal(read(second), 19);
  assert.equal(
    evaluate(cloned.layers[0], {}, cloned.imageAssets, cloned).parameterValue('sink', 'value'),
    7,
  );
  first.switchComposition('comp-b');
  send(first, 'set-parameter', { layerId: 'art-b', nodeId: 'driver', key: 'value', value: 13 });
  first.switchComposition('comp-a');
  assert.equal(read(first), 13);
  assert.equal(read(second), 19);
  send(first, 'undo');
  assert.equal(read(first), 7);
  send(first, 'redo');
  assert.equal(read(first), 13);
  const reloaded = createCompositionSessionOwner(
    deserializeCompositionProject(serializeCompositionProject(second.projectSnapshot())),
  );
  assert.equal(read(reloaded), 19);
  assert.equal(
    evaluate(oldFirst.layers[0], {}, oldFirst.imageAssets, oldFirst).parameterValue(
      'sink',
      'value',
    ),
    7,
  );
  assert.equal(
    evaluate(oldSecond.layers[0], {}, oldSecond.imageAssets, oldSecond).parameterValue(
      'sink',
      'value',
    ),
    19,
  );
});

test('project picker excludes cross-Composition cycles and unsupported Vector2 destinations', () => {
  const project = fixture(),
    rect = node('Rectangle');
  rect.id = 'rect';
  project.compositions[0].layers[0].nodes.push(rect);
  const owner = createCompositionSessionOwner(project);
  apply(owner, ref);
  const p = owner.projectSnapshot();
  assert.ok(
    !expressionReferenceTargetsForProject(p, 'comp-b', 'art-b', 'driver', 'Scalar', 'value').some(
      (t) => t.nodeId === 'sink',
    ),
  );
  assert.ok(
    !expressionReferenceTargetsForProject(p, 'comp-a', 'art-a', 'rect', 'Scalar', 'center').some(
      (t) => t.compositionId === 'comp-b',
    ),
  );
});

test('cycle check is symmetric and qualified references reject wrong scope and source type', () => {
  const project = fixture(),
    vector = node('Vector');
  vector.id = 'vector';
  project.compositions[1].layers[0].nodes.push(vector);
  const owner = createCompositionSessionOwner(project, { activeCompositionId: 'comp-b' });
  send(owner, 'set-parameter-expression', {
    layerId: 'art-b',
    nodeId: 'driver',
    key: 'value',
    expression: 'ref(composition, "comp-a", "art-a", "sink", "value")',
  });
  owner.switchComposition('comp-a');
  const before = owner.projectSnapshot(),
    state = owner.state;
  assert.throws(() => apply(owner, ref), /cycle/i);
  assert.throws(
    () => apply(owner, 'ref(composition, "comp-b", "art-b", "vector", "value")'),
    /Scalar/i,
  );
  assert.throws(
    () => apply(owner, 'ref(composition, "comp-a", "art-b", "driver", "value")'),
    /different|Composition/i,
  );
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.state, state);
});

test('node copy, duplicate and generic Artwork duplicate refuse the new reference atomically', () => {
  const owner = createCompositionSessionOwner(fixture());
  apply(owner, ref);
  const before = owner.projectSnapshot(),
    state = owner.state;
  for (const [type, payload] of [
    ['copy-nodes', { layerId: 'art-a', nodeIds: ['sink'] }],
    ['duplicate-nodes', { layerId: 'art-a', nodeIds: ['sink'] }],
    ['duplicate-layer', { layerId: 'art-a' }],
  ]) {
    assert.throws(() => send(owner, type, payload), /not supported/i, type);
    assert.deepEqual(owner.projectSnapshot(), before);
    assert.deepEqual(owner.state, state);
  }
});

test('legacy Text spacing defaults match active and cross-Composition evaluation through history and reopen', () => {
  const project = fixture();
  project.schemaVersion = 62;
  project.compositionSchemaVersion = 2;
  const source = project.compositions[1].layers[0];
  const textNode = node('Text');
  textNode.id = 'legacy-text';
  delete textNode.params.letterSpacing;
  delete textNode.params.lineSpacing;
  source.nodes.push(textNode);
  source.nodes.find((n) => n.id === 'driver').params.value.expression =
    'ref("legacy-text", "letterSpacing") + 7';
  const owner = createCompositionSessionOwner(project, { activeCompositionId: 'comp-b' });
  const active = owner.snapshot();
  assert.equal(
    evaluate(active.layers[0], {}, active.imageAssets, active).parameterValue('driver', 'value'),
    7,
  );
  owner.switchComposition('comp-a');
  apply(owner, ref);
  assert.equal(read(owner), 7);
  assert.equal(
    Object.hasOwn(
      owner.projectSnapshot().compositions[1].layers[0].nodes.find((n) => n.id === 'legacy-text')
        .params,
      'letterSpacing',
    ),
    false,
  );
  const cold = createCompositionSessionOwner(
    deserializeCompositionProject(serializeCompositionProject(owner.projectSnapshot())),
    { activeCompositionId: 'comp-a' },
  );
  assert.equal(read(cold), 7);
  owner.switchComposition('comp-b');
  send(owner, 'set-parameter', {
    layerId: 'art-b',
    nodeId: 'legacy-text',
    key: 'letterSpacing',
    value: 3,
  });
  owner.switchComposition('comp-a');
  assert.equal(read(owner), 10);
  send(owner, 'undo');
  assert.equal(read(owner), 7);
  send(owner, 'redo');
  assert.equal(read(owner), 10);
  const reopened = createCompositionSessionOwner(
    deserializeCompositionProject(serializeCompositionProject(owner.projectSnapshot())),
    { activeCompositionId: 'comp-a' },
  );
  assert.equal(read(reopened), 10);
});

test('schema 11 inactive Resample migration remains valid on promotion and reopen', () => {
  const project = fixture();
  project.schemaVersion = 11;
  project.compositionSchemaVersion = 1;
  for (const composition of project.compositions) delete composition.editor;
  const resample = node('Resample Path');
  resample.id = 'old-resample';
  delete resample.params.spacing;
  project.compositions[1].layers[0].nodes.push(resample);
  const owner = createCompositionSessionOwner(project, { activeCompositionId: 'comp-b' });
  const spacing = owner.snapshot().layers[0].nodes.find((n) => n.id === 'old-resample').params
    .spacing.value;
  owner.switchComposition('comp-a');
  apply(owner, ref);
  assert.equal(read(owner), 7);
  assert.equal(
    owner.projectSnapshot().compositions[1].layers[0].nodes.find((n) => n.id === 'old-resample')
      .params.spacing.value,
    spacing,
  );
  send(owner, 'undo');
  assert.equal(owner.projectSnapshot().schemaVersion, 11);
  assert.equal(
    Object.hasOwn(
      owner.projectSnapshot().compositions[1].layers[0].nodes.find((n) => n.id === 'old-resample')
        .params,
      'spacing',
    ),
    false,
  );
  send(owner, 'redo');
  assert.equal(read(owner), 7);
  const reopened = createCompositionSessionOwner(
    deserializeCompositionProject(serializeCompositionProject(owner.projectSnapshot())),
    { activeCompositionId: 'comp-a' },
  );
  assert.equal(read(reopened), 7);
  reopened.switchComposition('comp-b');
  const doc = reopened.snapshot();
  assert.equal(
    doc.layers[0].nodes.find((n) => n.id === 'old-resample').params.spacing.value,
    spacing,
  );
});

test('legacy default Scalar is a direct eligible source; missing properties still reject', () => {
  const project = fixture();
  project.schemaVersion = 62;
  project.compositionSchemaVersion = 2;
  const textNode = node('Text');
  textNode.id = 'legacy-text';
  delete textNode.params.letterSpacing;
  project.compositions[1].layers[0].nodes.push(textNode);
  const owner = createCompositionSessionOwner(project);
  const target = expressionReferenceTargetsForProject(
    owner.projectSnapshot(),
    'comp-a',
    'art-a',
    'sink',
    'Scalar',
    'value',
  );
  const expression = 'ref(composition, "comp-b", "art-b", "legacy-text", "letterSpacing")';
  assert.ok(target.some((t) => t.expression === expression));
  const before = owner.projectSnapshot(),
    state = owner.state;
  assert.throws(
    () => apply(owner, 'ref(composition, "comp-b", "art-b", "legacy-text", "missing")'),
    /Scalar/i,
  );
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.state, state);
  apply(owner, expression);
  assert.equal(read(owner), 0);
  const reopened = createCompositionSessionOwner(
    deserializeCompositionProject(serializeCompositionProject(owner.projectSnapshot())),
  );
  assert.equal(read(reopened), 0);
});

import fs from 'node:fs';
import { Worker } from 'node:worker_threads';
import { makeVariableFontAsset } from '../src/fonts.js';
import { createVariableDocumentPreparer, bindProjectDocumentContext } from '../src/core.js';
const variableWorkerFactory = () => {
  const worker = new Worker(new URL('./variable-font-worker-node.mjs', import.meta.url));
  const bridge = {
    postMessage: (data) => worker.postMessage(data),
    terminate: () => worker.terminate(),
  };
  worker.on('message', (data) => bridge.onmessage?.({ data }));
  worker.on('error', (error) => bridge.onerror?.(error));
  return bridge;
};

test('variable preparation retains project binding, and prepared edits reopen with project history', async () => {
  const project = fixture();
  const raw = fs.readFileSync(
    new URL('./fixtures/variable-font/Roboto-variable.ttf', import.meta.url),
  );
  const asset = makeVariableFontAsset(
    raw.buffer.slice(raw.byteOffset, raw.byteOffset + raw.byteLength),
    'Roboto.ttf',
  );
  project.fontAssets = { [asset.id]: asset.asset };
  const textNode = node('Text');
  textNode.id = 'variable-text';
  textNode.font = asset.id;
  textNode.text = 'AV';
  textNode.variation = { version: 1, fontId: asset.id, axes: {} };
  project.compositions[0].layers[0].nodes.push(textNode);
  const bridge = createVariableDocumentPreparer({ workerFactory: variableWorkerFactory });
  try {
    const initial = activeCompositionDocument(project, 'comp-a');
    bindProjectDocumentContext(initial, project, 'comp-a');
    bridge.activate(await bridge.prepareDocument(initial));
    const owner = createCompositionSessionOwner(project, {
      activeCompositionId: 'comp-a',
      fonts: () => bridge.fonts(),
    });
    apply(owner, ref);
    const before = owner.projectSnapshot(),
      input = owner.snapshot();
    assert.equal(Object.isFrozen(input.layers[0]), false);
    const ticket = await bridge.prepareDocument(input),
      prepared = bridge.preparedDocument(ticket);
    assert.equal(
      evaluate(
        prepared.layers[0],
        bridge.preparedFonts(ticket),
        prepared.imageAssets,
        prepared,
      ).parameterValue('sink', 'value'),
      7,
    );
    assert.deepEqual(owner.projectSnapshot(), before);
    assert.equal(Object.isFrozen(input.layers[0]), false);
    assert.equal(Object.isFrozen(prepared.layers[0]), true);
    bridge.activate(ticket);
    const token = await owner.prepareCommand(
      {
        ...owner.state,
        type: 'set-variable-font',
        payload: {
          layerId: 'art-a',
          nodeId: 'variable-text',
          fontId: asset.id,
          axes: { wght: 900 },
        },
      },
      bridge,
    );
    const preparedPreview = owner.preparedSnapshot(token),
      preview = preparedPreview.document;
    assert.equal(
      evaluate(
        preview.layers[0],
        preparedPreview.fonts,
        preview.imageAssets,
        preview,
      ).parameterValue('sink', 'value'),
      7,
    );
    owner.commitPrepared(token);
    assert.equal(
      owner.snapshot().layers[0].nodes.find((n) => n.id === 'variable-text').variation.axes.wght,
      900,
    );
    owner.commitPrepared(await owner.prepareCommand({ ...owner.state, type: 'undo' }, bridge));
    assert.deepEqual(
      owner.snapshot().layers[0].nodes.find((n) => n.id === 'variable-text').variation.axes,
      {},
    );
    owner.commitPrepared(await owner.prepareCommand({ ...owner.state, type: 'redo' }, bridge));
    assert.equal(
      owner.snapshot().layers[0].nodes.find((n) => n.id === 'variable-text').variation.axes.wght,
      900,
    );
    const saved = deserializeCompositionProject(
      serializeCompositionProject(owner.projectSnapshot()),
    );
    const openDocument = activeCompositionDocument(saved, 'comp-a');
    bindProjectDocumentContext(openDocument, saved, 'comp-a');
    bridge.activate(await bridge.prepareDocument(openDocument));
    const reopened = createCompositionSessionOwner(saved, {
      activeCompositionId: 'comp-a',
      fonts: () => bridge.fonts(),
    });
    assert.equal(read(reopened), 7);
    assert.equal(
      reopened.snapshot().layers[0].nodes.find((n) => n.id === 'variable-text').variation.axes.wght,
      900,
    );
  } finally {
    bridge.dispose();
  }
});

test('schema 1 source migration keeps generated IDs stable through commit, Undo and Redo', () => {
  const project = fixture();
  project.schemaVersion = 1;
  project.compositionSchemaVersion = 1;
  for (const composition of project.compositions) {
    delete composition.editor;
    const layer = composition.layers[0],
      output = layer.nodes.find((n) => n.id === layer.output);
    output.params.path.type = 'Path';
  }
  const owner = createCompositionSessionOwner(project, { activeCompositionId: 'comp-a' });
  apply(owner, ref);
  assert.equal(read(owner), 7);
  const source = owner.projectSnapshot().compositions[1].layers[0];
  const migrated = source.nodes.find((n) => n.type === 'Fill');
  assert.ok(migrated);
  const persistedId = migrated.id;
  const stableSnapshot = owner.snapshot();
  const stableProject = serializeCompositionProject(owner.projectSnapshot());
  for (let i = 0; i < 3; i++) {
    assert.equal(
      evaluate(
        stableSnapshot.layers[0],
        {},
        stableSnapshot.imageAssets,
        stableSnapshot,
      ).parameterValue('sink', 'value'),
      7,
    );
    assert.equal(serializeCompositionProject(owner.projectSnapshot()), stableProject);
    assert.equal(
      owner.projectSnapshot().compositions[1].layers[0].nodes.find((n) => n.type === 'Fill').id,
      persistedId,
    );
  }
  send(owner, 'undo');
  assert.ok(
    !owner.projectSnapshot().compositions[1].layers[0].nodes.some((n) => n.type === 'Fill'),
  );
  send(owner, 'redo');
  assert.equal(
    owner.projectSnapshot().compositions[1].layers[0].nodes.find((n) => n.type === 'Fill').id,
    persistedId,
  );
  const reopened = createCompositionSessionOwner(
    deserializeCompositionProject(serializeCompositionProject(owner.projectSnapshot())),
  );
  assert.equal(read(reopened), 7);
  assert.equal(
    reopened.projectSnapshot().compositions[1].layers[0].nodes.find((n) => n.type === 'Fill').id,
    persistedId,
  );
});
