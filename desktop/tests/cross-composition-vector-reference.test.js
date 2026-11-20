import test, { mock } from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  blankLayer,
  node,
  evaluate,
  validate,
  CommandSession,
  bindProjectDocumentContext,
  cloneDocument,
  expressionReferenceTargetsForProject,
  connect,
  previewColorExpression,
} from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
  validateCompositionProject,
  activeCompositionDocument,
  applyActiveCompositionDocument,
} from '../src/composition-project-model.js';
import { prepareProjectCompositionsImport } from '../src/composition-project-import.js';

function fixture() {
  const a = blankDocument(),
    b = blankDocument();
  a.layers[0].id = 'art-a';
  b.layers[0].id = 'art-b';
  const sink = node('Value'),
    vector = node('Vector'),
    color = node('Color');
  sink.id = 'sink';
  vector.id = 'vector';
  color.id = 'color';
  a.layers[0].nodes.push(sink, vector, color);
  const curve = node('Curve'),
    driver = node('Vector'),
    transform = node('Transform');
  curve.id = 'curve';
  driver.id = 'driver';
  transform.id = 'transform';
  curve.points[0].id = 'point';
  curve.points[0].position.value = [7, 11];
  curve.points[0].handleA.value = [3, 4];
  driver.params.value.value = [13, 17];
  b.layers[0].nodes.push(curve, driver, transform);
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
const send = (owner, type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });
const apply = (owner, nodeId, key, expression) =>
  send(owner, 'set-parameter-expression', { layerId: 'art-a', nodeId, key, expression });
const ref = (nodeId, key, component) =>
  `ref(componentComposition, "comp-b", "art-b", "${nodeId}", "${key}", "${component}")`;
const read = (owner, id, key) => {
  const doc = owner.snapshot();
  return evaluate(doc.layers[0], {}, doc.imageAssets, doc).parameterValue(id, key);
};

test('source-local Curve and Vector components remain live through history and reload', () => {
  const owner = createCompositionSessionOwner(fixture());
  const x = ref('curve', 'point.position', 'x');
  const y = ref('curve', 'point.position', 'y');
  const length = ref('curve', 'point.handleA', 'length');
  const candidates = expressionReferenceTargetsForProject(
    owner.projectSnapshot(),
    'comp-a',
    'art-a',
    'sink',
    'Scalar',
    'value',
  );
  assert.ok(candidates.some((target) => target.expression === x && target.component === 'x'));
  assert.ok(candidates.some((target) => target.expression === length));
  const vectorCandidates = expressionReferenceTargetsForProject(
    owner.projectSnapshot(),
    'comp-a',
    'art-a',
    'vector',
    'Vector2',
    'value',
  );
  assert.ok(vectorCandidates.some((target) => target.expression === x));
  assert.ok(vectorCandidates.some((target) => target.expression === y));
  assert.ok(
    !vectorCandidates.some(
      (target) => target.compositionId === 'comp-b' && target.component === undefined,
    ),
  );
  apply(owner, 'sink', 'value', length);
  apply(owner, 'vector', 'value', `${x}; ${y}`);
  apply(owner, 'color', 'value', `alpha(value, ${length} / 10)`);
  assert.equal(owner.projectSnapshot().schemaVersion, 65);
  assert.equal(read(owner, 'sink', 'value'), 5);
  assert.deepEqual(read(owner, 'vector', 'value'), [7, 11]);
  assert.equal(read(owner, 'color', 'value').alpha, 0.5);
  owner.switchComposition('comp-b');
  send(owner, 'set-parameter', {
    layerId: 'art-b',
    nodeId: 'curve',
    key: 'point.handleA',
    value: [6, 8],
  });
  owner.switchComposition('comp-a');
  assert.equal(read(owner, 'sink', 'value'), 10);
  assert.equal(read(owner, 'color', 'value').alpha, 1);
  send(owner, 'undo');
  assert.equal(read(owner, 'sink', 'value'), 5);
  assert.equal(read(owner, 'color', 'value').alpha, 0.5);
  send(owner, 'redo');
  assert.equal(read(owner, 'sink', 'value'), 10);
  const reopened = createCompositionSessionOwner(
    deserializeCompositionProject(serializeCompositionProject(owner.projectSnapshot())),
  );
  assert.equal(read(reopened, 'sink', 'value'), 10);
  assert.deepEqual(read(reopened, 'vector', 'value'), [7, 11]);
  assert.equal(read(reopened, 'color', 'value').alpha, 1);
});

