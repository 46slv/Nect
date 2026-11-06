import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  blankLayer,
  node,
  evaluate,
  CommandSession,
  serialize,
  deserialize,
  bindProjectDocumentContext,
  previewColorExpression,
} from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
  activeCompositionDocument,
  applyActiveCompositionDocument,
} from '../src/composition-project-model.js';

function fixture(version = 60) {
  const a = blankDocument(),
    b = blankDocument();
  a.layers[0].id = 'art-a';
  b.layers[0].id = 'art-b';
  const sink = node('Value'),
    local = node('Value'),
    color = node('Color'),
    vector = node('Vector');
  sink.id = 'sink';
  local.id = 'local';
  color.id = 'color';
  vector.id = 'vector';
  const localCurve = node('Curve');
  localCurve.id = 'local-curve';
  localCurve.points[0].id = 'local-point';
  localCurve.points[0].handleA.value = [3, 4];
  a.layers[0].nodes.push(sink, local, color, vector, localCurve);
  const curve = node('Curve'),
    driver = node('Value');
  curve.id = 'curve';
  curve.points[0].id = 'point';
  curve.points[0].handleA.value = [3, 4];
  driver.id = 'driver';
  driver.params.value.value = 7;
  b.layers[0].nodes.push(curve, driver);
  const extra = blankLayer('Extra'),
    extraDriver = node('Value'),
    extraColor = node('Color');
  extra.id = 'art-extra';
  extraDriver.id = 'extra-driver';
  extraDriver.params.value.value = 19;
  extraColor.id = 'extra-color';
  extra.nodes.push(extraDriver, extraColor);
  a.layers.push(extra);
  const project = migrateLegacyDocumentToCompositionProject(a);
  project.schemaVersion = version;
  project.compositionSchemaVersion = version >= 60 ? 2 : 1;
  project.compositions[0].id = 'comp-a';
  project.compositions.push({
    id: 'comp-b',
    name: 'B',
    width: b.width,
    height: b.height,
    layers: b.layers,
    editor: b.editor,
  });
  return project;
}
const send = (session, type, payload = {}) => session.dispatch({ ...session.state, type, payload });
const setExpr = (session, nodeId, key, expression) =>
  send(session, 'set-parameter-expression', {
    layerId: 'art-a',
    nodeId,
    key,
    expression,
  });
const component =
  'ref(componentComposition, "comp-b", "art-b", "curve", "point.handleA", "length")';
const boundSession = (project) =>
  new CommandSession(createCompositionSessionOwner(project).snapshot());

test('schema 65 clipboard transfers unrelated Color into schema 60 or 61 without upgrading', () => {
  for (const version of [60, 61]) {
    const session = boundSession(fixture(version));
    setExpr(session, 'sink', 'value', component);
    send(session, 'copy-nodes', { layerId: 'art-a', nodeIds: ['color'] });
    send(session, 'undo');
    const before = session.snapshot();
    send(session, 'paste-nodes', { layerId: 'art-a' });
    const after = session.snapshot();
    assert.equal(after.schemaVersion, version);
    send(session, 'undo');
    assert.deepEqual(session.snapshot(), before);
    send(session, 'redo');
    assert.deepEqual(session.snapshot(), after);
  }
});

test('schema 65 clipboard retains the actual Curve handle and Artwork floors', () => {
  for (const [expression, expected, value] of [
    ['ref("local-curve", "local-point.handleA", "length")', 61, 5],
    ['ref(artwork, "art-extra", "extra-driver", "value")', 62, 19],
  ]) {
    const session = boundSession(fixture(60));
    setExpr(session, 'sink', 'value', component);
    setExpr(session, 'local', 'value', expression);
    send(session, 'copy-nodes', { layerId: 'art-a', nodeIds: ['local'] });
    send(session, 'undo');
    send(session, 'undo');
    const id = send(session, 'paste-nodes', { layerId: 'art-a' }).nodeIds[0];
    const doc = session.snapshot();
    assert.equal(doc.schemaVersion, expected);
    assert.equal(
      evaluate(doc.layers[0], {}, doc.imageAssets, doc).parameterValue(id, 'value'),
      value,
    );
  }
});

test('schema 64 clipboard also preserves an unrelated restored schema 61', () => {
  const session = boundSession(fixture(61));
  setExpr(session, 'color', 'value', 'ref(colorArtwork, "art-extra", "extra-color", "value")');
  send(session, 'copy-nodes', { layerId: 'art-a', nodeIds: ['vector'] });
  send(session, 'undo');
  send(session, 'paste-nodes', { layerId: 'art-a' });
  assert.equal(session.snapshot().schemaVersion, 61);
});

