import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { blankDocument, connect, evaluate, node, pathBounds } from '../src/core.js';
import { parseFont } from '../src/fonts.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { resolveArtworkMoveTarget } from '../src/artwork-move-target.js';
import { planTransformEffect, TRANSFORM_EFFECT_ID } from '../src/effect-insert-plan.js';

function fixture(options = {}, sourceType = 'Rectangle') {
  const doc = blankDocument(),
    layer = doc.layers[0],
    source = node(sourceType),
    fill = node('Fill'),
    output = node('Output');
  if (sourceType === 'Rectangle') {
    source.params.center.value = [210, 180];
    source.params.size.value = [120, 80];
  }
  fill.params.color.value = { hex: '#345678', alpha: 0.65 };
  layer.nodes = [source, fill, output];
  layer.output = output.id;
  connect(layer, source.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  const project = migrateLegacyDocumentToCompositionProject(doc),
    owner = createCompositionSessionOwner(project, options);
  const send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });
  const plan = () => planTransformEffect(owner.historyView.doc, layer.id);
  const apply = (expectedTarget = plan().target) =>
    send('apply-effect', { layerId: layer.id, effectId: TRANSFORM_EFFECT_ID, expectedTarget });
  return { owner, send, plan, apply, layer, source, fill, output, project };
}

function painted(doc, fonts = {}) {
  const result = evaluate(doc.layers[0], fonts, {}, doc);
  assert.equal(result.error, null);
  return result.path;
}

function equalGeometry(a, b) {
  assert.deepEqual(a.fill, b.fill);
  assert.equal(a.contours.length, b.contours.length);
  for (const [index, contour] of a.contours.entries()) {
    const after = b.contours[index];
    assert.equal(after.id, contour.id);
    assert.equal(after.points.length, contour.points.length);
    for (const [pointIndex, point] of contour.points.entries()) {
      const next = after.points[pointIndex];
      assert.equal(next.id, point.id);
      for (const key of ['position', 'handleA', 'handleB'])
        for (const axis of [0, 1])
          assert.ok(Math.abs(point[key][axis] - next[key][axis]) < 1e-6, key);
    }
  }
}

test('Apply Transform preserves appearance and IDs, and is one undoable project edit', () => {
  const f = fixture(),
    before = f.owner.snapshot(),
    beforeProject = f.owner.projectSnapshot(),
    prior = painted(before),
    past = f.owner.historyView.past.length,
    result = f.apply(),
    after = f.owner.snapshot(),
    next = painted(after);
  assert.equal(result.changed, true);
  assert.equal(result.effectId, TRANSFORM_EFFECT_ID);
  assert.equal(f.owner.historyView.past.length, past + 1);
  equalGeometry(prior, next);
  const actualLayer = after.layers[0],
    transform = actualLayer.nodes.find((item) => item.id === result.nodeId),
    changedFill = actualLayer.nodes.find((item) => item.id === f.fill.id);
  assert.equal(transform.type, 'Transform');
  assert.equal(transform.params.path.source, f.source.id);
  assert.equal(changedFill.params.path.source, transform.id);
  const originalFill = before.layers[0].nodes.find((item) => item.id === f.fill.id);
  const expectedFill = structuredClone(originalFill);
  expectedFill.params.path.source = transform.id;
  assert.deepEqual(changedFill, expectedFill);
  assert.deepEqual(
    actualLayer.nodes.filter((item) => ![f.fill.id, transform.id].includes(item.id)),
    before.layers[0].nodes.filter((item) => item.id !== f.fill.id),
  );
  assert.equal(resolveArtworkMoveTarget(after, f.layer.id).target.nodeId, transform.id);
  const afterProject = f.owner.projectSnapshot();
  f.send('undo');
  assert.deepEqual(f.owner.projectSnapshot(), beforeProject);
  f.send('redo');
  assert.deepEqual(f.owner.projectSnapshot(), afterProject);
  assert.ok(f.owner.snapshot().layers[0].nodes.some((item) => item.id === result.nodeId));

  f.send('set-parameter', {
    layerId: f.layer.id,
    nodeId: result.nodeId,
    key: 'offset',
    value: [15, -7],
  });
  const moved = f.owner.snapshot();
  for (const [axis, delta] of [15, -7].entries())
    pathBounds(painted(moved))[axis].forEach((value, index) =>
      assert.ok(Math.abs(value - (pathBounds(prior)[axis][index] + delta)) < 1e-6),
    );
  f.send('translate-artworks', { layerIds: [f.layer.id], delta: [3, 4] });
  assert.deepEqual(
    f.owner.snapshot().layers[0].nodes.find((item) => item.id === result.nodeId).params.offset
      .value,
    [18, -3],
  );
  f.send('align-artworks', { layerIds: [f.layer.id], action: 'left', reference: 'artboard' });
  const serialized = serializeCompositionProject(f.owner.projectSnapshot()),
    reopened = createCompositionSessionOwner(deserializeCompositionProject(serialized));
  assert.deepEqual(reopened.projectSnapshot(), f.owner.projectSnapshot());
  assert.ok(reopened.snapshot().layers[0].nodes.some((item) => item.id === result.nodeId));
  assert.equal(resolveArtworkMoveTarget(reopened.snapshot(), f.layer.id).supported, true);
});

test('Identity Transform retains the output geometry of each supported artwork and Text', () => {
  const bytes = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url)),
    font = parseFont(bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength)),
    fonts = { default: font };
  for (const type of ['Circle', 'Ellipse', 'Polygon', 'Star', 'Text']) {
    const f = fixture({ fonts: () => fonts }, type),
      before = painted(f.owner.snapshot(), fonts),
      plan = planTransformEffect(f.owner.historyView.doc, f.layer.id, { fonts });
    assert.equal(plan.supported, true, `${type}: ${plan.reason}`);
    f.apply(plan.target);
    equalGeometry(before, painted(f.owner.snapshot(), fonts));
  }
});

