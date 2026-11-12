import test from 'node:test';
import assert from 'node:assert/strict';
import { blankDocument, connect, evaluate, node, point, setCharacterOffsets } from '../src/core.js';
import {
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

function sourceProject() {
  const doc = blankDocument(),
    layer = doc.layers[0];
  doc.schemaVersion = 65;
  layer.id = 'source-layer';
  layer.nodes[0].id = 'source-output';
  layer.output = 'source-output';
  const driver = node('Value'),
    sink = node('Value'),
    text = node('Text');
  driver.id = 'driver';
  driver.params.value.value = 37;
  sink.id = 'sink';
  sink.params.value.source = 'driver';
  sink.params.value.expression = 'ref("missing", "value")';
  text.id = 'text';
  text.text = 'AV';
  text.characterPositions = setCharacterOffsets(text.text, undefined, { c1: [3, -4] });
  const paths = ['a', 'b'].map((suffix) => {
    const n = node('Multi Path'),
      a = point(10, 20),
      b = point(100, 20),
      c = point(50, 100);
    n.id = `path-${suffix}`;
    a.id = `point-${suffix}-1`;
    b.id = `point-${suffix}-2`;
    c.id = `point-${suffix}-3`;
    n.points = [a, b, c];
    // Contours and units are local to their Multi Path. The same contour ID is
    // deliberately shared with another path and a global node ID.
    n.contours = [{ id: 'driver', points: [a.id, b.id, c.id], closed: true }];
    n.units = [{ id: 'unit', contours: ['driver'] }];
    n.metadata = { id: 'driver', source: 'point-a-1', nested: ['driver', 'point-a-1'] };
    n.notes = ['driver', 'point-a-1'];
    return n;
  });
  const fill = node('Fill');
  fill.id = 'fill';
  layer.nodes.push(driver, sink, text, ...paths, fill);
  connect(layer, 'path-a', 'fill', 'path');
  connect(layer, 'fill', layer.output, 'path');
  layer.nodeGroups = [{ id: 'group', name: 'Group', nodeIds: ['driver', 'path-a', 'path-b'] }];
  const project = migrateLegacyDocumentToCompositionProject(doc);
  project.compositions[0].id = 'source-comp';
  return project;
}

function unrelatedProject() {
  const doc = blankDocument();
  doc.schemaVersion = 65;
  const p = migrateLegacyDocumentToCompositionProject(doc);
  p.compositions[0].id = 'destination-comp';
  return p;
}
const send = (owner, type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });

for (const collision of [false, true]) {
  test(`Project Import keeps Node Group and Multi Path references with${collision ? '' : 'out'} collisions`, () => {
    const source = sourceProject(),
      sourceBefore = structuredClone(source),
      destination = collision ? structuredClone(source) : unrelatedProject(),
      owner = createCompositionSessionOwner(destination),
      before = serializeCompositionProject(owner.projectSnapshot()),
      revision = owner.projectRevision;
    const result = send(owner, 'import-project-compositions', { sourceProject: source });
    const after = serializeCompositionProject(owner.projectSnapshot());
    assert.equal(result.changed, true);
    assert.equal(owner.projectRevision, revision + 1);
    assert.equal(owner.historyView.past.length, 1);
    const copied = owner
      .projectSnapshot()
      .compositions.find((c) => c.id === result.importedCompositionIds[0]);
    const layer = copied.layers[0],
      suffix = collision ? '-import-1' : '';
    assert.equal(evaluate(layer).error, null);
    const geometry = (l, id) =>
      evaluate(l)
        .run(id)
        .contours.map((contour) => contour.points.map((p) => p.position));
    assert.deepEqual(
      geometry(layer, 'path-a' + suffix),
      geometry(source.compositions[0].layers[0], 'path-a'),
    );
    assert.deepEqual(
      layer.nodeGroups[0].nodeIds,
      ['driver', 'path-a', 'path-b'].map((id) => id + suffix),
    );
    for (const path of layer.nodes.filter((n) => n.type === 'Multi Path')) {
      const original = source.compositions[0].layers[0].nodes.find(
        (n) => n.id + suffix === path.id,
      );
      assert.deepEqual(
        path.contours[0].points,
        path.points.map((p) => p.id),
      );
      assert.deepEqual(path.units[0].contours, [path.contours[0].id]);
      assert.deepEqual(
        path.points.map((p) => p.id),
        original.points.map((p) => p.id + suffix),
      );
      assert.equal(path.contours[0].id, 'driver' + suffix);
      assert.deepEqual(path.metadata, original.metadata);
      assert.deepEqual(path.notes, original.notes);
    }
    const sink = layer.nodes.find((n) => n.id === 'sink' + suffix);
    assert.equal(sink.params.value.source, 'driver' + suffix);
    assert.equal(sink.params.value.expression, 'ref("missing", "value")');
    assert.equal(
      layer.nodes.find((n) => n.id === 'fill' + suffix).params.path.source,
      'path-a' + suffix,
    );
    assert.equal(
      layer.nodes.find((n) => n.id === 'source-output' + suffix).params.path.source,
      'fill' + suffix,
    );
    assert.deepEqual(
      layer.nodes.find((n) => n.type === 'Text').characterPositions,
      source.compositions[0].layers[0].nodes.find((n) => n.type === 'Text').characterPositions,
    );
    assert.deepEqual(source, sourceBefore);
    const reopened = createCompositionSessionOwner(deserializeCompositionProject(after), {
      activeCompositionId: copied.id,
    });
    assert.equal(serializeCompositionProject(reopened.projectSnapshot()), after);
    assert.deepEqual(
      geometry(reopened.activeDocument().layers[0], 'path-a' + suffix),
      geometry(source.compositions[0].layers[0], 'path-a'),
    );
    send(owner, 'undo');
    assert.equal(serializeCompositionProject(owner.projectSnapshot()), before);
    send(owner, 'redo');
    assert.equal(serializeCompositionProject(owner.projectSnapshot()), after);
  });
}

for (const kind of ['group', 'point', 'contour']) {
  test(`Project Import rejects a missing ${kind} array member instead of allocating its target`, () => {
    const source = sourceProject(),
      layer = source.compositions[0].layers[0];
    if (kind === 'group') layer.nodeGroups[0].nodeIds = ['driver-import-1'];
    const path = layer.nodes.find((n) => n.id === 'path-a');
    if (kind === 'point') path.contours[0].points[0] = 'point-a-1-import-1';
    if (kind === 'contour') path.units[0].contours[0] = 'driver-import-1';
    const current = sourceProject(),
      owner = createCompositionSessionOwner(current);
    owner.createComposition({ name: 'Undoable' });
    send(owner, 'undo');
    const sourceBefore = structuredClone(source),
      before = owner.projectSnapshot(),
      state = owner.state,
      history = owner.historyState;
    assert.throws(
      () => send(owner, 'import-project-compositions', { sourceProject: source }),
      kind === 'group'
        ? /Missing group member/
        : kind === 'point'
          ? /Invalid Multi Path point reference/
          : /Invalid Multi Path unit contour/,
    );
    assert.deepEqual(source, sourceBefore);
    assert.deepEqual(owner.projectSnapshot(), before);
    assert.deepEqual(owner.state, state);
    assert.deepEqual(owner.historyState, history);
    send(owner, 'redo');
    assert.equal(owner.projectSnapshot().compositions.length, 2);
  });
}

for (const kind of ['null contour', 'invalid units']) {
  test(`Project Import retains normal validation for a ${kind}`, () => {
    const source = sourceProject(),
      path = source.compositions[0].layers[0].nodes.find((n) => n.id === 'path-a');
    if (kind === 'null contour') path.contours[0] = null;
    else path.units = {};
    const owner = createCompositionSessionOwner(sourceProject()),
      before = owner.projectSnapshot(),
      state = owner.state,
      history = owner.historyState,
      original = structuredClone(source);
    assert.throws(
      () => send(owner, 'import-project-compositions', { sourceProject: source }),
      kind === 'null contour' ? /Invalid Multi Path contour/ : /Invalid Multi Path units/,
    );
    assert.deepEqual(source, original);
    assert.deepEqual(owner.projectSnapshot(), before);
    assert.deepEqual(owner.state, state);
    assert.deepEqual(owner.historyState, history);
  });
}
