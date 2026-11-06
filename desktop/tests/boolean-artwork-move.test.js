import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  blankLayer,
  bindProjectDocumentContext,
  connect,
  CommandSession,
  deserialize,
  evaluate,
  node,
  pathBounds,
  serialize,
} from '../src/core.js';
import { resolveArtworkMoveTarget } from '../src/artwork-move-target.js';
import { resolveArtworkMoveSelection } from '../src/artwork-multi-move.js';
import { resolveArtworkDuplicateTarget } from '../src/artwork-duplication.js';
import { resolveArtworkAlignment } from '../src/artwork-alignment.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';

function fixture({ transformed = true } = {}) {
  const doc = blankDocument(),
    l = doc.layers[0],
    output = l.nodes[0];
  const outer = node('Circle'),
    inner = node('Circle'),
    boolean = node('Boolean');
  outer.params.center.value = [240, 200];
  outer.params.radius.value = 100;
  inner.params.center.value = [240, 200];
  inner.params.radius.value = 50;
  boolean.operation = 'subtract';
  const transform = transformed ? node('Transform') : null,
    fill = node('Fill');
  l.nodes = [outer, inner, boolean, ...(transform ? [transform] : []), fill, output];
  connect(l, outer.id, boolean.id, 'a');
  connect(l, inner.id, boolean.id, 'b');
  if (transform) connect(l, boolean.id, transform.id, 'path');
  connect(l, (transform || boolean).id, fill.id, 'path');
  connect(l, fill.id, output.id, 'path');
  return { doc, l, outer, inner, boolean, transform, fill, output };
}
function command(session, type, payload = {}) {
  return session.dispatch({ ...session.state, type, payload });
}
const close = (a, b) => assert.ok(Math.abs(a - b) < 1e-6, `${a} != ${b}`);

test('Canvas translation moves only a dedicated Boolean terminal Transform and round-trips', () => {
  for (const [rotation, scale, key] of [
    [0, 1, 'offset'],
    [37, -1.5, 'offset'],
    [-24, 1.2, 'position'],
  ]) {
    const f = fixture();
    f.transform.params.rotation.value = rotation;
    f.transform.params.scale.value = scale;
    if (key === 'position')
      f.transform.params.position = {
        type: 'Vector2',
        value: [12, 4],
        expression: '',
        locked: false,
      };
    const session = new CommandSession(f.doc),
      before = session.snapshot();
    const prior = evaluate(before.layers[0], {}, {}, before).path;
    const selection = resolveArtworkMoveSelection(before, [f.l.id]);
    assert.equal(selection.supported, true, selection.reason);
    assert.equal(selection.target.nodeId, f.transform.id);
    assert.equal(selection.target.parameter, key);
    command(session, 'translate-artworks', { layerIds: [f.l.id], delta: [17.25, -31.5] });
    const after = session.snapshot(),
      result = evaluate(after.layers[0], {}, {}, after).path;
    for (let axis = 0; axis < 2; axis++)
      pathBounds(prior)[axis].forEach((v, i) =>
        close(pathBounds(result)[axis][i], v + [17.25, -31.5][axis]),
      );
    assert.deepEqual(result.fill, prior.fill);
    assert.equal(result.contours.length, prior.contours.length);
    for (const [i, contour] of prior.contours.entries()) {
      const next = result.contours[i];
      assert.equal(next.id, contour.id);
      assert.equal(next.points.length, contour.points.length);
      for (const [j, point] of contour.points.entries()) {
        const movedPoint = next.points[j];
        assert.equal(movedPoint.id, point.id);
        for (let axis = 0; axis < 2; axis++) {
          close(movedPoint.position[axis], point.position[axis] + [17.25, -31.5][axis]);
          close(movedPoint.handleA[axis], point.handleA[axis]);
          close(movedPoint.handleB[axis], point.handleB[axis]);
        }
      }
    }
    const originalNodes = before.layers[0].nodes.filter((n) => n.id !== f.transform.id);
    assert.deepEqual(
      after.layers[0].nodes.filter((n) => n.id !== f.transform.id),
      originalNodes,
    );
    assert.deepEqual(
      after.layers[0].nodes.find((n) => n.id === f.boolean.id).params,
      f.boolean.params,
    );
    const restoredTransform = structuredClone(
      after.layers[0].nodes.find((n) => n.id === f.transform.id),
    );
    restoredTransform.params[key].value = before.layers[0].nodes.find(
      (n) => n.id === f.transform.id,
    ).params[key].value;
    assert.deepEqual(
      restoredTransform,
      before.layers[0].nodes.find((n) => n.id === f.transform.id),
    );
    assert.deepEqual(deserialize(serialize(after)), after);
    command(session, 'undo');
    assert.deepEqual(session.snapshot(), before);
    command(session, 'redo');
    assert.deepEqual(session.snapshot(), after);
  }
});