test('projection is source-local before a connected nonuniform downstream Transform', () => {
  const project = fixture();
  project.schemaVersion = 27;
  const layer = project.compositions[1].layers[0];
  const transform = layer.nodes.find((n) => n.id === 'transform');
  transform.params.uniformScale = { type: 'Boolean', value: false, expression: '', locked: false };
  transform.params.scaleWidth = { type: 'Scalar', value: 3, expression: '', locked: false };
  transform.params.scale.value = 2;
  transform.params.offset.value = [100, 200];
  connect(layer, 'curve', 'transform', 'path');
  const owner = createCompositionSessionOwner(project);
  apply(owner, 'sink', 'value', ref('curve', 'point.handleA', 'length'));
  assert.equal(read(owner, 'sink', 'value'), 5);
  apply(
    owner,
    'vector',
    'value',
    `${ref('curve', 'point.position', 'x')}; ${ref('curve', 'point.position', 'y')}`,
  );
  assert.deepEqual(read(owner, 'vector', 'value'), [7, 11]);
  apply(owner, 'sink', 'value', ref('transform', 'offset', 'x'));
  assert.equal(read(owner, 'sink', 'value'), 100);
  owner.switchComposition('comp-b');
  send(owner, 'set-parameter', {
    layerId: 'art-b',
    nodeId: 'transform',
    key: 'offset',
    value: [125, 200],
  });
  owner.switchComposition('comp-a');
  assert.equal(read(owner, 'sink', 'value'), 125);
  send(owner, 'undo');
  assert.equal(read(owner, 'sink', 'value'), 100);
  send(owner, 'redo');
  assert.equal(read(owner, 'sink', 'value'), 125);
});

test('general Vector2 component and connected Curve handle use effective source values', () => {
  const project = fixture();
  project.schemaVersion = 27;
  const layer = project.compositions[1].layers[0];
  layer.nodes.find((n) => n.id === 'curve').points[0].handleA.expression = 'value * 2; value * 2';
  connect(layer, 'driver', 'curve', 'point.handleA');
  const owner = createCompositionSessionOwner(project);
  apply(owner, 'sink', 'value', ref('driver', 'value', 'x'));
  assert.equal(read(owner, 'sink', 'value'), 13);
  apply(owner, 'sink', 'value', ref('curve', 'point.handleA', 'length'));
  assert.equal(read(owner, 'sink', 'value'), Math.hypot(13, 17));
  owner.switchComposition('comp-b');
  send(owner, 'set-parameter', { layerId: 'art-b', nodeId: 'driver', key: 'value', value: [3, 4] });
  owner.switchComposition('comp-a');
  assert.equal(read(owner, 'sink', 'value'), 5);
  send(owner, 'undo');
  assert.equal(read(owner, 'sink', 'value'), Math.hypot(13, 17));
  send(owner, 'redo');
  assert.equal(read(owner, 'sink', 'value'), 5);
  const doc = owner.snapshot();
  assert.equal(
    previewColorExpression(
      doc.layers[0],
      'color',
      'value',
      `alpha(value, ${ref('driver', 'value', 'x')} / 10)`,
      {},
      doc.imageAssets,
      doc,
    ).alpha,
    0.3,
  );
  owner.switchComposition('comp-b');
  send(owner, 'disconnect', { layerId: 'art-b', nodeId: 'curve', key: 'point.handleA' });
  owner.switchComposition('comp-a');
  assert.equal(read(owner, 'sink', 'value'), 10);
});