test('schema 65 clipboard guards inactive missing-owner length and preserves x/y opacity', () => {
  for (const componentName of ['length', 'x', 'y']) {
    const original = fixture(60),
      session = boundSession(original);
    setExpr(session, 'sink', 'value', component);
    setExpr(session, 'local', 'value', 'ref("local-curve", "local-point.handleA", "length")');
    send(session, 'copy-nodes', { layerId: 'art-a', nodeIds: ['local'] });
    send(session, 'undo');
    send(session, 'undo');
    const project = structuredClone(original),
      holder = node('Value');
    holder.id = 'missing-holder';
    holder.params.value.expression = `ref("deleted", "missing.handleA", "${componentName}")`;
    project.compositions[1].layers[0].nodes.push(holder);
    assert.deepEqual(deserializeCompositionProject(serializeCompositionProject(project)), project);
    session.setProjectContext(project, 'comp-a');
    const before = session.snapshot(),
      state = session.state,
      future = session.historyView.future.length;
    if (componentName === 'length') {
      assert.throws(() => send(session, 'paste-nodes', { layerId: 'art-a' }), /unresolved/i);
      assert.deepEqual(session.snapshot(), before);
      assert.deepEqual(session.state, state);
      assert.equal(session.historyView.future.length, future);
      send(session, 'redo');
      assert.equal(session.snapshot().schemaVersion, 65);
    } else {
      send(session, 'paste-nodes', { layerId: 'art-a' });
      assert.equal(session.snapshot().schemaVersion, 61);
      const next = applyActiveCompositionDocument(project, 'comp-a', session.snapshot());
      assert.equal(
        next.compositions[1].layers[0].nodes.find((n) => n.id === 'missing-holder').params.value
          .expression,
        holder.params.value.expression,
      );
    }
  }
});

test('standalone Apply and isolated preview reject inactive opaque missing-owner length', () => {
  const project = fixture(60),
    holder = node('Value');
  holder.id = 'missing-holder';
  holder.params.value.expression = 'ref("deleted", "missing.handleA", "length")';
  project.compositions[1].layers[0].nodes.push(holder);
  const doc = activeCompositionDocument(project, 'comp-a');
  bindProjectDocumentContext(doc, project, 'comp-a');
  const session = new CommandSession(doc);
  send(session, 'set-parameter', { layerId: 'art-a', nodeId: 'sink', key: 'value', value: 2 });
  send(session, 'undo');
  const before = session.snapshot(),
    state = session.state,
    future = session.historyView.future.length;
  assert.throws(() => setExpr(session, 'sink', 'value', component), /unresolved/i);
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
    /unresolved/i,
  );
  assert.deepEqual(session.snapshot(), before);
  assert.deepEqual(session.state, state);
  assert.equal(session.historyView.future.length, future);
  send(session, 'redo');
  assert.equal(
    session.snapshot().layers[0].nodes.find((n) => n.id === 'sink').params.value.value,
    2,
  );
});

test('strict schema 65 Document rejects new component syntax in the wrong expression grammar', () => {
  for (const [nodeId, expression] of [
    ['sink', `alpha(value, ${component} / 10)`],
    ['vector', component],
  ]) {
    for (const connected of [false, true]) {
      const project = fixture(65),
        layer = project.compositions[0].layers[0];
      const doc = activeCompositionDocument(project, 'comp-a');
      const q = layer.nodes.find((n) => n.id === nodeId).params.value;
      const docQ = doc.layers[0].nodes.find((n) => n.id === nodeId).params.value;
      q.expression = expression;
      docQ.expression = expression;
      if (connected) {
        const source = node(nodeId === 'vector' ? 'Vector' : 'Value');
        source.id = 'connected-source';
        layer.nodes.push(source);
        doc.layers[0].nodes.push(structuredClone(source));
        q.source = source.id;
        docQ.source = source.id;
      }
      assert.throws(
        () => serializeCompositionProject(project),
        undefined,
        `${nodeId}:${connected}`,
      );
      assert.throws(() => deserialize(serialize(doc)), undefined, `${nodeId}:${connected}`);
    }
    const session = boundSession(fixture(64));
    send(session, 'set-parameter', { layerId: 'art-a', nodeId: 'sink', key: 'value', value: 2 });
    send(session, 'undo');
    const before = session.snapshot(),
      state = session.state,
      future = session.historyView.future.length;
    assert.throws(() => setExpr(session, nodeId, 'value', expression));
    assert.deepEqual(session.snapshot(), before);
    assert.deepEqual(session.state, state);
    assert.equal(session.historyView.future.length, future);
    send(session, 'redo');
    assert.equal(
      session.snapshot().layers[0].nodes.find((n) => n.id === 'sink').params.value.value,
      2,
    );
  }
});