test('Invalid, stale and unsupported Apply requests preserve Project, History and Redo', () => {
  const f = fixture(),
    target = f.plan().target;
  f.send('set-document-settings', { key: 'width', value: 1024 });
  f.send('undo');
  const before = f.owner.projectSnapshot(),
    state = f.owner.state,
    future = f.owner.historyView.future.length,
    past = f.owner.historyView.past.length;
  for (const payload of [
    { layerId: f.layer.id, effectId: TRANSFORM_EFFECT_ID },
    { layerId: f.layer.id, effectId: 'builtin.unknown', expectedTarget: target },
    {
      layerId: f.layer.id,
      effectId: TRANSFORM_EFFECT_ID,
      expectedTarget: { ...target, producerNodeId: 'other' },
    },
    {
      layerId: f.layer.id,
      effectId: TRANSFORM_EFFECT_ID,
      expectedTarget: { ...target, producerPort: 'position' },
    },
    { layerId: 'missing', effectId: TRANSFORM_EFFECT_ID, expectedTarget: target },
  ]) {
    assert.throws(() => f.send('apply-effect', payload));
    assert.deepEqual(f.owner.projectSnapshot(), before);
    assert.deepEqual(f.owner.state, state);
    assert.equal(f.owner.historyView.past.length, past);
    assert.equal(f.owner.historyView.future.length, future);
  }
  const stale = { ...f.owner.state };
  f.send('redo');
  assert.throws(
    () =>
      f.owner.dispatch({
        ...stale,
        type: 'apply-effect',
        payload: { layerId: f.layer.id, effectId: TRANSFORM_EFFECT_ID, expectedTarget: target },
      }),
    /STALE/,
  );
  assert.equal(f.owner.snapshot().width, 1024);
});

test('A reentrant project change during Apply cannot commit the old target', () => {
  let armed = false,
    owner,
    nested;
  const f = fixture({
    fonts: () => {
      if (armed) {
        armed = false;
        owner.dispatch({
          ...owner.state,
          type: 'set-document-settings',
          payload: { key: 'width', value: 1130 },
        });
        nested = owner.projectSnapshot();
      }
      return {};
    },
  });
  owner = f.owner;
  const target = f.plan().target;
  armed = true;
  assert.throws(() => f.apply(target), /STALE/);
  assert.deepEqual(owner.projectSnapshot(), nested);
  assert.equal(owner.snapshot().width, 1130);
  assert.equal(
    owner.snapshot().layers[0].nodes.some((item) => item.type === 'Transform'),
    false,
  );
});

test('An active Composition switch during Apply cannot affect the new or previous Composition', () => {
  const f = fixture(),
    project = f.owner.projectSnapshot(),
    sourceId = project.compositions[0].id,
    other = migrateLegacyDocumentToCompositionProject(blankDocument()).compositions[0];
  other.id = 'other-composition';
  project.compositions.push(other);
  let owner,
    armed = false;
  owner = createCompositionSessionOwner(project, {
    activeCompositionId: sourceId,
    fonts: () => {
      if (armed) {
        armed = false;
        owner.switchComposition(other.id);
      }
      return {};
    },
  });
  const target = planTransformEffect(owner.historyView.doc, f.layer.id).target,
    before = owner.projectSnapshot(),
    state = owner.state;
  armed = true;
  assert.throws(
    () =>
      owner.dispatch({
        ...state,
        type: 'apply-effect',
        payload: { layerId: f.layer.id, effectId: TRANSFORM_EFFECT_ID, expectedTarget: target },
      }),
    /STALE/,
  );
  assert.equal(owner.activeCompositionId, other.id);
  assert.deepEqual(owner.projectSnapshot(), before);
});

test('A Text draft started during Apply remains editable and preserves Redo', () => {
  const f = fixture(),
    project = f.owner.projectSnapshot(),
    text = node('Text');
  text.text = 'A';
  project.compositions[0].layers[0].nodes.push(text);
  let owner,
    armed = false,
    draft;
  owner = createCompositionSessionOwner(project, {
    fonts: () => {
      if (armed) {
        armed = false;
        draft = owner.dispatch({
          ...owner.state,
          type: 'begin-text-draft',
          payload: { layerId: f.layer.id, nodeId: text.id },
        });
        draft = owner.dispatch({
          ...owner.state,
          type: 'stage-text-draft',
          payload: {
            draftId: draft.draftId,
            expectedDraftRevision: draft.draftRevision,
            action: 'edit',
            previousRaw: 'A',
            nextRaw: 'AB',
          },
        });
      }
      return {};
    },
  });
  const send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });
  send('set-document-settings', { key: 'width', value: 1024 });
  send('undo');
  const target = planTransformEffect(owner.historyView.doc, f.layer.id).target,
    before = owner.projectSnapshot(),
    future = owner.historyView.future;
  armed = true;
  assert.throws(
    () =>
      send('apply-effect', {
        layerId: f.layer.id,
        effectId: TRANSFORM_EFFECT_ID,
        expectedTarget: target,
      }),
    /draft|gesture|active/i,
  );
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.historyView.future, future);
  assert.equal(owner.hasChangedTextDrafts, true);
  send('commit-text-draft', { draftId: draft.draftId, expectedDraftRevision: draft.draftRevision });
  assert.equal(owner.snapshot().layers[0].nodes.find((item) => item.id === text.id).text, 'AB');
});