test('typed Color and Component Composition references can share a Color expression', () => {
  const project = fixture(),
    source = node('Color');
  source.id = 'source-color';
  source.params.value.value = { hex: '#123456', alpha: 0.8 };
  project.compositions[1].layers[0].nodes.push(source);
  const owner = createCompositionSessionOwner(project);
  const expression = `alpha(ref(colorComposition, "comp-b", "art-b", "source-color", "value"), ${ref('curve', 'point.handleA', 'length')} / 10)`;
  const view = owner.snapshot();
  assert.deepEqual(
    previewColorExpression(
      view.layers[0],
      'color',
      'value',
      expression,
      {},
      view.imageAssets,
      view,
    ),
    { hex: '#123456', alpha: 0.5 },
  );
  apply(owner, 'color', 'value', expression);
  assert.deepEqual(read(owner, 'color', 'value'), { hex: '#123456', alpha: 0.5 });
});

test('separate project owners, clones, Undo and reopen keep their own component source context', () => {
  const seed = createCompositionSessionOwner(fixture());
  apply(seed, 'sink', 'value', ref('curve', 'point.handleA', 'length'));
  const firstProject = seed.projectSnapshot();
  const secondProject = {
    ...firstProject,
    compositions: [firstProject.compositions[0], structuredClone(firstProject.compositions[1])],
  };
  secondProject.compositions[1].layers[0].nodes.find(
    (n) => n.id === 'curve',
  ).points[0].handleA.value = [30, 40];
  const first = createCompositionSessionOwner(firstProject),
    second = createCompositionSessionOwner(secondProject);
  assert.equal(read(first, 'sink', 'value'), 5);
  assert.equal(read(second, 'sink', 'value'), 50);
  const cloned = cloneDocument(second.snapshot());
  assert.equal(
    evaluate(cloned.layers[0], {}, cloned.imageAssets, cloned).parameterValue('sink', 'value'),
    50,
  );
  const detached = structuredClone(second.snapshot());
  assert.throws(
    () =>
      evaluate(detached.layers[0], {}, detached.imageAssets, detached).parameterValue(
        'sink',
        'value',
      ),
    /project context/i,
  );
  second.switchComposition('comp-b');
  send(second, 'set-parameter', {
    layerId: 'art-b',
    nodeId: 'curve',
    key: 'point.handleA',
    value: [6, 8],
  });
  second.switchComposition('comp-a');
  assert.equal(read(second, 'sink', 'value'), 10);
  send(second, 'undo');
  assert.equal(read(second, 'sink', 'value'), 50);
  send(second, 'redo');
  assert.equal(read(first, 'sink', 'value'), 5);
  const reopened = createCompositionSessionOwner(
    deserializeCompositionProject(serializeCompositionProject(second.projectSnapshot())),
  );
  assert.equal(read(reopened, 'sink', 'value'), 10);
});

test('nested references use the source Composition context and standalone Apply rejects self scope', () => {
  const project = fixture();
  project.schemaVersion = 65;
  project.compositionSchemaVersion = 2;
  const source = project.compositions[1].layers[0].nodes.find((n) => n.id === 'driver');
  source.params.value.expression =
    'ref(componentComposition, "comp-a", "art-a", "vector", "value", "x"); 4';
  const owner = createCompositionSessionOwner(project);
  const session = new CommandSession(owner.snapshot());
  assert.throws(
    () =>
      apply(
        session,
        'sink',
        'value',
        'ref(componentComposition, "comp-a", "art-a", "vector", "value", "x")',
      ),
    /different/i,
  );
  apply(owner, 'sink', 'value', ref('driver', 'value', 'x'));
  assert.equal(read(owner, 'sink', 'value'), 300);
});

test('invalid hierarchy, projection and source types reject atomically without losing Redo', () => {
  const project = fixture();
  const color = node('Color');
  color.id = 'source-color';
  project.compositions[1].layers[0].nodes.push(color);
  const owner = createCompositionSessionOwner(project);
  send(owner, 'set-parameter', { layerId: 'art-a', nodeId: 'sink', key: 'value', value: 2 });
  send(owner, 'undo');
  const before = owner.projectSnapshot(),
    history = owner.historyState,
    state = owner.state;
  for (const text of [
    ref('source-color', 'value', 'x'),
    ref('curve', 'closed', 'x'),
    ref('driver', 'value', 'length'),
    ref('curve', 'point.position', 'length'),
    ref('driver', 'point.handleA', 'x'),
    ref('curve', 'missing.position', 'x'),
    ref('curve', 'point.invalid', 'x'),
    ref('curve', 'point.position', 'z'),
    'ref(componentComposition, "comp-a", "art-a", "vector", "value", "x")',
    'ref(componentComposition, "comp-b", "art-a", "vector", "value", "x")',
    'ref(componentComposition, "comp-b", "art-b", "sink", "value", "x")',
  ]) {
    assert.throws(() => apply(owner, 'sink', 'value', text), undefined, text);
    assert.deepEqual(owner.projectSnapshot(), before);
    assert.deepEqual(owner.state, state);
    assert.deepEqual(owner.historyState, history);
  }
  send(owner, 'redo');
  assert.equal(read(owner, 'sink', 'value'), 2);
});

