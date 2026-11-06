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
  point,
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
function projectFixture(f) {
  const project = migrateLegacyDocumentToCompositionProject(f.doc);
  project.compositions[0].id = 'mark-composition';
  const other = blankDocument(),
    consumer = node('Value');
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
  return { project, otherLayerId: other.layers[0].id, consumerId: consumer.id };
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

test('collapsed rotated Boolean transforms reject the whole selection and preserve Redo', () => {
  for (const [uniform, scale, width] of [
    [false, 1, 0],
    [false, 0, 1],
    [true, 0, 1],
  ]) {
    const f = fixture(),
      other = blankLayer('Other');
    const shape = node('Rectangle'),
      fill = node('Fill');
    other.nodes.unshift(shape, fill);
    connect(other, shape.id, fill.id, 'path');
    connect(other, fill.id, other.output, 'path');
    f.doc.layers.push(other);
    f.transform.params.rotation.value = 45;
    f.transform.params.scale.value = scale;
    f.transform.params.uniformScale = {
      type: 'Boolean',
      value: uniform,
      expression: '',
      locked: false,
    };
    f.transform.params.scaleWidth = { type: 'Scalar', value: width, expression: '', locked: false };
    const session = new CommandSession(f.doc);
    command(session, 'set-document-settings', { key: 'width', value: 961 });
    command(session, 'undo');
    const before = session.snapshot(),
      state = session.state;
    assert.equal(resolveArtworkMoveSelection(before, [f.l.id]).code, 'empty-output');
    assert.throws(() =>
      command(session, 'translate-artworks', { layerIds: [other.id, f.l.id], delta: [3, 4] }),
    );
    assert.deepEqual(session.snapshot(), before);
    assert.deepEqual(session.state, state);
    command(session, 'redo');
    assert.equal(session.snapshot().width, 961);
  }
  for (const [uniform, scale, width] of [
    [true, 1, 0],
    [false, -1, -0.25],
  ]) {
    const f = fixture();
    f.transform.params.rotation.value = 45;
    f.transform.params.scale.value = scale;
    f.transform.params.uniformScale = {
      type: 'Boolean',
      value: uniform,
      expression: '',
      locked: false,
    };
    f.transform.params.scaleWidth = { type: 'Scalar', value: width, expression: '', locked: false };
    assert.equal(resolveArtworkMoveSelection(f.doc, [f.l.id]).supported, true);
  }
});

test('a move cannot change aliased artwork or metadata descriptors', () => {
  for (const where of ['other-artwork', 'own-anchor']) {
    const f = fixture();
    if (where === 'other-artwork') {
      const other = blankLayer('Other'),
        shape = node('Rectangle'),
        fill = node('Fill');
      shape.params.center = f.transform.params.offset;
      other.nodes.unshift(shape, fill);
      connect(other, shape.id, fill.id, 'path');
      connect(other, fill.id, other.output, 'path');
      f.doc.layers.push(other);
    } else {
      f.transform.params.anchor = f.transform.params.offset;
      f.transform.metadata = { shared: f.transform.params.offset };
    }
    f.doc.metadata = {
      layers: f.doc.layers,
      layer: f.l,
      nodes: f.l.nodes,
      node: f.transform,
      params: f.transform.params,
      descriptor: f.transform.params.offset,
    };
    const session = new CommandSession(f.doc);
    command(session, 'set-document-settings', { key: 'width', value: 961 });
    command(session, 'undo');
    const before = session.snapshot();
    const beforeState = session.state;
    command(session, 'translate-artworks', { layerIds: [f.l.id], delta: [0, 0] });
    assert.deepEqual(session.snapshot(), before);
    assert.deepEqual(session.state, beforeState);
    assert.equal(session.historyView.future.length, 1);
    assert.equal(resolveArtworkMoveSelection(before, [f.l.id]).supported, true, where);
    command(session, 'translate-artworks', { layerIds: [f.l.id], delta: [5, 9] });
    const after = session.snapshot(),
      moved = after.layers[0].nodes.find((n) => n.id === f.transform.id);
    assert.deepEqual(moved.params.offset.value, [5, 9]);
    assert.deepEqual(after.metadata, before.metadata);
    assert.deepEqual(
      moved.params.anchor,
      before.layers[0].nodes.find((n) => n.id === f.transform.id).params.anchor,
    );
    if (where === 'own-anchor')
      assert.deepEqual(
        moved.metadata,
        before.layers[0].nodes.find((n) => n.id === f.transform.id).metadata,
      );
    else assert.deepEqual(after.layers[1], before.layers[1]);
    command(session, 'undo');
    assert.deepEqual(session.snapshot(), before);
    command(session, 'redo');
    assert.deepEqual(session.snapshot(), after);
  }
});

test('Composition owner moves an aliased Boolean target without changing the other artwork', () => {
  const f = fixture(),
    other = blankLayer('Other'),
    shape = node('Rectangle'),
    fill = node('Fill');
  shape.params.center = f.transform.params.offset;
  other.nodes.unshift(shape, fill);
  connect(other, shape.id, fill.id, 'path');
  connect(other, fill.id, other.output, 'path');
  f.doc.layers.push(other);
  const project = migrateLegacyDocumentToCompositionProject(f.doc);
  project.compositions[0].layers[1].nodes.find((n) => n.id === shape.id).params.center =
    project.compositions[0].layers[0].nodes.find((n) => n.id === f.transform.id).params.offset;
  const owner = createCompositionSessionOwner(project);
  const initial = owner.snapshot();
  assert.equal(
    initial.layers[0].nodes.find((n) => n.id === f.transform.id).params.offset,
    initial.layers[1].nodes.find((n) => n.id === shape.id).params.center,
  );
  const before = owner.projectSnapshot();
  command(owner, 'translate-artworks', { layerIds: [f.l.id], delta: [5, 9] });
  const after = owner.projectSnapshot();
  assert.deepEqual(after.compositions[0].layers[1], before.compositions[0].layers[1]);
  assert.deepEqual(
    after.compositions[0].layers[0].nodes.find((n) => n.id === f.transform.id).params.offset.value,
    [5, 9],
  );
  command(owner, 'undo');
  assert.deepEqual(owner.projectSnapshot(), before);
  command(owner, 'redo');
  assert.deepEqual(owner.projectSnapshot(), after);
});

test('mixed Boolean moves detach each target sharing a descriptor and preserve metadata', () => {
  const first = fixture(),
    second = fixture();
  second.transform.params.offset = first.transform.params.offset;
  first.doc.layers.push(second.l);
  first.doc.metadata = {
    layers: first.doc.layers,
    first: first.l,
    second: second.l,
    offset: first.transform.params.offset,
  };
  const session = new CommandSession(first.doc),
    before = session.snapshot();
  command(session, 'translate-artworks', { layerIds: [first.l.id, second.l.id], delta: [5, 9] });
  const after = session.snapshot();
  for (const f of [first, second])
    assert.deepEqual(
      after.layers.find((l) => l.id === f.l.id).nodes.find((n) => n.id === f.transform.id).params
        .offset.value,
      [5, 9],
    );
  assert.deepEqual(after.metadata, before.metadata);
  command(session, 'undo');
  assert.deepEqual(session.snapshot(), before);
  command(session, 'redo');
  assert.deepEqual(session.snapshot(), after);
});

test('mixed Boolean and shape or Curve moves isolate every edited path from aliases', () => {
  for (const kind of ['Rectangle', 'Curve', 'Multi Path']) {
    const f = fixture(),
      other = blankLayer('Other'),
      item = node(kind),
      fill = node('Fill');
    f.transform.params.anchor.value = [30, 40];
    if (kind === 'Rectangle') item.params.center = f.transform.params.anchor;
    else {
      item.points = [point(30, 40), point(130, 40), point(80, 140)];
      if (kind === 'Curve') item.params.closed.value = true;
      else {
        f.doc.schemaVersion = 32;
        item.contours = [
          { id: 'mixed-contour', closed: true, points: item.points.map((p) => p.id) },
        ];
        item.units = [{ id: 'mixed-unit', contours: ['mixed-contour'] }];
      }
      item.points[0].position = f.transform.params.anchor;
    }
    other.nodes.unshift(item, fill);
    connect(other, item.id, fill.id, 'path');
    connect(other, fill.id, other.output, 'path');
    f.doc.layers.push(other);
    f.doc.metadata = {
      anchor: f.transform.params.anchor,
      layer: other,
      node: item,
      ...(kind !== 'Rectangle' ? { points: item.points, first: item.points[0] } : {}),
    };
    const session = new CommandSession(f.doc),
      before = session.snapshot();
    const beforePath = evaluate(before.layers[0], {}, {}, before).path;
    command(session, 'translate-artworks', { layerIds: [f.l.id, other.id], delta: [5, 9] });
    const after = session.snapshot(),
      transform = after.layers[0].nodes.find((n) => n.id === f.transform.id);
    assert.deepEqual(transform.params.anchor.value, [30, 40]);
    assert.deepEqual(transform.params.offset.value, [5, 9]);
    assert.deepEqual(after.metadata, before.metadata);
    const afterPath = evaluate(after.layers[0], {}, {}, after).path;
    for (let axis = 0; axis < 2; axis++)
      pathBounds(beforePath)[axis].forEach((v, i) =>
        close(pathBounds(afterPath)[axis][i], v + [5, 9][axis]),
      );
    const movedItem = after.layers[1].nodes.find((n) => n.id === item.id);
    assert.deepEqual(
      kind !== 'Rectangle' ? movedItem.points[0].position.value : movedItem.params.center.value,
      [35, 49],
    );
    command(session, 'undo');
    assert.deepEqual(session.snapshot(), before);
    command(session, 'redo');
    assert.deepEqual(session.snapshot(), after);
  }
});

test('owner callback reentry cannot commit a move based on an obsolete project or gesture', () => {
  for (const phase of ['translate', 'begin', 'update', 'commit']) {
    const f = fixture(),
      { project, otherLayerId, consumerId } = projectFixture(f);
    let owner,
      armed = false,
      token = null,
      nested = null;
    const notifications = [];
    owner = createCompositionSessionOwner(project, {
      onCommitted: (event) => notifications.push(event),
      fonts: () => {
        if (armed) {
          armed = false;
          if (token) command(owner, 'cancel-gesture', { gestureId: token.gestureId });
          owner.switchComposition('other-composition');
          command(owner, 'set-parameter-expression', {
            layerId: otherLayerId,
            nodeId: consumerId,
            key: 'value',
            expression: `ref(componentComposition, "mark-composition", "${f.l.id}", "${f.transform.id}", "offset", "x")`,
          });
          owner.switchComposition('mark-composition');
          nested = {
            project: owner.projectSnapshot(),
            state: owner.state,
            history: owner.historyState,
          };
        }
        return {};
      },
    });
    if (phase === 'update' || phase === 'commit') {
      token = command(owner, 'begin-gesture', {
        target: { kind: 'artwork-translate', layerIds: [f.l.id] },
      });
      if (phase === 'commit')
        token = command(owner, 'update-gesture', {
          gestureId: token.gestureId,
          expectedPreviewRevision: token.expectedPreviewRevision,
          update: { delta: [8, 0] },
        });
    }
    notifications.length = 0;
    armed = true;
    const invoke = () =>
      phase === 'translate'
        ? command(owner, 'translate-artworks', { layerIds: [f.l.id], delta: [8, 0] })
        : phase === 'begin'
          ? command(owner, 'begin-gesture', {
              target: { kind: 'artwork-translate', layerIds: [f.l.id] },
            })
          : command(owner, `${phase}-gesture`, {
              gestureId: token.gestureId,
              expectedPreviewRevision: token.expectedPreviewRevision,
              ...(phase === 'update' ? { update: { delta: [8, 0] } } : {}),
            });
    assert.throws(invoke, /STALE_(SESSION|REVISION|GESTURE)/, phase);
    assert.ok(nested, phase);
    assert.deepEqual(owner.projectSnapshot(), nested.project, phase);
    assert.deepEqual(owner.state, nested.state, phase);
    assert.deepEqual(owner.historyState, nested.history, phase);
    assert.equal(owner.hasGesture, false, phase);
    assert.equal(notifications.length, 1, phase);
    assert.equal(resolveArtworkMoveSelection(owner.snapshot(), [f.l.id]).supported, false, phase);
  }
});

test('standalone callback reentry keeps the nested edit and emits only its notification', () => {
  const f = fixture(),
    notifications = [];
  let session,
    armed = false;
  session = new CommandSession(f.doc, {
    onCommitted: (event) => notifications.push(event),
    fonts: () => {
      if (armed) {
        armed = false;
        command(session, 'set-document-settings', { key: 'width', value: 961 });
      }
      return {};
    },
  });
  armed = true;
  assert.throws(
    () => command(session, 'translate-artworks', { layerIds: [f.l.id], delta: [8, 0] }),
    /STALE_REVISION/,
  );
  assert.equal(session.snapshot().width, 961);
  assert.deepEqual(
    session.snapshot().layers[0].nodes.find((n) => n.id === f.transform.id).params.offset.value,
    [0, 0],
  );
  assert.equal(notifications.length, 1);
});

test('owner precommit callback cannot overwrite a nested project-only edit', () => {
  const f = fixture(),
    { project } = projectFixture(f);
  let owner,
    armed = false,
    calls = 0,
    nested = null;
  const notifications = [];
  owner = createCompositionSessionOwner(project, {
    onCommitted: (event) => notifications.push(event),
    fonts: () => {
      if (armed && ++calls === 4) {
        armed = false;
        owner.renameComposition('other-composition', 'Renamed');
        nested = {
          project: owner.projectSnapshot(),
          state: owner.state,
          history: owner.historyState,
        };
      }
      return {};
    },
  });
  armed = true;
  assert.throws(
    () => command(owner, 'translate-artworks', { layerIds: [f.l.id], delta: [8, 0] }),
    /STALE_(SESSION|REVISION)/,
  );
  assert.ok(nested);
  assert.deepEqual(owner.projectSnapshot(), nested.project);
  assert.deepEqual(owner.state, nested.state);
  assert.deepEqual(owner.historyState, nested.history);
  assert.equal(notifications.length, 1);
});

test('the final move authority check cannot overwrite a reentrant Session edit', () => {
  const f = fixture();
  let session,
    calls = 0;
  session = new CommandSession(f.doc, {
    captureArtworkMoveAuthority: () => () => {
      if (++calls === 2) command(session, 'set-document-settings', { key: 'width', value: 961 });
    },
  });
  assert.throws(
    () => command(session, 'translate-artworks', { layerIds: [f.l.id], delta: [8, 0] }),
    /STALE_REVISION/,
  );
  assert.equal(session.snapshot().width, 961);
  assert.deepEqual(
    session.snapshot().layers[0].nodes.find((n) => n.id === f.transform.id).params.offset.value,
    [0, 0],
  );
});

test('an artwork gesture cannot resume after its project binding changes at the same revision', () => {
  const f = fixture(),
    { project, otherLayerId, consumerId } = projectFixture(f);
  f.doc.schemaVersion = 65;
  const session = new CommandSession(f.doc);
  session.setProjectContext(project, 'mark-composition');
  command(session, 'set-document-settings', { key: 'width', value: 961 });
  command(session, 'undo');
  let token = command(session, 'begin-gesture', {
    target: { kind: 'artwork-translate', layerIds: [f.l.id] },
  });
  token = command(session, 'update-gesture', {
    gestureId: token.gestureId,
    expectedPreviewRevision: token.expectedPreviewRevision,
    update: { delta: [8, 0] },
  });
  const changedProject = structuredClone(project);
  changedProject.compositions[1].layers
    .find((l) => l.id === otherLayerId)
    .nodes.find((n) => n.id === consumerId).params.value.expression =
    `ref(componentComposition, "mark-composition", "${f.l.id}", "${f.transform.id}", "offset", "x")`;
  session.setProjectContext(changedProject, 'mark-composition');
  const before = session.snapshot(),
    state = session.state;
  assert.throws(
    () =>
      command(session, 'commit-gesture', {
        gestureId: token.gestureId,
        expectedPreviewRevision: token.expectedPreviewRevision,
      }),
    /STALE_(SESSION|REVISION|GESTURE)/,
  );
  assert.deepEqual(session.snapshot(), before);
  assert.deepEqual(session.state, state);
  command(session, 'cancel-gesture', { gestureId: token.gestureId });
  command(session, 'redo');
  assert.equal(session.snapshot().width, 961);
});

test('a changed project binding rejects the next artwork preview', () => {
  const f = fixture(),
    { project } = projectFixture(f);
  f.doc.schemaVersion = 65;
  const session = new CommandSession(f.doc);
  session.setProjectContext(project, 'mark-composition');
  const token = command(session, 'begin-gesture', {
    target: { kind: 'artwork-translate', layerIds: [f.l.id] },
  });
  session.setProjectContext(structuredClone(project), 'mark-composition');
  const before = session.snapshot(),
    state = session.state;
  assert.throws(
    () =>
      command(session, 'update-gesture', {
        gestureId: token.gestureId,
        expectedPreviewRevision: token.expectedPreviewRevision,
        update: { delta: [8, 0] },
      }),
    /STALE_GESTURE/,
  );
  assert.deepEqual(session.snapshot(), before);
  assert.deepEqual(session.state, state);
  command(session, 'cancel-gesture', { gestureId: token.gestureId });
});

test('a nested gesture cancellation remains cancelled without committing or clearing Redo', () => {
  const f = fixture(),
    notifications = [];
  let session,
    token,
    armed = false;
  session = new CommandSession(f.doc, {
    onCommitted: (event) => notifications.push(event),
    fonts: () => {
      if (armed) {
        armed = false;
        command(session, 'cancel-gesture', { gestureId: token.gestureId });
      }
      return {};
    },
  });
  command(session, 'set-document-settings', { key: 'width', value: 961 });
  command(session, 'undo');
  token = command(session, 'begin-gesture', {
    target: { kind: 'artwork-translate', layerIds: [f.l.id] },
  });
  token = command(session, 'update-gesture', {
    gestureId: token.gestureId,
    expectedPreviewRevision: token.expectedPreviewRevision,
    update: { delta: [8, 0] },
  });
  const before = session.snapshot(),
    state = session.state;
  notifications.length = 0;
  armed = true;
  assert.throws(
    () =>
      command(session, 'commit-gesture', {
        gestureId: token.gestureId,
        expectedPreviewRevision: token.expectedPreviewRevision,
      }),
    /STALE_GESTURE/,
  );
  assert.deepEqual(session.snapshot(), before);
  assert.deepEqual(session.state, state);
  assert.equal(session.hasGesture, false);
  assert.equal(notifications.length, 0);
  command(session, 'redo');
  assert.equal(session.snapshot().width, 961);
});

test('a nested gesture update keeps its newer preview when an outer commit becomes stale', () => {
  const f = fixture();
  let session,
    token,
    armed = false,
    newer;
  session = new CommandSession(f.doc, {
    fonts: () => {
      if (armed) {
        armed = false;
        token = command(session, 'update-gesture', {
          gestureId: token.gestureId,
          expectedPreviewRevision: token.expectedPreviewRevision,
          update: { delta: [12, 0] },
        });
        newer = session.previewSnapshot(token.gestureId);
      }
      return {};
    },
  });
  token = command(session, 'begin-gesture', {
    target: { kind: 'artwork-translate', layerIds: [f.l.id] },
  });
  token = command(session, 'update-gesture', {
    gestureId: token.gestureId,
    expectedPreviewRevision: token.expectedPreviewRevision,
    update: { delta: [8, 0] },
  });
  const before = session.snapshot(),
    state = session.state;
  armed = true;
  assert.throws(
    () =>
      command(session, 'commit-gesture', {
        gestureId: token.gestureId,
        expectedPreviewRevision: token.expectedPreviewRevision,
      }),
    /STALE_GESTURE/,
  );
  assert.deepEqual(session.snapshot(), before);
  assert.deepEqual(session.state, state);
  assert.deepEqual(session.previewSnapshot(token.gestureId), newer);
  assert.deepEqual(
    newer.document.layers[0].nodes.find((n) => n.id === f.transform.id).params.offset.value,
    [12, 0],
  );
  command(session, 'commit-gesture', {
    gestureId: token.gestureId,
    expectedPreviewRevision: token.expectedPreviewRevision,
  });
  assert.deepEqual(
    session.snapshot().layers[0].nodes.find((n) => n.id === f.transform.id).params.offset.value,
    [12, 0],
  );
});

test('translation that overflows the affine result or collapses its geometry is atomic', () => {
  for (const [anchor, delta] of [
    [1e308, 1e308],
    [0, 1e19],
  ]) {
    const f = fixture();
    f.transform.params.anchor.value = [anchor, 0];
    const session = new CommandSession(f.doc);
    command(session, 'set-document-settings', { key: 'width', value: 961 });
    command(session, 'undo');
    const before = session.snapshot(),
      state = session.state;
    assert.equal(resolveArtworkMoveSelection(before, [f.l.id]).supported, true);
    assert.throws(() =>
      command(session, 'translate-artworks', { layerIds: [f.l.id], delta: [delta, 0] }),
    );
    assert.deepEqual(session.snapshot(), before);
    assert.deepEqual(session.state, state);
    command(session, 'redo');
    assert.equal(session.snapshot().width, 961);
  }
});

test('a diagonal floating-point collapse after a large translation is atomic', () => {
  const f = fixture();
  f.transform.params.rotation.value = 45;
  const session = new CommandSession(f.doc);
  command(session, 'set-document-settings', { key: 'width', value: 961 });
  command(session, 'undo');
  const before = session.snapshot(),
    state = session.state;
  assert.throws(() =>
    command(session, 'translate-artworks', {
      layerIds: [f.l.id],
      delta: [2 ** 60 + 512, 2 ** 60 + 256],
    }),
  );
  assert.deepEqual(session.snapshot(), before);
  assert.deepEqual(session.state, state);
  command(session, 'redo');
  assert.equal(session.snapshot().width, 961);
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