test('pre-65 pure wrapper rejects mixed old qualifier expressions while component-only remains opaque', () => {
  const cases = [
    [62, 'sink', `ref(artwork, "art-extra", "extra-driver", "value") + ${component}`],
    [62, 'vector', `ref(artwork, "art-extra", "extra-driver", "value"); ${component}`],
    [
      62,
      'color',
      `alpha(value, (ref(artwork, "art-extra", "extra-driver", "value") + ${component}) / 100)`,
    ],
    [63, 'sink', `ref(composition, "comp-b", "art-b", "driver", "value") + ${component}`],
    [
      63,
      'color',
      `alpha(value, (ref(composition, "comp-b", "art-b", "driver", "value") + ${component}) / 100)`,
    ],
    [
      64,
      'color',
      `alpha(ref(colorArtwork, "art-extra", "extra-color", "value"), ${component} / 10)`,
    ],
    [
      64,
      'color',
      `alpha(ref(colorComposition, "comp-b", "art-b", "missing-color", "value"), ${component} / 10)`,
    ],
  ];
  for (const [version, id, text] of cases) {
    const project = fixture(version);
    project.compositions[0].layers[0].nodes.find((n) => n.id === id).params.value.expression = text;
    assert.throws(
      () => serializeCompositionProject(project),
      undefined,
      `${version}:${id}:${text}`,
    );
  }
  const old = fixture(64);
  old.compositions[0].layers[0].nodes.find((n) => n.id === 'sink').params.value.expression =
    component;
  assert.deepEqual(deserializeCompositionProject(serializeCompositionProject(old)), old);
  const edited = activeCompositionDocument(old, 'comp-a');
  edited.schemaVersion = 65;
  assert.throws(() => applyActiveCompositionDocument(old, 'comp-a', edited), /opaque.*65/i);
});

test('old qualifier parsing preserves the old wrapper boundary across descriptor kinds', () => {
  const qualifiers = [
    [62, 'artwork', 'ref(artwork, "art-extra", "extra-driver", "value")', false],
    [63, 'composition', 'ref(composition, "comp-b", "art-b", "driver", "value")', false],
    [64, 'colorArtwork', 'ref(colorArtwork, "art-extra", "extra-color", "value")', true],
    [
      64,
      'colorComposition',
      'ref(colorComposition, "comp-b", "art-b", "missing-color", "value")',
      true,
    ],
  ];
  for (const [version, kind, legacy, isColor] of qualifiers)
    for (const descriptor of [
      'Scalar',
      'Vector2',
      'Color',
      'Boolean',
      'Path',
      'PaintedPath',
      'Image',
      'String',
      'point',
      'edit',
      'preset',
    ])
      for (const connected of [false, true]) {
        const project = fixture(version),
          layer = project.compositions[0].layers[0];
        const type = ['point', 'edit'].includes(descriptor)
          ? 'Vector2'
          : descriptor === 'preset'
            ? 'Boolean'
            : descriptor;
        const expression =
          type === 'Color'
            ? `alpha(${isColor ? legacy : 'value'}, ${isColor ? component : `(${legacy} + ${component})`} / 100)`
            : type === 'Vector2'
              ? `${legacy}; ${component}`
              : `${legacy} + ${component}`;
        const q = {
          type,
          expression,
          locked: false,
          value:
            type === 'Vector2'
              ? [0, 0]
              : type === 'Color'
                ? { hex: '#010203', alpha: 1 }
                : type === 'Boolean'
                  ? false
                  : ['Path', 'PaintedPath', 'Image'].includes(type)
                    ? null
                    : type === 'String'
                      ? ''
                      : 1,
          ...(connected ? { source: 'wire' } : {}),
        };
        if (descriptor === 'point') {
          const curve = node('Curve');
          curve.id = 'holder-curve';
          curve.points[0].position = q;
          layer.nodes.push(curve);
        } else if (descriptor === 'edit') {
          const remap = node('Path Remap');
          remap.id = 'holder-remap';
          remap.edits.push({ pointId: 'edit-pt', params: { position: q } });
          layer.nodes.push(remap);
        } else if (descriptor === 'preset') {
          project.nodePresets = [{ id: 'opaque-preset', nodes: [{ params: { payload: q } }] }];
        } else {
          const holder = node('Value');
          holder.id = 'holder';
          holder.params.extra = q;
          layer.nodes.push(holder);
        }
        if (kind === 'artwork' && descriptor === 'String') {
          assert.deepEqual(
            deserializeCompositionProject(serializeCompositionProject(project)),
            project,
          );
          const edited = activeCompositionDocument(project, 'comp-a');
          edited.schemaVersion = 65;
          assert.throws(
            () => applyActiveCompositionDocument(project, 'comp-a', edited),
            /opaque.*65/i,
          );
        } else
          assert.throws(
            () => serializeCompositionProject(project),
            undefined,
            `${kind}:${descriptor}:${connected}`,
          );
      }
});