test('mixed wired Curve handle and Composition property cycle rejects and preserves Redo', () => {
  const project = fixture();
  const a = node('Combine Vector'),
    b = node('Combine Vector');
  a.id = 'combine-a';
  b.id = 'combine-b';
  b.params.x.value = 3;
  b.params.y.value = 4;
  project.compositions[0].layers[0].nodes.push(a);
  const layer = project.compositions[1].layers[0];
  layer.nodes.push(b);
  connect(layer, 'combine-b', 'curve', 'point.handleA');
  const owner = createCompositionSessionOwner(project);
  apply(owner, 'combine-a', 'x', ref('curve', 'point.handleA', 'length'));
  owner.switchComposition('comp-b');
  send(owner, 'set-parameter', { layerId: 'art-b', nodeId: 'combine-b', key: 'x', value: 8 });
  send(owner, 'undo');
  const before = owner.projectSnapshot(),
    history = owner.historyState;
  assert.throws(
    () =>
      send(owner, 'set-parameter-expression', {
        layerId: 'art-b',
        nodeId: 'combine-b',
        key: 'x',
        expression: 'ref(composition, "comp-a", "art-a", "combine-a", "x")',
      }),
    /cycle/i,
  );
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.historyState, history);
  send(owner, 'redo');
  owner.switchComposition('comp-a');
  assert.equal(read(owner, 'combine-a', 'x'), Math.hypot(8, 4));
});

test('the reverse order of the mixed wired cycle is also rejected', () => {
  const project = fixture();
  const a = node('Combine Vector'),
    b = node('Combine Vector');
  a.id = 'combine-a';
  b.id = 'combine-b';
  project.compositions[0].layers[0].nodes.push(a);
  const layer = project.compositions[1].layers[0];
  layer.nodes.push(b);
  connect(layer, 'combine-b', 'curve', 'point.handleA');
  const owner = createCompositionSessionOwner(project, { activeCompositionId: 'comp-b' });
  send(owner, 'set-parameter-expression', {
    layerId: 'art-b',
    nodeId: 'combine-b',
    key: 'x',
    expression: 'ref(composition, "comp-a", "art-a", "combine-a", "x")',
  });
  owner.switchComposition('comp-a');
  const before = owner.projectSnapshot(),
    history = owner.historyState;
  assert.throws(
    () => apply(owner, 'combine-a', 'x', ref('curve', 'point.handleA', 'length')),
    /cycle/i,
  );
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.historyState, history);
});

test('reading a Curve point remains independent of downstream Curve path output', () => {
  const project = fixture(),
    layer = project.compositions[1].layers[0];
  const combine = node('Combine Vector');
  combine.id = 'combine-b';
  layer.nodes.push(combine);
  connect(layer, 'curve', 'transform', 'path');
  connect(layer, 'combine-b', 'transform', 'offset');
  const owner = createCompositionSessionOwner(project, { activeCompositionId: 'comp-b' });
  send(owner, 'set-parameter-expression', {
    layerId: 'art-b',
    nodeId: 'combine-b',
    key: 'x',
    expression: 'ref(composition, "comp-a", "art-a", "sink", "value")',
  });
  owner.switchComposition('comp-a');
  apply(owner, 'sink', 'value', ref('curve', 'point.position', 'x'));
  assert.equal(read(owner, 'sink', 'value'), 7);
});