test('the new route is limited to movement; existing copy and alignment contracts remain strict', () => {
  const f = fixture();
  assert.equal(resolveArtworkMoveSelection(f.doc, [f.l.id]).supported, true);
  assert.equal(resolveArtworkMoveTarget(f.doc, f.l.id).supported, false);
  assert.equal(resolveArtworkDuplicateTarget(f.doc, f.l.id).supported, false);
  assert.equal(
    resolveArtworkAlignment(f.doc, [f.l.id], { reference: 'artboard' }).supported,
    false,
  );
});

test('missing Transform, driven or locked movement and shared output reject without changing Redo', () => {
  for (const mutation of [
    (f) => {
      f.l.nodes = f.l.nodes.filter((n) => n !== f.transform);
      f.fill.params.path.source = f.boolean.id;
    },
    (f) => {
      f.transform.params.offset.locked = true;
    },
    (f) => {
      f.transform.bypassed = true;
    },
    (f) => {
      const extra = node('Transform');
      extra.params.path.source = f.transform.id;
      f.l.nodes.push(extra);
    },
  ]) {
    const f = fixture();
    mutation(f);
    const session = new CommandSession(f.doc);
    command(session, 'set-document-settings', { key: 'width', value: 961 });
    command(session, 'undo');
    const before = session.snapshot(),
      state = session.state;
    assert.equal(resolveArtworkMoveSelection(before, [f.l.id]).supported, false);
    assert.throws(() =>
      command(session, 'translate-artworks', { layerIds: [f.l.id], delta: [3, 4] }),
    );
    assert.deepEqual(session.snapshot(), before);
    assert.deepEqual(session.state, state);
    command(session, 'redo');
    assert.equal(session.snapshot().width, 961);
  }
});

test('paused local and other-Artwork references and Composition output consumers keep the route immovable', () => {
  for (const kind of ['paused-local', 'other-artwork', 'composition-output']) {
    const f = fixture();
    if (kind === 'paused-local') {
      const driver = node('Value'),
        consumer = node('Value');
      consumer.params.value.expression = `ref("${f.transform.id}", "offset", "x")`;
      f.l.nodes.push(driver, consumer);
      connect(f.l, driver.id, consumer.id, 'value');
    } else {
      const other = blankLayer('Other');
      if (kind === 'other-artwork') {
        f.doc.schemaVersion = 62;
        const consumer = node('Value');
        consumer.params.value.expression = `ref(artwork, "${f.l.id}", "${f.transform.id}", "offset", "x")`;
        other.nodes.push(consumer);
      } else {
        f.doc.schemaVersion = 31;
        const consumer = node('Composition Reference');
        consumer.compositionReference = { compositionId: f.l.id };
        other.nodes.push(consumer);
      }
      f.doc.layers.push(other);
    }
    const session = new CommandSession(f.doc),
      before = session.snapshot();
    assert.equal(resolveArtworkMoveSelection(before, [f.l.id]).supported, false, kind);
    assert.throws(
      () => command(session, 'translate-artworks', { layerIds: [f.l.id], delta: [3, 4] }),
      /consumer/i,
    );
    assert.deepEqual(session.snapshot(), before);
  }
});

test('an inactive Composition referencing the Transform blocks the same Session command', () => {
  for (const paused of [false, true]) {
    const f = fixture(),
      project = migrateLegacyDocumentToCompositionProject(f.doc);
    project.compositions[0].id = 'mark-composition';
    const other = blankDocument();
    const consumer = node('Value');
    consumer.params.value.expression = `ref(componentComposition, "mark-composition", "${f.l.id}", "${f.transform.id}", "offset", "x")`;
    other.layers[0].nodes.push(consumer);
    if (paused) {
      const driver = node('Value');
      other.layers[0].nodes.push(driver);
      connect(other.layers[0], driver.id, consumer.id, 'value');
    }
    project.compositions.push({
      id: 'other-composition',
      name: 'Other',
      width: other.width,
      height: other.height,
      layers: other.layers,
      editor: other.editor,
    });
    project.schemaVersion = 65;
    project.compositionSchemaVersion = 2;
    const owner = createCompositionSessionOwner(project);
    command(owner, 'set-document-settings', { key: 'width', value: 961 });
    command(owner, 'undo');
    const before = owner.projectSnapshot(),
      state = owner.state;
    assert.equal(resolveArtworkMoveSelection(owner.snapshot(), [f.l.id]).supported, false);
    assert.throws(
      () => command(owner, 'translate-artworks', { layerIds: [f.l.id], delta: [3, 4] }),
      /consumer/i,
    );
    assert.deepEqual(owner.projectSnapshot(), before);
    assert.deepEqual(owner.state, state);
    command(owner, 'redo');
    assert.equal(owner.snapshot().width, 961);
  }
});

