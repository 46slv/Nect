import assert from 'node:assert/strict';
import test from 'node:test';
import { deserialize, exportSVG, serialize } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  deserializeCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createLiveEditorProtocol } from '../src/live-editor-protocol.js';
import { createLiveSolidFillColor, readLiveSolidFillColor } from '../src/live-solid-fill-color.js';
import { orderingDocument } from './fixtures/artwork-ordering.js';

test('solid Fill matches the GUI, preserves alpha and history, and never edits another node', () => {
  const doc = orderingDocument();
  const [layer, other] = doc.layers;
  const [shape, fill] = layer.nodes;
  fill.params.color.value.alpha = 0.4;
  other.locked = true;
  const seed = serialize(doc);
  const owner = () =>
    createCompositionSessionOwner(deserializeCompositionProject(seed, deserialize));
  const live = owner(),
    gui = owner();
  const state = (o) => serializeCompositionProject(o.projectSnapshot());
  const readTarget = () => ({
    documentGeneration: 1,
    compositionId: live.activeCompositionId,
    sessionId: live.state.sessionId,
    sessionRevision: live.state.expectedRevision,
    projectRevision: live.projectRevision,
  });
  let blocked = false;
  const setSolidFillColor = createLiveSolidFillColor({
    readTarget,
    getOwner: () => live,
    isEditBlocked: () => blocked,
    commit: (o, cmd) => o.dispatch(cmd),
  });
  const handle = createLiveEditorProtocol({ readTarget, setSolidFillColor });
  const request = {
    version: 1,
    operation: 'fill.setSolidColor',
    arguments: { target: readTarget(), layerId: layer.id, nodeId: fill.id, hex: '#1177AA' },
  };
  const edit = (override) =>
    handle({ ...request, arguments: { ...request.arguments, ...override } });
  const original = state(live);
  assert.equal(createLiveEditorProtocol({ readTarget })(request).status, 'unsupported-operation');
  assert.equal(edit({ nodeId: shape.id }).status, 'unsupported-target');
  assert.equal(edit({ layerId: other.id, nodeId: other.nodes[1].id }).status, 'rejected');
  for (const invalid of [
    { hex: '#fff' },
    { hex: '#11223344' },
    { hex: '#123456\n' },
    { alpha: 0.7 },
  ])
    assert.equal(edit(invalid).status, 'invalid-arguments');
  blocked = true;
  assert.equal(handle(request).status, 'edit-in-progress');
  blocked = false;
  assert.equal(state(live), original);
  gui.dispatch({
    ...gui.state,
    type: 'set-parameter',
    payload: {
      layerId: layer.id,
      nodeId: fill.id,
      key: 'color',
      value: { hex: '#1177AA', alpha: 0.4 },
    },
  });
  assert.equal(handle(request).status, 'changed');
  const after = state(live);
  assert.equal(after, state(gui));
  assert.equal(exportSVG(live.snapshot()), exportSVG(gui.snapshot()));
  assert.match(exportSVG(live.snapshot()), /#1177aa/i);
  assert.equal(live.snapshot().layers[0].nodes[1].params.color.value.alpha, 0.4);
  assert.equal(handle(request).status, 'stale-target');
  assert.equal(edit({ target: readTarget(), hex: '#1177aa' }).status, 'unchanged');
  live.dispatch({ ...live.state, type: 'undo' });
  assert.equal(state(live), original);
  live.dispatch({ ...live.state, type: 'redo' });
  assert.equal(state(live), after);
  assert.equal(
    state(createCompositionSessionOwner(deserializeCompositionProject(after, deserialize))),
    after,
  );
});

test('rejects driven, shared, locked, and bypassed Fill without dispatch', () => {
  const doc = orderingDocument();
  const first = doc.layers[0];
  const fillId = first.nodes[1].id;
  const target = {
    documentGeneration: 1,
    compositionId: 'composition-test',
    sessionId: 'session-test',
    sessionRevision: 0,
    projectRevision: 0,
  };
  for (const alter of [
    (n) => {
      n.params.color.source = 'color-node';
    },
    (n) => {
      n.params.color.expression = 'base';
    },
    (n) => {
      n.params.color.colorSlotId = 'shared-color';
    },
    (n) => {
      n.params.color.locked = true;
    },
    (n) => {
      n.locked = true;
    },
    (n) => {
      n.bypassed = true;
    },
  ]) {
    const copy = structuredClone(doc);
    alter(copy.layers[0].nodes[1]);
    const handle = createLiveSolidFillColor({
      readTarget: () => target,
      getOwner: () => ({ snapshot: () => copy }),
      isEditBlocked: () => false,
      commit: () => assert.fail('protected Fill reached Session dispatch'),
    });
    assert.equal(
      handle({ target, layerId: first.id, nodeId: fillId, hex: '#1177aa' }).status,
      'rejected',
    );
  }
});

test('read exact Fill exposes only its authored color and leaves revision and history intact', () => {
  const doc = orderingDocument();
  const [layer, other] = doc.layers;
  const [shape, fill] = layer.nodes;
  fill.params.color.value = { hex: '#1177AA', alpha: 0.4 };
  other.locked = true;
  const owner = createCompositionSessionOwner(
    deserializeCompositionProject(serialize(doc), deserialize),
  );
  const target = {
    documentGeneration: 1,
    compositionId: owner.activeCompositionId,
    sessionId: owner.state.sessionId,
    sessionRevision: owner.state.expectedRevision,
    projectRevision: owner.projectRevision,
  };
  const readTarget = () => target;
  const read = (args) =>
    readLiveSolidFillColor({ readTarget, readDocument: () => owner.snapshot() }, args);
  const handle = createLiveEditorProtocol({ readTarget, readSolidFillColor: read });
  const request = {
    version: 1,
    operation: 'fill.readSolidColor',
    arguments: { target, layerId: layer.id, nodeId: fill.id },
  };
  const snap = serializeCompositionProject(owner.projectSnapshot());
  assert.equal(createLiveEditorProtocol({ readTarget })(request).status, 'unsupported-operation');
  assert.deepEqual(handle(request), {
    status: 'ok',
    target,
    mode: 'literal',
    rgb: '#1177aa',
    alpha: 0.4,
    locked: false,
    bypassed: false,
    editable: true,
  });
  const override = (extra) => handle({ ...request, arguments: { ...request.arguments, ...extra } });
  const locked = override({ layerId: other.id, nodeId: other.nodes[1].id });
  assert.equal(locked.mode, 'literal');
  assert.equal(locked.locked, true);
  assert.equal(locked.editable, false);
  assert.equal(override({ nodeId: shape.id }).status, 'unsupported-target');
  assert.equal(override({ hex: '#1177aa' }).status, 'invalid-arguments');
  assert.equal(override({ target: { ...target, documentGeneration: 2 } }).status, 'stale-target');
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), snap);
  assert.equal(owner.state.expectedRevision, target.sessionRevision);
  assert.equal(owner.projectRevision, target.projectRevision);

  for (const control of [
    { expression: 'private-expression' },
    { source: 'private-id' },
    { colorSlotId: 'private-slot' },
  ]) {
    const color = fill.params.color;
    fill.params.color = { ...color, ...control };
    const actual = readLiveSolidFillColor(
      { readTarget, readDocument: () => doc },
      request.arguments,
    );
    assert.deepEqual(actual, {
      status: 'ok',
      target,
      mode: 'controlled',
      rgb: null,
      alpha: null,
      locked: false,
      bypassed: false,
      editable: false,
    });
    fill.params.color = color;
  }
  let reads = 0;
  const stale = readLiveSolidFillColor(
    {
      readTarget: () => (reads++ ? { ...target, documentGeneration: 2 } : target),
      readDocument: () => doc,
    },
    request.arguments,
  );
  assert.deepEqual(stale, { status: 'stale-target' });
});