test('inactive dependent constraints validate edits and gesture previews atomically', () => {
  const project = fixture(),
    repeat = node('Geometry Repeater');
  project.schemaVersion = 37;
  repeat.id = 'repeat';
  repeat.operation = 'ae-2d';
  project.compositions[0].layers[0].nodes.push(repeat);
  const owner = createCompositionSessionOwner(project);
  apply(owner, 'repeat', 'count', ref('curve', 'point.position', 'x'));
  owner.switchComposition('comp-b');
  const before = owner.projectSnapshot(),
    state = owner.state;
  assert.throws(
    () =>
      send(owner, 'set-parameter', {
        layerId: 'art-b',
        nodeId: 'curve',
        key: 'point.position',
        value: [1000, 11],
      }),
    /repeater/i,
  );
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.state, state);
  const started = send(owner, 'begin-gesture', {
    target: { kind: 'parameter', layerId: 'art-b', nodeId: 'curve', key: 'point.position' },
  });
  assert.throws(
    () =>
      send(owner, 'update-gesture', {
        ...started,
        update: { value: [1000, 11] },
      }),
    /repeater/i,
  );
  send(owner, 'cancel-gesture', started);
  assert.deepEqual(owner.projectSnapshot(), before);
});

test('a connected consumer pauses its component expression and validates when disconnected', () => {
  const project = fixture();
  const local = node('Value');
  local.id = 'local';
  local.params.value.value = 2;
  const layer = project.compositions[0].layers[0];
  layer.nodes.push(local);
  const owner = createCompositionSessionOwner(project);
  apply(owner, 'sink', 'value', `1 / (1 + ${ref('curve', 'point.position', 'x')} - 7)`);
  send(owner, 'connect', { layerId: 'art-a', sourceId: 'local', nodeId: 'sink', key: 'value' });
  owner.switchComposition('comp-b');
  send(owner, 'set-parameter', {
    layerId: 'art-b',
    nodeId: 'curve',
    key: 'point.position',
    value: [6, 11],
  });
  owner.switchComposition('comp-a');
  assert.equal(read(owner, 'sink', 'value'), 2);
  send(owner, 'set-parameter', { layerId: 'art-a', nodeId: 'local', key: 'value', value: 3 });
  send(owner, 'undo');
  const before = owner.projectSnapshot(),
    history = owner.historyState;
  assert.throws(
    () => send(owner, 'disconnect', { layerId: 'art-a', nodeId: 'sink', key: 'value' }),
    /finite|zero|divide/i,
  );
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.historyState, history);
  send(owner, 'redo');
  assert.equal(read(owner, 'sink', 'value'), 3);
});

test('deleting a referenced point diagnoses missing; Undo restores and identity reuse rejects', () => {
  const project = fixture();
  project.compositions[1].layers[0].nodes.find((n) => n.id === 'curve').points[0].id =
    'point-collision';
  const owner = createCompositionSessionOwner(project);
  apply(owner, 'sink', 'value', ref('curve', 'point-collision.handleA', 'length'));
  owner.switchComposition('comp-b');
  send(owner, 'delete-point', { layerId: 'art-b', nodeId: 'curve', pointId: 'point-collision' });
  owner.switchComposition('comp-a');
  assert.throws(() => read(owner, 'sink', 'value'), /missing/i);
  send(owner, 'undo');
  assert.equal(read(owner, 'sink', 'value'), 5);
  send(owner, 'redo');
  assert.throws(() => read(owner, 'sink', 'value'), /missing/i);
  const reopened = createCompositionSessionOwner(
    deserializeCompositionProject(serializeCompositionProject(owner.projectSnapshot())),
  );
  assert.throws(() => read(reopened, 'sink', 'value'), /missing/i);
  owner.switchComposition('comp-b');
  const before = owner.projectSnapshot(),
    history = owner.historyState;
  const random = mock.method(globalThis.crypto, 'randomUUID', () => 'collision');
  try {
    assert.throws(
      () =>
        send(owner, 'create-point', {
          layerId: 'art-b',
          nodeId: 'curve',
          position: [1, 2],
        }),
      /retarget/i,
    );
  } finally {
    random.mock.restore();
  }
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.historyState, history);
});