test('inactive preserved nonstandard parameter descriptors cannot hide a route reference', () => {
  const f = fixture(),
    project = migrateLegacyDocumentToCompositionProject(f.doc);
  project.compositions[0].id = 'mark-composition';
  const other = blankDocument(),
    consumer = node('Boolean');
  consumer.params.extra = {
    type: 'Scalar',
    value: 0,
    locked: false,
    expression: `ref(componentComposition, "mark-composition", "${f.l.id}", "${f.transform.id}", "offset", "x")`,
  };
  other.layers[0].nodes.push(consumer);
  project.compositions.push({
    id: 'other-composition',
    name: 'Other',
    width: other.width,
    height: other.height,
    layers: other.layers,
    editor: other.editor,
  });
  project.schemaVersion = 65;
  project.compositionSchemaVersion = 2;
  f.doc.schemaVersion = 65;
  const bound = bindProjectDocumentContext(f.doc, project, 'mark-composition');
  assert.equal(resolveArtworkMoveSelection(bound, [f.l.id]).supported, false);
});

test('schema 65 remains eligible when other Compositions do not use this route', () => {
  const f = fixture(),
    project = migrateLegacyDocumentToCompositionProject(f.doc);
  project.compositions[0].id = 'mark-composition';
  const other = blankDocument();
  project.compositions.push({
    id: 'other-composition',
    name: 'Other',
    width: other.width,
    height: other.height,
    layers: other.layers,
    editor: other.editor,
  });
  project.schemaVersion = 65;
  project.compositionSchemaVersion = 2;
  const owner = createCompositionSessionOwner(project);
  assert.equal(resolveArtworkMoveSelection(owner.snapshot(), [f.l.id]).supported, true);
  command(owner, 'translate-artworks', { layerIds: [f.l.id], delta: [3, 4] });
  assert.deepEqual(
    owner.snapshot().layers[0].nodes.find((n) => n.id === f.transform.id).params.offset.value,
    [3, 4],
  );
});

test('mixed selection moves together, while a locked member rejects the whole command', () => {
  const f = fixture(),
    other = blankLayer('Other');
  const shape = node('Rectangle'),
    fill = node('Fill'),
    output = other.nodes[0];
  other.nodes = [shape, fill, output];
  connect(other, shape.id, fill.id, 'path');
  connect(other, fill.id, output.id, 'path');
  f.doc.layers.push(other);
  const session = new CommandSession(f.doc),
    before = session.snapshot();
  command(session, 'translate-artworks', { layerIds: [f.l.id, other.id], delta: [8, -4] });
  const after = session.snapshot();
  assert.deepEqual(
    after.layers[0].nodes.find((n) => n.id === f.transform.id).params.offset.value,
    [8, -4],
  );
  assert.deepEqual(
    after.layers[1].nodes.find((n) => n.id === shape.id).params.center.value,
    shape.params.center.value.map((v, axis) => v + [8, -4][axis]),
  );
  command(session, 'undo');
  assert.deepEqual(session.snapshot(), before);
  command(session, 'redo');
  assert.deepEqual(session.snapshot(), after);
  command(session, 'undo');
  const state = session.state;
  command(session, 'translate-artworks', { layerIds: [f.l.id, other.id], delta: [0, 0] });
  assert.deepEqual(session.state, state, 'zero movement preserves the pending Redo');
  command(session, 'redo');
  assert.deepEqual(session.snapshot(), after);

  const locked = fixture(),
    bad = blankLayer('Locked');
  bad.locked = true;
  locked.doc.layers.push(bad);
  const rejected = new CommandSession(locked.doc),
    original = rejected.snapshot();
  assert.throws(() =>
    command(rejected, 'translate-artworks', { layerIds: [locked.l.id, bad.id], delta: [8, -4] }),
  );
  assert.deepEqual(rejected.snapshot(), original);
});

test('unrelated extension metadata does not become an external consumer', () => {
  const f = fixture(),
    unrelated = node('Value');
  unrelated.metadata = {
    reference: { nodeId: f.transform.id },
    pathAttachment: { nodeId: f.transform.id },
    compositionReference: { compositionId: f.l.id },
  };
  f.l.nodes.push(unrelated);
  const session = new CommandSession(f.doc);
  assert.equal(resolveArtworkMoveSelection(session.snapshot(), [f.l.id]).supported, true);
  command(session, 'translate-artworks', { layerIds: [f.l.id], delta: [1, 2] });
});

test('stale envelopes reject and preserve the document and pending Redo', () => {
  const f = fixture(),
    session = new CommandSession(f.doc),
    stale = session.state;
  command(session, 'set-document-settings', { key: 'width', value: 961 });
  command(session, 'undo');
  const before = session.snapshot(),
    state = session.state;
  assert.throws(
    () =>
      session.dispatch({
        ...stale,
        type: 'translate-artworks',
        payload: { layerIds: [f.l.id], delta: [1, 2] },
      }),
    /STALE_REVISION/,
  );
  assert.deepEqual(session.snapshot(), before);
  assert.deepEqual(session.state, state);
  command(session, 'redo');
  assert.equal(session.snapshot().width, 961);
});