test('deleting the source node retains a missing reference and refuses node ID reuse', () => {
  const project = fixture();
  project.compositions[1].layers[0].nodes.find((n) => n.id === 'driver').id = 'node-collision';
  const owner = createCompositionSessionOwner(project);
  apply(owner, 'sink', 'value', ref('node-collision', 'value', 'x'));
  owner.switchComposition('comp-b');
  send(owner, 'delete-node', { layerId: 'art-b', nodeId: 'node-collision' });
  owner.switchComposition('comp-a');
  assert.throws(() => read(owner, 'sink', 'value'), /missing/i);
  owner.switchComposition('comp-b');
  const before = owner.projectSnapshot();
  const random = mock.method(globalThis.crypto, 'randomUUID', () => 'collision');
  try {
    assert.throws(
      () => send(owner, 'create-node', { layerId: 'art-b', nodeType: 'Vector' }),
      /retarget|collid/i,
    );
  } finally {
    random.mock.restore();
  }
  assert.deepEqual(owner.projectSnapshot(), before);
  send(owner, 'undo');
  owner.switchComposition('comp-a');
  assert.equal(read(owner, 'sink', 'value'), 13);
});

test('deleting the source Composition leaves a missing diagnostic and Undo restores it', () => {
  const owner = createCompositionSessionOwner(fixture());
  apply(owner, 'sink', 'value', ref('curve', 'point.handleA', 'length'));
  owner.deleteComposition('comp-b');
  assert.throws(() => read(owner, 'sink', 'value'), /missing/i);
  send(owner, 'undo');
  assert.equal(read(owner, 'sink', 'value'), 5);
  send(owner, 'delete-composition', {
    compositionId: 'comp-b',
    expectedName: 'Source',
    expectedProjectRevision: owner.projectRevision,
  });
  assert.throws(() => read(owner, 'sink', 'value'), /missing/i);
  send(owner, 'undo');
  assert.equal(read(owner, 'sink', 'value'), 5);
});

test('legacy opaque reserved prefix blocks promotion across all descriptors and presets', () => {
  for (const version of [1, 11, 59, 60, 61, 62, 63, 64])
    for (const descriptor of ['connected', 'unsupported', 'string', 'point', 'edit', 'preset']) {
      const project = fixture();
      project.schemaVersion = version;
      project.compositionSchemaVersion = version >= 60 ? 2 : 1;
      const layer = project.compositions[1].layers[0];
      const opaque = 'ref(componentComposition, malformed';
      if (descriptor === 'connected') {
        const q = layer.nodes.find((n) => n.id === 'driver').params.value;
        q.expression = opaque;
        q.source = layer.output;
      }
      if (descriptor === 'unsupported') layer.nodes[0].params.path.expression = opaque;
      if (descriptor === 'string')
        layer.nodes.find((n) => n.id === 'driver').params.extra = {
          type: 'String',
          value: 'opaque',
          expression: opaque,
        };
      if (descriptor === 'point')
        layer.nodes.find((n) => n.id === 'curve').points[0].position.expression = opaque;
      if (descriptor === 'edit') {
        const remap = node('Path Remap');
        remap.edits.push({
          params: {
            position: { type: 'Vector2', value: [0, 0], expression: opaque, locked: false },
          },
        });
        layer.nodes.push(remap);
      }
      if (descriptor === 'preset')
        project.nodePresets = [
          {
            id: 'opaque-preset',
            nodes: [{ params: { payload: { type: 'Boolean', value: false, expression: opaque } } }],
          },
        ];
      assert.deepEqual(
        deserializeCompositionProject(serializeCompositionProject(project)),
        project,
      );
      const edited = activeCompositionDocument(project, 'comp-a');
      edited.schemaVersion = 65;
      assert.throws(
        () => applyActiveCompositionDocument(project, 'comp-a', edited),
        /opaque.*65/i,
        descriptor,
      );
    }
});

test('strict old Document rejects new syntax, while old wrapper preserves it as opaque', () => {
  const project = fixture();
  project.schemaVersion = 64;
  project.compositionSchemaVersion = 2;
  project.compositions[0].layers[0].nodes.find((n) => n.id === 'sink').params.value.expression =
    ref('driver', 'value', 'x');
  assert.deepEqual(deserializeCompositionProject(serializeCompositionProject(project)), project);
  assert.throws(() => validate(activeCompositionDocument(project, 'comp-a')), /65/i);
});

test('schema 65 rejects malformed reserved syntax in connected, unsupported and preset descriptors', () => {
  for (const descriptor of ['connected', 'unsupported', 'string', 'point', 'edit', 'preset']) {
    const project = fixture(),
      layer = project.compositions[1].layers[0];
    project.schemaVersion = 65;
    project.compositionSchemaVersion = 2;
    const opaque = 'ref(componentComposition, malformed';
    if (descriptor === 'connected') {
      const q = layer.nodes.find((n) => n.id === 'driver').params.value;
      q.expression = opaque;
      q.source = layer.output;
    }
    if (descriptor === 'unsupported') layer.nodes[0].params.path.expression = opaque;
    if (descriptor === 'string')
      layer.nodes.find((n) => n.id === 'driver').params.extra = {
        type: 'String',
        value: 'opaque',
        expression: opaque,
      };
    if (descriptor === 'point')
      layer.nodes.find((n) => n.id === 'curve').points[0].position.expression = opaque;
    if (descriptor === 'edit') {
      const remap = node('Path Remap');
      remap.edits.push({
        params: { position: { type: 'Vector2', value: [0, 0], expression: opaque } },
      });
      layer.nodes.push(remap);
    }
    if (descriptor === 'preset')
      project.nodePresets = [
        {
          id: 'bad-preset',
          nodes: [{ params: { payload: { type: 'Boolean', value: false, expression: opaque } } }],
        },
      ];
    assert.throws(() => validateCompositionProject(project), undefined, descriptor);
  }
});

test('standalone Apply and isolated preview preflight every earlier project syntax boundary', () => {
  for (const [version, opaque] of [
    [64, 'ref(componentComposition, malformed'],
    [63, 'ref(colorArtwork, malformed'],
    [62, 'ref(composition, malformed'],
    [61, 'ref(artwork, malformed'],
    [60, 'ref("legacy-curve", "missing.handleA", "x")'],
  ]) {
    const project = fixture(),
      c = blankDocument();
    project.schemaVersion = version;
    project.compositionSchemaVersion = 2;
    c.layers[0].id = 'art-c';
    const holder = node('Value');
    holder.id = 'legacy-holder';
    holder.params.value.expression = opaque;
    c.layers[0].nodes.push(holder);
    if (version === 60) {
      const curve = node('Curve');
      curve.id = 'legacy-curve';
      c.layers[0].nodes.push(curve);
    }
    project.compositions.push({
      id: 'comp-c',
      name: 'Opaque',
      width: c.width,
      height: c.height,
      layers: c.layers,
      editor: c.editor,
    });
    const original = structuredClone(project);
    assert.deepEqual(deserializeCompositionProject(serializeCompositionProject(project)), project);
    const doc = activeCompositionDocument(project, 'comp-a');
    bindProjectDocumentContext(doc, project, 'comp-a');
    const session = new CommandSession(doc);
    send(session, 'set-parameter', { layerId: 'art-a', nodeId: 'sink', key: 'value', value: 2 });
    send(session, 'undo');
    const before = session.snapshot(),
      state = session.state;
    const future = session.historyView.future.length;
    const component = ref('curve', 'point.handleA', 'length');
    assert.throws(
      () => apply(session, 'sink', 'value', component),
      /opaque|unresolved/i,
      String(version),
    );
    assert.deepEqual(session.snapshot(), before);
    assert.deepEqual(session.state, state);
    assert.equal(session.historyView.future.length, future);
    const view = session.snapshot();
    assert.throws(
      () =>
        previewColorExpression(
          view.layers[0],
          'color',
          'value',
          `alpha(value, ${component} / 10)`,
          {},
          view.imageAssets,
          view,
        ),
      /opaque|unresolved/i,
      String(version),
    );
    assert.deepEqual(session.snapshot(), before);
    assert.deepEqual(session.state, state);
    assert.equal(session.historyView.future.length, future);
    assert.deepEqual(project, original);
    send(session, 'redo');
    assert.equal(
      session.snapshot().layers[0].nodes.find((n) => n.id === 'sink').params.value.value,
      2,
    );
  }
});

test('wrapper accepts schema 64–66 and rejects future schema 67', () => {
  const project = fixture();
  project.compositionSchemaVersion = 2;
  for (const schemaVersion of [64, 65, 66])
    assert.doesNotThrow(() => validateCompositionProject({ ...project, schemaVersion }));
  assert.throws(
    () => validateCompositionProject({ ...project, schemaVersion: 67 }),
    /unsupported/i,
  );
});

test('unrelated clipboard data from schema 65 does not promote an older restored document', () => {
  for (const version of [63, 64]) {
    const project = fixture();
    project.schemaVersion = version;
    project.compositionSchemaVersion = 2;
    const owner = createCompositionSessionOwner(project);
    const session = new CommandSession(owner.snapshot());
    apply(session, 'sink', 'value', ref('curve', 'point.position', 'x'));
    assert.equal(session.snapshot().schemaVersion, 65);
    send(session, 'copy-nodes', { layerId: 'art-a', nodeIds: ['color'] });
    send(session, 'undo');
    send(session, 'paste-nodes', { layerId: 'art-a' });
    assert.equal(session.snapshot().schemaVersion, version);
  }
});

test('clipboard from schema 65 retains the required schema of copied numeric Artwork references', () => {
  const project = fixture();
  project.schemaVersion = 60;
  project.compositionSchemaVersion = 2;
  const local = node('Value');
  local.id = 'local';
  project.compositions[0].layers[0].nodes.push(local);
  const extra = blankLayer('Extra'),
    driver = node('Value');
  extra.id = 'art-extra';
  driver.id = 'extra-driver';
  driver.params.value.value = 19;
  extra.nodes.push(driver);
  project.compositions[0].layers.push(extra);
  const owner = createCompositionSessionOwner(project),
    session = new CommandSession(owner.snapshot());
  apply(session, 'sink', 'value', ref('curve', 'point.position', 'x'));
  apply(session, 'local', 'value', 'ref(artwork, "art-extra", "extra-driver", "value")');
  send(session, 'copy-nodes', { layerId: 'art-a', nodeIds: ['local'] });
  send(session, 'undo');
  send(session, 'undo');
  assert.equal(session.snapshot().schemaVersion, 60);
  const pastedId = send(session, 'paste-nodes', { layerId: 'art-a' }).nodeIds[0];
  const pasted = session.snapshot();
  assert.equal(pasted.schemaVersion, 62);
  assert.equal(
    evaluate(pasted.layers[0], {}, pasted.imageAssets, pasted).parameterValue(pastedId, 'value'),
    19,
  );
});

test('copy, duplicate, preset and Composition imports refuse atomically; unrelated import stays older', () => {
  const owner = createCompositionSessionOwner(fixture());
  apply(owner, 'sink', 'value', ref('curve', 'point.position', 'x'));
  const source = owner.projectSnapshot(),
    before = owner.projectSnapshot(),
    state = owner.state;
  for (const [type, payload] of [
    ['copy-nodes', { layerId: 'art-a', nodeIds: ['sink'] }],
    ['duplicate-nodes', { layerId: 'art-a', nodeIds: ['sink'] }],
    ['duplicate-layer', { layerId: 'art-a' }],
    ['save-node-preset', { layerId: 'art-a', nodeIds: ['sink'], name: 'Attempt' }],
    [
      'import-project-compositions',
      { sourceProject: source, compositionIds: ['comp-a', 'comp-b'] },
    ],
  ]) {
    assert.throws(() => send(owner, type, payload), /not support|external/i, type);
    assert.deepEqual(owner.projectSnapshot(), before);
    assert.deepEqual(owner.state, state);
  }
  for (const ids of [['comp-a'], ['comp-a', 'comp-b']])
    assert.throws(() => prepareProjectCompositionsImport(fixture(), source, ids), /not supported/i);
  const imported = prepareProjectCompositionsImport(fixture(), source, ['comp-b']).project;
  assert.ok(imported.schemaVersion < 65);
  const older = fixture();
  older.schemaVersion = 64;
  older.compositionSchemaVersion = 2;
  older.compositions[0].layers[0].nodes.find((n) => n.id === 'sink').params.value.expression =
    'ref(componentComposition, malformed';
  assert.throws(() => prepareProjectCompositionsImport(source, older, ['comp-a']), /opaque.*65/i);
});
